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
		srat::array<i32v2, 3> screenPos {};
		srat::array<float, 3> depth {};
		srat::array<float, 3> perspectiveW {};
		srat::array<f32v2, 3> uv {};
	};

	// tile bin structure
	struct TileBin {
		std::vector<u32> triangleIndices;
	};

	struct TileGridCreateInfo {
		i32 imageWidth {0};
		i32 imageHeight {0};
	};

	TileGrid tile_grid_create(TileGridCreateInfo const & createInfo);
	void tile_grid_destroy(TileGrid const & grid);

	i32v2 tile_grid_tile_count(TileGrid const & grid);

	// call once per frame before binning
	void tile_grid_clear(TileGrid const & grid);

	srat::slice<TileTriangleData const>
	tile_grid_triangle_data(
		TileGrid const & grid
	);

	// assign a triangle to a tile bin
	void tile_grid_bin_triangle_bbox(
		TileGrid & grid,
		TileTriangleData const & triangleData
	);

	TileBin & tile_grid_bin(TileGrid const & grid, i32v2 const & tile);

	bool tile_grid_valid(TileGrid const & grid);

	// --------------------------------------------------------------------------
	// -- utility functions
	// --------------------------------------------------------------------------

	// calculates the number of tiles needed to cover a viewport
	// of the given dimensions
	i32v2 viewport_tile_count(srat::gfx::Viewport const & viewport);

} // namespace srat
