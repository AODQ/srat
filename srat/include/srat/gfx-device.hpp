#pragma once

#include <srat/core-types.hpp>

namespace srat::gfx {

	struct Device { u64 id; };

	Device device_create();
	void device_destroy(Device const & device);

} // namespace srat::gfx
