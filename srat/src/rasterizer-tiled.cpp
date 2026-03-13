#include <srat/rasterizer-tiled.hpp>

void srat::rasterize_tiled(
	srat::DrawInfo const & drawInfo,
	srat::TileGrid & tileGrid
) {
	srat::tile_grid_clear(tileGrid);
	SRAT_ASSERT(drawInfo.vertexCount % 3 == 0);
	SRAT_ASSERT(drawInfo.indices != nullptr);
	SRAT_ASSERT(drawInfo.vertexAttributes.position.data != nullptr);
	SRAT_ASSERT(drawInfo.targetColor.id != 0);
	SRAT_ASSERT(drawInfo.targetDepth.id != 0);
	
	// -- binning pass
	for (u32 vtx = 0; vtx < drawInfo.vertexCount/3; ++vtx) {
		u32 const ind0 = drawInfo.indices[vtx * 3 + 0];
		u32 const ind1 = drawInfo.indices[vtx * 3 + 1];
		u32 const ind2 = drawInfo.indices[vtx * 3 + 2];

		f32v4 const v0 = (
			  drawInfo.modelViewProjection
			* srat::attr_fetch<f32v4>(drawInfo.vertexAttributes.position, ind0)
		);
		f32v4 const v1 = (
			  drawInfo.modelViewProjection
			* srat::attr_fetch<f32v4>(drawInfo.vertexAttributes.position, ind1)
		);
		f32v4 const v2 = (
			  drawInfo.modelViewProjection
			* srat::attr_fetch<f32v4>(drawInfo.vertexAttributes.position, ind2)
		);

		if (v0.w <= 0.f || v1.w <= 0.f || v2.w <= 0.f) {
			// triangle is behind near plane, skip rasterization
			continue;
		}

		i32v2 const s0 = (
			f32v4_clip_to_screen(v0, srat::image_dim(drawInfo.targetColor))
		);
		i32v2 const s1 = (
			f32v4_clip_to_screen(v1, srat::image_dim(drawInfo.targetColor))
		);
		i32v2 const s2 = (
			f32v4_clip_to_screen(v2, srat::image_dim(drawInfo.targetColor))
		);

		// -- put into tile space
		i32bbox2 const bboxTri = i32bbox2_from_triangle(s0, s1, s2);
		srat::tile_grid_bin_triangle_bbox(tileGrid, bboxTri, vtx);
	}
}
