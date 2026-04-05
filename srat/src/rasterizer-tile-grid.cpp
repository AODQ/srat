#include <srat/rasterizer-tile-grid.hpp>

#include <srat/core-config.hpp>
#include <srat/core-array.hpp>
#include <srat/core-math.hpp>
#include <srat/alloc-arena.hpp>
#include <srat/core-handle.hpp>

#include <vector>

namespace {

struct ImplTileGrid {
	u32 tileCountX;
	u32 tileCountY;
	srat::AllocArena<srat::TileTriangleData> binAllocatorTriangles;
	std::vector<srat::TileTriangleData *> triangleDataPtrs;
	std::vector<srat::TileBin> bins;
	u32 initialBinCapacity;
};


static srat::HandlePool<srat::TileGrid, ImplTileGrid> sTileGridPool = (
	srat::HandlePool<srat::TileGrid, ImplTileGrid>::create(128, "TileGridPool")
);

void tile_grid_bin_triangle(
	srat::TileGrid & grid,
	u32v2 tile,
	u32 const triangleIndex
) {
	// assign a triangle to a tile bin
	srat::TileBin & bin = tile_grid_bin(grid, tile);
	bin.triangleIndices.emplace_back(triangleIndex);
#if 0 // reference for contiguous memory approach
	ImplTileGrid & impl = *sTileGridPool.get(grid);
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
			srat::AllocArena<srat::TileTriangleData>::create(
				maxVertices, "TileGridBinTriangles"
			)
		),
		.triangleDataPtrs = {},
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
	Let impl = *sTileGridPool.get(grid);
	SRAT_ASSERT(triangleIndex < impl.triangleDataPtrs.size());
	return *impl.triangleDataPtrs[triangleIndex];
}

void srat::tile_grid_clear(TileGrid const & grid) {
	Ref impl = *sTileGridPool.get(grid);
	impl.binAllocatorTriangles.clear();
	impl.triangleDataPtrs.clear();
	for (auto & bin : impl.bins) {
		bin.triangleIndices.clear();
	}
}

void srat::tile_grid_bin_triangle_bbox(
	TileGrid & grid,
	srat::TileTriangleData const & triangleData
) {
	Ref impl = *sTileGridPool.get(grid);
	Let kTileDim = u64{srat_tile_size()};

	Let bounds = i32bbox2 {
		i32bbox2_from_triangle(
			triangleData.screenPos[0],
			triangleData.screenPos[1],
			triangleData.screenPos[2]
		)
	};

	// compute tile range covered by the triangle's bounding box
	// TODO: optimization is to allocate ahead-of-time
	Let minTile = (
		i32v2_clamp(
			bounds.min / i32v2((i32)kTileDim, (i32)kTileDim),
			i32v2(0, 0),
			i32v2((i32)impl.tileCountX, (i32)impl.tileCountY) - i32v2(1, 1)
		)
	);
	Let maxTile = (
		i32v2_clamp(
			(bounds.max - i32v2(1, 1)) / i32v2((i32)kTileDim, (i32)kTileDim),
			i32v2(0, 0),
			i32v2((i32)impl.tileCountX, (i32)impl.tileCountY) - i32v2(1, 1)
		)
	);
	if (maxTile.x < minTile.x || maxTile.y < minTile.y) {
		// triangle bbox does not intersect grid, skip binning
		return;
	}

	// -- cache the triangle data for binning, so multiple bins can refer to
	//    a single index
	Let triangleIndex = [&]() -> u32 {
		// store triangle data in the bin's triangle list
		srat::TileTriangleData * const triangleDataPtr = (
			impl.binAllocatorTriangles.allocate(1)
		);
		// store the triangle data at the allocated index
		*triangleDataPtr = triangleData;

		impl.triangleDataPtrs.push_back(triangleDataPtr);
		return impl.triangleDataPtrs.size()-1;
	}();

	for (Mut y = i32{minTile.y}; y <= maxTile.y; ++y)
	for (Mut x = i32{minTile.x}; x <= maxTile.x; ++x) {
		tile_grid_bin_triangle(grid, u32v2(x, y), triangleIndex);
	};
}

srat::TileBin & srat::tile_grid_bin(TileGrid const & grid, u32v2 const & tile) {
	ImplTileGrid & impl = *sTileGridPool.get(grid);
	Let tileIndex = u32 {(tile.y * impl.tileCountX) + tile.x};
	SRAT_ASSERT(tileIndex < (impl.tileCountX * impl.tileCountY));
	return impl.bins[tileIndex];
}

bool srat::tile_grid_valid(TileGrid const & grid) {
	return sTileGridPool.valid(grid);
}

// -----------------------------------------------------------------------------
// -- utility functions
// -----------------------------------------------------------------------------

#include <srat/gfx-image.hpp>

u32v2 srat::viewport_tile_count(srat::gfx::Viewport const & viewport) {
	Let kTileDim = srat_tile_size();
	return (
		  (viewport.dim + u32v2(kTileDim - 1, kTileDim - 1))
		/ u32v2(kTileDim, kTileDim)
	);
}
