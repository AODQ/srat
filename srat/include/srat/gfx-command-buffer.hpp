#pragma once

// draw command buffer

#include <srat/core-array.hpp>
#include <srat/core-math.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-device.hpp>
#include <srat/gfx-image.hpp>
#include <srat/profiler.hpp>

namespace srat::gfx {

	struct VertexAttributeDescriptor {
		u32 byteStride { 0u };
		srat::slice<u8 const> data {};
	};

	struct VertexAttributes {
		VertexAttributeDescriptor position;
		VertexAttributeDescriptor normal;
		VertexAttributeDescriptor uv;
	};

	struct DrawInfo {
		srat::gfx::Image boundTexture;
		f32m44 modelViewProjection {};
		VertexAttributes vertexAttributes {};
		srat::slice<u32 const> indices {};
		u32 indexCount { 0u };
	};

	struct CommandBuffer { u64 id; };

	srat::gfx::CommandBuffer command_buffer_create();
	void command_buffer_destroy(srat::gfx::CommandBuffer const & cmdBuf);

	void command_buffer_draw(
		srat::gfx::CommandBuffer & cmdBuf, srat::gfx::DrawInfo const & drawInfo
	);
	void command_buffer_bind_framebuffer(
		srat::gfx::CommandBuffer const & cmdBuf,
		srat::gfx::Viewport const & viewport,
		srat::gfx::Image const & colorTarget,
		srat::gfx::Image const & depthTarget
	);

	void command_buffer_submit(
		srat::gfx::Device const & device,
		srat::gfx::CommandBuffer const & cmdBuf
	);


} // namespace srat
