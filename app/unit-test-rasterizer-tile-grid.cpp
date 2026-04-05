#include <srat/core-types.hpp>
#include <srat/rasterizer-tile-grid.hpp>

#include <doctest/doctest.h> // NOLINT

// -----------------------------------------------------------------------------
// -- helpers
// -----------------------------------------------------------------------------

static constexpr u32 kTileSize = SRAT_TILE_SIZE();
static constexpr u32 kImageW   = kTileSize * 4; // 4x4 tiles
static constexpr u32 kImageH   = kTileSize * 4;

//NOLINTBEGIN

static srat::TileGrid make_grid(
	u32 w = kImageW,
	u32 h = kImageH,
	u32 maxTris = 64
) {
	return srat::tile_grid_create({
		.imageWidth  = w,
		.imageHeight = h,
		.maxTriangles = maxTris,
	});
}

// construct a triangle fully inside a single tile at tile (tx, ty)
static srat::TileTriangleData make_triangle(
	i32v2 * pos,
	float	  * depth,
	float	  * perspW,
	f32v4 * color,
	u32 tx, u32 ty,
	u32 margin = 4,
	float depthV = 0.5f
) {
	u32 const ox = tx * kTileSize + margin;
	u32 const oy = ty * kTileSize + margin;
	pos[0] = { (i32)(ox),			  (i32)(oy)			  };
	pos[1] = { (i32)(ox + margin),	 (i32)(oy + margin * 2) };
	pos[2] = { (i32)(ox + margin * 2), (i32)(oy)			  };
	depth[0] = depth[1] = depth[2] = depthV;
	perspW[0] = perspW[1] = perspW[2] = 1.f;
	color[0] = color[1] = color[2] = { 1.f, 0.f, 0.f, 1.f };
	return srat::TileTriangleData {
		.screenPos   = srat::array<i32v2, 3> { pos[0], pos[1], pos[2] },
		.depth	   = srat::array<float,		3> { depth[0], depth[1], depth[2] },
		.perspectiveW = srat::array<float,		3> { perspW[0], perspW[1], perspW[2] },
		.color	   = srat::array<f32v4,  3> { color[0], color[1], color[2] },
	};
}

// -----------------------------------------------------------------------------
// -- creation / destruction
// -----------------------------------------------------------------------------

