#pragma once

#include <srat/alloc-arena.hpp>
#include <srat/core-math.hpp>
#include <srat/core-types.hpp>

#include <vector>

namespace srat::gfx { struct Viewport; }

namespace srat {

	struct TileGrid {
		u64 id;
	};

	struct TileTriangleData {
		srat::slice<i32v2, 3> screenPos;
		srat::slice<float, 3> depth;
		srat::slice<float, 3> perspectiveW;
		srat::slice<f32v4, 3> color;
	};

	// tile bin structure
	struct TileBin {
		std::vector<u32> triangleIndices;
	};

	struct TileGridCreateInfo {
		u32 imageWidth {0};
		u32 imageHeight {0};
		u32 maxTriangles = SRAT_MAX_TRIANGLES_PER_TILE();
		u32 initialBinCapacity = 1024u*1024u;
	};

	TileGrid tile_grid_create(TileGridCreateInfo const & createInfo);
	void tile_grid_destroy(TileGrid const & grid);

	u32v2 tile_grid_tile_count(TileGrid const & grid);

	// call once per frame before binning
	void tile_grid_clear(TileGrid const & grid);

	TileTriangleData const & tile_grid_triangle_data(
		TileGrid const & grid,
		u32 triangleIndex
	);

	// assign a triangle to a tile bin
	void tile_grid_bin_triangle_bbox(
		TileGrid & grid,
		TileTriangleData const & triangleData
#if SRAT_BINNING_TWO_PHASES()
		, bool secondPhase = false
#endif
	);

#if SRAT_BINNING_TWO_PHASES()
	void tile_grid_bin_finalize_allocations(TileGrid & grid);
#endif

	TileBin & tile_grid_bin(TileGrid const & grid, u32v2 const & tile);

	bool tile_grid_valid(TileGrid const & grid);

	// --------------------------------------------------------------------------
	// -- utility functions
	// --------------------------------------------------------------------------

	// calculates the number of tiles needed to cover a viewport
	// of the given dimensions
	u32v2 viewport_tile_count(srat::gfx::Viewport const & viewport);

} // namespace srat
