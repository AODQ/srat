#pragma once

// configuration for the tiled rasterizer

#define SRAT_DEBUG() SRAT_DEBUG_IMPL

#include <srat/core-types.hpp>

#define Let auto const &
#define Mut auto
#define Ref auto &

#define defBinUseIndexCaching true

namespace config {
	static constexpr bool skBinUseIndexCaching = true;
	static constexpr bool skBinUseSimd = false;
	static constexpr bool skBinUseParallel = false;
	static constexpr bool skBinUseTwoPhase = false;
	static constexpr u32 skTileSize = 64;
	static size_t constexpr skSimdLaneWidth = 8;
}

// -- note: everything below is deprecated! ha!

//NOLINTBEGIN(cppcoreguidelines-macro-usage)

// -----------------------------------------------------------------------------
// -- compile-time configuration
// -----------------------------------------------------------------------------

// if this is false, then each tile bin will allocate its own triangle memory,
// without indexing, so an entire triangle copy (pos, depth, etc).
// in this case the bin allocator will index directly into binAllocatorTriangles
//
// with index caching, the bin allocator will store the triangle copy into
// binAllocatorTriangles as cache, and the bin will index into
// binAllocatorIndices to fetch triangle data
//
// the pay-off is whether it's better performance to make tons of triangle
// copies (large triangles could span multiple tiles), or to have more
// indirection with index caching
#define SRAT_BINNING_USE_INDEX_CACHE() true

// if this is false, binning phase will be a single pass, which might result
// in additional memory allocation (and copying) during binning
//
// if true, then binning phase will use two passes, the first will
// calculate allocation requirements then second will bin.
#define SRAT_BINNING_TWO_PHASES() false

// -----------------------------------------------------------------------------
// -- tunable configurations
// -----------------------------------------------------------------------------

#define SRAT_PERFORMANCE_MODE 1

#if SRAT_PERFORMANCE_MODE

#define SRAT_RUNTIME_CONFIGURABLE() 1
#define SRAT_INFORMATION_PROPAGATION() 0
#define SRAT_TILE_SIZE() 64
#define SRAT_RASTERIZE_PARALLEL() 1

#else // -- debug mode

#define SRAT_RUNTIME_CONFIGURABLE() 1
#define SRAT_DEBUG() 1
#define SRAT_INFORMATION_PROPAGATION() 1
#define SRAT_TILE_SIZE() 64
#define SRAT_RASTERIZE_PARALLEL() 1

#endif

#define SRAT_TRACY_ENABLE() 1

// -----------------------------------------------------------------------------
// -- runtime configuration
// -----------------------------------------------------------------------------

#if SRAT_RUNTIME_CONFIGURABLE()

// TODO, for now these functions live in rasterizer-tiled.cpp

u64 & srat_tile_size();
bool & srat_rasterize_parallel();

bool & srat_binning_simd();

#else

#define srat_tile_size() SRAT_TILE_SIZE()
#define srat_rasterize_parallel() SRAT_RASTERIZE_PARALLEL()
#define srat_binning_simd() false

#endif

// -----------------------------------------------------------------------------
// -- runtime information sharing
// -----------------------------------------------------------------------------
// this is to propagate debug information to the client

#if SRAT_INFORMATION_PROPAGATION()

#include <vector>
std::vector<u64> & srat_debug_triangle_counts();
bool & srat_information_propagation();

#endif

bool & srat_enable_rasterize_binning();
bool & srat_enable_rasterize_rasterization();

// -----------------------------------------------------------------------------
// -- clean exit macro
// -----------------------------------------------------------------------------

#if SRAT_DEBUG()
#define SRAT_CLEAN_EXIT() \
	SRAT_ASSERT(srat::virtual_range_allocator_all_empty());
#else
#define SRAT_CLEAN_EXIT()
#endif // SRAT_DEBUG

// -----------------------------------------------------------------------------
// -- tracy
// -----------------------------------------------------------------------------

#if SRAT_TRACY_ENABLE()
// disable all warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <tracy/Tracy.hpp>
#pragma GCC diagnostic pop
#define TracyZoneScoped ZoneScoped
#define TracyFrameMark FrameMark
#else
#define TracyZoneScoped
#define TracyFrameMark
#endif

//NOLINTEND(cppcoreguidelines-macro-usage)
