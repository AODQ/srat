#include <srat/tile-grid.hpp>

#include <srat/handle.hpp>

#include <vector>

namespace {

struct ImplTileGrid {
	u32 tileCountX;
	u32 tileCountY;
	srat::ArenaAllocator<u32> binAllocator;
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
	u32 const tileCountX = (createInfo.imageWidth + kTileDim - 1) / kTileDim;
	u32 const tileCountY = (createInfo.imageHeight + kTileDim - 1) / kTileDim;
	u32 const tileCount = createInfo.maxTriangleIndices;
	return sTileGridPool.allocate(ImplTileGrid {
		.tileCountX = tileCountX,
		.tileCountY = tileCountY,
		.binAllocator = ArenaAllocator<u32>::create(
			tileCount, "TileGridBinAllocator"
		),
		.bins = std::vector<srat::TileBin>(tileCountX * tileCountY),
		.initialBinCapacity = createInfo.initialBinCapacity,
	});
}

void srat::tile_grid_destroy(srat::TileGrid const & grid) {
	sTileGridPool.free(grid);
}

void srat::tile_grid_clear(TileGrid & grid) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	SRAT_ASSERT(impl != nullptr);
	impl->binAllocator.clear();
	for (auto & bin : impl->bins) {
		bin.triangleCount = 0;
		bin.triangleCapacity = 0;
		bin.triangleIndices = nullptr;
	}
}

void srat::tile_grid_bin_triangle(
	TileGrid & grid, u32v2 tile, u32 const triangleIndex
) {
	// assign a triangle to a tile bin
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	srat::TileBin & bin = tile_grid_bin(grid, tile);

	// first check if we need to grow bin's triangle index storage
	if (bin.triangleCount == bin.triangleCapacity) {
		u32 * oldIndices = bin.triangleIndices;
		// -- allocate new storage for triangle indices
		u32 const newCapacity = (
			(bin.triangleCapacity == 0) ? 32 : (bin.triangleCapacity * 2)
		);
		bin.triangleIndices = (
			impl.binAllocator.allocate(newCapacity)
		);
		if (bin.triangleIndices == nullptr) {
			SRAT_ASSERT(false && "tile bin triangle index allocation failed");
			return;
		}
		// -- memcpy old indices to new storage if needed
		if (oldIndices != nullptr) {
			std::memcpy(
				bin.triangleIndices,
				oldIndices,
				bin.triangleCount * sizeof(u32)
			);
		}

		// -- update bin capacity/ptr
		bin.triangleCapacity = newCapacity;
	}

	SRAT_ASSERT(bin.triangleCount < bin.triangleCapacity);
	bin.triangleIndices[bin.triangleCount] = triangleIndex;
	bin.triangleCount += 1;
}

void srat::tile_grid_bin_triangle_bbox(
	TileGrid & grid,
	i32bbox2 const & bounds,
	u32 const triangleIndex
) {
	auto & impl = *sTileGridPool.get(grid);
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
	for (i32 y = minTile.y; y <= maxTile.y; ++y)
	for (i32 x = minTile.x; x <= maxTile.x; ++x) {
		tile_grid_bin_triangle(grid, u32v2(x, y), triangleIndex);
	};
}

srat::TileBin & srat::tile_grid_bin(TileGrid & grid, u32v2 tile) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	return impl->bins[(tile.y * impl->tileCountX) + tile.x];
}

srat::TileBin const & srat::tile_grid_bin(TileGrid const & grid, u32v2 tile) {
	ImplTileGrid * impl = sTileGridPool.get(grid);
	SRAT_ASSERT(impl != nullptr);
	u32 const tileIndex = (tile.y * impl->tileCountX) + tile.x;
	SRAT_ASSERT(tileIndex < (impl->tileCountX * impl->tileCountY));
	return impl->bins[tileIndex];
}
