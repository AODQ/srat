#include <srat/core-config.hpp>

// -----------------------------------------------------------------------------
// -- runtime configuration
// -----------------------------------------------------------------------------

#if SRAT_RUNTIME_CONFIGURABLE()
u64 & srat_tile_size() {
	static u64 tileSize = SRAT_TILE_SIZE();
	return tileSize;
}

bool & srat_rasterize_parallel() {
	static bool parallel = SRAT_RASTERIZE_PARALLEL();
	return parallel;
}

bool & srat_binning_simd() {
	static bool binningSimd = false;
	return binningSimd;
}
#endif

#if SRAT_INFORMATION_PROPAGATION()
std::vector<u64> & srat_debug_triangle_counts() {
	static std::vector<u64> triCounts;
	return triCounts;
}

bool & srat_information_propagation() {
	static bool infoPropagation = SRAT_INFORMATION_PROPAGATION();
	return infoPropagation;
}
#endif

#if SRAT_RUNTIME_CONFIGURABLE()
bool & srat_enable_rasterize_binning() {
	static bool enable = true;
	return enable;
}

bool & srat_enable_rasterize_rasterization() {
	static bool enable = true;
	return enable;
}
#endif
