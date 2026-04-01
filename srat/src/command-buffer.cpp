#include <srat/command-buffer.hpp>

#include <srat/command-buffer.hpp>
#include <srat/handle.hpp>
#include <srat/math.hpp>
#include <srat/rasterizer-rasterize.hpp>
#include <srat/rasterizer-binning.hpp>
#include <srat/rasterizer-stage-vertex.hpp>

#include <vector>

namespace {

struct ImplCommandBuffer {
	std::vector<srat::DrawInfo> drawCommands;
	srat::Image targetColor;
	srat::Image targetDepth;
};


srat::HandlePool<srat::CommandBuffer, ImplCommandBuffer> sCommandBufferPool = (
	srat::HandlePool<srat::CommandBuffer, ImplCommandBuffer>::create(
		128, "CommandBufferPool"
	)
);

}

//------------------------------------------------------------------------------
// -- public api
//------------------------------------------------------------------------------

srat::CommandBuffer srat::command_buffer_create() {
	return sCommandBufferPool.allocate({});
}

void srat::command_buffer_destroy(CommandBuffer const & cmdBuf) {
	sCommandBufferPool.free(cmdBuf);
}

void srat::command_buffer_draw(CommandBuffer & cmdBuf, DrawInfo const & drawInfo) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);
	impl.drawCommands.emplace_back(drawInfo);
}

void srat::command_buffer_bind_framebuffer(
	srat::CommandBuffer const & cmdBuf,
	srat::Image const & colorTarget,
	srat::Image const & depthTarget
) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);
	impl.targetColor = colorTarget;
	impl.targetDepth = depthTarget;
}

void srat::command_buffer_submit(CommandBuffer const & cmdBuf) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);

	srat::RasterizerBinningConfig const config = {
		.imageWidth = srat::image_dim(impl.targetColor).x,
		.imageHeight = srat::image_dim(impl.targetColor).y,
	};
	let viewport = i32bbox2 {
		.min = i32v2 { 0, 0, },
		.max = i32v2 { (i32)config.imageWidth, (i32)config.imageHeight, },
	};

	// reset rasterizer binning
	srat::rasterizer_bin_reset(config);

	static std::vector<triangle_position_t> cachedAttrPos;
	static std::vector<triangle_depth_t> cachedAttrDepth;
	static std::vector<triangle_perspective_w_t> cachedAttrPerspW;

	// -- precalculate cached allocations
	mut numTriangles = 0u;
	{
		for (srat::DrawInfo const & drawCommand : impl.drawCommands) {
			SRAT_ASSERT(drawCommand.indexCount % 3 == 0);
			numTriangles += drawCommand.indexCount / 3u;
		}
		cachedAttrPos.resize(numTriangles*3);
		cachedAttrDepth.resize(numTriangles*3);
		cachedAttrPerspW.resize(numTriangles*3);
	}

	// -- compute cached triangle data
	{
		mut numAttrs = 0u;
		for (srat::DrawInfo const & drawCommand : impl.drawCommands) {
			SRAT_ASSERT(numTriangles*3 >= numAttrs);
			srat::rasterizer_stage_vertex(RasterizerStageVertexParams {
				.draw = drawCommand,
				.viewport = viewport,
				.outPositions = cachedAttrPos.data(),
				.outDepth = cachedAttrDepth.data(),
				.outPerspectiveW = cachedAttrPerspW.data(),
				.outAttrsWritten = numAttrs,
			});
			SRAT_ASSERT(numTriangles*3 >= numAttrs);
		}
	}

	// -- bin cached triangle data
	printf("binning %zu tris\n", numTriangles);
	srat::rasterizer_bin_triangles({
		.triangleCount = numTriangles,
		.positions = cachedAttrPos.data(),
		.depth = cachedAttrDepth.data(),
		.perspectiveW = cachedAttrPerspW.data(),
	});

	// -- rasterizer
	srat::rasterizer_rasterize(impl.targetColor, impl.targetDepth);
}
