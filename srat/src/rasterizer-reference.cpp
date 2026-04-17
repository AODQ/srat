#include <srat/rasterizer-reference.hpp>

static inline f32 scalarEdge(
	f32v2 const & a,
	f32v2 const & b,
	f32 const px,
	f32 const py
) {
	return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

void srat::rasterizer_reference_render(
	srat::gfx::Image const & boundTexture,
	srat::slice<ReferenceTriangle const> const & triangles,
	srat::gfx::Viewport const & viewport,
	srat::gfx::Image const & targetColor,
	srat::gfx::Image const & targetDepth
) {
	auto colorData = srat::gfx::image_data32(targetColor);
	auto depthData = srat::gfx::image_data16(targetDepth);
	i32v2 const dim = viewport.dim;

	for (auto const& tri : triangles)
	{
		f32v2 const v0 = as_f32v2(tri.screenPos[0]);
		f32v2 const v1 = as_f32v2(tri.screenPos[1]);
		f32v2 const v2 = as_f32v2(tri.screenPos[2]);

		// Robust bounding box
		i32 minX = i32_max(0, (i32)std::floor(f32_min(v0.x, f32_min(v1.x, v2.x))));
		i32 minY = i32_max(0, (i32)std::floor(f32_min(v0.y, f32_min(v1.y, v2.y))));
		i32 maxX = i32_min((i32)dim.x - 1, (i32)std::ceil(f32_max(v0.x, f32_max(v1.x, v2.x))));
		i32 maxY = i32_min((i32)dim.y - 1, (i32)std::ceil(f32_max(v0.y, f32_max(v1.y, v2.y))));

		if (minX > maxX || minY > maxY) continue;

		f32 const area = f32v2_triangle_parallelogram_area(v0, v1, v2);
		// if (area <= 0.0001f) continue; // backface or degenerate

		f32 const rcpArea = 1.0f / area;

		// Top-left fill rule
		f32 const bias0 = topLeftRuleBias(tri.screenPos[1], tri.screenPos[2]);
		f32 const bias1 = topLeftRuleBias(tri.screenPos[2], tri.screenPos[0]);
		f32 const bias2 = topLeftRuleBias(tri.screenPos[0], tri.screenPos[1]);

		for (i32 y = minY; y <= maxY; ++y)
		{
			for (i32 x = minX; x <= maxX; ++x)
			{
				f32 const px = (f32)x + 0.5f;
				f32 const py = (f32)y + 0.5f;

				f32 const w0 = scalarEdge(v1, v2, px, py) + bias0;
				f32 const w1 = scalarEdge(v2, v0, px, py) + bias1;
				f32 const w2 = scalarEdge(v0, v1, px, py) + bias2;

				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

				f32 const b0 = w0 * rcpArea;
				f32 const b1 = w1 * rcpArea;
				f32 const b2 = w2 * rcpArea;

				// Perspective-correct interpolation
				f32 const invW = (
					b0 * tri.perspectiveW[0] +
					b1 * tri.perspectiveW[1] +
					b2 * tri.perspectiveW[2]
				);
				if (invW <= 0.0f) continue;

				f32 const w = 1.0f / invW;

				f32 const interpDepth = (
					  b0 * tri.depth[0] * tri.perspectiveW[0]
					+ b1 * tri.depth[1] * tri.perspectiveW[1]
					+ b2 * tri.depth[2] * tri.perspectiveW[2]
				) * w;

				f32v2 const interpUV = (
					tri.uv[0] * tri.perspectiveW[0] * b0 +
					tri.uv[1] * tri.perspectiveW[1] * b1 +
					tri.uv[2] * tri.perspectiveW[2] * b2
				) * w;

				f32v4 const finalColor = (
					srat::gfx::image_reference_sample(boundTexture, interpUV)
				);

				// Depth test + write
				// SRAT_ASSERT(interpDepth >= 0.0f && interpDepth <= 1.0f);
				u16 depth16 = (
					T_roundf_positive<u16>(
						f32_clamp(interpDepth, 0.0f, 1.0f) * 65535.0f
					)
				);
				size_t idx = (size_t)y * dim.x + (size_t)x;
				if (depth16 < depthData[idx]) { continue; }

				depthData[idx] = depth16;
				colorData[idx] = as_rgba(finalColor);
			}
		}
	}
}
