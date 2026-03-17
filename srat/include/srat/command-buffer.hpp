#pragma once

// draw command buffer

#include "math.hpp"
#include "tile-grid.hpp"
#include "types.hpp"
#include "image.hpp"

namespace srat {

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

	struct DrawInfo {
		f32m44 modelViewProjection;
		VertexAttributes vertexAttributes;
		u32 * indices;
		u32 indexCount;
	};

	struct CommandBuffer { u64 id; };

	CommandBuffer command_buffer_create();
	void command_buffer_destroy(CommandBuffer const & cmdBuf);

	void command_buffer_draw(CommandBuffer & cmdBuf, DrawInfo const & drawInfo);
	void command_buffer_bind_framebuffer(
		srat::CommandBuffer const & cmdBuf,
		srat::Image const & colorTarget,
		srat::Image const & depthTarget
	);

	void command_buffer_submit(
		CommandBuffer const & cmdBuf,
		srat::TileGrid & tileGrid
	);


} // namespace srat
