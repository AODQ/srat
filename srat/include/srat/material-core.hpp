#pragma once

#include <srat/core-config.hpp>
#include <srat/core-handle.hpp>
#include <srat/core-math.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-image.hpp>

namespace srat {

// input to fragment shader
struct FragmentInput {
	// interpolated vertex outputs
	f32v2x8 uv;
	f32v3x8 normal;

	// material data pointer, e.g. 'MaterialParameterBlockUnlit'
	void const * material;
};

} // namespace srat
