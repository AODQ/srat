#include <srat/tile-grid.hpp>

#include <srat/rasterizer.hpp>
#include <srat/image.hpp>

namespace srat {
	struct DrawInfo {
		Image targetColor;
		Image targetDepth;
		f32m44 modelViewProjection;
		VertexAttributes vertexAttributes;
		u32 * indices;
		u32 vertexCount;
	};


	void rasterize_tiled(
		DrawInfo const & drawInfo,
		TileGrid & tileGrid
	);
}
