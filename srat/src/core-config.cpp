#include <srat/core-config.hpp>

// -----------------------------------------------------------------------------
// -- runtime configuration
// -----------------------------------------------------------------------------

u64 & srat_tile_size() {
	static u64 tileSize = config::skTileSize;
	return tileSize;
}

bool & srat_rasterize_parallel() {
	static bool parallel = true;
	return parallel;
}

bool & srat_vertex_parallel() {
	static bool parallel = true;
	return parallel;
}
