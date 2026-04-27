#pragma once

#include <srat/material-core.hpp>
#include <srat/gfx-material.hpp>

namespace srat {

struct MaterialFragmentPbrMetallicRoughness {
	[[nodiscard]] static inline f32v4x8 shade(FragmentInput const & input);
	static constexpr gfx::MaterialType materialType() {
		return gfx::MaterialType::PbrMetallicRoughness;
	}
};

} // namespace srat

inline f32v4x8 srat::MaterialFragmentPbrMetallicRoughness::shade(
	[[maybe_unused]] FragmentInput const & input
) {
	auto const & paramBlock = (
		*reinterpret_cast<
			srat::gfx::MaterialParameterBlockPbrMetallicRoughness  const *
		>(
			input.material
		)
	);

	f32v4x8 metallicRoughness = [&]() {
		if (paramBlock.metallicRoughnessTexture.id == 0) {
			return (
				f32v4x8(
					paramBlock.metallicFactor,
					paramBlock.roughnessFactor,
					f32x8_zero(),
					f32x8_zero()
				)
			);
		}
		return srat::gfx::image_sample(
			paramBlock.metallicRoughnessTexture,
			input.uv
		);
	}();

	[[maybe_unused]] f32v4x8 const albedo = [&]() {
		if (paramBlock.albedoTexture.id == 0) {
			return paramBlock.albedoColor;
		}
		return (
			  srat::gfx::image_sample(paramBlock.albedoTexture, input.uv)
			* paramBlock.albedoColor
		);
	}();


	[[maybe_unused]] f32x8 const metallic = [&]() {
		if (paramBlock.metallicRoughnessTexture.id == 0) {
			return paramBlock.metallicFactor;
		}
		return metallicRoughness.x;
	}();

	[[maybe_unused]] f32x8 const roughness = [&]() {
		if (paramBlock.metallicRoughnessTexture.id == 0) {
			return paramBlock.roughnessFactor;
		}
		return metallicRoughness.y;
	}();

	f32v3x8 const normal = [&]() {
		if (paramBlock.normalTexture.id == 0) {
			return f32v3x8_zero();
		}
		f32v4x8 const norRgba = (
			srat::gfx::image_sample(paramBlock.normalTexture, input.uv)
		);
	 	return norRgba.xyz() * f32v3x8_splat(2.0f,2.0f,2.0f) - f32v3x8_splat(1.0f,1.0f,1.0f);
	}();

	// apply basic pbr shading for now, using roughnses metalic and normal
	f32v3x8 const lightDir = (
		f32v3x8_splat(f32v3_normalize(f32v3(0.5f, 1.0f, 0.3f)))
	);
	f32v3x8 const lightColor = f32v3x8_splat(1.0f, 1.0f, 1.0f);

	f32x8 nDotL = f32v3x8_dot(normal, f32v3x8(lightDir));

	f32v4x8 const diffuse = (albedo * nDotL);
	f32v3x8 const specular = lightColor * nDotL * metallic;
	return diffuse + f32v4x8(specular, f32x8_zero());
}
