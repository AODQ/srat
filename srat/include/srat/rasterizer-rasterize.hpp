#pragma once

#include <srat/types.hpp>
#include <srat/image.hpp>

namespace srat {
	// rasterizes data that's populated in the tile-grid
	void rasterizer_rasterize(
		srat::Image const imageColor,
		srat::Image const imageCepth
	);
}
