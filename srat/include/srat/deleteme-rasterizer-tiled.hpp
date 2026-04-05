#pragma once

#if 0
#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-image.hpp>
#include <srat/rasterizer.hpp>
#include <srat/rasterizer-tile-grid.hpp>

namespace srat {

	void rasterize_phase_binning(
		u32v2 const & targetDim,
		srat::gfx::DrawInfo const * const drawInfos,
		size_t const drawInfoCount,
		TileGrid & tileGrid
	);

	void rasterize_phase_rasterization(
		TileGrid & tileGrid,
		srat::gfx::Image const & targetColor,
		srat::gfx::Image const & targetDepth
	);
}
#endif
