#include <cstdio>
#include <srat/tile-grid.hpp>

#include <srat/handle.hpp>

#include <vector>

namespace {

struct ImplTileGrid {
	u32 tileCountX;
	u32 tileCountY;
	srat::ArenaAllocator<i32v2> binAllocatorScreenPos;
	srat::ArenaAllocator<float> binAllocatorDepth;
	srat::ArenaAllocator<float> binAllocatorPerspectiveW;
	srat::ArenaAllocator<f32v4> binAllocatorColor;
	std::vector<srat::TileBin> bins;
	u32 initialBinCapacity;
};


static srat::HandlePool<srat::TileGrid, ImplTileGrid> sTileGridPool = (
	srat::HandlePool<srat::TileGrid, ImplTileGrid>::create(128, "TileGridPool")
);

}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

srat::TileGrid srat::tile_grid_create(TileGridCreateInfo const & createInfo) {
	auto const kTileDim = srat_tile_size();
	u32 const tileCountX = (createInfo.imageWidth + kTileDim - 1) / kTileDim;
	u32 const tileCountY = (createInfo.imageHeight + kTileDim - 1) / kTileDim;
	u32 const maxVertices = createInfo.maxTriangles * 3;
	return sTileGridPool.allocate(ImplTileGrid {
		.tileCountX = tileCountX,
		.tileCountY = tileCountY,
		.binAllocatorScreenPos = srat::ArenaAllocator<i32v2>::create(
			maxVertices, "TileGridBinScreenPos"
		),
		.binAllocatorDepth = srat::ArenaAllocator<float>::create(
			maxVertices, "TileGridBinDepth"
		),
		.binAllocatorPerspectiveW = srat::ArenaAllocator<float>::create(
			maxVertices, "TileGridBinPerspectiveW"
		),
		.binAllocatorColor = srat::ArenaAllocator<f32v4>::create(
			maxVertices, "TileGridBinColor"
		),
		.bins = std::vector<srat::TileBin>(tileCountX * tileCountY),
		.initialBinCapacity = createInfo.initialBinCapacity,
	});
}

void srat::tile_grid_destroy(srat::TileGrid const & grid) {
	sTileGridPool.free(grid);
}

u32v2 srat::tile_grid_tile_count(TileGrid const & grid) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	SRAT_ASSERT(impl != nullptr);
	return u32v2(impl->tileCountX, impl->tileCountY);
}

void srat::tile_grid_clear(TileGrid & grid) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	SRAT_ASSERT(impl != nullptr);
	impl->binAllocatorScreenPos.clear();
	impl->binAllocatorDepth.clear();
	impl->binAllocatorPerspectiveW.clear();
	impl->binAllocatorColor.clear();
	for (auto & bin : impl->bins) {
		bin = {};
	}
}

