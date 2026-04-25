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

bool & srat_temp_opt() {
	static bool opt = false;
	return opt;
}

bool & srat_sequential_writes() {
	static bool sequential = true; // no perf difference
	return sequential;
}

bool & srat_vertex_parallel() {
	static bool parallel = true;
	return parallel;
}

ShaderMode & srat_shader_mode() {
	static ShaderMode mode = ShaderMode::DisplayColor;
	return mode;
}
