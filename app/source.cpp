#include "gui.hpp"
#include "perf-suite.hpp"

#include <srat/alloc-virtual-range.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-image.hpp>
#include <srat/profiler.hpp>

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include <cstdio>
#include <cstdint>
#include <numbers>

static constexpr i32v2 kWindowDim = { 1024, 576 };
static constexpr i32v2 kTargetDim = { 512, 512 };
static bool animationEnabled = true;

// -----------------------------------------------------------------------------
// -- app mode selection
// -----------------------------------------------------------------------------
// only one mode runs at a time.  the first N entries mirror PerfSuiteMode,
// the last entry is the normal scene renderer.

enum struct AppMode : u32 {
	PerfVertex  = (u32)PerfSuiteMode::Vertex,
	PerfBinning = (u32)PerfSuiteMode::Binning,
	PerfRaster  = (u32)PerfSuiteMode::Raster,
	RunScene,
	Count,
};

static constexpr char const * kAppModeLabels[] = {
	"perf vertex",
	"perf binning",
	"perf raster",
	"run scene",
};
static_assert(
	sizeof(kAppModeLabels) / sizeof(kAppModeLabels[0]) == (u32)AppMode::Count
);

static AppMode sAppMode = AppMode::RunScene;

void unit_tests(i32 const argc, char const * const * argv);

void raylib_init()
{
	SetTraceLogLevel(LOG_NONE);
	InitWindow(kWindowDim.x, kWindowDim.y, "srat");
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

// this draws the scene a single time as a unit-test for debugging
void draw_scene_unit_tests(
	srat::gfx::Device const & device,
	SratModel const & model,
	f32 const deltaTime,
	srat::gfx::Image const & target,
	srat::gfx::Image const & depthTarget
)
{
	// -- clear image
	srat::slice<u8> imagePtr = srat::gfx::image_data8(target);
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		imagePtr[i*4 + 0] = 128; // r
		imagePtr[i*4 + 1] = 0; // g
		imagePtr[i*4 + 2] = 0; // b
		imagePtr[i*4 + 3] = 255; // a
	}

	// -- clear depth
	Let depthPtr = srat::slice<u16> {srat::gfx::image_data16(depthTarget)};
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		depthPtr[i] = 0; // max depth
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
			.dim = { kTargetDim.x, kTargetDim.y },
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
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		imagePtr[i*4 + 0] = 0; // r
		imagePtr[i*4 + 1] = 0; // g
		imagePtr[i*4 + 2] = 0; // b
		imagePtr[i*4 + 3] = 255; // a
	}

	// -- clear depth
	Let depthPtr = srat::slice<u16> {srat::gfx::image_data16(depthTarget)};
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		depthPtr[i] = 0; // max depth
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
			.dim = { kTargetDim.x, kTargetDim.y },
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

void depth_image_to_grayscale_texture(
	srat::gfx::Image const & depthImage,
	Image const & outTexture
) {
	Let depthData = srat::gfx::image_data16(depthImage);
	std::vector<u8> grayscaleData(depthData.size() * 4);
	for (size_t i = 0; i < depthData.size(); ++i) {
		// Map depth to [0, 255], where closer is brighter
		u8 const depthValue = (u8)((f32(depthData[i]) / (f32)UINT16_MAX) * 255);
		grayscaleData[i*4 + 0] = depthValue;
		grayscaleData[i*4 + 1] = depthValue;
		grayscaleData[i*4 + 2] = depthValue;
		grayscaleData[i*4 + 3] = 255;
	}
	memcpy(outTexture.data, grayscaleData.data(), grayscaleData.size());
}

// -----------------------------------------------------------------------------

