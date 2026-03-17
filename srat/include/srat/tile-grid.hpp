#pragma once

#include <srat/types.hpp>

#include <srat/math.hpp>

#include <srat/arena-allocator.hpp>

namespace srat {

	// tile bin structure
	struct TileBin {
		i32v2 * triangleScreenPos { nullptr };
		float * triangleDepth { nullptr };
		float * trianglePerspectiveW { nullptr };
		f32v4 * triangleColor { nullptr };
		u32 triangleCount { 0 };
		u32 triangleCapacity { 0 };
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

	struct TileTriangleData {
		i32v2 screenPos[3];
		float depth[3];
		float perspectiveW[3];
		f32v4 color[3];
	};

	// assign a triangle to a tile bin
	void tile_grid_bin_triangle(
		TileGrid & grid,
		u32v2 tile,
		TileTriangleData const & triangleData
	);

	void tile_grid_bin_triangle_bbox(
		TileGrid & grid,
		i32bbox2 const & bounds,
		TileTriangleData const & triangleData
	);

	TileBin & tile_grid_bin(TileGrid & grid, u32v2 tile);

	TileBin const & tile_grid_bin(TileGrid const & grid, u32v2 tile);

} // namespace srat
