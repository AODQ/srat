#pragma once

// configuration for the tiled rasterizer

#include "srat/types.hpp"
// -----------------------------------------------------------------------------
// -- compile-time configuration
// -----------------------------------------------------------------------------

#define SRAT_RUNTIME_CONFIGURABLE() 1
#define SRAT_DEBUG() 1
#define SRAT_INFORMATION_PROPAGATION() 1
#define SRAT_TILE_SIZE() 64
#define SRAT_RASTERIZE_PARALLEL() 1
#define SRAT_MAX_TRIANGLES_PER_TILE() 64 * 1024 * 1024

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

#endif

// -----------------------------------------------------------------------------
// -- runtime information sharing
// -----------------------------------------------------------------------------
// this is to propagate debug information to the client

#if SRAT_INFORMATION_PROPAGATION()

#include <vector>
std::vector<u64> & srat_debug_triangle_counts();
bool & srat_information_propagation();
bool & srat_information_propagation();

#endif


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