void srat::tile_grid_bin_triangle(
	srat::TileGrid & grid,
	u32v2 tile,
	srat::TileTriangleData const & triangleData
) {
	// assign a triangle to a tile bin
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	srat::TileBin & bin = tile_grid_bin(grid, tile);

	// first check if we need to grow bin's triangle index storage
	if (bin.triangleCount == bin.triangleCapacity) {
		i32v2 * const oldScreenPos = bin.triangleScreenPos;
		float * const oldDepth = bin.triangleDepth;
		float * const oldPerspectiveW = bin.trianglePerspectiveW;
		f32v4 * const oldColor = bin.triangleColor;
		// -- allocate new storage for triangle indices
		u32 const newCapacityTriangles = (
			  (bin.triangleCapacity == 0)
			? impl.initialBinCapacity
			: (bin.triangleCapacity * 2)
		);
		u32 const newCapacity = newCapacityTriangles * 3;
		bin.triangleScreenPos = (
			impl.binAllocatorScreenPos.allocate(newCapacity)
		);
		bin.triangleDepth = (
			impl.binAllocatorDepth.allocate(newCapacity)
		);
		bin.trianglePerspectiveW = (
			impl.binAllocatorPerspectiveW.allocate(newCapacity)
		);
		bin.triangleColor = (
			impl.binAllocatorColor.allocate(newCapacity)
		);
		SRAT_ASSERT(
			   bin.triangleScreenPos != nullptr
			&& bin.triangleDepth != nullptr
			&& bin.trianglePerspectiveW != nullptr
			&& bin.triangleColor != nullptr
		);
		if (!bin.triangleScreenPos) {
			printf("ran out of arena allocation for tile grid\n");
			exit(1);
		}
		// -- memcpy old indices to new storage if needed
		if (oldScreenPos != nullptr) {
			std::memcpy(
				bin.triangleScreenPos,
				oldScreenPos,
				bin.triangleCount * sizeof(i32v2) * 3
			);
			std::memcpy(
				bin.trianglePerspectiveW,
				oldPerspectiveW,
				bin.triangleCount * sizeof(float) * 3
			);
			std::memcpy(
				bin.triangleDepth,
				oldDepth,
				bin.triangleCount * sizeof(float) * 3
			);
			std::memcpy(
				bin.triangleColor,
				oldColor,
				bin.triangleCount * sizeof(f32v4) * 3
			);
		}

		// -- update bin capacity/ptr
		bin.triangleCapacity = newCapacityTriangles;
	}

	SRAT_ASSERT(bin.triangleCount < bin.triangleCapacity);
	// bin.triangleIndices[bin.triangleCount] = triangleIndex;
	auto const tc = bin.triangleCount * 3;
	bin.triangleScreenPos[tc+0] = triangleData.screenPos[0];
	bin.triangleScreenPos[tc+1] = triangleData.screenPos[1];
	bin.triangleScreenPos[tc+2] = triangleData.screenPos[2];
	bin.trianglePerspectiveW[tc+0] = triangleData.perspectiveW[0];
	bin.trianglePerspectiveW[tc+1] = triangleData.perspectiveW[1];
	bin.trianglePerspectiveW[tc+2] = triangleData.perspectiveW[2];
	bin.triangleDepth[tc+0] = triangleData.depth[0];
	bin.triangleDepth[tc+1] = triangleData.depth[1];
	bin.triangleDepth[tc+2] = triangleData.depth[2];
	bin.triangleColor[tc+0] = triangleData.color[0];
	bin.triangleColor[tc+1] = triangleData.color[1];
	bin.triangleColor[tc+2] = triangleData.color[2];
	bin.triangleCount += 1;
}

void srat::tile_grid_bin_triangle_bbox(
	TileGrid & grid,
	i32bbox2 const & bounds,
	srat::TileTriangleData const & triangleData
) {
	auto & impl = *sTileGridPool.get(grid);
	auto const kTileDim = srat_tile_size();
	// compute tile range covered by the triangle's bounding box
	// TODO: optimization is to allocate ahead-of-time
	i32v2 const minTile = (
		i32v2_clamp(
			bounds.min / i32v2(kTileDim, kTileDim),
			i32v2(0, 0),
			i32v2(impl.tileCountX, impl.tileCountY) - i32v2(1, 1)
		)
	);
	i32v2 const maxTile = (
		i32v2_clamp(
			(bounds.max - i32v2(1, 1)) / i32v2(kTileDim, kTileDim),
			i32v2(0, 0),
			i32v2(impl.tileCountX, impl.tileCountY) - i32v2(1, 1)
		)
	);
	if (maxTile.x < minTile.x || maxTile.y < minTile.y) {
		// triangle bbox does not intersect grid, skip binning
		return;
	}
	for (i32 y = minTile.y; y <= maxTile.y; ++y)
	for (i32 x = minTile.x; x <= maxTile.x; ++x) {
		tile_grid_bin_triangle(grid, u32v2(x, y), triangleData);
	};
}

srat::TileBin & srat::tile_grid_bin(TileGrid & grid, u32v2 tile) {
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	return impl.bins[(tile.y * impl.tileCountX) + tile.x];
}

srat::TileBin const & srat::tile_grid_bin(TileGrid const & grid, u32v2 tile) {
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	u32 const tileIndex = (tile.y * impl.tileCountX) + tile.x;
	SRAT_ASSERT(tileIndex < (impl.tileCountX * impl.tileCountY));
	return impl.bins[tileIndex];
}
