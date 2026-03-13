#include <srat/types.hpp>
#include <srat/tile-grid.hpp>

#include <doctest/doctest.h>

TEST_SUITE("tile grid") {

TEST_CASE("tile grid resets") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 64,
		.initialBinCapacity = 4,
	});

	for (u32 i = 0; i < 1000; ++i) {
		for (u32 i = 0; i < 4; ++i) {
			srat::tile_grid_bin_triangle(grid, { 0, 0 }, i);
		}
		srat::tile_grid_clear(grid);
	}

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid create and destroy") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 64,
	});

	// 512 / 16 = 32 tiles per axis
	auto const & bin00 = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(bin00.triangleCount == 0);

	auto const & bin31 = srat::tile_grid_bin(grid, { 31, 31 });
	CHECK(bin31.triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid non-power-of-two image rounds up") {
	// 100 / 16 = 6.25 → 7 tiles
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 100,
		.imageHeight = 100,
		.maxTriangleIndices = 64,
	});

	// tile (6,6) should be valid — the 7th tile
	auto const & bin = srat::tile_grid_bin(grid, { 6, 6 });
	CHECK(bin.triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bin single triangle") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 64,
	});

	srat::tile_grid_bin_triangle(grid, { 0, 0 }, 42);

	auto const & bin = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(bin.triangleCount == 1);
	CHECK(bin.triangleIndices[0] == 42);

	// other bins untouched
	auto const & other = srat::tile_grid_bin(grid, { 1, 0 });
	CHECK(other.triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bin multiple triangles to same tile") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 256,
	});

	for (u32 i = 0; i < 100; ++i) {
		srat::tile_grid_bin_triangle(grid, { 5, 5 }, i);
	}

	auto const & bin = srat::tile_grid_bin(grid, { 5, 5 });
	CHECK(bin.triangleCount == 100);
	for (u32 i = 0; i < 100; ++i) {
		CHECK(bin.triangleIndices[i] == i);
	}

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bin triangle bbox single tile") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 64,
	});

	// bbox entirely within tile (1,1): pixels [16,31] x [16,31]
	srat::tile_grid_bin_triangle_bbox(grid, { .min = { 16, 16 }, .max = { 31, 31 } }, 7);

	auto const & hit = srat::tile_grid_bin(grid, { 1, 1 });
	CHECK(hit.triangleCount == 1);
	CHECK(hit.triangleIndices[0] == 7);

	// neighboring tiles should be empty
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleCount == 0);
	CHECK(srat::tile_grid_bin(grid, { 0, 1 }).triangleCount == 0);
	CHECK(srat::tile_grid_bin(grid, { 1, 0 }).triangleCount == 0);
	CHECK(srat::tile_grid_bin(grid, { 2, 2 }).triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bin triangle bbox spanning multiple tiles") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 1024,
	});

	// bbox from pixel (0,0) to (47,47) → covers tiles (0,0) through (2,2) = 9 tiles
	srat::tile_grid_bin_triangle_bbox(grid, { .min = { 0, 0 }, .max = { 47, 47 } }, 3);

	for (i32 ty = 0; ty <= 2; ++ty) {
		for (i32 tx = 0; tx <= 2; ++tx) {
			auto const & bin = srat::tile_grid_bin(grid, { (u32)tx, (u32)ty });
			CHECK_MESSAGE(bin.triangleCount == 1, "tile (", tx, ",", ty, ")");
			CHECK(bin.triangleIndices[0] == 3);
		}
	}

	// tile (3,0) should not be hit
	CHECK(srat::tile_grid_bin(grid, { 3, 0 }).triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid clear resets all bins") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 1'024,
	});

	srat::tile_grid_bin_triangle(grid, { 0, 0 }, 1);
	srat::tile_grid_bin_triangle(grid, { 5, 5 }, 2);
	srat::tile_grid_bin_triangle(grid, { 31, 31 }, 3);

	srat::tile_grid_clear(grid);

	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleCount == 0);
	CHECK(srat::tile_grid_bin(grid, { 5, 5 }).triangleCount == 0);
	CHECK(srat::tile_grid_bin(grid, { 31, 31 }).triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid clear then reuse") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 64,
	});

	srat::tile_grid_bin_triangle(grid, { 0, 0 }, 10);
	srat::tile_grid_bin_triangle(grid, { 0, 0 }, 11);
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleCount == 2);

	srat::tile_grid_clear(grid);

	// bin into different tiles after clear
	srat::tile_grid_bin_triangle(grid, { 3, 3 }, 99);
	CHECK(srat::tile_grid_bin(grid, { 3, 3 }).triangleCount == 1);
	CHECK(srat::tile_grid_bin(grid, { 3, 3 }).triangleIndices[0] == 99);

	// old tile should still be empty
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleCount == 0);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bin grows past initial capacity") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 32,
		.imageHeight = 32,
		.maxTriangleIndices = 4'096,
	});

	// push way more than the initial 32 capacity
	u32 const count = 200;
	for (u32 i = 0; i < count; ++i) {
		srat::tile_grid_bin_triangle(grid, { 0, 0 }, i);
	}

	auto const & bin = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(bin.triangleCount == count);
	CHECK(bin.triangleCapacity >= count);

	// verify data integrity after multiple growths
	for (u32 i = 0; i < count; ++i) {
		CHECK(bin.triangleIndices[i] == i);
	}

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid multiple triangles across many tiles") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 512,
		.imageHeight = 512,
		.maxTriangleIndices = 8192,
	});

	// triangle 0: covers tiles (0,0)-(1,1)
	srat::tile_grid_bin_triangle_bbox(grid, { .min = { 0, 0 }, .max = { 31, 31 } }, 0);
	// triangle 1: covers tiles (1,1)-(2,2)
	srat::tile_grid_bin_triangle_bbox(grid, { .min = { 16, 16 }, .max = { 47, 47 } }, 1);

	// tile (0,0): only triangle 0
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleCount == 1);
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices[0] == 0);

	// tile (1,1): both triangles
	auto const & overlap = srat::tile_grid_bin(grid, { 1, 1 });
	CHECK(overlap.triangleCount == 2);
	CHECK(overlap.triangleIndices[0] == 0);
	CHECK(overlap.triangleIndices[1] == 1);

	// tile (2,2): only triangle 1
	CHECK(srat::tile_grid_bin(grid, { 2, 2 }).triangleCount == 1);
	CHECK(srat::tile_grid_bin(grid, { 2, 2 }).triangleIndices[0] == 1);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid bbox clamped to grid bounds") {
	auto grid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = 64,
		.imageHeight = 64,
		.maxTriangleIndices = 4096,
	});

	// 64/16 = 4 tiles per axis (0..3)
	// bbox that would extend beyond the grid
	srat::tile_grid_bin_triangle_bbox(grid, { .min = { 0, 0 }, .max = { 999, 999 } }, 0);

	// all 16 tiles should have the triangle
	for (u32 ty = 0; ty < 4; ++ty) {
		for (u32 tx = 0; tx < 4; ++tx) {
			auto const & bin = srat::tile_grid_bin(grid, { tx, ty });
			CHECK_MESSAGE(bin.triangleCount == 1, "tile (", tx, ",", ty, ")");
		}
	}

	srat::tile_grid_destroy(grid);
}

} // -- end tile grid test suite
