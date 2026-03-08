#pragma once

#include <srat/types.hpp>
#include <srat/math.hpp>
#include <srat/image.hpp>

namespace srat
{

struct VertexAttributeDescriptor {
	u32 byteOffset { 0 };
	u32 byteStride { 0 };
	void const * data { nullptr };
};

struct VertexAttributes {
	VertexAttributeDescriptor position;
	VertexAttributeDescriptor color;
	VertexAttributeDescriptor normal;
	VertexAttributeDescriptor uv;
};

// just a placeholder function for now
void rasterize(
	Image const & target,
	Image const & depthTarget,
	f32m44 const & modelViewProjection,
	VertexAttributes const & attribs,
	u32 * indices,
	u32 vertexCount
);

}
