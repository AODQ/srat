#include <srat/rasterizer-phase-rasterize.hpp>

#include <srat/gfx-image.hpp>
#include <srat/rasterizer-interpolant.hpp>
#include <srat/rasterizer-tile-grid.hpp>

#include <tbb/parallel_for.h>

static inline void rasterize_tile_write_pixel(
	i32 const x,
	i32 const y,
	f32v4x8 const & interpColor,
	f32x8 const & lanesDepth,
	u32x8 const & lanesMask,
	i32v2 const & targetDim,
	srat::slice<u32> const & imageData,
	srat::slice<u16> const & depthData,
	srat::array<f32, 8> const & depth16Lanes,
	srat::array<u32, 8> & maskLanes
) {
	Let rowPixels = imageData.subslice((y*targetDim.x) + x, 8);
	Let rowDepths = depthData.subslice((y*targetDim.x) + x, 8);
	// sequential write
	if (srat_sequential_writes()) {
		f32x8_store(lanesDepth, depth16Lanes);
		u32x8_store(lanesMask, maskLanes);
		for (auto lane = 0; lane < 8; ++lane) {
			if (!maskLanes[lane]) { continue; }
			i32 const pixelX = x + lane;
			if (pixelX < 0 || pixelX >= (i32)targetDim.x) { continue; }
			// -- depth test
			u16 depth16 = (
				T_roundf_positive<u16>(
					f32_clamp(depth16Lanes[lane], 0.f, 1.f) * (f32)UINT16_MAX
				)
			);
			if (depth16 < rowDepths[lane]) {
				continue;
			}

			// -- write depth
			rowDepths[lane] = depth16;

			// -- write color
			rowPixels[lane] = (
				as_rgba(f32v4(
					interpColor.x.lane(lane),
					interpColor.y.lane(lane),
					interpColor.z.lane(lane),
					interpColor.w.lane(lane)
				))
			);
		}
		return;
	}
	return;
	// SIMD write with atomic depth test
	// convert to 16-bit depth using _mm256_cvtps_epi32
	f32x8 const depthScaled = lanesDepth * f32x8_splat((f32)UINT16_MAX);
	f32x8 const depthClamped = (
		f32x8_clamp(depthScaled, f32x8_zero(), f32x8_splat((f32)UINT16_MAX))
	);
	__m256i const depth16i = _mm256_cvtps_epi32(depthClamped.v); // 8xi32

	// pack to 8xu16 using __m128i
	__m128i const depth16iLo = _mm256_extractf128_si256(depth16i, 0);
	__m128i const depth16iHi = _mm256_extractf128_si256(depth16i, 1);
	__m128i const depth16u16 = _mm_packus_epi32(depth16iLo, depth16iHi);

	// load current depth
	__m128i const depthCmp16 = _mm_loadu_si128(
		reinterpret_cast<const __m128i *>(rowDepths.ptr())
	);

	// zero extend both for 32-bit comparison
	__m256i const depthCmp32 = _mm256_cvtepu16_epi32(depthCmp16);

	// perform depth test: newDepth < currentDepth
	__m256i const depthMask = _mm256_cmpgt_epi32(depthCmp32, depth16i);
	u32x8 const depthPass = { _mm256_xor_si256(depthMask, _mm256_set1_epi32(-1)) };
	u32x8 const finalMask = lanesMask & depthPass;

	// Compress finalMask from 32-bit lanes to 16-bit lanes
__m128i const maskLo = _mm256_castsi256_si128(finalMask.v);
__m128i const maskHi = _mm256_extracti128_si256(finalMask.v, 1);
__m128i const mask16 = _mm_packs_epi32(maskLo, maskHi); // 0x0000 or 0xFFFF per lane

// Blend: where mask16 is 0xFFFF, pick new; else keep old
__m128i const blendedDepth = _mm_blendv_epi8(depth16u16, depthCmp16, mask16);

// Single store of all 8 × u16
_mm_storeu_si128((__m128i*)rowDepths.ptr(), blendedDepth);

auto cvt255 = [](f32x8 ch) -> __m256i {
    f32x8 c = f32x8_max(f32x8_zero(), f32x8_min(ch, f32x8_splat(1.0f)));
    return _mm256_cvtps_epi32((c * f32x8_splat(255.0f) + f32x8_splat(0.5f)).v);
};

__m256i const r = cvt255(interpColor.x);              // low byte of each i32
__m256i const g = _mm256_slli_epi32(cvt255(interpColor.y), 8);
__m256i const b = _mm256_slli_epi32(cvt255(interpColor.z), 16);
__m256i const a = _mm256_slli_epi32(cvt255(interpColor.w), 24);

__m256i const packed = _mm256_or_si256(r, _mm256_or_si256(g, _mm256_or_si256(b, a)));

_mm256_maskstore_epi32((int*)rowPixels.ptr(), finalMask.v, packed);

}

