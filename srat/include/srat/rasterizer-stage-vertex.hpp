#pragma once

#include <srat/types.hpp>
#include <srat/math.hpp>

// vertex shader stage

namespace srat { struct DrawInfo; }

#include <srat/config.hpp>

namespace srat {
	struct RasterizerStageVertexParams {
		DrawInfo const & draw;
		i32bbox2 const & viewport;
		// base ptr, outAttrsWritten will offset by this
		triangle_position_t * outPositions;
		triangle_depth_t * outDepth;
		triangle_perspective_w_t * outPerspectiveW;
		u32 & outAttrsWritten;
	};
	void rasterizer_stage_vertex(RasterizerStageVertexParams const & params);
}
