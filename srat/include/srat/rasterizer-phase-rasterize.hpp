#pragma once

#include <srat/core-config.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-material.hpp>

namespace srat { struct TileGrid; }
namespace srat::gfx { struct Image; }
namespace srat::gfx { struct Viewport; }

namespace srat {
	struct RasterizerPhaseRasterizationParams {
		srat::TileGrid const & tileGrid;
		srat::gfx::Viewport const & viewport;
		srat::gfx::Image const & targetColor;
		srat::gfx::Image const & targetDepth;
		srat::gfx::MaterialHandle const & boundMaterial;
	};

	// rasterizes triangle data populated from tile grid into the
	// target framebuffer
	template <typename FusedShaderFragment>
	void rasterizer_phase_rasterization(
		RasterizerPhaseRasterizationParams const & params
	);
}
