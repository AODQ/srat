#include <srat/gfx-command-buffer.hpp>

#include "internal/gfx-device.hpp"
#include "internal/gfx-material-impl.hpp"

#include <srat/material-fragment-pbr-metallic-roughness.hpp>
#include <srat/core-handle.hpp>
#include <srat/core-math.hpp>
#include <srat/material-fragment-unlit.hpp>
#include <srat/rasterizer-phase-bin.hpp>
#include <srat/rasterizer-phase-rasterize.hpp>
#include <srat/rasterizer-phase-vertex.hpp>

#include <tbb/parallel_for.h>

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
	// the vertex stage will write into these buffers directly, linearizing
	// the data
	static std::vector<triangle_position_t> cachedAttrPos;
	static std::vector<triangle_depth_t> cachedAttrDepth;
	static std::vector<triangle_perspective_w_t> cachedAttrPerspW;
	static std::vector<f32v2> cachedAttrUv;
	// this wil enable parallelization of vertex processing
	static std::vector<size_t> cachedAttrOffsetsPerDrawCommand;

	cachedAttrPos.clear();
	cachedAttrDepth.clear();
	cachedAttrPerspW.clear();
	cachedAttrUv.clear();
	cachedAttrOffsetsPerDrawCommand.clear();

	// -- break up draw commands into smaller batches to allow parallelization
	//    of vertex processing
	static std::vector<srat::gfx::DrawInfo> drawCommandsBatched;
	drawCommandsBatched.clear();

	// this is dumb as fuck below
	srat::gfx::MaterialHandle referenceMaterialHandle;
	for (srat::gfx::DrawInfo const & drawCommand : impl.drawCommands) {
		Let numTriangles = drawCommand.indexCount / 3u;
		if (referenceMaterialHandle.id == 0) {
			referenceMaterialHandle = drawCommand.boundMaterial;
		}
		// split it up into 32 commands if 1024 or more triangles
		if (numTriangles < 1024) {
			drawCommandsBatched.emplace_back(drawCommand);
			continue;
		}
		u32 const batchSize = 32;
		u32 const trianglesPerBatch = (numTriangles + batchSize - 1) / batchSize;
		u32 const indicesPerBatch = trianglesPerBatch * 3u;
		for (u32 batchIt = 0; batchIt < batchSize; ++batchIt) {
			u32 const batchStart = batchIt * indicesPerBatch;
			u32 const batchEnd = (
				u32_min(batchStart + indicesPerBatch, drawCommand.indexCount)
			);
			SRAT_ASSERT(batchStart < batchEnd);
			drawCommandsBatched.emplace_back(srat::gfx::DrawInfo {
				.modelViewProjection = drawCommand.modelViewProjection,
				.vertexAttributes = drawCommand.vertexAttributes,
				.indices = (
					drawCommand.indices.subslice(batchStart, batchEnd - batchStart)
				),
				.indexCount = batchEnd - batchStart,
				.boundMaterial = drawCommand.boundMaterial,
			});
		}
	}

	// -- precalculate cached allocations
	auto numTriangles = 0u;
	for (srat::gfx::DrawInfo const & drawCommand : drawCommandsBatched) {
		SRAT_ASSERT(drawCommand.indexCount % 3 == 0);
		// offset into the beginning of the buffer
		cachedAttrOffsetsPerDrawCommand.emplace_back(numTriangles * 3u);
		// the current capacity of the buffer
		numTriangles += drawCommand.indexCount / 3u;
	}
	cachedAttrPos.resize(numTriangles*3);
	cachedAttrDepth.resize(numTriangles*3);
	cachedAttrPerspW.resize(numTriangles*3);
	cachedAttrUv.resize(numTriangles*3);

	Let cachedAttrPosSlice = srat::slice(cachedAttrPos.data(), numTriangles*3);
	Let cachedAttrDepthSlice = (
		srat::slice(cachedAttrDepth.data(), numTriangles*3)
	);
	Let cachedAttrPerspWSlice = (
		srat::slice(cachedAttrPerspW.data(), numTriangles*3)
	);
	Let cachedAttrUvSlice = (
		srat::slice(cachedAttrUv.data(), numTriangles*3)
	);

	// -- verify every attribute has data (for now)