TEST_SUITE("tile grid") {

TEST_CASE("tile grid [create]") {
	auto grid = make_grid();
	auto const count = srat::tile_grid_tile_count(grid);
	CHECK(count.x == kImageW / kTileSize);
	CHECK(count.y == kImageH / kTileSize);
	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [create 1x1 tile]") {
	auto grid = make_grid(kTileSize, kTileSize);
	auto const count = srat::tile_grid_tile_count(grid);
	CHECK(count.x == 1);
	CHECK(count.y == 1);
	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [create non-square]") {
	auto grid = make_grid(kTileSize * 2, kTileSize * 8);
	auto const count = srat::tile_grid_tile_count(grid);
	CHECK(count.x == 2);
	CHECK(count.y == 8);
	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [unique ids]") {
	auto a = make_grid();
	auto b = make_grid();
	CHECK(a.id != b.id);
	srat::tile_grid_destroy(a);
	srat::tile_grid_destroy(b);
}

// -----------------------------------------------------------------------------
// -- clear
// -----------------------------------------------------------------------------

TEST_CASE("tile grid [clear empties bins]") {
	auto grid = make_grid();

	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	auto tri = make_triangle(pos, depth, perspW, col, 0, 0);
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	auto & bin = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(!bin.triangleIndices.empty());

	srat::tile_grid_clear(grid);

	auto & binAfter = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(binAfter.triangleIndices.empty());

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [clear then rebin]") {
	auto grid = make_grid();

	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	auto tri = make_triangle(pos, depth, perspW, col, 0, 0);
	srat::tile_grid_bin_triangle_bbox(grid, tri);
	srat::tile_grid_clear(grid);

	srat::tile_grid_bin_triangle_bbox(grid, tri);
	auto & bin = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(bin.triangleIndices.size() == 1);

	srat::tile_grid_destroy(grid);
}

// -----------------------------------------------------------------------------
// -- binning single triangle
// -----------------------------------------------------------------------------

TEST_CASE("tile grid [bin triangle into single tile]") {
	auto grid = make_grid();

	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	auto tri = make_triangle(pos, depth, perspW, col, 1, 1);
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	auto & bin = srat::tile_grid_bin(grid, { 1, 1 });
	CHECK(bin.triangleIndices.size() == 1);

	// other tiles must be empty
	CHECK(srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices.empty());
	CHECK(srat::tile_grid_bin(grid, { 2, 2 }).triangleIndices.empty());

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [triangle data retrievable after bin]") {
	auto grid = make_grid();

	// construct on stack then let it go out of scope to verify grid copied it
	u32 triIdx = 0;
	{
		i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
		auto tri = make_triangle(pos, depth, perspW, col, 0, 0);
		srat::tile_grid_bin_triangle_bbox(grid, tri);
		auto & bin = srat::tile_grid_bin(grid, { 0, 0 });
		REQUIRE(!bin.triangleIndices.empty());
		triIdx = bin.triangleIndices[0];
	}
	// stack data is gone — grid must have its own copy
	auto const & data = srat::tile_grid_triangle_data(grid, triIdx);
	CHECK(data.screenPos[0].x >= 0);
	CHECK(data.depth[0] == doctest::Approx(0.5f));
	CHECK(data.perspectiveW[0] == doctest::Approx(1.f));

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [bin preserves triangle data]") {
	auto grid = make_grid();

	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 color[3];
	pos[0] = { 10, 10 }; pos[1] = { 20, 30 }; pos[2] = { 30, 10 };
	depth[0] = 0.1f; depth[1] = 0.5f; depth[2] = 0.9f;
	perspW[0] = 0.25f; perspW[1] = 0.5f; perspW[2] = 0.75f;
	color[0] = { 1.f, 0.f, 0.f, 1.f };
	color[1] = { 0.f, 1.f, 0.f, 1.f };
	color[2] = { 0.f, 0.f, 1.f, 1.f };

	srat::TileTriangleData tri {
		.screenPos	= { pos[0], pos[1], pos[2] },
		.depth		= { depth[0], depth[1], depth[2] },
		.perspectiveW = { perspW[0], perspW[1], perspW[2] },
		.color		= { color[0], color[1], color[2] },
	};
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	auto & bin = srat::tile_grid_bin(grid, { 0, 0 });
	REQUIRE(!bin.triangleIndices.empty());
	auto const & data = (
		srat::tile_grid_triangle_data(grid, bin.triangleIndices[0])
	);

	CHECK(data.screenPos[0].x == 10); CHECK(data.screenPos[0].y == 10);
	CHECK(data.screenPos[1].x == 20); CHECK(data.screenPos[1].y == 30);
	CHECK(data.screenPos[2].x == 30); CHECK(data.screenPos[2].y == 10);
	CHECK(data.depth[0] == doctest::Approx(0.1f));
	CHECK(data.depth[1] == doctest::Approx(0.5f));
	CHECK(data.depth[2] == doctest::Approx(0.9f));
	CHECK(data.perspectiveW[0] == doctest::Approx(0.25f));
	CHECK(data.perspectiveW[1] == doctest::Approx(0.5f));
	CHECK(data.perspectiveW[2] == doctest::Approx(0.75f));

	srat::tile_grid_destroy(grid);
}

// -----------------------------------------------------------------------------
// -- binning spanning multiple tiles
// -----------------------------------------------------------------------------

TEST_CASE("tile grid [triangle spanning 2x1 tiles]") {
	auto grid = make_grid();

	// triangle straddling tile (0,0) and (1,0)
	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	pos[0] = { (i32)(kTileSize / 2),	 4			  };
	pos[1] = { (i32)(kTileSize + 4),	 4			  };
	pos[2] = { (i32)(kTileSize / 2),	 (i32)kTileSize - 4 };
	depth[0] = depth[1] = depth[2] = 0.5f;
	perspW[0] = perspW[1] = perspW[2] = 1.f;
	col[0] = col[1] = col[2] = { 1.f, 1.f, 1.f, 1.f };

	srat::TileTriangleData tri {
		.screenPos	= { pos[0], pos[1], pos[2] },
		.depth		= { depth[0], depth[1], depth[2] },
		.perspectiveW = { perspW[0], perspW[1], perspW[2] },
		.color		= { col[0], col[1], col[2] },
	};
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	CHECK(!srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices.empty());
	CHECK(!srat::tile_grid_bin(grid, { 1, 0 }).triangleIndices.empty());
	CHECK(srat::tile_grid_bin(grid, { 0, 1 }).triangleIndices.empty());

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [triangle spanning 2x2 tiles]") {
	auto grid = make_grid();

	// triangle straddling all four corner tiles
	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	pos[0] = { (i32)(kTileSize / 2),	 (i32)(kTileSize / 2)	 };
	pos[1] = { (i32)(kTileSize + 4),	 (i32)(kTileSize / 2)	 };
	pos[2] = { (i32)(kTileSize / 2),	 (i32)(kTileSize + 4)	 };
	depth[0] = depth[1] = depth[2] = 0.5f;
	perspW[0] = perspW[1] = perspW[2] = 1.f;
	col[0] = col[1] = col[2] = { 1.f, 1.f, 1.f, 1.f };

	srat::TileTriangleData tri {
		.screenPos	= { pos[0],pos[1],pos[2] },
		.depth		= { depth[0],depth[1],depth[2] },
		.perspectiveW = { perspW[0],perspW[1],perspW[2] },
		.color		= { col[0],col[1],col[2] },
	};
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	CHECK(!srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices.empty());
	CHECK(!srat::tile_grid_bin(grid, { 1, 0 }).triangleIndices.empty());
	CHECK(!srat::tile_grid_bin(grid, { 0, 1 }).triangleIndices.empty());
	CHECK(!srat::tile_grid_bin(grid, { 1, 1 }).triangleIndices.empty());

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [same triangle index in all spanned tiles]") {
	auto grid = make_grid();

	i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
	pos[0] = { (i32)(kTileSize / 2),	 (i32)(kTileSize / 2)	 };
	pos[1] = { (i32)(kTileSize + 4),	 (i32)(kTileSize / 2)	 };
	pos[2] = { (i32)(kTileSize / 2),	 (i32)(kTileSize + 4)	 };
	depth[0] = depth[1] = depth[2] = 0.5f;
	perspW[0] = perspW[1] = perspW[2] = 1.f;
	col[0] = col[1] = col[2] = { 1.f, 1.f, 1.f, 1.f };

	srat::TileTriangleData tri {
		.screenPos	= { pos[0],pos[1],pos[2] },
		.depth		= { depth[0],depth[1],depth[2] },
		.perspectiveW = { perspW[0],perspW[1],perspW[2] },
		.color		= { col[0],col[1],col[2] },
	};
	srat::tile_grid_bin_triangle_bbox(grid, tri);

	// all tiles must reference the same triangle index
	u32 const idx00 = srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices[0];
	u32 const idx10 = srat::tile_grid_bin(grid, { 1, 0 }).triangleIndices[0];
	u32 const idx01 = srat::tile_grid_bin(grid, { 0, 1 }).triangleIndices[0];
	u32 const idx11 = srat::tile_grid_bin(grid, { 1, 1 }).triangleIndices[0];
	CHECK(idx00 == idx10);
	CHECK(idx00 == idx01);
	CHECK(idx00 == idx11);

	srat::tile_grid_destroy(grid);
}

// -----------------------------------------------------------------------------
// -- multiple triangles
// -----------------------------------------------------------------------------

TEST_CASE("tile grid [multiple triangles same tile]") {
	auto grid = make_grid();

	for (u32 i = 0; i < 4; ++i) {
		i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
		auto tri = make_triangle(pos, depth, perspW, col, 0, 0, 2 + i);
		srat::tile_grid_bin_triangle_bbox(grid, tri);
	}

	auto & bin = srat::tile_grid_bin(grid, { 0, 0 });
	CHECK(bin.triangleIndices.size() == 4);

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [multiple triangles different tiles]") {
	auto grid = make_grid();

	for (u32 tx = 0; tx < 4; ++tx) {
		i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
		auto tri = make_triangle(pos, depth, perspW, col, tx, 0);
		srat::tile_grid_bin_triangle_bbox(grid, tri);
	}

	for (u32 tx = 0; tx < 4; ++tx) {
		auto & bin = srat::tile_grid_bin(grid, { tx, 0 });
		CHECK(bin.triangleIndices.size() == 1);
	}
	CHECK(srat::tile_grid_bin(grid, { 0, 1 }).triangleIndices.empty());

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [triangle indices are unique per triangle]") {
	auto grid = make_grid();

	u32 indices[4];
	for (u32 i = 0; i < 4; ++i) {
		i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
		auto tri = make_triangle(pos, depth, perspW, col, i, 0);
		srat::tile_grid_bin_triangle_bbox(grid, tri);
		auto & bin = srat::tile_grid_bin(grid, { i, 0 });
		REQUIRE(!bin.triangleIndices.empty());
		indices[i] = bin.triangleIndices[0];
	}

	// all indices must be distinct
	for (u32 i = 0; i < 4; ++i) {
		for (u32 j = i + 1; j < 4; ++j) {
			CHECK(indices[i] != indices[j]);
		}
	}

	srat::tile_grid_destroy(grid);
}

TEST_CASE("tile grid [triangle data independent per triangle]") {
	auto grid = make_grid();

	// bin two triangles with distinct depth values
	i32v2 posA[3]; float depthA[3]; float perspWA[3]; f32v4 colA[3];
	i32v2 posB[3]; float depthB[3]; float perspWB[3]; f32v4 colB[3];

	auto triA = make_triangle(posA, depthA, perspWA, colA, 0, 0, 4, 0.1f);
	srat::tile_grid_bin_triangle_bbox(grid, triA);

	auto triB = make_triangle(posB, depthB, perspWB, colB, 1, 0, 4, 0.9f);
	srat::tile_grid_bin_triangle_bbox(grid, triB);

	auto & binA = srat::tile_grid_bin(grid, { 0, 0 });
	auto & binB = srat::tile_grid_bin(grid, { 1, 0 });

	CHECK_EQ(binA.triangleIndices.size(), 1);
	CHECK_EQ(binB.triangleIndices.size(), 1);

	u32 const idxA = binA.triangleIndices[0];
	u32 const idxB = binB.triangleIndices[0];

	CHECK(srat::tile_grid_triangle_data(grid, idxA).depth[0] == doctest::Approx(0.1f));
	CHECK(srat::tile_grid_triangle_data(grid, idxB).depth[0] == doctest::Approx(0.9f));

	srat::tile_grid_destroy(grid);
}

// -----------------------------------------------------------------------------
// -- stack data lifetime
// -----------------------------------------------------------------------------

TEST_CASE("tile grid [triangle data survives source going out of scope]") {
	auto grid = make_grid();

	u32 triIdx = 0;
	float expectedDepth = 0.f;
	{
		i32v2 pos[3]; float depth[3]; float perspW[3]; f32v4 col[3];
		depth[0] = depth[1] = depth[2] = 0.77f;
		expectedDepth = depth[0];
		pos[0] = { 4, 4 }; pos[1] = { 12, 20 }; pos[2] = { 20, 4 };
		perspW[0] = perspW[1] = perspW[2] = 1.f;
		col[0] = col[1] = col[2] = { 1.f, 1.f, 1.f, 1.f };

		srat::TileTriangleData tri {
			.screenPos	= { pos[0],pos[1],pos[2] },
			.depth		= { depth[0],depth[1],depth[2] },
			.perspectiveW = { perspW[0],perspW[1],perspW[2] },
			.color		= { col[0],col[1],col[2] },
		};
		srat::tile_grid_bin_triangle_bbox(grid, tri);
		triIdx = srat::tile_grid_bin(grid, { 0, 0 }).triangleIndices[0];
	}
	// pos/depth/perspW/col arrays are gone — grid must own the data
	auto const & data = srat::tile_grid_triangle_data(grid, triIdx);
	CHECK(data.depth[0] == doctest::Approx(expectedDepth));

	srat::tile_grid_destroy(grid);
}

} // -- end tile grid test suite

//NOLINTEND
