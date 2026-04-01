#include <srat/rasterizer-stage-vertex.hpp>

#include <srat/math.hpp>
#include <srat/command-buffer.hpp>

// -----------------------------------------------------------------------------
// -- private api
// -----------------------------------------------------------------------------

template <typename T>
static T const & attr_fetch(
	srat::VertexAttributeDescriptor const & attr,
	u32 const index
) {
	return *reinterpret_cast<T const *>(attr.data + attr.byteStride * index);
}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::rasterizer_stage_vertex(
	srat::RasterizerStageVertexParams const & params
) {
	SRAT_ASSERT(params.draw.indexCount % 3 == 0);
	let triCount = u32 { params.draw.indexCount / 3u };
	let & va = VertexAttributes { params.draw.vertexAttributes };

	let viewportDim = i32v2 { params.viewport.max - params.viewport.min };

	for (mut triIt = 0u; triIt < triCount; ++triIt) {
		let i0 = u32{params.draw.indices[triIt*3 + 0]};
		let i1 = u32{params.draw.indices[triIt*3 + 1]};
		let i2 = u32{params.draw.indices[triIt*3 + 2]};

		// -- fetch positions
		let p0 = attr_fetch<f32v3>(va.position, i0);
		let p1 = attr_fetch<f32v3>(va.position, i1);
		let p2 = attr_fetch<f32v3>(va.position, i2);

		// -- put positions into homogeneous clip space
		let c0 = f32v4{params.draw.modelViewProjection * f32v4(p0, 1.0f)};
		let c1 = f32v4{params.draw.modelViewProjection * f32v4(p1, 1.0f)};
		let c2 = f32v4{params.draw.modelViewProjection * f32v4(p2, 1.0f)};

		// reject triangle if it's degenerate
		if (c0.w <= skEpsilon) { continue; }

		// TODO reject triangle if it doesn't fit on screen

		// -- perspective divide into normalized device coordinates
		let ndc0 = f32v3 {c0.xyz() / c0.w};
		let ndc1 = f32v3 {c1.xyz() / c1.w};
		let ndc2 = f32v3 {c2.xyz() / c2.w};

		// -- NDC to screen-space
		let ndcToScreen = [&viewportDim](f32v3 const ndc) -> i32v2 {
			return i32v2 {
				i32((ndc.x + 1.0f) * 0.5f * viewportDim.x + 0.5f),
				i32((1.0f - ndc.y) * 0.5f * viewportDim.y + 0.5f),
			};
		};

		// -- store parameters
		let outAttrIdx = params.outAttrsWritten;
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