#if SRAT_DEBUG()
	{
		for (srat::gfx::DrawInfo const & drawCommand : drawCommandsBatched) {
			Let va = srat::gfx::VertexAttributes { drawCommand.vertexAttributes };
			SRAT_ASSERT(va.position.data.size() > 0);
			SRAT_ASSERT(va.uv.data.size() > 0);
		}
	}

	// -- verify every draw command has indices
	{
		for (srat::gfx::DrawInfo const & drawCommand : drawCommandsBatched) {
			SRAT_ASSERT(drawCommand.indices.size() > 0);
		}
	}

	// -- verify every draw command has at least one triangle
	{
		for (srat::gfx::DrawInfo const & drawCommand : drawCommandsBatched) {
			SRAT_ASSERT(drawCommand.indexCount >= 3);
		}
	}


	// -- verify indices are all within bounds of vertex attributes
	for (srat::gfx::DrawInfo const & drawCommand : drawCommandsBatched) {
		Let va = srat::gfx::VertexAttributes { drawCommand.vertexAttributes };
		for (Let i : drawCommand.indices) {
			SRAT_ASSERT(va.position.data.size() > i * va.position.byteStride);
			SRAT_ASSERT(va.uv.data.size() > i * va.uv.byteStride);
		}
	}
#endif

	// -- compute cached triangle through vertex phase
	auto const & phaseVertexApply = (
		[&](tbb::blocked_range<size_t> const & range) {
			for (size_t i = range.begin(); i < range.end(); ++i) {
				srat::gfx::DrawInfo const & drawCommand = drawCommandsBatched[i];
				uint const attrOffset = (
					cachedAttrOffsetsPerDrawCommand[i]
				);
				SRAT_ASSERT(numTriangles*3 >= attrOffset);
				srat::rasterizer_phase_vertex(RasterizerStageVertexParams {
					.draw = drawCommand,
					.viewport = impl.viewport,
					.outPositions = cachedAttrPosSlice,
					.outDepth = cachedAttrDepthSlice,
					.outPerspectiveW = cachedAttrPerspWSlice,
					.outUvs = cachedAttrUvSlice,
					.attrOffset = attrOffset,
				});
			}
		}
	);

	{
		SRAT_PROFILE_SCOPE("vtx");
		if (srat_vertex_parallel()) {
			tbb::parallel_for(
				tbb::blocked_range<size_t>(0, drawCommandsBatched.size()),
				phaseVertexApply
			);
		} else {
			phaseVertexApply(tbb::blocked_range<size_t>(0, drawCommandsBatched.size()));
		}
	}

	// -- bin triangle data into tile grid
	{
		SRAT_PROFILE_SCOPE("bin");
		srat::rasterizer_phase_bin(RasterizerPhaseBinParams {
			.tileGrid = srat::gfx::device_tile_grid(device),
			.trianglePositions = cachedAttrPosSlice,
			.triangleDepths = cachedAttrDepthSlice,
			.trianglePerspectiveW = cachedAttrPerspWSlice,
			.triangleUvs = cachedAttrUvSlice,
		});
	}

	// -- rasterize binned triangles into target framebuffer
	{
		SRAT_PROFILE_SCOPE("rast");
		auto const & mtrl = (
			srat::gfx::impl_material_from_handle(referenceMaterialHandle)
		);
		switch (mtrl.type) {
			case srat::gfx::MaterialType::Unlit: {
				srat::rasterizer_phase_rasterization<
					srat::MaterialFragmentUnlit
				>(RasterizerPhaseRasterizationParams {
					.tileGrid = srat::gfx::device_tile_grid(device),
					.viewport = impl.viewport,
					.targetColor = impl.targetColor,
					.targetDepth = impl.targetDepth,
					.boundMaterial = referenceMaterialHandle,
				});
				break;
			}
			case srat::gfx::MaterialType::PbrMetallicRoughness: {
				srat::rasterizer_phase_rasterization<
					srat::MaterialFragmentPbrMetallicRoughness
				>(RasterizerPhaseRasterizationParams {
					.tileGrid = srat::gfx::device_tile_grid(device),
					.viewport = impl.viewport,
					.targetColor = impl.targetColor,
					.targetDepth = impl.targetDepth,
					.boundMaterial = referenceMaterialHandle,
				});
			}
			default: {
				SRAT_ASSERT(false && "unsupported material type");
				break;
			}
		}
	}
}
