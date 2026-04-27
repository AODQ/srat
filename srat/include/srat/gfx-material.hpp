#pragma once

#include <srat/core-config.hpp>
#include <srat/gfx-image.hpp>

// user facing material definitionso

namespace srat::gfx {
	struct MaterialHandle {
		u64 id { 0u };
	};

	enum struct MaterialType : u32 {
		Unlit,
		PbrMetallicRoughness,
	};

	struct MaterialParameterBlockPbrMetallicRoughness {
		srat::gfx::Image albedoTexture {};
		srat::gfx::Image metallicRoughnessTexture {};
		srat::gfx::Image normalTexture {};
		f32v4x8 albedoColor {};
		f32x8 metallicFactor {};
		f32x8 roughnessFactor {};
	};

	struct MaterialParameterBlockUnlit {
		// it's okay to leave this empty, will derive from albedoColor
		srat::gfx::Image albedoTexture {};
		f32v4x8 albedoColor {};
	};

	MaterialHandle material_create(
		MaterialParameterBlockUnlit const & paramBlock
	);
	MaterialHandle material_create(
		MaterialParameterBlockPbrMetallicRoughness const & paramBlock
	);
	void material_destroy(MaterialHandle const & handle);


	// kind of a silly thing here but often you might want a material to just
	// be cleaned up at end of process; this shuold probably be part of some
	// more general purpose resource lifetime management system
	void material_destroy_defer(MaterialHandle const & handle);
	void material_destroy_deferred();
}
