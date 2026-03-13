#pragma once

#include <srat/types.hpp>

#include <srat/math.hpp>

#include <srat/arena-allocator.hpp>

namespace srat {

	static constexpr u32 kTileDim = 16u;

	// tile bin structure
	struct TileBin {
		u32 * triangleIndices;
		u32 triangleCount;
		u32 triangleCapacity;
	};

	struct TileGrid {
		u64 id;
	};

	struct TileGridCreateInfo {
		u32 imageWidth;
		u32 imageHeight;
		u32 maxTriangleIndices;
		u32 initialBinCapacity = 4;
	};

	TileGrid tile_grid_create(TileGridCreateInfo const & createInfo);
	void tile_grid_destroy(TileGrid const & grid);

	// call once per frame before binning
	void tile_grid_clear(TileGrid & grid);

	// assign a triangle to a tile bin
	void tile_grid_bin_triangle(
		TileGrid & grid, u32v2 tile, u32 const triangleIndex
	);

	void tile_grid_bin_triangle_bbox(
		TileGrid & grid,
		i32bbox2 const & bounds,
		u32 const triangleIndex
	);

	TileBin & tile_grid_bin(TileGrid & grid, u32v2 tile);

	TileBin const & tile_grid_bin(TileGrid const & grid, u32v2 tile);

} // namespace srat
