#pragma once

#include <srat/core-config.hpp>
#include <srat/core-types.hpp>

namespace srat { struct TileGrid; }

namespace srat {
	struct RasterizerPhaseBinParams {
		srat::TileGrid & tileGrid;
		srat::slice<triangle_position_t>  trianglePositions;
		srat::slice<triangle_depth_t> triangleDepths;
		srat::slice<triangle_perspective_w_t> trianglePerspectiveW;
		srat::slice<f32v2> triangleUvs;
		srat::slice<f32v3> triangleNormals;
	};

	void rasterizer_phase_bin(
		RasterizerPhaseBinParams const & params
	);
}
