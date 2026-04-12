#include <srat/rasterizer-phase-rasterize.hpp>

#include <srat/gfx-image.hpp>
#include <srat/rasterizer-interpolant.hpp>
#include <srat/rasterizer-tile-grid.hpp>

#include <tbb/parallel_for.h>

static inline void rasterize_tile_write_pixel(
	i32 const x,
	i32 const y,
	f32v4x8 const & interpColor,
	f32x8 const & interpDepth,
	u32x8 const & mask,
	srat::gfx::Image const & targetColor,
	srat::gfx::Image const & targetDepth
) {
	Let targetDim = u32v2{srat::gfx::image_dim(targetColor)};
	Let imageData = srat::gfx::image_data32(targetColor);
	Let rowPixels = imageData.subslice((y*targetDim.x) + x, 8);
	Let depthData = srat::gfx::image_data16(targetDepth);
	Let rowDepths = depthData.subslice((y*targetDim.x) + x, 8);

	// -- scale depth to u16 and convert
	f32x8 const depthClamped = f32x8_max(f32x8_zero(), f32x8_min(interpDepth, f32x8_splat(1.0f)));
	__m256i const newDepthI32 = _mm256_cvtps_epi32((depthClamped * f32x8_splat((f32)UINT16_MAX)).v);
	__m128i const newDepth16 = _mm_packus_epi32(
		_mm256_castsi256_si128(newDepthI32),
		_mm256_extracti128_si256(newDepthI32, 1)
	);

	// -- depth test: new >= old  →  invert (old > new) via XOR with all-ones
	__m128i const oldDepth16 = _mm_loadu_si128((__m128i const*)rowDepths.ptr());
	__m256i const oldDepthI32 = _mm256_cvtepu16_epi32(oldDepth16);
	u32x8 const depthPass = { _mm256_xor_si256(
		_mm256_cmpgt_epi32(oldDepthI32, newDepthI32),
		_mm256_set1_epi32(-1)
	)};
	u32x8 const finalMask = mask & depthPass;

	// -- write depth: load-blend-store (no u16 masked-store in AVX2)
	__m128i const mask16 = _mm_packs_epi32(
		_mm256_castsi256_si128(finalMask.v),
		_mm256_extracti128_si256(finalMask.v, 1)
	);
	_mm_storeu_si128(
		(__m128i*)const_cast<u16*>(rowDepths.ptr()),
		_mm_blendv_epi8(oldDepth16, newDepth16, mask16)
	);

	// -- pack color channels: as_rgba layout = R byte0, G byte1, B byte2, A byte3
	auto const scale255 = [](f32x8 const & ch) -> __m256i {
		f32x8 const c = f32x8_max(f32x8_zero(), f32x8_min(ch, f32x8_splat(1.0f)));
		return _mm256_cvtps_epi32((c * f32x8_splat(255.0f) + f32x8_splat(0.5f)).v);
	};
	__m256i const packed = _mm256_or_si256(
		_mm256_or_si256(scale255(interpColor.x), _mm256_slli_epi32(scale255(interpColor.y), 8)),
		_mm256_or_si256(_mm256_slli_epi32(scale255(interpColor.z), 16), _mm256_slli_epi32(scale255(interpColor.w), 24))
	);

	// -- masked color store: only writes lanes where bit 31 of finalMask is set
	_mm256_maskstore_epi32(reinterpret_cast<int*>(const_cast<u32*>(rowPixels.ptr())), finalMask.v, packed);
}

namespace {
	struct RasterizeTriangleParams {
		srat::TileGrid const & tileGrid;
		srat::TileBin const & tileBin;
		u32v2 tile;
		u32 triIdx;
		srat::gfx::Image imageColor;
		srat::gfx::Image imageDepth;
	};
}

