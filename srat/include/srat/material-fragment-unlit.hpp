#pragma once

#include <srat/material-core.hpp>
#include <srat/gfx-material.hpp>

namespace srat {

struct MaterialFragmentUnlit {
	[[nodiscard]] static inline f32v4x8 shade(FragmentInput const & input);
	static constexpr gfx::MaterialType materialType() {
		return gfx::MaterialType::Unlit;
	}
};

} // namespace srat

inline f32v4x8 srat::MaterialFragmentUnlit::shade(
	[[maybe_unused]] FragmentInput const & input
) {
	auto const & paramBlock = (
		*reinterpret_cast<srat::gfx::MaterialParameterBlockUnlit const *>(
			input.material
		)
	);
	if (paramBlock.albedoTexture.id == 0) {
		return paramBlock.albedoColor;
	}
	f32v4x8 const albedo = (
		srat::gfx::image_sample(paramBlock.albedoTexture, input.uv)
	);
	return albedo * paramBlock.albedoColor;
}
