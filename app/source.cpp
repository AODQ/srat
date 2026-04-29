#include "gui.hpp"
#include "batch-render.hpp"
#include "perf-suite.hpp"
#include "model-loader.hpp"

#include <srat/alloc-virtual-range.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-material.hpp>
#include <srat/gfx-image.hpp>
#include <srat/profiler.hpp>

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include <cstdio>
#include <cstdint>
#include <numbers>

static constexpr i32v2 kWindowDim = { 1920, 1080 };
static constexpr i32v2 kTargetDim = { 1024, 1024 };
static bool animationEnabled = true;

// -----------------------------------------------------------------------------
// -- app mode selection
// -----------------------------------------------------------------------------
// only one mode runs at a time.  the first N entries mirror PerfSuiteMode,
// the last entry is the normal scene renderer.

static ImageViewerCameraInput sCameraInput {};

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

// -----------------------------------------------------------------------------
// -- camera state (controlled via ImGui)
// -----------------------------------------------------------------------------

f32m44 compute_orbit_view(SratModel const & model)
{
	auto & input = sCameraInput;
	// Camera constants
	constexpr float kBaseOrbitAngle   = 0.0f;  // default orbit angle
	constexpr float kBaseElevation	= 0.0f;  // default elevation (in radians), change to e.g. pi/6 for isometric
	constexpr float kBaseRadiusMult   = 1.2f;  // default zoom
	constexpr float kBaseHeightOffset = 0.3f;
	constexpr float kOrbitSpeed	   = 0.01f;
	constexpr float kElevSpeed		= 0.01f;
	constexpr float kPanSpeed		 = 0.01f;
	constexpr float kZoomSpeed		= 0.05f;

	// Center and size from model
	f32v3 const center = model.center();
	f32v3 const size   = model.size();
	f32 const maxDim   = f32_max(size.x, f32_max(size.y, size.z));

	// ----- Compute camera parameter deltas from input -----
	float orbitAngle   = kBaseOrbitAngle + input.orbitDX * kOrbitSpeed + input.orbitXDX * kOrbitSpeed;  // add more sources as needed
	float elevation	= kBaseElevation  + input.orbitDY * kElevSpeed;
	float radiusMult   = kBaseRadiusMult + input.scroll   * kZoomSpeed;
	float heightOffset = kBaseHeightOffset;
	float panX		 = input.panDX * kPanSpeed;
	float panY		 = input.panDY * kPanSpeed;

	// Handle reset buttons
	if (input.resetForward) {
		orbitAngle   = 0.0f;
		elevation	= 0.0f;
		panX		 = 3.14159265f;
		panY		 = 0.0f;
		radiusMult   = kBaseRadiusMult;
	} else if (input.resetTop) {
		orbitAngle   = 0.0f;
		elevation	= 3.14159265f/2.0f;
		panX		 = 0.0f;
		panY		 = 0.0f;
		radiusMult   = kBaseRadiusMult;
	} else if (input.resetSide) {
		orbitAngle   = 3.14159265f/2.0f;
		elevation	= 0.0f;
		panX		 = 0.0f;
		panY		 = 0.0f;
		radiusMult   = kBaseRadiusMult;
	}
	if (radiusMult < 0.05f) radiusMult = 0.05f;

	float radius = maxDim * radiusMult;

	// Spherical coordinates
	float cx = center.x + radius * cosf(elevation) * cosf(orbitAngle);
	float cy = center.y + radius * sinf(elevation) + maxDim * heightOffset;
	float cz = center.z + radius * cosf(elevation) * sinf(orbitAngle);

	// Pan (right/up in view)
	auto eye0	= f32v3{cx, cy, cz};
	f32v3 forward = f32v3_normalize(center - eye0);
	f32v3 right   = f32v3_normalize(f32v3_cross(forward, f32v3{0.0f, 1.0f, 0.0f}));
	f32v3 upVec   = f32v3_cross(right, forward);

	f32v3 panOffset = right * panX + upVec * panY;

	f32v3 eye	= eye0 + panOffset;
	f32v3 target = center + panOffset;
	auto up	 = f32v3{0.0f, 1.0f, 0.0f};

	return f32m44_lookat(eye, target, up);
}