static void rasterize_triangle(
	RasterizeTriangleParams const & ci
) {
	u32v2 const targetDim = srat::gfx::image_dim(ci.imageColor);
	Let tri = (
		srat::tile_grid_triangle_data(
			ci.tileGrid, ci.tileBin.triangleIndices[ci.triIdx]
		)
	);
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
	f32v4 const c0 = tri.color[0];
	f32v4 const c1 = tri.color[1];
	f32v4 const c2 = tri.color[2];

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
	auto color = (
		srat::Interpolant<f32v4>::make(
			/*a0=*/ c0 * perspectiveW[0],
			/*a1=*/ c1 * perspectiveW[1],
			/*a2=*/ c2 * perspectiveW[2],
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
	alignas(32) static srat::array<f32, 8> const laneOffsets = {
		0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
	};
	f32x8 const laneOffsetsX = f32x8_load(srat::slice<f32, 8>(laneOffsets));
	f32x8 const w0LaneInit = f32x8_splat(dw0Dx) * laneOffsetsX;
	f32x8 const w1LaneInit = f32x8_splat(dw1Dx) * laneOffsetsX;
	f32x8 const w2LaneInit = f32x8_splat(dw2Dx) * laneOffsetsX;
	f32x8 const w0Step8 = f32x8_splat(dw0Dx * 8.0f);
	f32x8 const w1Step8 = f32x8_splat(dw1Dx * 8.0f);
	f32x8 const w2Step8 = f32x8_splat(dw2Dx * 8.0f);

	for (auto y = i32{bbox.min.y}; y <= bbox.max.y; ++y) {
		// SIMD coverage
		f32x8 w0 = f32x8_splat(w0Row) + w0LaneInit;
		f32x8 w1 = f32x8_splat(w1Row) + w1LaneInit;
		f32x8 w2 = f32x8_splat(w2Row) + w2LaneInit;

		// -- SIMD attributes
		f32v4x8 laneColor = color.simdRow(laneOffsetsX);
		f32x8 laneDepth = depth.simdRow(laneOffsetsX);
		f32x8 laneInvW = invW.simdRow(laneOffsetsX);

		bool anyPixelWritten = false;

		for (i32 x = bbox.min.x & ~7; x <= bbox.max.x; x += 8) {
			f32x8 const pixX = f32x8_splat((f32)x + 0.5f) + laneOffsetsX;
			f32x8 const zero = f32x8_zero();
			u32x8 const inside = (w0 >= zero) & (w1 >= zero) & (w2 >= zero);
			u32x8 const inBounds = (pixX < f32x8_splat((f32)targetDim.x));
			u32x8 const mask = inside & inBounds;

			// -- write to tile if any lanes are covered
			if (!u32x8_any(mask)) {
				if (anyPixelWritten) {
					break;
				}
			}
			else {
				anyPixelWritten = true;
				f32x8 const wPersp = f32x8_splat(1.0f) / laneInvW;
				f32v4x8 const interpColor = laneColor * wPersp;
				f32x8 const interpDepth = laneDepth;

				rasterize_tile_write_pixel(
					x, y, interpColor, interpDepth, mask,
					ci.imageColor, ci.imageDepth
				);
			}

			// -- increment lanes
			w0 = w0 + w0Step8;
			w1 = w1 + w1Step8;
			w2 = w2 + w2Step8;
			laneColor = laneColor + color.ddxStep8;
			laneDepth = laneDepth + depth.ddxStep8;
			laneInvW = laneInvW + invW.ddxStep8;
		}

		// increment row
		w0Row += dw0Dy;
		w1Row += dw1Dy;
		w2Row += dw2Dy;
		color.stepRow();
		depth.stepRow();
		invW.stepRow();
	}
}

void srat::rasterizer_phase_rasterization(
	srat::RasterizerPhaseRasterizationParams const & ci
) {
	u32v2 const tileCount = srat::tile_grid_tile_count(ci.tileGrid);
	u32 const numTiles = tileCount.x * tileCount.y;

	auto const & applyTile = (
		[&](tbb::blocked_range<u32> const & range) {
			for (u32 tileId = range.begin(); tileId < range.end(); ++tileId) {
				u32v2 const tile = { tileId % tileCount.x, tileId / tileCount.x };
				srat::TileBin const & bin = srat::tile_grid_bin(ci.tileGrid, tile);
				for (auto tri = 0u; tri < bin.triangleIndices.size(); ++tri) {
					rasterize_triangle({
						.tileGrid = ci.tileGrid,
						.tileBin = bin,
						.tile = tile,
						.triIdx = tri,
						.imageColor = ci.targetColor,
						.imageDepth = ci.targetDepth
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
