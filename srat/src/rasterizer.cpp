#include <srat/rasterizer.hpp>


void srat::rasterize(
	srat::Image const & target,
	srat::Image const & depthTarget,
	f32m44 const & modelViewProjection,
	VertexAttributes const & attribs,
	u32 * indices,
	u32 vertexCount
) {

	auto const & process_vertex = [&](u32 vtx) -> f32v4 {
		f32v4 const pos = attr_fetch_f32v4(attribs.position, vtx);
		auto const clipPos = modelViewProjection * pos;
		return clipPos;
	};
	auto const & process_color = [&](u32 vtx) {
		return attribs.color.data != nullptr
			? attr_fetch_f32v4(attribs.color, vtx)
			: f32v4{ 1.f, 1.f, 1.f, 1.f };
	};

	for (u32 vtx = 0; vtx < vertexCount/3; ++vtx) {
		u32 const ind0 = indices[vtx * 3 + 0];
		u32 const ind1 = indices[vtx * 3 + 1];
		u32 const ind2 = indices[vtx * 3 + 2];

		f32v4 const v0 = process_vertex(ind0);
		f32v4 const v1 = process_vertex(ind1);
		f32v4 const v2 = process_vertex(ind2);

		if (v0.w <= 0.f || v1.w <= 0.f || v2.w <= 0.f) {
			// triangle is behind near plane, skip rasterization
			continue;
		}

		i32v2 const s0 = f32v4_clip_to_screen(v0, image_dim(target));
		i32v2 const s1 = f32v4_clip_to_screen(v1, image_dim(target));
		i32v2 const s2 = f32v4_clip_to_screen(v2, image_dim(target));

		f32 const v0Depth = depth_ndc(v0);
		f32 const v1Depth = depth_ndc(v1);
		f32 const v2Depth = depth_ndc(v2);

		f32v4 const v0Color = process_color(ind0);
		f32v4 const v1Color = process_color(ind1);
		f32v4 const v2Color = process_color(ind2);

		i32v2 const targetDim = image_dim(target);

		// -- per-vertex needed for perspective-correct interpolation
		float const v0InvW = 1.0f / v0.w;
		float const v1InvW = 1.0f / v1.w;
		float const v2InvW = 1.0f / v2.w;
		float const v0DepthCorrected = v0Depth * v0InvW;
		float const v1DepthCorrected = v1Depth * v1InvW;
		float const v2DepthCorrected = v2Depth * v2InvW;

		// -- bounding box
		i32bbox2 const bboxTri = i32bbox2_from_triangle(s0, s1, s2);
		i32bbox2 const bboxImg = {
			.min = { 0, 0 },
			.max = { targetDim.x - 1, targetDim.y - 1 },
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

		if (bbox.min.x > bbox.max.x || bbox.min.y > bbox.max.y) {
			// triangle is degenerate, skip rasterization
			continue;
		}

		f32v2 const v0f = as_f32v2(s0);
		f32v2 const v1f = as_f32v2(s1);
		f32v2 const v2f = as_f32v2(s2);

		// -- prepare attributes
		f32v4x8 const v0AttrColor = f32v4x8_splat(v0Color) * f32x8_splat(v0InvW);
		f32v4x8 const v1AttrColor = f32v4x8_splat(v1Color) * f32x8_splat(v1InvW);
		f32v4x8 const v2AttrColor = f32v4x8_splat(v2Color) * f32x8_splat(v2InvW);

		// -- calculate triangle area
		f32 const area = f32v2_triangle_area(v0f, v1f, v2f);
		if (area <= 0.f) {
			// triangle is backfacing or degenerate, skip rasterization
			continue;
		}

		f32 const rcpArea = 1.0f / area;

		// -- x offsets for 8 lanes: px = x + [0..7]
		alignas(32) static constexpr f32 skLaneOffsets[8] = {
			0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f
		};
		f32x8 const laneOffsets = f32x8_load(skLaneOffsets);

		for (i32 py = bbox.min.y; py <= bbox.max.y; ++py) {
			// pixel center X for scanline
			bool anyPixelWritten = false;
			f32x8 const pixY = f32x8_splat((f32)py + 0.5f);
			for (i32 px = bbox.min.x & ~7; px <= bbox.max.x; px += 8) {
				// pixel centers for this simdgroup
				f32x8 const pixX = (f32x8_splat((f32)px + 0.5f) + laneOffsets);

				// -- edge test/barycentrics
				f32x8 const w0 = f32x8_barycentric(v1f, v2f, pixX, pixY);
				f32x8 const w1 = f32x8_barycentric(v2f, v0f, pixX, pixY);
				f32x8 const w2 = f32x8_barycentric(v0f, v1f, pixX, pixY);

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
					continue;
				}
				anyPixelWritten = true;

				// -- interpolate attributes
				f32x8 const b0 = w0 * f32x8_splat(rcpArea);
				f32x8 const b1 = w1 * f32x8_splat(rcpArea);
				f32x8 const b2 = w2 * f32x8_splat(rcpArea);

				// -- interpolate 1/w and z/w
				f32x8 const interpInvW = (
					  b0 * f32x8_splat(v0InvW)
					+ b1 * f32x8_splat(v1InvW)
					+ b2 * f32x8_splat(v2InvW)
				);
				f32x8 const interpZOverW = (
					  b0 * f32x8_splat(v0DepthCorrected)
					+ b1 * f32x8_splat(v1DepthCorrected)
					+ b2 * f32x8_splat(v2DepthCorrected)
				);

				// first depth
				f32x8 const interpDepth = (
					  interpZOverW * f32x8_splat(0.5f)
					+ f32x8_splat(0.5f)
				);

				// perspective-correct attributes
				f32x8 const w = f32x8_splat(1.0f) / interpInvW;
				f32v4x8 const interpColor = (
					((v0AttrColor * b0) + (v1AttrColor * b1) + (v2AttrColor * b2)) * w
				);

				// -- depth test + write
				alignas(32) float lanesDepth[8];
				alignas(32) u32   lanesMask[8];
				f32x8_store(interpDepth, lanesDepth);
				u32x8_store(mask, lanesMask);

				// sequential write out
				{
					u32 * const imageData = (u32 *)(image_data(target));
					u32 * const rowPixels = imageData + (py*targetDim.x) + px;
					u16 * const depthData = (u16 *)(image_data(depthTarget));
					u16 * const rowDepths = depthData + (py*targetDim.x) + px;
					for (i32 lane = 0; lane < 8; ++lane) {
						if (!lanesMask[lane]) { continue; }
						i32 const pixelX = px + lane;
						if (pixelX < 0 || pixelX >= targetDim.x) { continue; }
						// -- depth test
						u16 depth16 = (u16)(
							f32_clamp(lanesDepth[lane], 0.f, 1.f) * (f32)UINT16_MAX + 0.5f
						);
						if (depth16 >= rowDepths[lane]) {
							continue; // fail depth test
						}

						// -- write depth
						rowDepths[lane] = depth16;

						// -- write color
						rowPixels[lane] = (
							0xFF000000 |
							(as_rgba(f32v4(
								interpColor.x.lane(lane),
								interpColor.y.lane(lane),
								interpColor.z.lane(lane),
								interpColor.w.lane(lane)
							)))
						);
					}
				}
			}
		}
	}
}
