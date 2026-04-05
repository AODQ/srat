#pragma once

#include <srat/core-types.hpp>
#include <srat/core-math.hpp>
#include <srat/gfx-image.hpp>

namespace srat
{


// template <typename T>
// T const & attr_fetch(
// 	VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// );

// u8 const * attr_fetch_ptr(
// 	VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// );

// f32v4 attr_fetch_f32v4(
// 	VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// );

// just a placeholder function for now
// void rasterize(
// 	Image const & target,
// 	Image const & depthTarget,
// 	f32m44 const & modelViewProjection,
// 	VertexAttributes const & attribs,
// 	u32 * indices,
// 	u32 vertexCount
// );

}

// template <typename T>
// inline T const & srat::attr_fetch(
// 	srat::VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// ) {
// 	return *(T *)attr_fetch_ptr(desc, vertexIndex);
// }

// inline u8 const * srat::attr_fetch_ptr(
// 	srat::VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// ) {
// 	return (u8 *)desc.data + desc.byteOffset + vertexIndex * desc.byteStride;
// }

// inline f32v4 srat::attr_fetch_f32v4(
// 	srat::VertexAttributeDescriptor const & desc,
// 	u32 vertexIndex
// ) {
// 	return *(f32v4 const *)attr_fetch_ptr(desc, vertexIndex);
// }
