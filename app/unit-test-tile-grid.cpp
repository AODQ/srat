#include <srat/types.hpp>
#include <srat/tile-grid.hpp>

#include <doctest/doctest.h>

TEST_SUITE("tile grid") {

TEST_CASE("tile grid resets") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangles = 64,
		.initialBinCapacity = 4,
	});

	for (u32 i = 0; i < 1000; ++i) {
		for (u32 i = 0; i < 4; ++i) {
			srat::tile_grid_bin_triangle(grid, { 0, 0 }, {});
		}
		srat::tile_grid_clear(grid);
	}

	srat::tile_grid_destroy(grid);
}

// TEST_CASE("tile grid create and destroy") {
// 	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
// 		.imageWidth = 512,
// 		.imageHeight = 512,
// 		.maxTriangles = 64,
// 	});

// 	// 512 / 16 = 32 tiles per axis
// 	auto const & bin00 = srat::tile_grid_bin(grid, { 0, 0 });
// 	CHECK(bin00.triangleCount == 0);

// 	auto const & bin31 = srat::tile_grid_bin(grid, { 31, 31 });
// 	CHECK(bin31.triangleCount == 0);

// 	srat::tile_grid_destroy(grid);
// }

// TEST_CASE("tile grid non-power-of-two image rounds up") {
// 	// 100 / 16 = 6.25 → 7 tiles
// 	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
// 		.imageWidth = 100,
// 		.imageHeight = 100,
// 		.maxTriangles = 64,
// 	});

// 	// tile (6,6) should be valid — the 7th tile
// 	auto const & bin = srat::tile_grid_bin(grid, { 6, 6 });
// 	CHECK(bin.triangleCount == 0);

// 	srat::tile_grid_destroy(grid);
// }


} // -- end tile grid test suite