namespace {
	struct RasterizeTriangleParams {
		srat::TileGrid const & tileGrid;
		srat::TileBin const & tileBin;
		srat::slice<srat::TileTriangleData const> const & triangleData;
		i32v2 tile;
		u32 triIdx;
		i32v2 targetDim;
		srat::slice<u32> const & imageData;
		srat::slice<u16> const & depthData;
		srat::gfx::Image const & boundTexture;
	};
}

static void rasterize_triangle(
	RasterizeTriangleParams const & ci
) {
	auto & targetDim = ci.targetDim;
	auto & imageData = ci.imageData;
	auto & depthData = ci.depthData;
	Let tri = ci.triangleData[ci.tileBin.triangleIndices[ci.triIdx]];
	i32v2 const sp0 = tri.screenPos[0];
	i32v2 const sp1 = tri.screenPos[1];
	i32v2 const sp2 = tri.screenPos[2];
	f32 const d0 = tri.depth[0];
	f32 const d1 = tri.depth[1];
	f32 const d2 = tri.depth[2];
	srat::array<f32, 3> const perspectiveW = {
		tri.perspectiveW[0],
		tri.perspectiveW[1],
		tri.perspectiveW[2]
	};
	f32v2 const uv0 = tri.uv[0];
	f32v2 const uv1 = tri.uv[1];
	f32v2 const uv2 = tri.uv[2];
// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
	alignas(32) srat::array<f32, 8> depth16Lanes;
	alignas(32) srat::array<u32, 8> maskLanes;
// NOLINTEND(cppcoreguidelines-pro-type-member-init)

	// -- calculate triangle area
	f32v2 const v0f = as_f32v2(sp0);
	f32v2 const v1f = as_f32v2(sp1);
	f32v2 const v2f = as_f32v2(sp2);
	f32 const area = f32v2_triangle_parallelogram_area(v0f, v1f, v2f);
	f32 const rcpArea = 1.0f / area;

	// -- bounding box
	i32bbox2 const bboxTri = i32bbox2_from_triangle(sp0, sp1, sp2);
	i32bbox2 const bboxImg = {
		.min = {
			(i32)ci.tile.x * (i32)srat_tile_size(),
			(i32)ci.tile.y * (i32)srat_tile_size(),
		},
		.max = {
			(i32)(ci.tile.x+1) * (i32)srat_tile_size() - 1,
			(i32)(ci.tile.y+1) * (i32)srat_tile_size() - 1
		},
	};
	i32bbox2 const bbox = {
		.min = {
			i32_max(bboxTri.min.x, bboxImg.min.x),
			i32_max(bboxTri.min.y, bboxImg.min.y),
		},
		.max = {
			i32_min(bboxTri.max.x, bboxImg.max.x),
			i32_min(bboxTri.max.y, bboxImg.max.y),
		}
	};

	// -- precompute barycentric interpolations
	f32 const dw0Dx = (v1f.y - v2f.y);
	f32 const dw1Dx = (v2f.y - v0f.y);
	f32 const dw2Dx = (v0f.y - v1f.y);
	f32 const dw0Dy = (v2f.x - v1f.x);
	f32 const dw1Dy = (v0f.x - v2f.x);
	f32 const dw2Dy = (v1f.x - v0f.x);

	// -- x offsets for 8 lanes

	Let scalarEdge = (
		[](f32v2 const & a, f32v2 const & b, f32 const x, f32 const y) -> float {
			return (a.y - b.y) * x + (b.x - a.x) * y + (a.x * b.y - a.y * b.x);
		}
	);

	f32 const evalX = (f32)(bbox.min.x & ~7) + 0.5f;
	f32 const evalY = (f32)(bbox.min.y) + 0.5f;

	// -- compute initial barycentrics at start of scanline
	f32 const w0Start = scalarEdge(v1f, v2f, evalX, evalY);
	f32 const w1Start = scalarEdge(v2f, v0f, evalX, evalY);
	f32 const w2Start = scalarEdge(v0f, v1f, evalX, evalY);

	// top-left fill rule biases
	f32 const bias0 = topLeftRuleBias(sp1, sp2);
	f32 const bias1 = topLeftRuleBias(sp2, sp0);
	f32 const bias2 = topLeftRuleBias(sp0, sp1);

	// -- coverage interpolants
	f32 w0Row = w0Start + bias0;
	f32 w1Row = w1Start + bias1;
	f32 w2Row = w2Start + bias2;

	// -- attribute interpolants (premul by 1/w)
	// rcpArea folds into ddx/ddy
	auto uv = (
		srat::Interpolant<f32v2>::make(
			/*a0=*/ uv0 * perspectiveW[0],
			/*a1=*/ uv1 * perspectiveW[1],
			/*a2=*/ uv2 * perspectiveW[2],
			/*dw0dx=*/ dw0Dx * rcpArea,
			/*dw1dx=*/ dw1Dx * rcpArea,
			/*dw2dx=*/ dw2Dx * rcpArea,
			/*dw0dy=*/ dw0Dy * rcpArea,
			/*dw1dy=*/ dw1Dy * rcpArea,
			/*dw2dy=*/ dw2Dy * rcpArea,
			/*w0Start=*/ w0Start * rcpArea,
			/*w1Start=*/ w1Start * rcpArea,
			/*w2Start=*/ w2Start * rcpArea
		)
	);
	auto depth = (
		srat::Interpolant<f32>::make(
			/*a0=*/ d0,
			/*a1=*/ d1,
			/*a2=*/ d2,
			/*dw0dx=*/ dw0Dx * rcpArea,
			/*dw1dx=*/ dw1Dx * rcpArea,
			/*dw2dx=*/ dw2Dx * rcpArea,
			/*dw0dy=*/ dw0Dy * rcpArea,
			/*dw1dy=*/ dw1Dy * rcpArea,
			/*dw2dy=*/ dw2Dy * rcpArea,
			/*w0Start=*/ w0Start * rcpArea,
			/*w1Start=*/ w1Start * rcpArea,
			/*w2Start=*/ w2Start * rcpArea
		)
	);
	auto invW = (
		srat::Interpolant<f32>::make(
			/*a0=*/ perspectiveW[0],
			/*a1=*/ perspectiveW[1],
			/*a2=*/ perspectiveW[2],
			/*dw0dx=*/ dw0Dx * rcpArea,
			/*dw1dx=*/ dw1Dx * rcpArea,
			/*dw2dx=*/ dw2Dx * rcpArea,
			/*dw0dy=*/ dw0Dy * rcpArea,
			/*dw1dy=*/ dw1Dy * rcpArea,
			/*dw2dy=*/ dw2Dy * rcpArea,
			/*w0Start=*/ w0Start * rcpArea,
			/*w1Start=*/ w1Start * rcpArea,
			/*w2Start=*/ w2Start * rcpArea
		)
	);

	// -- precompute SIMD constants
	alignas(32) constexpr srat::array<f32, 8> const laneOffsets = {
		0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
	};
	f32x8 const laneOffsetsX = f32x8_load(srat::slice<f32, 8>(laneOffsets));
	f32x8 const w0LaneInit = f32x8_splat(dw0Dx) * laneOffsetsX;
	f32x8 const w1LaneInit = f32x8_splat(dw1Dx) * laneOffsetsX;
	f32x8 const w2LaneInit = f32x8_splat(dw2Dx) * laneOffsetsX;
	f32x8 const w0Step8 = f32x8_splat(dw0Dx * 8.0f);
	f32x8 const w1Step8 = f32x8_splat(dw1Dx * 8.0f);
	f32x8 const w2Step8 = f32x8_splat(dw2Dx * 8.0f);
	f32x8 const targetDimSplat = f32x8_splat((f32)targetDim.x);

	for (auto y = i32{bbox.min.y}; y <= bbox.max.y; ++y) {
		// -- find earliest x-block that could have coverage
		auto const & firstHit = [](f32 const wRow, f32 const dwDx) -> u32 {
			if (dwDx <= 0.0f) {
				return (wRow >= 0.0f) ? 0 : UINT32_MAX;
			}
			if (wRow >= 0.0f) {
				return 0;
			}
			return (i32)ceilf(-wRow / dwDx);
		};
		u32 const firstPixelOffset = std::max({
			firstHit(w0Row, dw0Dx),
			firstHit(w1Row, dw1Dx),
			firstHit(w2Row, dw2Dx),
		});
		if (firstPixelOffset == UINT32_MAX) {
			// entire row is outside all edges, can skip row
			w0Row += dw0Dy; w1Row += dw1Dy; w2Row += dw2Dy;
			uv.stepRow(); depth.stepRow(); invW.stepRow();
			continue;
		}

		// -- try to skip to first pixel with coverage
		u32 const skipBlocks = firstPixelOffset / 8u;
		i32 const xStart = (bbox.min.x & ~7) + (i32)(skipBlocks * 8);
		f32 const skipOffset = 8.0f * (f32)skipBlocks;
		f32x8 const laneOffsetsSkipped = laneOffsetsX + f32x8_splat(skipOffset);

		// -- compute barycentrics and attributes, will be incremented in X loop
		f32x8 w0It = (
			  f32x8_splat(w0Row) + w0LaneInit + f32x8_splat(dw0Dx * skipOffset)
		);
		f32x8 w1It = (
			  f32x8_splat(w1Row) + w1LaneInit + f32x8_splat(dw1Dx * skipOffset)
		);
		f32x8 w2It = (
			  f32x8_splat(w2Row) + w2LaneInit + f32x8_splat(dw2Dx * skipOffset)
		);
		f32v2x8 laneUvIt = uv.simdRow(laneOffsetsSkipped);
		f32x8 laneDepthIt = depth.simdRow(laneOffsetsSkipped);
		f32x8 laneInvWIt = invW.simdRow(laneOffsetsSkipped);

		bool anyPixelWritten = false;
		for (i32 x = xStart; x <= bbox.max.x; x += 8) {
			f32x8 const pixX = f32x8_splat((f32)x + 0.5f) + laneOffsetsX;
			f32x8 const zero = f32x8_zero();
			u32x8 const inside = (w0It >= zero) & (w1It >= zero) & (w2It >= zero);
			u32x8 const inBounds = (pixX < targetDimSplat);
			u32x8 const mask = inside & inBounds;
			bool anyMasked = u32x8_mask_sign_bit(mask);

			// -- skip rest of row if no pixels covered
			if (!anyMasked && anyPixelWritten) {
				break;
			}

			// -- write covered pixels
			if (anyMasked)
			{
				anyPixelWritten = true;
				f32x8 const wPersp = laneInvWIt.reciprocal(); // TODO newton rhaps?
				f32v2x8 const interpUv = laneUvIt * wPersp;
				// load from texture
				f32v4x8 const texturedColor = (
					ci.boundTexture.id != 0
					?
					srat::gfx::image_sample(
						/*image=*/ ci.boundTexture,
						/*uv=*/ interpUv
					)
					:
					f32v4x8_splat(1.0f, 0.0f, 1.0f, 1.0f) // magenta
				);
				f32x8 const interpDepth = laneDepthIt * wPersp;

				f32v4x8 finalColor;
				switch (srat_shader_mode()) {
					case ShaderMode::DisplayColor: {
						finalColor = texturedColor;
						break;
					}
					case ShaderMode::DisplayUv: {
						finalColor = (
							f32v4x8(
								interpUv.x,
								interpUv.y,
								f32x8_splat(0.0f),
								f32x8_splat(1.0f)
							)
						);
						break;
					}
					case ShaderMode::DisplayDepth: {
						finalColor = (
							f32v4x8(
								interpDepth,
								interpDepth,
								interpDepth,
								f32x8_splat(1.0f)
							)
						);
						break;
					}
					default: {
						finalColor = f32v4x8_splat(1.0f, 0.0f, 1.0f, 1.0f); // magenta
						break;
					}
				}

				rasterize_tile_write_pixel(
					x, y, finalColor, interpDepth, mask,
					/*targetDim=*/ targetDim,
					/*imageData=*/ imageData,
					/*depthData=*/ depthData,
					/*depth16Lanes=*/ depth16Lanes,
					/*maskLanes=*/ maskLanes

				);
			}

			// -- increment lanes
			w0It = w0It + w0Step8;
			w1It = w1It + w1Step8;
			w2It = w2It + w2Step8;
			laneUvIt = laneUvIt + uv.ddxStep8;
			laneDepthIt = laneDepthIt + depth.ddxStep8;
			laneInvWIt = laneInvWIt + invW.ddxStep8;
		}

		// increment row
		w0Row += dw0Dy; w1Row += dw1Dy; w2Row += dw2Dy;
		uv.stepRow(); depth.stepRow(); invW.stepRow();
	}
}

