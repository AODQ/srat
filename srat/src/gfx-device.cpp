#include <srat/gfx-device.hpp>

#include "internal-gfx-device.hpp"

#include <srat/core-handle.hpp>
#include <srat/rasterizer-tile-grid.hpp>

// -----------------------------------------------------------------------------
// -- private
// -----------------------------------------------------------------------------

namespace {
	struct ImplDevice {
		srat::TileGrid tileGrid;
	};

	static srat::HandlePool<srat::gfx::Device, ImplDevice> sDevicePool = (
		srat::HandlePool<srat::gfx::Device, ImplDevice>::create(1, "DevicePool")
	);
}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

srat::gfx::Device srat::gfx::device_create() {
	Let impl = ImplDevice { };
	return sDevicePool.allocate(impl);
}

void srat::gfx::device_destroy(Device const & device) {
	return sDevicePool.free(device);
}

void srat::gfx::device_prepare_draw(
	srat::gfx::Device const & device,
	srat::gfx::Viewport const & viewport
) {
	Let impl = sDevicePool.get(device);
	SRAT_ASSERT(impl != nullptr);

	// -- calculate the requested dimensions of the tile grid
	Let tileGrid = srat::TileGrid{impl->tileGrid};
	Let requestedTileCount = u32v2 { srat::viewport_tile_count(viewport) };

	// -- create or clear the tile grid
	Let tileGridValid = bool{srat::tile_grid_valid(tileGrid)};
	if (
		   tileGridValid
		&& srat::tile_grid_tile_count(tileGrid) == requestedTileCount
	) {
		// tile grid is already the correct size, just clear it for the new frame
		srat::tile_grid_clear(tileGrid);
	}
	else {
		// tile grid not correct size or not been created yet
		if (tileGridValid) {
			srat::tile_grid_destroy(tileGrid);
		}
		impl->tileGrid = srat::tile_grid_create(srat::TileGridCreateInfo {
			.imageWidth = viewport.dim.x,
			.imageHeight = viewport.dim.y,
		});
	}
}

srat::TileGrid & srat::gfx::device_tile_grid(Device const & device) {
	Let impl = sDevicePool.get(device);
	SRAT_ASSERT(impl != nullptr);
	SRAT_ASSERT(tile_grid_valid(impl->tileGrid));
	return impl->tileGrid;
}
