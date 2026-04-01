#include <cstdio>
#include <srat/tile-grid.hpp>

#include <srat/handle.hpp>

#include <vector>

namespace {

struct ImplTileGrid {
	u32 tileCountX;
	u32 tileCountY;
	srat::ArenaAllocator<srat::TileTriangleData> binAllocatorTriangles;
#if SRAT_BINNING_USE_INDEX_CACHE()
	size_t triangleCount { 0 };
#endif
	std::vector<srat::TileBin> bins;
	u32 initialBinCapacity;
};


static srat::HandlePool<srat::TileGrid, ImplTileGrid> sTileGridPool = (
	srat::HandlePool<srat::TileGrid, ImplTileGrid>::create(128, "TileGridPool")
);

void tile_grid_bin_triangle(
	srat::TileGrid & grid,
	u32v2 tile,
#if SRAT_BINNING_USE_INDEX_CACHE()
	u32 const triangleIndex
#else
	srat::TileTriangleData const & triangleData
#endif
) {
	// assign a triangle to a tile bin
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	srat::TileBin & bin = tile_grid_bin(grid, tile);

	// first check if we need to grow bin's triangle index storage
	// all memory needs to be contiguous, so requires memcpy
#if SRAT_BINNING_USE_INDEX_CACHE()
	bin.triangleIndices.emplace_back(triangleIndex);
#else
	// allocate more memory if necessary, copying old data if it exists
	if (bin.triangleCount == bin.triangleCapacity) {
		srat::TileTriangleData * const oldData = bin.triangleData;
		u32 const newCapacity = (
			  (bin.triangleCapacity == 0)
			? impl.initialBinCapacity
			: (bin.triangleCapacity * 2)
		);
		srat::TileTriangleData * const newData = (
			impl.binAllocatorTriangles.allocate(newCapacity)
		);
		SRAT_ASSERT(newData != nullptr); // out of memory
		if (oldData != nullptr) {
			memcpy(
				newData, oldData, bin.triangleCount * sizeof(srat::TileTriangleData)
			);
		}

		// -- update bin capacity/ptr
		bin.triangleCapacity = newCapacity;
		bin.triangleData = newData;
		SRAT_ASSERT(bin.triangleCount < bin.triangleCapacity);
	}
	// store triangle data
	memcpy(
		&bin.triangleData[bin.triangleCount],
		&triangleData,
		sizeof(srat::TileTriangleData)
	);
	bin.triangleCount += 1;
#endif
}

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
		.binAllocatorTriangles = (
			srat::ArenaAllocator<srat::TileTriangleData>::create(
				maxVertices, "TileGridBinTriangles"
			)
		),
#if SRAT_BINNING_USE_INDEX_CACHE()
		.triangleCount = 0,
#endif
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

srat::TileTriangleData const & srat::tile_grid_triangle_data(
	TileGrid const & grid,
	u32 triangleIndex
) {
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	return impl.binAllocatorTriangles.data_ptr()[triangleIndex];
}

void srat::tile_grid_clear(TileGrid & grid) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	SRAT_ASSERT(impl != nullptr);
	impl->binAllocatorTriangles.clear();
#if SRAT_BINNING_USE_INDEX_CACHE()
	impl->triangleCount = 0;
#endif
	for (auto & bin : impl->bins) {
#if SRAT_BINNING_USE_INDEX_CACHE()
		bin.triangleIndices.clear();
#else
		bin.triangleData = nullptr;
		bin.triangleCount = 0;
		bin.triangleCapacity = 0;
#endif
	}
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

	// -- cache the triangle data for binning, so multiple bins can refer to
	//    a single index
#if SRAT_BINNING_USE_INDEX_CACHE()
	u32 triangleIndex = impl.triangleCount;
	{
		// store triangle data in the bin's triangle list
		srat::TileTriangleData * const triangleDataPtr = (
			impl.binAllocatorTriangles.allocate(1)
		);
		SRAT_ASSERT(triangleDataPtr != nullptr); // out of memory
		// *triangleDataPtr = triangleData;
		memcpy(triangleDataPtr, &triangleData, sizeof(srat::TileTriangleData));

		SRAT_ASSERT(triangleDataPtr == impl.binAllocatorTriangles.data_ptr() + triangleIndex);

		triangleIndex = impl.triangleCount;
		impl.triangleCount += 1;
	}
#endif

	for (i32 y = minTile.y; y <= maxTile.y; ++y)
	for (i32 x = minTile.x; x <= maxTile.x; ++x) {
#if SRAT_BINNING_USE_INDEX_CACHE()
		tile_grid_bin_triangle(grid, u32v2(x, y), triangleIndex);
#else
		tile_grid_bin_triangle(grid, u32v2(x, y), triangleData);
#endif
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
