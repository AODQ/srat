#include <srat/rasterizer-binning.hpp>

#include <srat/config.hpp>
#include <srat/arena-allocator.hpp>

#include <memory>
#include <vector>

// TODO swap "bin_tile" with "tile_bin" ; just for clarity

// -----------------------------------------------------------------------------
// -- private structures
// -----------------------------------------------------------------------------

namespace {

using ImplTileTriangleAllocator =
	srat::SoAArenaAllocator<
		srat::triangle_position_t,
		srat::triangle_depth_t,
		srat::triangle_perspective_w_t
	>;

struct ImplTileBinCache {
	std::tuple<
		srat::triangle_position_t *,
		srat::triangle_depth_t *,
		srat::triangle_perspective_w_t *
	> data;
	u32 count;
	u32 capacity;
};

// with index-caching it has a local allocation of indices into global data,
//   the trade-off being triangle data is non-contiguous but doesn't require
//   memcpies while growing bins
// without index-caching, it's a tuple of pointers to contiguous data.
//   this might require memcpies when growing bins without a second pass,
//   but allows for better memory locality during rasterization
using ImplTileBin =
#if defBinUseIndexCaching
	std::vector<srat::triangle_index_t>
#else
	ImplTileBinCache
#endif
;

struct ImplRasterizerBin {
	u32v2 tileCount;
	ImplTileTriangleAllocator tileTriangleAllocator;
	std::vector<ImplTileBin> tileBins;
};

static std::unique_ptr<ImplRasterizerBin> sRasterizerBinImpl;

}

// -----------------------------------------------------------------------------
// -- private functions
// -----------------------------------------------------------------------------

static void bin_tile_triangle(
	ImplRasterizerBin & impl,
	u32 const tileIndex,
	srat::triangle_index_t const triangleIndex,
	srat::triangle_position_t const * const trianglePosition,
	srat::triangle_depth_t const * const triangleDepth,
	srat::triangle_perspective_w_t const * const trianglePerspectiveW
) {
#if defBinUseIndexCaching
	impl.tileBins[tileIndex].push_back(triangleIndex);
#else
	ImplTileBinCache & bin = impl.tileBins[tileIndex];
	if (bin.count == bin.capacity) {
		u32 newCapacity = bin.capacity == 0 ? 8'192u : bin.capacity * 2u;
		let oldPtrs = bin.data;
		bin.data = impl.tileTriangleAllocator.allocate(newCapacity);
		bin.capacity = newCapacity;
		memcpy(
			std::get<0>(bin.data),
			std::get<0>(oldPtrs),
			bin.count * sizeof(srat::triangle_position_t)
		);
		memcpy(
			std::get<1>(bin.data),
			std::get<1>(oldPtrs),
			bin.count * sizeof(srat::triangle_depth_t)
		);
		memcpy(
			std::get<2>(bin.data),
			std::get<2>(oldPtrs),
			bin.count * sizeof(srat::triangle_perspective_w_t)
		);
	}
	std::get<0>(bin.data)[bin.count] = trianglePosition;
	std::get<1>(bin.data)[bin.count] = triangleDepth;
	std::get<2>(bin.data)[bin.count] = trianglePerspectiveW;
	bin.count++;
#endif
}

