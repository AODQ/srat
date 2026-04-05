#include <srat/rasterizer-phase-bin.hpp>

#include <srat/rasterizer-tile-grid.hpp>

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::rasterizer_phase_bin(
	RasterizerPhaseBinParams const & params
) {
	Let triangleCount = params.trianglePositions.size() / 3;
	for (Mut triIt = 0u; triIt < triangleCount; ++triIt) {
		Let triData = srat::TileTriangleData {
			.screenPos = params.trianglePositions.subslice(triIt*3).as<3>(),
			.depth = params.triangleDepths.subslice(triIt*3).as<3>(),
			.perspectiveW = params.trianglePerspectiveW.subslice(triIt*3).as<3>(),
			.color = {} // TODO
		};
		srat::tile_grid_bin_triangle_bbox(params.tileGrid, triData);
	}
}