void gui_profiler() {
	// -- profiler panel
	ImGui::Begin("profiler");

	double const frameTotalMs = srat::Profiler::snapshot_frame_total_ms();
	ImGui::Text("frame: %.3f ms (%.1f fps)",
		frameTotalMs,
		frameTotalMs > 0.0 ? 1000.0 / frameTotalMs : 0.0
	);
	ImGui::Separator();

	auto const & sections = srat::Profiler::snapshot();
	if (!sections.empty()) {
		constexpr ImGuiTableFlags kTableFlags = (
			  ImGuiTableFlags_Borders
			| ImGuiTableFlags_RowBg
			| ImGuiTableFlags_SizingStretchProp
		);
		if (ImGui::BeginTable("profiler_table", 4, kTableFlags)) {
			ImGui::TableSetupColumn("Section",  ImGuiTableColumnFlags_WidthStretch, 100.0f);
			ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("% Frame",   ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableSetupColumn("Avg (ms)",  ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableHeadersRow();

			for (auto const & sec : sections) {
				double const pct = (
					frameTotalMs > 0.0
						? (sec.lastMs / frameTotalMs) * 100.0
						: 0.0
				);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(sec.name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", sec.lastMs);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.1f%%", pct);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.3f", sec.avgMs);
			}
			ImGui::EndTable();
		}
	} else {
		ImGui::TextDisabled("(no data yet)");
	}

	ImGui::End();
}

// -----------------------------------------------------------------------------

i32 main(i32 const argc, char const * const * argv)
{
	unit_tests(argc, argv);

	raylib_init();

	Image const defaultImgRl = GenImageColor(kTargetDim.x, kTargetDim.y, BLACK);
	Texture2D const deviceTexOut = LoadTextureFromImage(defaultImgRl);

	srat::gfx::Image const imageColor = (
		srat::gfx::image_create(srat::gfx::ImageCreateInfo {
			.dim = { kTargetDim.x, kTargetDim.y },
			.layout = srat::gfx::ImageLayout::Linear,
			.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
		})
	);

	srat::gfx::Image const sratImageDepth = (
		srat::gfx::image_create(srat::gfx::ImageCreateInfo {
			.dim = { kTargetDim.x, kTargetDim.y },
			.layout = srat::gfx::ImageLayout::Linear,
			.format = srat::gfx::ImageFormat::depth16_unorm,
		})
	);

	srat::gfx::Device const deviceReference = srat::gfx::device_create({
		.referenceMode = true,
	});
	srat::gfx::Device const device = srat::gfx::device_create({
		.referenceMode = false,
	});

	// debug uses a cheap model
#if SRAT_DEBUG()
	SratModel model = loadModel("assets/suzanne.obj");
#else
	// SratModel model = loadModel("assets/blade.obj");
	// SratModel model = loadModel("assets/sphere.obj");
	// SratModel model = loadModel("assets/bunny.obj");
	SratModel model = loadModel("assets/dragon.obj");
#endif

	// -- generate two unit test images with reference and normal device
	auto const imgUnitTestImageReference = (
		GenImageColor(kTargetDim.x, kTargetDim.y, BLACK)
	);
	auto const imgUnitTestImageReferenceDepth = (
		GenImageColor(kTargetDim.x, kTargetDim.y, BLACK)
	);
	auto const imgUnitTestImageDevice = (
		GenImageColor(kTargetDim.x, kTargetDim.y, BLACK)
	);
	auto const imgUnitTestImageDeviceDepth = (
		GenImageColor(kTargetDim.x, kTargetDim.y, BLACK)
	);
	{
		srat::gfx::Image const unitTestImageReference = (
			srat::gfx::image_create(srat::gfx::ImageCreateInfo {
				.dim = { kTargetDim.x, kTargetDim.y },
				.layout = srat::gfx::ImageLayout::Linear,
				.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
			})
		);
		srat::gfx::Image const unitTestImageReferenceDepth = (
			srat::gfx::image_create(srat::gfx::ImageCreateInfo {
				.dim = { kTargetDim.x, kTargetDim.y },
				.layout = srat::gfx::ImageLayout::Linear,
				.format = srat::gfx::ImageFormat::depth16_unorm,
			})
		);
		srat::gfx::Image const unitTestImageDevice = (
			srat::gfx::image_create(srat::gfx::ImageCreateInfo {
				.dim = { kTargetDim.x, kTargetDim.y },
				.layout = srat::gfx::ImageLayout::Linear,
				.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
			})
		);
		srat::gfx::Image const unitTestImageDeviceDepth = (
			srat::gfx::image_create(srat::gfx::ImageCreateInfo {
				.dim = { kTargetDim.x, kTargetDim.y },
				.layout = srat::gfx::ImageLayout::Linear,
				.format = srat::gfx::ImageFormat::depth16_unorm,
			})
		);
		draw_scene_unit_tests(
			deviceReference,
			model,
			12.f,
			unitTestImageReference,
			unitTestImageReferenceDepth
		);
		draw_scene_unit_tests(
			device,
			model,
			12.f,
			unitTestImageDevice,
			unitTestImageDeviceDepth
		);
		memcpy(
			imgUnitTestImageReference.data,
			srat::gfx::image_data8(unitTestImageReference).ptr(),
			(size_t)kTargetDim.x * (size_t)kTargetDim.y * 4
		);
		depth_image_to_grayscale_texture(
			unitTestImageReferenceDepth,
			imgUnitTestImageReferenceDepth
		);
		memcpy(
			imgUnitTestImageDevice.data,
			srat::gfx::image_data8(unitTestImageDevice).ptr(),
			(size_t)kTargetDim.x * (size_t)kTargetDim.y * 4
		);
		depth_image_to_grayscale_texture(
			unitTestImageDeviceDepth,
			imgUnitTestImageDeviceDepth
		);
		srat::gfx::image_destroy(unitTestImageReference);
		srat::gfx::image_destroy(unitTestImageReferenceDepth);
		srat::gfx::image_destroy(unitTestImageDevice);
		srat::gfx::image_destroy(unitTestImageDeviceDepth);
	}

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);

		// -- here is the srat hookup
		srat::Profiler::frame_begin();

		if (sAppMode == AppMode::RunScene) {
			draw_scene(device, model, 0.0f, imageColor, sratImageDepth);
		} else {
			perf_suite_run(
				static_cast<PerfSuiteMode>(sAppMode),
				PerfSuiteConfig {
					.device = device,
					.targetColor = imageColor,
					.targetDepth = sratImageDepth,
					.targetDim = kTargetDim,
				}
			);
		}

		srat::Profiler::frame_end(GetFrameTime() * 1000.0);

		// lastly copy srat data into raylib texture
		UpdateTexture(deviceTexOut, srat::gfx::image_data8(imageColor).ptr());

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

		// -- draw the UI
		rlImGuiBegin();

		// viewport
		{
			ImGui::Begin("viewport");
			rlImGuiImage(&deviceTexOut);
			ImGui::End();
		}

		{
			// convert srat images to raylib images
			guiUnitTestImages(
				imgUnitTestImageReference,
				imgUnitTestImageReferenceDepth,
				imgUnitTestImageDevice,
				imgUnitTestImageDeviceDepth
			);
		}

		// runtime settings
		{
			ImGui::DockSpaceOverViewport();
			ImGui::Begin("settings");

			// -- mode selection
			{
				i32 current = (i32)sAppMode;
				if (
					ImGui::Combo(
						"mode",
						&current,
						kAppModeLabels,
						(i32)AppMode::Count
					)
				) {
					sAppMode = static_cast<AppMode>(current);
				}
			}
			ImGui::Separator();

			ImGui::Text("average frame time: %.2f ms", timeSinceLastUpdateTime);

			ImGui::PlotHistogram(
				"frame time history",
				timings.ptr(),
				(int)timings.size(),
				0,
				nullptr,
				0.f,
				200.0f,
				ImVec2(0, 80)
			);

			// animation
			ImGui::Checkbox("animation", &animationEnabled);

			// configure render opts
			ImGui::Checkbox("sequential writes", &srat_sequential_writes());

			// temporary options for specific optimizations
			ImGui::Checkbox("temp opt", &srat_temp_opt());

			// configure parallel
			ImGui::Checkbox("rasterize parallel", &srat_rasterize_parallel());
			ImGui::Checkbox("vertex parallel", &srat_vertex_parallel());
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
			ImGui::Text("(tile size: %d)", (i32)srat_tile_size());
			ImGui::End();
			gui_profiler();
			rlImGuiEnd();
		}

		EndDrawing();

		TracyFrameMark;
	}

	srat::gfx::device_destroy(device);
	srat::gfx::device_destroy(deviceReference);
	srat::gfx::image_destroy(imageColor);
	srat::gfx::image_destroy(sratImageDepth);
	SRAT_CLEAN_EXIT();

	// raylib destroy
	UnloadImage(defaultImgRl);
	UnloadImage(imgUnitTestImageReference);
	UnloadImage(imgUnitTestImageReferenceDepth);
	UnloadImage(imgUnitTestImageDevice);
	UnloadImage(imgUnitTestImageDeviceDepth);
	UnloadTexture(deviceTexOut);
	rlImGuiShutdown();
	raylib_shutdown();
	return 0;
}
