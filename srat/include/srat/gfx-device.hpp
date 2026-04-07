#pragma once

#include <srat/core-types.hpp>

namespace srat::gfx {

	struct Device { u64 id; };

	struct DeviceCreateInfo {
		bool referenceMode { false };
	};

	Device device_create(DeviceCreateInfo const & createInfo);
	void device_destroy(Device const & device);

} // namespace srat::gfx
