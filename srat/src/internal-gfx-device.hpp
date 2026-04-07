#pragma once

#include <srat/gfx-device.hpp>
#include <srat/gfx-image.hpp>

namespace srat { struct TileGrid; }

namespace srat::gfx {
	srat::TileGrid & device_tile_grid(Device const & device);
	void device_prepare_draw(
		srat::gfx::Device const & device,
		srat::gfx::Viewport const & viewport
	);
	bool device_reference_mode(Device const & device);
}
