#include <cstdio>

#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-image.hpp>
#include <srat/core-types.hpp>
#include <srat/alloc-virtual-range.hpp>

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include <cstdint>
#include <numbers>


static constexpr i32v2 kWindowDim = { 512, 512 };
static bool animationEnabled = true;

void unit_tests(i32 const argc, char const * const * argv);

void raylib_init()
{
	SetTraceLogLevel(LOG_NONE);
	InitWindow(1024, 720, "srat");
	SetTargetFPS(0);
	rlImGuiSetup(true);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

void raylib_shutdown()
{
	CloseWindow();
}

// generates a command buffer to draw for with a model
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tinyobjloader.h"
#pragma GCC diagnostic pop

struct SratModel {
	std::vector<f32v3> positions;
	std::vector<f32v4> colors;
	std::vector<f32v3> normals;
	std::vector<f32v2> uvcoords;
	struct Mesh {
		std::vector<u32> indices;
		srat::gfx::DrawInfo drawInfo;
	};
	std::vector<Mesh> meshes;

	f32v3 boundsMin { FLT_MAX, FLT_MAX, FLT_MAX };
	f32v3 boundsMax { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	[[nodiscard]] f32v3 center() const { return (boundsMin + boundsMax) * 0.5f; }
	[[nodiscard]] f32v3 size() const { return boundsMax - boundsMin; }
};

// ai slop camera orbit
f32m44 compute_orbit_view(const SratModel& model, f32 time)
{
	f32v3 const center = model.center();
	f32v3 const size   = model.size();
	f32 const maxDim = f32_max(size.x, f32_max(size.y, size.z));
	f32 const radius = maxDim * 1.2f;

	// Camera position orbiting in XZ plane, slightly above center
	f32 const camX = center.x + radius * cosf(time);
	f32 const camZ = center.z + radius * sinf(time);
	f32 const camY = center.y + maxDim * 0.3f;   // look slightly from above

	auto const eye = f32v3{camX, camY, camZ};
	f32v3 const target = center;
	auto const up = f32v3{0.0f, 1.0f, 0.0f};

	return f32m44_lookat(eye, target, up);
}

SratModel loadModel(char const * objPath) {
	SratModel model {};
	tinyobj::ObjReaderConfig readerConfig;
	readerConfig.mtl_search_path = "./"; // Path to material files
	tinyobj::ObjReader reader;

	if (!reader.ParseFromFile(objPath, readerConfig)) {
		if (!reader.Error().empty()) {
			fprintf(stderr, "TinyObjReader: %s\n", reader.Error().c_str());
		}
		exit(1);
	}

	if (!reader.Warning().empty()) {
		fprintf(stderr, "TinyObjReader: %s\n", reader.Warning().c_str());
	}

	auto & attrib = reader.GetAttrib();
	auto & shapes = reader.GetShapes();
	[[maybe_unused]] auto & materials = reader.GetMaterials();

	// -- loop over attributes and store them
	for (size_t v = 0; v < attrib.vertices.size() / 3; ++v) {
		model.positions.emplace_back(
			attrib.vertices[3*v+0],
			attrib.vertices[3*v+1],
			attrib.vertices[3*v+2]
		);
	}
	for (size_t v = 0; v < attrib.normals.size() / 3; ++v) {
		model.normals.emplace_back(
			attrib.normals[3*v+0],
			attrib.normals[3*v+1],
			attrib.normals[3*v+2]
		);
	}
	for (size_t v = 0; v < attrib.texcoords.size() / 2; ++v) {
		model.uvcoords.emplace_back(
			attrib.texcoords[2*v+0],
			attrib.texcoords[2*v+1]
		);
	}
	for (size_t v = 0; v < attrib.colors.size() / 3; ++v) {
		// emplace a random color
		model.colors.emplace_back(
			rand() / (f32)RAND_MAX, // random red
			rand() / (f32)RAND_MAX, // random red
			rand() / (f32)RAND_MAX, // random red
			1.f // alpha
		);
	}


	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	auto const & modelSlice = vec2slice(model.positions);
	auto const & colorSlice = vec2slice(model.colors);
	auto const & normalSlice = vec2slice(model.normals);
	auto const & uvSlice = vec2slice(model.uvcoords);

	// -- loop shapes
	for (const auto & shape : shapes) {

		SratModel::Mesh mesh{};

		for (auto const & index : shape.mesh.indices) {
			mesh.indices.push_back(index.vertex_index);
		}

		mesh.drawInfo = srat::gfx::DrawInfo {
			.modelViewProjection = f32m44_identity(),
			.vertexAttributes = {
				.position = {
					.byteStride = sizeof(f32v3),
					.data = modelSlice,
				},
				.color = {
					.byteStride = sizeof(f32v4),
					.data = colorSlice,
				},
				.normal = {
					.byteStride = sizeof(f32v3),
					.data = normalSlice,
				},
				.uv = {
					.byteStride = sizeof(f32v2),
					.data = uvSlice,
				},
			},
			.indices = vec2slice(mesh.indices).cast<u32 const>(),
			.indexCount = (u32)mesh.indices.size(),
		};

		model.meshes.emplace_back(std::move(mesh));
	}

	// -- calculate bounds
	for (const auto & pos : model.positions) {
		model.boundsMin = f32v3_min(model.boundsMin, pos);
		model.boundsMax = f32v3_max(model.boundsMax, pos);
	}

	return model;
}

// just a placeholder function
void draw_scene(
	srat::gfx::Device const & device,
	SratModel const & model,
	f32 const deltaTime,
	srat::gfx::Image const & target,
	srat::gfx::Image const & depthTarget
)
{
	// -- clear image
	srat::slice<u8> imagePtr = srat::gfx::image_data8(target);
	for (u64 i = 0; i < (u64)kWindowDim.x * (u64)kWindowDim.y; ++i) {
		imagePtr[i*4 + 0] = 0; // r
		imagePtr[i*4 + 1] = 0; // g
		imagePtr[i*4 + 2] = 0; // b
		imagePtr[i*4 + 3] = 255; // a
	}

	// -- clear depth
	Let depthPtr = srat::slice<u16> {srat::gfx::image_data16(depthTarget)};
	for (u64 i = 0; i < (u64)kWindowDim.x * (u64)kWindowDim.y; ++i) {
		depthPtr[i] = UINT16_MAX; // max depth
	}


	// -- build modelviewproj
	// f32 const time = animationEnabled ? fmodf(deltaTime, 1000.f) : 0.f;
	f32m44 const proj = f32m44_perspective(
		90.f * (std::numbers::pi_v<float> / 180.f), /*aspect=*/ 1.0f, 0.1f, 500.0f
	);
	f32v3 const center = model.center();
	f32v3 const size = model.size();
	// rotate around the center of the model, and move back so it's visible
	f32m44 const view = compute_orbit_view(model, deltaTime);
	f32m44 const modelViewProj = proj * view;
	// -- record command buffer
	srat::gfx::CommandBuffer cmdBuf = srat::gfx::command_buffer_create();
	srat::gfx::command_buffer_bind_framebuffer(
		cmdBuf,
		srat::gfx::Viewport {
			.offset = { 0, 0 },
			.dim = { kWindowDim.x, kWindowDim.y },
		},
		target,
		depthTarget
	);

	for (auto & mesh : model.meshes) {
		srat::gfx::DrawInfo const & drawInfo = mesh.drawInfo;
		srat::gfx::command_buffer_draw(cmdBuf, srat::gfx::DrawInfo {
			.modelViewProjection = modelViewProj,
			.vertexAttributes = drawInfo.vertexAttributes,
			.indices = drawInfo.indices,
			.indexCount = drawInfo.indexCount,
		});
	}

	srat::gfx::command_buffer_submit(device, cmdBuf);
	srat::gfx::command_buffer_destroy(cmdBuf);
}

// -----------------------------------------------------------------------------

i32 main(i32 const argc, char const * const * argv)
{
	unit_tests(argc, argv);

	raylib_init();

	Image const img = GenImageColor(kWindowDim.x, kWindowDim.y, BLACK);
	Texture2D tex = LoadTextureFromImage(img);

	srat::gfx::Image const imageColor = (
		srat::gfx::image_create(srat::gfx::ImageCreateInfo {
			.dim = { kWindowDim.x, kWindowDim.y },
			.layout = srat::gfx::ImageLayout::Linear,
			.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
		})
	);

	srat::gfx::Image const sratImageDepth = (
		srat::gfx::image_create(srat::gfx::ImageCreateInfo {
			.dim = { kWindowDim.x, kWindowDim.y },
			.layout = srat::gfx::ImageLayout::Linear,
			.format = srat::gfx::ImageFormat::depth16_unorm,
		})
	);

	srat::gfx::Device const device = srat::gfx::device_create({
		.referenceMode = true,
	});
	SratModel model = loadModel("assets/suzanne.obj");

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);

		// -- here is the srat hookup
		// just draw triangle from 0,0->kWindowDim,0->256,kWindowDim
		draw_scene(device, model, (f32)GetTime(), imageColor, sratImageDepth);

		// lastly copy srat data into raylib texture
		UpdateTexture(tex, srat::gfx::image_data8(imageColor).ptr());

		static srat::array<float, 16> timings {};
		static float timeSinceLastUpdate = 0.f;
		timeSinceLastUpdate += GetFrameTime();
		static float timeSinceLastUpdateTime = 0.f;
		{
			// convert to ms and store in timings for averaging
			for (size_t i = timings.size() - 1; i > 0; --i) {
				timings[i] = timings[i-1];
			}
			timings[0] = GetFrameTime() * 1000.0f;
			float avgTime = 0.f;
			for (float t : timings) {
				avgTime += t;
			}
			avgTime /= timings.size();
			if (timeSinceLastUpdate >= 0.5f) {
				timeSinceLastUpdate = 0.f;
				timeSinceLastUpdateTime = avgTime;
			}
		}

		// draw the tile count on each tile for debugging
// #if SRAT_INFORMATION_PROPAGATION()
// 		if (srat_information_propagation()) {
// 			u32v2 const targetDim = srat::gfx::image_dim(imageColor);
// 			u32 const tileSize = srat_tile_size();
// 			u32v2 const tileCount = {
// 				(u32)(targetDim.x + tileSize - 1) / tileSize,
// 				(u32)(targetDim.y + tileSize - 1) / tileSize,
// 			};
// 			for (u32 y = 0; y < tileCount.y; ++y)
// 			for (u32 x = 0; x < tileCount.x; ++x)
// 			{
// 				u32 const tileX = x * (u32)srat_tile_size();
// 				u32 const tileY = y * (u32)srat_tile_size();
// 				u64 const triCount = srat_debug_triangle_counts()[
// 					(y * tileCount.x) + x
// 				];
// 				char buf4[128];
// 				snprintf(buf4, sizeof(buf4), "%d", (i32)triCount);
// 				DrawText(buf4, tileX + 5, tileY + 5, 10, WHITE);
// 				DrawLine(tileX, tileY, tileX + srat_tile_size(), tileY, WHITE);
// 				DrawLine(tileX, tileY, tileX, tileY + srat_tile_size(), WHITE);
// 				if (x == tileCount.x - 1) {
// 					DrawLine(
// 						tileX + srat_tile_size(), tileY,
// 						tileX + srat_tile_size(), tileY + srat_tile_size(),
// 						WHITE
// 					);
// 				}
// 				if (y == tileCount.y - 1) {
// 					DrawLine(
// 						tileX, tileY + srat_tile_size(),
// 						tileX + srat_tile_size(), tileY + srat_tile_size(),
// 						WHITE
// 					);
// 				}
// 			}
// 		}
// #endif

		// -- draw the UI
		rlImGuiBegin();

		// viewport
		{
			ImGui::Begin("viewport");
			rlImGuiImage(&tex);
			ImGui::End();
		}

		// runtime settings
		{
			ImGui::DockSpaceOverViewport();
			ImGui::Begin("settings");

			ImGui::Text("average frame time: %.2f ms", timeSinceLastUpdateTime);

			ImGui::PlotHistogram(
				"frame time history",
				timings.ptr(),
				(int)timings.size(),
				0,
				nullptr,
				0.f,
				50.0f,
				ImVec2(0, 80)
			);

			// animation
			ImGui::Checkbox("animation", &animationEnabled);

#if SRAT_RUNTIME_CONFIGURABLE()
			// configure parallel
			ImGui::Checkbox("parallel", &srat_rasterize_parallel());
			ImGui::Checkbox("binning simd", &srat_binning_simd());
			// configure tile size, must be at least 16 and a multiple of 8
			static int tileSize = (int)srat_tile_size() / 8;
			if (
				ImGui::SliderInt(
					/*label=*/ "tile size (multiple of 8)",
					/*v=*/ &tileSize,
					/*v_min=*/ 2,
					/*v_max=*/ 64,
					/*format=*/ "%d",
					/*flags=*/ ImGuiSliderFlags_AlwaysClamp
				)
			) {
				srat_tile_size() = tileSize * 8;
			}

			ImGui::Checkbox(
				"enable binning phase",
				&srat_enable_rasterize_binning()
			);
			ImGui::Checkbox(
				"enable rasterization phase",
				&srat_enable_rasterize_rasterization()
			);
#endif
			ImGui::Text("(tile size: %d)", (i32)srat_tile_size());
			ImGui::End();
			rlImGuiEnd();
		}

		EndDrawing();

		TracyFrameMark;
	}

	srat::gfx::device_destroy(device);
	srat::gfx::image_destroy(imageColor);
	srat::gfx::image_destroy(sratImageDepth);
	SRAT_CLEAN_EXIT();

	// raylib destroy
	UnloadImage(img);
	UnloadTexture(tex);
	rlImGuiShutdown();
	raylib_shutdown();
	return 0;
}
