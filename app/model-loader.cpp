#include "model-loader.hpp"

#include <srat/core-types.hpp>
#include <srat/core-math.hpp>

#include <raylib.h> // just for image loading

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

static f32m44 cgltf_node_world_matrix(cgltf_node const & node)
{
	f32m44 mat;
	cgltf_node_transform_world(&node, mat.m.data);
	return mat;
}

f32v3 cgltf_accessor_data_as_vec3(
	cgltf_accessor const & accessor,
	cgltf_size const elementIndex
) {
	f32v3 out {};
	cgltf_accessor_read_float(
		&accessor, elementIndex, &out.x, accessor.count
	);
	return out;
}

f32v2 cgltf_accessor_data_as_vec2(
	cgltf_accessor const & accessor,
	cgltf_size const elementIndex
) {
	f32v2 out {};
	cgltf_accessor_read_float(
		&accessor, elementIndex, &out.x, accessor.count
	);
	return out;
}

SratModel load_gltf_model_from_file(char const * const filepath)
{
	cgltf_options options { };
	cgltf_data * data { nullptr };

	cgltf_result result = cgltf_parse_file(&options, filepath, &data);
	if (result != cgltf_result_success) {
		fprintf(stderr, "Failed to parse glTF file: %s [%d]\n", filepath, result);
		exit(1);
	}

	result = cgltf_load_buffers(&options, data, filepath);
	if (result != cgltf_result_success) {
		fprintf(stderr, "Failed to load buffers for glTF file: %s\n", filepath);
		cgltf_free(data);
		exit(1);
	}

	result = cgltf_validate(data);
	if (result != cgltf_result_success) {
		fprintf(stderr, "Failed to validate glTF file: %s\n", filepath);
		cgltf_free(data);
		exit(1);
	}

	printf("---------\n");
	printf("model info:\n");
	printf("  gltf path: %s\n", filepath);
	printf("  material count: %zu\n", data->materials_count);
	printf("  buffer count: %zu\n", data->buffers_count);
	printf("  accessor count: %zu\n", data->accessors_count);
	printf("  node count: %zu\n", data->nodes_count);
	printf("  mesh count: %zu\n", data->meshes_count);
	printf("  texture count: %zu\n", data->textures_count);
	printf("  image count: %zu\n", data->images_count);
	printf("  sampler count: %zu\n", data->samplers_count);
	printf("  animation count: %zu\n", data->animations_count);
	printf("  skin count: %zu\n", data->skins_count);
	printf("  camera count: %zu\n", data->cameras_count);
	printf("  light count: %zu\n", data->lights_count);
	printf("  animation count: %zu\n", data->animations_count);
	printf("  scene count: %zu\n", data->scenes_count);


	SratModel model {};

	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	// ugh temporary patch combining
	auto const combinePath = (
		[&filepath](char const * const relativePath) -> std::string {
			std::string pathStr(filepath);
			size_t lastSlash = pathStr.find_last_of("/\\");
			std::string dir = (
				(lastSlash != std::string::npos)
				? pathStr.substr(0, lastSlash + 1)
				: ""
			);
			return dir + relativePath;
		}
	);

	// -- load up images

	auto const & loadImageFromBuffer =
		[&](cgltf_image const & img) -> Image
	{
		char const * mime = ".png";
		if (strcmp(img.mime_type, "image/png") == 0) {
			mime = ".png";
		} else if (strcmp(img.mime_type, "image/jpeg") == 0) {
			mime = ".jpg";
		}
		printf("loading image from buffer, mime type: %s\n", mime);

		return LoadImageFromMemory(
			mime,
			(
				reinterpret_cast<u8 const *>(img.buffer_view->buffer->data)
				+ img.buffer_view->offset
			),
			(i32)img.buffer_view->size
		);
	};

	auto const & loadImage = [&](cgltf_image const & img) -> srat::gfx::Image {
		Image raylibImage {};
		if (img.uri == nullptr) {
			if (img.buffer_view == nullptr) {
				fprintf(stderr, "Image has no uri or buffer view, skipping\n");
				return srat::gfx::Image {}; // return empty image
			}
			raylibImage = loadImageFromBuffer(img);
		} else {
			std::string const imgPath = combinePath(img.uri);
			printf("loading image: %s\n", imgPath.c_str());
			raylibImage = LoadImage(imgPath.c_str());
		}
		// -- load image using raylib, and convert to srat image
		std::vector<u8> rgbaData(raylibImage.width * raylibImage.height * 4);
		// format all images into linear rgba
		ImageFormat(&raylibImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
		if (raylibImage.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
			// already RGBA, just copy
			memcpy(rgbaData.data(), raylibImage.data, rgbaData.size());
		} else {
			fprintf(
				stderr,
				"Unsupported image format for image: %d, skipping\n",
				raylibImage.format
			);
			return srat::gfx::Image {}; // return empty image
		}
		srat::gfx::Image const sratImage = srat::gfx::image_create(
			srat::gfx::ImageCreateInfo {
				.dim = { raylibImage.width, raylibImage.height },
				.layout = srat::gfx::ImageLayout::Linear,
				.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
				.optInitialData = srat::slice<u8 const>(
					rgbaData.data(),
					rgbaData.size()
				)
			}
		);
		UnloadImage(raylibImage);
		return sratImage;
	};

	// walk every node that has a mesh
	for (cgltf_size nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx) {
		cgltf_node const * const node = &data->nodes[nodeIdx];
		if (node->mesh == nullptr) {
			continue;
		}

		f32m44 const worldTransform = cgltf_node_world_matrix(*node);
		cgltf_mesh const * const gltfMesh = node->mesh;
		for (
			cgltf_size primIdx = 0;
			primIdx < gltfMesh->primitives_count;
			++primIdx
		) {
			cgltf_primitive const & prim = gltfMesh->primitives[primIdx];
			if (prim.type != cgltf_primitive_type_triangles) {
				fprintf(stderr, "Unsupported primitive type, non-tri indexing");
				cgltf_free(data);
				exit(1);
			}
			SratModel::Mesh mesh {};

			// find accessors for position, normal, uv
			cgltf_accessor const * accessorPos = nullptr;
			cgltf_accessor const * accessorNor = nullptr;
			cgltf_accessor const * accessorUv = nullptr;

			for (
				cgltf_size attrIdx = 0;
				attrIdx < prim.attributes_count;
				++attrIdx
			) {
				cgltf_attribute const & attr = prim.attributes[attrIdx];
				switch (attr.type) {
					case cgltf_attribute_type_position:
						accessorPos = attr.data;
					break;
					case cgltf_attribute_type_normal:
						accessorNor = attr.data;
					break;
					case cgltf_attribute_type_texcoord:
						accessorUv = attr.data;
					break;
					default:
					break;
				}

				if (!accessorPos) {
					fprintf(stderr, "Primitive is missing position attribute\n");
					continue;
				}

				cgltf_size const vertexCount = accessorPos->count;
				mesh.positions.resize(vertexCount);
				mesh.normals.resize(vertexCount);
				mesh.uvcoords.resize(vertexCount);

				for (cgltf_size v = 0; v < vertexCount; ++v) {
					// -- store transformed vertex data in mesh
					f32v3 const pos = cgltf_accessor_data_as_vec3(*accessorPos, v);
					f32v3 const nor = (
						accessorNor
						? cgltf_accessor_data_as_vec3(*accessorNor, v)
						: f32v3 { 0.0f, 0.0f, 0.0f }
					);
					f32v2 const uv = (
						accessorUv
						? cgltf_accessor_data_as_vec2(*accessorUv, v)
						: f32v2 { 0.0f, 0.0f }
					);
					mesh.positions[v] = pos;
					mesh.normals[v] = f32v3_normalize(nor);
					mesh.uvcoords[v] = uv;

					// -- update bounds in model-space (right now just world-space)
					f32v4 const worldPos = (
						worldTransform * f32v4 { pos.x, pos.y, pos.z, 1.0f }
					);
					f32v3 const worldPos3 = { worldPos.x, worldPos.y, worldPos.z };
					model.boundsMin = f32v3_min(model.boundsMin, worldPos3);
					model.boundsMax = f32v3_max(model.boundsMax, worldPos3);
				}

				// -- store indices
				if (prim.indices) {
					cgltf_size const indexCount = prim.indices->count;
					mesh.indices.resize(indexCount);
					for (cgltf_size i = 0; i < indexCount; ++i) {
						mesh.indices[i] = (
							(u32)cgltf_accessor_read_index(prim.indices, i)
						);
					}
				}
				// -- non-indexed sequential indices
				else {
					mesh.indices.resize(vertexCount);
					for (cgltf_size i = 0; i < vertexCount; ++i) {
						mesh.indices[i] = (u32)i;
					}
				}

				// -- load up material and texture (if any)
				srat::gfx::MaterialHandle matHandle { 0 };
				if (prim.material) {
					cgltf_material const & mat = *prim.material;
					if (mat.has_pbr_metallic_roughness) {
						srat::gfx::MaterialParameterBlockPbrMetallicRoughness
							matParams {};
						auto & pbrMR = mat.pbr_metallic_roughness;
						auto & baseColorTex = pbrMR.base_color_texture;
						auto & metallicRoughnessTex = (
							pbrMR.metallic_roughness_texture
						);
						matParams.albedoTexture = (
							  baseColorTex.texture
							? loadImage(*baseColorTex.texture->image)
							: srat::gfx::Image { 0 }
						);
						printf("albedo texture\n");

						matParams.metallicRoughnessTexture = (
							  metallicRoughnessTex.texture
							? loadImage(*metallicRoughnessTex.texture->image)
							: srat::gfx::Image { 0 }
						);
						printf("metalic roughness texture %zu\n",
							matParams.metallicRoughnessTexture.id);

						matParams.albedoColor = (
							f32v4x8_splat(
								pbrMR.base_color_factor[0],
								pbrMR.base_color_factor[1],
								pbrMR.base_color_factor[2],
								pbrMR.base_color_factor[3]
							)
						);
						printf("albedo color: (%f, %f, %f, %f)\n",
							pbrMR.base_color_factor[0],
							pbrMR.base_color_factor[1],
							pbrMR.base_color_factor[2],
							pbrMR.base_color_factor[3]
						);

						matParams.metallicFactor = (
							f32x8_splat(pbrMR.metallic_factor)
						);
						printf("metallic factor: %f\n", pbrMR.metallic_factor);

						matParams.roughnessFactor = (
							f32x8_splat(pbrMR.roughness_factor)
						);
						printf("roughness factor: %f\n", pbrMR.roughness_factor);

						
						matParams.normalTexture = (
							  mat.normal_texture.texture
							? loadImage(*mat.normal_texture.texture->image)
							: srat::gfx::Image { 0 }
						);
						printf("normal texture %zu\n", matParams.normalTexture.id);

						matHandle = srat::gfx::material_create(matParams);
					} else if (mat.unlit) {
						srat::gfx::MaterialParameterBlockUnlit matParams {};
						matParams.albedoTexture = srat::gfx::Image { 0 };
						matParams.albedoColor = f32v4x8_splat(1.f, 1.f, 1.f, 1.f);
						matHandle = srat::gfx::material_create(matParams);
					} else {
						fprintf(stderr, "Unsupported material type, skipping\n");
						matHandle = srat::gfx::MaterialHandle { 0 };
					}
				}

				srat::gfx::material_destroy_defer(matHandle);

				// -- build DrawInfo, baking node transform
				mesh.drawInfo = srat::gfx::DrawInfo {
					.modelViewProjection = worldTransform,
					.vertexAttributes = {
						.position = {
							.byteStride = sizeof(f32v3),
							.data = vec2slice(mesh.positions),
						},
						.normal = {
							.byteStride = sizeof(f32v3),
							.data = vec2slice(mesh.normals),
						},
						.uv = {
							.byteStride = sizeof(f32v2),
							.data = vec2slice(mesh.uvcoords),
						},
					},
					.indices = (
						vec2slice(mesh.indices).cast<u32 const>()
					),
					.indexCount = (u32)mesh.indices.size(),
					.boundMaterial = matHandle,
				};

				model.meshes.emplace_back(std::move(mesh));
			}
		}
	}

	// -- finished, cleanup
	cgltf_free(data);

	printf("  total meshes loaded: %zu\n", model.meshes.size());
	printf(
		"  bounds min: (%f, %f, %f)\n",
		model.boundsMin.x, model.boundsMin.y, model.boundsMin.z
	);
	printf(
		"  bounds max: (%f, %f, %f)\n",
		model.boundsMax.x, model.boundsMax.y, model.boundsMax.z
	);
	printf("---------\n");
	return model;
}
