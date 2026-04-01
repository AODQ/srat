#pragma once
// binning rasterizer

#include <srat/types.hpp>
#include <srat/math.hpp>

namespace srat {

// resets the rasterizer's binning state, must be called before binning
// triangles
struct RasterizerBinningConfig {
	u32 imageWidth;
	u32 imageHeight;
};
void rasterizer_bin_reset(RasterizerBinningConfig const & config);

// bins triangles into the tile grid.
struct RasterizerBinTrianglesParams {
	triangle_count_t triangleCount;
	triangle_position_t const * positions;
	triangle_depth_t const * depth;
	triangle_perspective_w_t const * perspectiveW;
};
void rasterizer_bin_triangles(RasterizerBinTrianglesParams const & params);

}
