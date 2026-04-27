#include <srat/gfx-material.hpp>

#include <variant>

namespace srat::gfx {

	struct ImplMaterial {
		MaterialType type;
		union {
			MaterialParameterBlockUnlit unlit;
			MaterialParameterBlockPbrMetallicRoughness pbrMetallicRoughness;
		};
	};

	ImplMaterial & impl_material_from_handle(MaterialHandle const & handle);

	void const * impl_material_parameter_block_from_handle(
		MaterialHandle const & handle
	);

} // srat::gfx
