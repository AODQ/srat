#include "gfx-material-impl.hpp"

#include "srat/core-handle.hpp"
#include <srat/core-types.hpp>

namespace {

static
srat::HandlePool<srat::gfx::MaterialHandle, srat::gfx::ImplMaterial>
sImplMaterialPool = (
	srat::HandlePool<srat::gfx::MaterialHandle, srat::gfx::ImplMaterial>::create(
		1'024u,
		"ImplMaterialPool"
	)
);

} // anon

srat::gfx::MaterialHandle srat::gfx::material_create(
	MaterialParameterBlockUnlit const & paramBlock
) {
	srat::gfx::ImplMaterial implMat {
		.type = MaterialType::Unlit,
		.unlit = paramBlock,
	};
	return sImplMaterialPool.allocate(implMat);
}

srat::gfx::MaterialHandle srat::gfx::material_create(
	MaterialParameterBlockPbrMetallicRoughness const & paramBlock
) {
	srat::gfx::ImplMaterial implMat {
		.type = MaterialType::PbrMetallicRoughness,
		.pbrMetallicRoughness = paramBlock,
	};
	return sImplMaterialPool.allocate(implMat);
}

void srat::gfx::material_destroy(MaterialHandle const & material) {
	sImplMaterialPool.free(material);
}

// -----------------------------------------------------------------------------
// silly lfietime hacks

#include <vector>
namespace {
	static std::vector<srat::gfx::MaterialHandle> sDeferredMaterialDestroys;
}

void srat::gfx::material_destroy_defer(MaterialHandle const & handle) {
	sDeferredMaterialDestroys.emplace_back(handle);
}

void srat::gfx::material_destroy_deferred() {
	for (auto const & handle : sDeferredMaterialDestroys) {
		material_destroy(handle);
	}
	sDeferredMaterialDestroys.clear();
}

srat::gfx::ImplMaterial & srat::gfx::impl_material_from_handle(
	MaterialHandle const & handle
) {
	return *sImplMaterialPool.get(handle);
}

void const * srat::gfx::impl_material_parameter_block_from_handle(
	MaterialHandle const & handle
) {
	ImplMaterial const & impl = *sImplMaterialPool.get(handle);
	switch (impl.type) {
		case MaterialType::Unlit: {
			return &impl.unlit;
		}
		case MaterialType::PbrMetallicRoughness: {
			return &impl.pbrMetallicRoughness;
		}
		default: {
			SRAT_ASSERT(false && "unknown material type");
			return nullptr;
		}
	}
}
