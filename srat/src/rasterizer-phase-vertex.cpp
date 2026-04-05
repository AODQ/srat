#include <srat/rasterizer-phase-vertex.hpp>

#include <srat/core-math.hpp>
#include <srat/gfx-command-buffer.hpp>

// -----------------------------------------------------------------------------
// -- private api
// -----------------------------------------------------------------------------

template <typename T>
static T const & attr_fetch(
	srat::gfx::VertexAttributeDescriptor const & attr,
	u32 const index
) {
	// TODO maybe optimize this?
	return (
		attr.data.subslice(attr.byteStride * index, sizeof(T)).cast<T const>()[0]
	);
}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::rasterizer_phase_vertex(
	srat::RasterizerStageVertexParams const & params
) {
	SRAT_ASSERT(params.draw.indexCount % 3 == 0);
	Let triCount = u32 { params.draw.indexCount / 3u };
	Let va = srat::gfx::VertexAttributes { params.draw.vertexAttributes };

	for (Mut triIt = 0u; triIt < triCount; ++triIt) {
		Let i0 = u32{params.draw.indices[triIt*3 + 0]};
		Let i1 = u32{params.draw.indices[triIt*3 + 1]};
		Let i2 = u32{params.draw.indices[triIt*3 + 2]};

		// -- fetch positions
		Let p0 = attr_fetch<f32v3>(va.position, i0);
		Let p1 = attr_fetch<f32v3>(va.position, i1);
		Let p2 = attr_fetch<f32v3>(va.position, i2);

		// -- put positions into homogeneous clip space
		Let c0 = f32v4{params.draw.modelViewProjection * f32v4(p0, 1.0f)};
		Let c1 = f32v4{params.draw.modelViewProjection * f32v4(p1, 1.0f)};
		Let c2 = f32v4{params.draw.modelViewProjection * f32v4(p2, 1.0f)};

		// reject triangle if it's degenerate
		if (c0.w <= skEpsilon) { continue; }

		// TODO reject triangle if it doesn't fit on screen

		// -- perspective divide into normalized device coordinates
		Let ndc0 = f32v3 {c0.xyz() / c0.w};
		Let ndc1 = f32v3 {c1.xyz() / c1.w};
		Let ndc2 = f32v3 {c2.xyz() / c2.w};

		// -- NDC to screen-space
		Let ndcToScreen = [&params](f32v3 const ndc) -> i32v2 {
			Let viewportDim = params.viewport.dim;
			return i32v2 {
				i32(std::round((ndc.x + 1.0f) * 0.5f * (f32)viewportDim.x)),
				i32(std::round((1.0f - ndc.y) * 0.5f * (f32)viewportDim.y)),
			};
		};

		// -- store parameters
		Let outAttrIdx = params.outAttrsWritten;
		params.outPositions[outAttrIdx + 0] = ndcToScreen(ndc0);
		params.outPositions[outAttrIdx + 1] = ndcToScreen(ndc1);
		params.outPositions[outAttrIdx + 2] = ndcToScreen(ndc2);
		params.outDepth[outAttrIdx + 0] = ndc0.z;
		params.outDepth[outAttrIdx + 1] = ndc1.z;
		params.outDepth[outAttrIdx + 2] = ndc2.z;
		params.outPerspectiveW[outAttrIdx + 0] = 1.0f / c0.w;
		params.outPerspectiveW[outAttrIdx + 1] = 1.0f / c1.w;
		params.outPerspectiveW[outAttrIdx + 2] = 1.0f / c2.w;
		params.outAttrsWritten += 3;
	}
}
