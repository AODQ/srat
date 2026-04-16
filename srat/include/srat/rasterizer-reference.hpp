#pragma once

#include <srat/core-array.hpp>
#include <srat/core-config.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-command-buffer.hpp>

// reference rasterizer, slow but correct. Used to generate golden images

namespace srat {
	struct ReferenceTriangle {
		srat::array<i32v2, 3> screenPos {};
		srat::array<float, 3> depth {};
		srat::array<float, 3> perspectiveW {};
		srat::array<f32v2, 3> uv {};
	};
	void rasterizer_reference_render(
		srat::gfx::Image const & boundTexture,
		srat::slice<ReferenceTriangle const> const & triangles,
		srat::gfx::Viewport const & viewport,
		srat::gfx::Image const & targetColor,
		srat::gfx::Image const & targetDepth
	);
};
