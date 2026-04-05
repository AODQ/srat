#include <srat/gfx-command-buffer.hpp>

#include <srat/core-handle.hpp>
#include <srat/core-math.hpp>
#include <srat/rasterizer-phase-rasterize.hpp>
#include <srat/rasterizer-phase-bin.hpp>
#include <srat/rasterizer-phase-vertex.hpp>

#include "internal-gfx-device.hpp"

#include <vector>

namespace sgfx = srat::gfx;

namespace {

struct ImplCommandBuffer {
	std::vector<srat::gfx::DrawInfo> drawCommands;
	srat::gfx::Viewport viewport;
	srat::gfx::Image targetColor;
	srat::gfx::Image targetDepth;
};


srat::HandlePool<sgfx::CommandBuffer, ImplCommandBuffer> sCommandBufferPool = (
	srat::HandlePool<sgfx::CommandBuffer, ImplCommandBuffer>::create(
		128, "CommandBufferPool"
	)
);

}

//------------------------------------------------------------------------------
// -- public api
//------------------------------------------------------------------------------

sgfx::CommandBuffer sgfx::command_buffer_create() {
	return sCommandBufferPool.allocate(ImplCommandBuffer {});
}

void sgfx::command_buffer_destroy(CommandBuffer const & cmdBuf) {
	sCommandBufferPool.free(cmdBuf);
}

void sgfx::command_buffer_draw(
	CommandBuffer & cmdBuf,
	DrawInfo const & drawInfo
) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);
	impl.drawCommands.emplace_back(drawInfo);
}

void sgfx::command_buffer_bind_framebuffer(
	sgfx::CommandBuffer const & cmdBuf,
	srat::gfx::Viewport const & viewport,
	srat::gfx::Image const & colorTarget,
	srat::gfx::Image const & depthTarget
) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);
	impl.viewport = viewport;
	impl.targetColor = colorTarget;
	impl.targetDepth = depthTarget;
}

void sgfx::command_buffer_submit(
	sgfx::Device const & device,
	sgfx::CommandBuffer const & cmdBuf
) {
	Let impl = *sCommandBufferPool.get(cmdBuf);

	SRAT_ASSERT(impl.targetColor.id != 0);
	SRAT_ASSERT(impl.targetDepth.id != 0);
	SRAT_ASSERT(!impl.drawCommands.empty());

	// -- prepare device for draw
	srat::gfx::device_prepare_draw(device, impl.viewport);

	// -- allocate cached triangle data for vertex processing
	static std::vector<triangle_position_t> cachedAttrPos;
	static std::vector<triangle_depth_t> cachedAttrDepth;
	static std::vector<triangle_perspective_w_t> cachedAttrPerspW;

	// -- precalculate cached allocations
	Mut numTriangles = 0u;
	for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
		SRAT_ASSERT(drawCommand.indexCount % 3 == 0);
		numTriangles += drawCommand.indexCount / 3u;
	}
	cachedAttrPos.resize(numTriangles*3);
	cachedAttrDepth.resize(numTriangles*3);
	cachedAttrPerspW.resize(numTriangles*3);

	Let cachedAttrPosSlice = srat::slice(cachedAttrPos.data(), numTriangles*3);
	Let cachedAttrDepthSlice = (
		srat::slice(cachedAttrDepth.data(), numTriangles*3)
	);
	Let cachedAttrPerspWSlice = (
		srat::slice(cachedAttrPerspW.data(), numTriangles*3)
	);

	// -- verify every attribute has data (for now)
#if SRAT_DEBUG()
	{
		for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
			Let va = srat::gfx::VertexAttributes { drawCommand.vertexAttributes };
			SRAT_ASSERT(va.position.data.size() > 0);
			SRAT_ASSERT(va.color.data.size() > 0);
		}
	}

	// -- verify every draw command has indices
	{
		for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
			SRAT_ASSERT(drawCommand.indices.size() > 0);
		}
	}

	// -- verify every draw command has at least one triangle
	{
		for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
			SRAT_ASSERT(drawCommand.indexCount >= 3);
		}
	}


	// -- verify indices are all within bounds of vertex attributes
	for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
		Let va = srat::gfx::VertexAttributes { drawCommand.vertexAttributes };
		for (Let i : drawCommand.indices) {
			SRAT_ASSERT(va.position.data.size() > i * va.position.byteStride);
			SRAT_ASSERT(va.color.data.size() > i * va.color.byteStride);
		}
	}
#endif

	// -- compute cached triangle through vertex phase
	{
		Mut numAttrs = 0u;
		for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
			SRAT_ASSERT(numTriangles*3 >= numAttrs);
			srat::rasterizer_phase_vertex(RasterizerStageVertexParams {
				.draw = drawCommand,
				.viewport = impl.viewport,
				.outPositions = cachedAttrPosSlice,
				.outDepth = cachedAttrDepthSlice,
				.outPerspectiveW = cachedAttrPerspWSlice,
				.outAttrsWritten = numAttrs,
			});
			SRAT_ASSERT(numTriangles*3 >= numAttrs);
		}
	}

	// -- bin triangle data into tile grid
	srat::rasterizer_phase_bin(RasterizerPhaseBinParams {
		.tileGrid = srat::gfx::device_tile_grid(device),
		.trianglePositions = cachedAttrPosSlice,
		.triangleDepths = cachedAttrDepthSlice,
		.trianglePerspectiveW = cachedAttrPerspWSlice,
	});

	// -- rasterize binned triangles into target framebuffer
	srat::rasterizer_phase_rasterization(RasterizerPhaseRasterizationParams {
		.tileGrid = srat::gfx::device_tile_grid(device),
		.viewport = impl.viewport,
		.targetColor = impl.targetColor,
		.targetDepth = impl.targetDepth,
	});
}
