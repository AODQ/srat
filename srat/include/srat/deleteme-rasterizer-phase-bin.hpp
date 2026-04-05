#pragma once

#include <srat/core-types.hpp>
#include <srat/core-math.hpp>

namespace srat {

// resets the rasterizer's binning state, must be called before binning
// triangles
struct RasterizerPhaseBinningConfig {
	u32 imageWidth;
	u32 imageHeight;
};
void rasterizer_phase_bin_reset(RasterizerPhaseBinningConfig const & config);

// bins triangles into the tile grid.
struct RasterizerPhaseBinTrianglesParams {
	srat::slice<triangle_position_t const> positions;
	srat::slice<triangle_depth_t const> depth;
	srat::slice<triangle_perspective_w_t const> perspectiveW;
};
void rasterizer_phase_bin_triangles(
	RasterizerPhaseBinTrianglesParams const & params
);

}
