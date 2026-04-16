#include <srat/rasterizer-phase-bin.hpp>

#include <srat/rasterizer-tile-grid.hpp>

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::rasterizer_phase_bin(
	RasterizerPhaseBinParams const & params
) {
	Let triangleCount = params.trianglePositions.size() / 3;
	for (auto triIt = 0u; triIt < triangleCount; ++triIt) {
		Let triData = srat::TileTriangleData {
			.screenPos = {
				params.trianglePositions[(triIt*3) + 0],
				params.trianglePositions[(triIt*3) + 1],
				params.trianglePositions[(triIt*3) + 2]
			},
			.depth = {
				params.triangleDepths[(triIt*3) + 0],
				params.triangleDepths[(triIt*3) + 1],
				params.triangleDepths[(triIt*3) + 2]
			},
			.perspectiveW = {
				params.trianglePerspectiveW[(triIt*3) + 0],
				params.trianglePerspectiveW[(triIt*3) + 1],
				params.trianglePerspectiveW[(triIt*3) + 2]
			},
			.uv = {
				params.triangleUvs[(triIt*3) + 0],
				params.triangleUvs[(triIt*3) + 1],
				params.triangleUvs[(triIt*3) + 2]
			},
			// .color = {
			// 	params.triangleColors[(triIt*3) + 0],
			// 	params.triangleColors[(triIt*3) + 1],
			// 	params.triangleColors[(triIt*3) + 2]
			// },
		};
		srat::tile_grid_bin_triangle_bbox(params.tileGrid, triData);
	}
}
