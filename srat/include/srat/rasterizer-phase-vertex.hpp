#pragma once

#include <srat/core-types.hpp>
#include <srat/core-math.hpp>

// vertex shader stage

namespace srat::gfx { struct DrawInfo; }
namespace srat::gfx { struct Viewport; }

#include <srat/core-config.hpp>

namespace srat {
	struct RasterizerStageVertexParams {
		srat::gfx::DrawInfo const & draw;
		srat::gfx::Viewport const & viewport;
		// base ptr, outAttrsWritten will offset by this
		srat::slice<triangle_position_t>  outPositions;
		srat::slice<triangle_depth_t> outDepth;
		srat::slice<triangle_perspective_w_t> outPerspectiveW;
		u32 & outAttrsWritten;
	};
	void rasterizer_phase_vertex(RasterizerStageVertexParams const & params);
}