// just a placeholder function
void draw_scene(
	srat::gfx::Device const & device,
	SratModel const & model,
	srat::gfx::Image const & target,
	srat::gfx::Image const & depthTarget
)
{
	// -- clear image
	srat::slice<u8> imagePtr = srat::gfx::image_data8(target);
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		imagePtr[i*4 + 0] = 255; // r
		imagePtr[i*4 + 1] = 255; // g
		imagePtr[i*4 + 2] = 255; // b
		imagePtr[i*4 + 3] = 255; // a
	}

	// -- clear depth
	Let depthPtr = srat::slice<u16> {srat::gfx::image_data16(depthTarget)};
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		depthPtr[i] = UINT16_MAX; // max depth
	}

	f32 fovDeg = 90.0f;


	// -- build modelviewproj
	f32m44 const proj = (
		f32m44_perspective(
			fovDeg * (std::numbers::pi_v<float> / 180.f),
			/*aspect=*/ 1.0f, 0.1f, 500.0f
		)
	);
	// rotate around the center of the model, and move back so it's visible
	f32m44 const view = compute_orbit_view(model);
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
			.modelViewProjection = modelViewProj * drawInfo.modelViewProjection,
			.vertexAttributes = drawInfo.vertexAttributes,
			.indices = drawInfo.indices,
			.indexCount = drawInfo.indexCount,
			.boundMaterial = drawInfo.boundMaterial
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
	if (argc == 2 && std::strcmp(argv[1], "--batch-render") == 0) {
		batch_render_all_models(
			"assets/glTF-Sample-Assets/Models",
			"image-output"
		);
		return 0;
	}
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

	srat::gfx::Device const device = srat::gfx::device_create({});

	// debug uses a cheap model
	#define MODEL "Cube"
	SratModel model = (
		load_gltf_model_from_file(
			"assets/glTF-Sample-Assets/Models/"
			MODEL "/glTF/" MODEL ".gltf"
		)
	);

	// // -- one-shot performance suite
	// {
	// 	std::vector<srat::gfx::DrawInfo> modelMeshDrawInfos;
	// 	modelMeshDrawInfos.reserve(model.meshes.size());
	// 	for (auto const & mesh : model.meshes) {
	// 		modelMeshDrawInfos.push_back(mesh.drawInfo);
	// 	}
	// 	perf_suite_run_startup(PerfSuiteStartupConfig {
	// 		.device	  = device,
	// 		.targetColor = imageColor,
	// 		.targetDepth = sratImageDepth,
	// 		.targetDim   = kTargetDim,
	// 		.modelMeshes = &modelMeshDrawInfos,
	// 	});
	// }

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);

		// -- here is the srat hookup
		srat::Profiler::frame_begin();
		if (sAppMode == AppMode::RunScene) {
			draw_scene(device, model, imageColor, sratImageDepth);
		} else {
			// perf_suite_run(
			// 	static_cast<PerfSuiteMode>(sAppMode),
			// 	PerfSuiteConfig {
			// 		.device = device,
			// 		.targetColor = imageColor,
			// 		.targetDepth = sratImageDepth,
			// 		.targetDim = kTargetDim,
			// 	}
			// );
		}
		srat::Profiler::frame_end(GetFrameTime() * 1000.0);

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
			UpdateTexture(deviceTexOut, srat::gfx::image_data8(imageColor).ptr());

			guiDisplayImage(deviceTexOut, "render output", &sCameraInput);
		}

		// runtime settings
		{
			ImGui::DockSpaceOverViewport();
			ImGui::Begin("settings");

			// -- mode selection
			{
				i32 current = (i32)sAppMode;
				ImGui::Text("mode");
				if (
					ImGui::Combo(
						"##mode",
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
			ImGui::Text(
				"	(%.1f fps)",
				timeSinceLastUpdateTime > 0.f ? 1000.f/timeSinceLastUpdateTime:0.f
			);

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
			
			// NOLINTEND(*)
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
	srat::gfx::image_destroy(imageColor);
	srat::gfx::image_destroy(sratImageDepth);

	// raylib destroy
	UnloadImage(defaultImgRl);
	UnloadTexture(deviceTexOut);
	rlImGuiShutdown();
	raylib_shutdown();
	return 0;
}
