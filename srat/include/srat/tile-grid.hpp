#pragma once

#include <srat/types.hpp>

#include <srat/math.hpp>

#include <srat/arena-allocator.hpp>

#if SRAT_BINNING_USE_INDEX_CACHE()
#include <vector>
#endif

namespace srat {

	struct TileTriangleData {
		i32v2 screenPos[3];
		float depth[3];
		float perspectiveW[3];
		f32v4 color[3];
	};

	// tile bin structure
	struct TileBin {
#if SRAT_BINNING_USE_INDEX_CACHE()
		std::vector<u32> triangleIndices;
#else
		srat::TileTriangleData * triangleData { nullptr };
		u32 triangleCount { 0 };
		u32 triangleCapacity { 0 };
#endif
	};

	struct TileGrid {
		u64 id;
	};

	struct TileGridCreateInfo {
		u32 imageWidth;
		u32 imageHeight;
		u32 maxTriangles = SRAT_MAX_TRIANGLES_PER_TILE();
		u32 initialBinCapacity = 1024u*1024u;
	};

	TileGrid tile_grid_create(TileGridCreateInfo const & createInfo);
	void tile_grid_destroy(TileGrid const & grid);

	u32v2 tile_grid_tile_count(TileGrid const & grid);

	// call once per frame before binning
	void tile_grid_clear(TileGrid & grid);

	TileTriangleData const & tile_grid_triangle_data(
		TileGrid const & grid,
		u32 triangleIndex
	);

	// assign a triangle to a tile bin
	void tile_grid_bin_triangle_bbox(
		TileGrid & grid,
		i32bbox2 const & bounds,
		TileTriangleData const & triangleData
#if SRAT_BINNING_TWO_PHASES()
		, bool secondPhase = false
#endif
	);

#if SRAT_BINNING_TWO_PHASES()
	void tile_grid_bin_finalize_allocations(TileGrid & grid);
#endif

	TileBin & tile_grid_bin(TileGrid & grid, u32v2 tile);

	TileBin const & tile_grid_bin(TileGrid const & grid, u32v2 tile);

} // namespace srat
