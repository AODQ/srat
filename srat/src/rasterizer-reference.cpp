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
	srat::slice<ReferenceTriangle const> const & triangles,
	srat::gfx::Viewport const & viewport,
	srat::gfx::Image const & targetColor,
	srat::gfx::Image const & targetDepth
) {
	auto colorData = srat::gfx::image_data32(targetColor);
	auto depthData = srat::gfx::image_data16(targetDepth);
	u32v2 const dim = viewport.dim;

	// Clear buffers
	std::fill_n(colorData.ptr(), colorData.size(), 0xFF000000u);  // black
	std::fill_n(depthData.ptr(), depthData.size(), UINT16_MAX);

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

		f32 const area = f32v2_triangle_area(v0, v1, v2);
		printf("Vertex: tri %u screen0=(%d, %d) depth0=%.4f perspw0=%.4f\n",
			(unsigned int)(&tri - triangles.ptr()),
			tri.screenPos[0].x, tri.screenPos[0].y, tri.depth[0], tri.perspectiveW[0]
		);
		if (area <= 0.0001f) continue; // backface or degenerate

		f32 const rcpArea = 1.0f / area;

		// Top-left fill rule
		auto isTopLeft = [](f32v2 a, f32v2 b) -> bool {
			return (a.y == b.y && a.x > b.x) || (a.y < b.y);
		};

		f32 const bias0 = isTopLeft(v1, v2) ? 0.0f : -0.5f;
		f32 const bias1 = isTopLeft(v2, v0) ? 0.0f : -0.5f;
		f32 const bias2 = isTopLeft(v0, v1) ? 0.0f : -0.5f;

		for (i32 y = minY; y <= maxY; ++y)
		{
			for (i32 x = minX; x <= maxX; ++x)
			{
				f32 const px = (f32)x + 0.5f;
				f32 const py = (f32)y + 0.5f;

				f32 w0 = scalarEdge(v1, v2, px, py) + bias0;
				f32 w1 = scalarEdge(v2, v0, px, py) + bias1;
				f32 w2 = scalarEdge(v0, v1, px, py) + bias2;

				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

				f32 b0 = w0 * rcpArea;
				f32 b1 = w1 * rcpArea;
				f32 b2 = w2 * rcpArea;

				// Perspective-correct interpolation
				f32 invW = b0 * tri.perspectiveW[0] +
						   b1 * tri.perspectiveW[1] +
						   b2 * tri.perspectiveW[2];

				if (invW <= 0.0f) continue;

				f32 w = 1.0f / invW;

				f32 interpDepth = (b0 * tri.depth[0] + b1 * tri.depth[1] + b2 * tri.depth[2]);

				f32v4 interpColor = (
					tri.color[0] * tri.perspectiveW[0] * b0 +
					tri.color[1] * tri.perspectiveW[1] * b1 +
					tri.color[2] * tri.perspectiveW[2] * b2
				) * w;

				// Depth test + write
				u16 depth16 = (u16)std::roundf(f32_clamp(interpDepth, 0.0f, 1.0f) * 65535.0f);

				size_t idx = (size_t)y * dim.x + (size_t)x;

				if (depth16 > depthData[idx]) continue;

				depthData[idx] = depth16;
				colorData[idx] = as_rgba(interpColor);
			}
		}
	}
}
