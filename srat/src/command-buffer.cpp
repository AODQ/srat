#include <srat/command-buffer.hpp>

#include <srat/tile-grid.hpp>
#include <srat/rasterizer-tiled.hpp>
#include <srat/handle.hpp>

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

void srat::command_buffer_submit(
	CommandBuffer const & cmdBuf,
	srat::TileGrid & tileGrid
) {
	ImplCommandBuffer & impl = *sCommandBufferPool.get(cmdBuf);

	srat::tile_grid_clear(tileGrid);

	rasterize_phase_binning(
		srat::image_dim(impl.targetColor),
		impl.drawCommands.data(),
		impl.drawCommands.size(),
		tileGrid
	);

	rasterize_phase_rasterization(
		tileGrid,
		impl.targetColor,
		impl.targetDepth
	);
}
