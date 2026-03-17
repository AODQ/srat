#include <srat/tile-grid.hpp>

#include <srat/command-buffer.hpp>

#include <srat/rasterizer.hpp>
#include <srat/image.hpp>

namespace srat {
	void rasterize_phase_binning(
		u32v2 const & targetDim,
		DrawInfo const * const drawInfos,
		size_t const drawInfoCount,
		TileGrid & tileGrid
	);

	void rasterize_phase_rasterization(
		TileGrid & tileGrid,
		srat::Image const & targetColor,
		srat::Image const & targetDepth
	);
}