static void bin_triangle(
	ImplRasterizerBin & impl,
	srat::triangle_position_t const * const triPos,
	srat::triangle_depth_t const * const triDepth,
	srat::triangle_perspective_w_t const * const triPerspW
) {
	let tileBounds = i32bbox2_from_triangle(triPos[0], triPos[1], triPos[2]);
	let dim = u32v2 {
		config::skTileSize * impl.tileCount.x,
		config::skTileSize * impl.tileCount.y,
	};
	if (tileBounds.min.x > i32(config::skTileSize * dim.x)) { return; }
	if (tileBounds.min.y > i32(config::skTileSize * dim.y)) { return; }
	if (tileBounds.max.x < 0) { return; }
	if (tileBounds.max.y < 0) { return; }

	let tileMapping = i32bbox2 {
		.min = tileBounds.min / i32v2(config::skTileSize, config::skTileSize),
		.max = (
			  tileBounds.max / i32v2(config::skTileSize, config::skTileSize)
			+ i32v2(1, 1)
		),
	};

	// -- for index caching, store triangle data so only index is stored in bin
	srat::triangle_index_t triangleIndex = 0;
#if defBinUseIndexCaching
	{
		// store triangle data in the bin's triangle list
		auto [posPtr, depthPtr, perspectiveWPtr] = (
			impl.tileTriangleAllocator.allocate(3)
		);
		SRAT_ASSERT(posPtr != nullptr); // out of memory
		for (size_t it = 0; it < 3; ++it) {
			posPtr[it] = triPos[it];
			depthPtr[it] = triDepth[it];
			perspectiveWPtr[it] = triPerspW[it];
		}
		triangleIndex = posPtr - std::get<0>(impl.tileTriangleAllocator.data_ptr());
	}
#endif
	for (mut tbx = i32{tileMapping.min.x}; tbx < tileMapping.max.x; ++ tbx)
	for (mut tby = i32{tileMapping.min.y}; tby < tileMapping.max.y; ++ tby) {
		let tileIndex = u32 { tby*impl.tileCount.x + tbx };
		bin_tile_triangle(
			impl, tileIndex,
			triangleIndex,
			triPos, triDepth, triPerspW
		);
	}
}

// -----------------------------------------------------------------------------
// -- module api
// -----------------------------------------------------------------------------

namespace srat::modul {

u32v2 rasterizer_bin_tile_count() { return sRasterizerBinImpl->tileCount; }

u32 rasterizer_bin_tile_triangle_count(u32v2 const tileIdx) {
	ImplRasterizerBin & impl = *sRasterizerBinImpl;
	return impl.tileBins[tileIdx.y*impl.tileCount.x + tileIdx.x].size();
}

std::tuple<
	srat::triangle_position_t *,
	srat::triangle_depth_t *,
	srat::triangle_perspective_w_t *
> rasterizer_bin_tile_triangle(u32v2 const tileIdx, size_t const triIdx) {
	ImplRasterizerBin & impl = *sRasterizerBinImpl;
	srat::triangle_index_t const cachedIdx = (
		impl.tileBins[tileIdx.y*impl.tileCount.x + tileIdx.x][triIdx]
	);
	let ptr = impl.tileTriangleAllocator.data_ptr();
	return {
		std::get<0>(ptr) + cachedIdx,
		std::get<1>(ptr) + cachedIdx,
		std::get<2>(ptr) + cachedIdx,
	};
}


} // namespace srat::modul

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::rasterizer_bin_reset(srat::RasterizerBinningConfig const & config) {
	if (sRasterizerBinImpl == nullptr) {
		ImplRasterizerBin bin = {
			.tileCount = {0, 0},
			.tileTriangleAllocator = ImplTileTriangleAllocator::create(
				1'000'000, // TODO temporary constant
				"RasterizerBinTileTriangleAllocator"
			),
			.tileBins = {}
		};
		sRasterizerBinImpl = std::make_unique<ImplRasterizerBin>(std::move(bin));
	}
	ImplRasterizerBin & impl = *sRasterizerBinImpl;
	impl.tileCount = {
		(config.imageWidth + config::skTileSize - 1) / config::skTileSize,
		(config.imageHeight + config::skTileSize - 1) / config::skTileSize
	};
	impl.tileTriangleAllocator.clear();
	impl.tileBins.clear();
	impl.tileBins.resize(impl.tileCount.x * impl.tileCount.y);
}

void srat::rasterizer_bin_triangles(
	srat::RasterizerBinTrianglesParams const & params
) {
	ImplRasterizerBin & impl = *sRasterizerBinImpl;

	for (u32 i = 0; i < params.triangleCount; ++i) {
		bin_triangle(
			impl,
			params.positions + i*3,
			params.depth + i*3,
			params.perspectiveW + i*3
		);
	}
}
