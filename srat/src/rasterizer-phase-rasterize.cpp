#include <srat/rasterizer-phase-rasterize.hpp>

#include <srat/gfx-image.hpp>
#include <srat/rasterizer-tile-grid.hpp>

static inline void rasterize_tile_write_pixel(
	i32 const x,
	i32 const y,
	f32v4x8 const & interpColor,
	srat::array<float, 8> const & lanesDepth,
	srat::array<u32, 8> const & lanesMask,
	srat::gfx::Image const & targetColor,
	srat::gfx::Image const & targetDepth
) {
	Let targetDim = u32v2{srat::gfx::image_dim(targetColor)};
	Let imageData = srat::gfx::image_data32(targetColor);
	Let rowPixels = imageData.subslice((y*targetDim.x) + x, 8);
	Let depthData = srat::gfx::image_data16(targetDepth);
	Let rowDepths = depthData.subslice((y*targetDim.x) + x, 8);
	// sequential write
	for (Mut lane = 0; lane < 8; ++lane) {
		if (!lanesMask[lane]) { continue; }
		i32 const pixelX = x + lane;
		if (pixelX < 0 || pixelX >= (i32)targetDim.x) { continue; }
		// -- depth test
		u16 depth16 = (u16)(
			std::roundl(f32_clamp(lanesDepth[lane], 0.f, 1.f) * (f32)UINT16_MAX)
		);
		if (depth16 > rowDepths[lane]) {
			continue; // fail depth test
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
}

static void rasterize_triangle(
	srat::TileGrid const & tileGrid,
	srat::TileBin const & tileBin,
	u32v2 const tile,
	u32 const triIdx,
	srat::gfx::Image const imageColor,
	srat::gfx::Image const imageDepth
) {
	u32v2 const targetDim = srat::gfx::image_dim(imageColor);
	Let tri = srat::tile_grid_triangle_data(tileGrid, tileBin.triangleIndices[triIdx]);
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
		// TODO color
		// f32v4 const c0 = tri.color[0];
		// f32v4 const c1 = tri.color[1];
		// f32v4 const c2 = tri.color[2];
		f32v4 const c0 = f32v4(1.0f, 1.0f, 1.0f, 1.0f);
		f32v4 const c1 = f32v4(1.0f, 0.0f, 0.0f, 1.0f);
		f32v4 const c2 = f32v4(1.0f, 0.0f, 1.0f, 1.0f);

		// -- prepare attributes to be interpolated
		f32v4x8 const v0AttrColor = (
			f32v4x8_splat(c0) * f32x8_splat(perspectiveW[0])
		);
		f32v4x8 const v1AttrColor = (
			f32v4x8_splat(c1) * f32x8_splat(perspectiveW[1])
		);
		f32v4x8 const v2AttrColor = (
			f32v4x8_splat(c2) * f32x8_splat(perspectiveW[2])
		);

		// -- calculate triangle area
		f32v2 const v0f = as_f32v2(sp0);
		f32v2 const v1f = as_f32v2(sp1);
		f32v2 const v2f = as_f32v2(sp2);
		f32 const area = f32v2_triangle_area(v0f, v1f, v2f);
		f32 const rcpArea = 1.0f / area;

		// -- bounding box
		i32bbox2 const bboxTri = i32bbox2_from_triangle(sp0, sp1, sp2);
		i32bbox2 const bboxImg = {
			.min = {
				(i32)tile.x * (i32)srat_tile_size(),
				(i32)tile.y * (i32)srat_tile_size(),
			},
			.max = {
				(i32)(tile.x+1) * (i32)srat_tile_size() - 1,
				(i32)(tile.y+1) * (i32)srat_tile_size() - 1
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
		f32x8 const dw0_dx = f32x8_splat(v1f.y - v2f.y);
		f32x8 const dw1_dx = f32x8_splat(v2f.y - v0f.y);
		f32x8 const dw2_dx = f32x8_splat(v0f.y - v1f.y);
		f32x8 const step = f32x8_splat(8.0f);
		f32x8 const dw0_step = dw0_dx * step;
		f32x8 const dw1_step = dw1_dx * step;
		f32x8 const dw2_step = dw2_dx * step;

		// -- x offsets for 8 lanes
		alignas(32) static srat::array<f32, 8> const laneOffsets = {
			0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
		};
		f32x8 const laneOffsetsX = f32x8_load(srat::slice<f32, 8>(laneOffsets));
		f32x8 const startX = (
			f32x8_splat((f32)(bbox.min.x & ~7) + 0.5f) + laneOffsetsX
		);
		for (i32 y = bbox.min.y; y <= bbox.max.y; ++y) {
			// -- compute barycentrics at start of scanline
			f32x8 const startY = f32x8_splat((f32)y + 0.5f);
			f32x8 w0 = f32x8_barycentric(v1f, v2f, startX, startY);
			f32x8 w1 = f32x8_barycentric(v2f, v0f, startX, startY);
			f32x8 w2 = f32x8_barycentric(v0f, v1f, startX, startY);
			auto const & scanline_step = [&]() {
				w0 = w0 + dw0_step;
				w1 = w1 + dw1_step;
				w2 = w2 + dw2_step;
			};
			bool anyPixelWritten = false;

			// -- step through scanline in chunks of 8 pixels
			for (i32 x = bbox.min.x & ~7; x <= bbox.max.x; x += 8) {
				f32x8 const pixX = (f32x8_splat((f32)x + 0.5f) + laneOffsetsX);

				// -- if all weights are positive, pixel is inside triangle
				f32x8 const zero = f32x8_zero();
				u32x8 const inside = (w0 > zero) & (w1 > zero) & (w2 > zero);

				// -- cull lanes beyond image bounds
				f32x8 const maxX = f32x8_splat((f32)targetDim.x);
				u32x8 const inBounds = (pixX < maxX);
				u32x8 const mask = inside & inBounds;
				// -- reject simdgroup if no pixels are inside the triangle
				if (!u32x8_any(mask)) {
					if (anyPixelWritten) {
						// skip scanline if hit triangle already
						break;
					}
					scanline_step();
					continue;
				}
				anyPixelWritten = true;

				// -- barycentric interpolation of attributes
				f32x8 const b0 = w0 * f32x8_splat(rcpArea);
				f32x8 const b1 = w1 * f32x8_splat(rcpArea);
				f32x8 const b2 = w2 * f32x8_splat(rcpArea);

				// -- linear depth interpolation
				f32x8 const interpDepth = (
					(
						  b0 * f32x8_splat(d0)
						+ b1 * f32x8_splat(d1)
						+ b2 * f32x8_splat(d2)
					)
				);

				// -- perspective-correct attributes
				f32x8 const interpInvW = (
					  b0 * f32x8_splat(perspectiveW[0])
					+ b1 * f32x8_splat(perspectiveW[1])
					+ b2 * f32x8_splat(perspectiveW[2])
				);
				f32x8 const w = f32x8_splat(1.0f) / interpInvW;
				f32v4x8 const interpColor = (
					(
						  (v0AttrColor * b0)
						+ (v1AttrColor * b1)
						+ (v2AttrColor * b2)
					) * w
				);

				// -- depth test + write
				alignas(32) srat::array<float, 8> lanesDepth {};
				alignas(32) srat::array<u32, 8>   lanesMask {};
				f32x8_store(interpDepth, lanesDepth);
				u32x8_store(mask, lanesMask);

				rasterize_tile_write_pixel(
					x, y,
					/*interpColor=*/interpColor,
					/*lanesDepth=*/lanesDepth,
					/*lanesMask=*/lanesMask,
					/*targetColor=*/imageColor,
					/*targetDepth=*/imageDepth
				);
				scanline_step();
			}
		}
}

void srat::rasterizer_phase_rasterization(
	srat::RasterizerPhaseRasterizationParams const & ci
) {
	// for each tile ...
	Let tileCount = srat::tile_grid_tile_count(ci.tileGrid);
	for (Mut tileX = 0u; tileX < tileCount.x; ++ tileX)
	for (Mut tileY = 0u; tileY < tileCount.y; ++ tileY) {
		// for each tri ...
		u32v2 const tile = {tileX, tileY};
		Let bin = srat::TileBin { srat::tile_grid_bin(ci.tileGrid, tile) };
		for (Mut triIdx = 0u; triIdx < bin.triangleIndices.size(); ++triIdx) {
			rasterize_triangle(
				ci.tileGrid,
				bin,
				tile,
				triIdx,
				ci.targetColor,
				ci.targetDepth
			);
		}
	}
}