void srat::rasterizer_phase_rasterization(
	srat::RasterizerPhaseRasterizationParams const & ci
) {
	i32v2 const tileCount = srat::tile_grid_tile_count(ci.tileGrid);
	i32 const numTiles = tileCount.x * tileCount.y;
	srat::slice<srat::TileTriangleData const> const triangleData = (
		srat::tile_grid_triangle_data(ci.tileGrid)
	);
	i32v2 const targetDim = srat::gfx::image_dim(ci.targetColor);
	srat::slice<u32> imageData = srat::gfx::image_data32(ci.targetColor);
	srat::slice<u16> depthData = srat::gfx::image_data16(ci.targetDepth);

	auto const & applyTile = (
		[&](tbb::blocked_range<u32> const & range) {
			auto const begin = (i32)range.begin();
			auto const end = (i32)range.end();
			for (i32 tileId = begin; tileId < end; ++tileId) {
				auto const tile = i32v2 {
					tileId % tileCount.x, tileId / tileCount.x
				};
				srat::TileBin const & bin = srat::tile_grid_bin(ci.tileGrid, tile);
				for (auto tri = 0u; tri < bin.triangleIndices.size(); ++tri) {
					rasterize_triangle(RasterizeTriangleParams {
						.tileGrid = ci.tileGrid,
						.tileBin = bin,
						.triangleData = triangleData,
						.tile = tile,
						.triIdx = tri,
						.targetDim = targetDim,
						.imageData = imageData,
						.depthData = depthData,
						.boundTexture = ci.boundTexture,
					});
				}
			}
		}
	);

	if (srat_rasterize_parallel()) {
		tbb::parallel_for(tbb::blocked_range<u32>(0, numTiles), applyTile);
	} else {
		applyTile(tbb::blocked_range<u32>(0, numTiles));
	}
}
