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

struct CameraState {
	f32 orbitAngle   = 0.0f;   // current orbit angle (radians)
	f32 radiusMult   = 1.2f;   // multiplier on maxDim for orbit radius
	f32 heightOffset = 0.3f;   // fraction of maxDim added to center.y
	f32 panX         = 0.0f;   // world-space lateral pan
	f32 panY         = 0.0f;   // world-space vertical pan
	f32 orbitSpeed   = 1.0f;   // animation speed multiplier
	f32 fovDeg       = 90.0f;  // vertical field of view in degrees
};

static CameraState sCam;

// ai slop camera orbit
f32m44 compute_orbit_view(SratModel const & model, CameraState const & cam)
{
	f32v3 const center = model.center();
	f32v3 const size   = model.size();
	f32 const maxDim   = f32_max(size.x, f32_max(size.y, size.z));
	f32 const radius   = maxDim * cam.radiusMult;

	// Camera position orbiting in XZ plane
	f32 const camX = center.x + radius * cosf(cam.orbitAngle);
	f32 const camZ = center.z + radius * sinf(cam.orbitAngle);
	f32 const camY = center.y + maxDim * cam.heightOffset;

	// Apply pan: move eye and target together along camera-right and up
	f32v3 const rawEye    = f32v3{camX, camY, camZ};
	f32v3 const forward   = f32v3_normalize(center - rawEye);
	f32v3 const right     = f32v3_normalize(f32v3_cross(forward, f32v3{0.0f, 1.0f, 0.0f}));
	f32v3 const upVec     = f32v3_cross(right, forward);
	f32v3 const panOffset = right * cam.panX + upVec * cam.panY;

	auto const eye     = rawEye + panOffset;
	f32v3 const target = center + panOffset;
	auto const up      = f32v3{0.0f, 1.0f, 0.0f};

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
		imagePtr[i*4 + 0] = 0; // r
		imagePtr[i*4 + 1] = 0; // g
		imagePtr[i*4 + 2] = 0; // b
		imagePtr[i*4 + 3] = 255; // a
	}

	// -- clear depth
	Let depthPtr = srat::slice<u16> {srat::gfx::image_data16(depthTarget)};
	for (u64 i = 0; i < (u64)kTargetDim.x * (u64)kTargetDim.y; ++i) {
		depthPtr[i] = UINT16_MAX; // max depth
	}

	// -- build modelviewproj
	f32m44 const proj = (
		f32m44_perspective(
			sCam.fovDeg * (std::numbers::pi_v<float> / 180.f),
			/*aspect=*/ 1.0f, 0.1f, 500.0f
		)
	);
	// rotate around the center of the model, and move back so it's visible
	f32m44 const view = compute_orbit_view(model, sCam);
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
	SratModel model = (
		load_gltf_model_from_file(
			// "assets/third-party/doom3player.glb"
			"assets/glTF-Sample-Assets/Models/SimpleTexture/glTF/SimpleTexture.gltf"
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
	// 		.device      = device,
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

		// -- advance orbit angle
		if (animationEnabled) {
			sCam.orbitAngle += GetFrameTime() * sCam.orbitSpeed;
		}

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
			// UpdateTexture(deviceTexOut, srat::gfx::image_data8(imageColor).ptr());

			// temporary hack just reverse X axis of the image
			Let imgData = srat::gfx::image_data8(imageColor);
			std::vector<u8> flippedData(imgData.size());
			for (i32 y = 0; y < kTargetDim.y; ++y) {
				for (i32 x = 0; x < kTargetDim.x; ++x) {
					i32 srcIndex = (y * kTargetDim.x + x) * 4;
					i32 dstIndex = (y * kTargetDim.x + (kTargetDim.x - 1 - x)) * 4;
					std::memcpy(&flippedData[dstIndex], &imgData[srcIndex], 4);
				}
			}
			UpdateTexture(deviceTexOut, flippedData.data());

			guiDisplayImage(deviceTexOut, "render output");
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
				"    (%.1f fps)",
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

			// -- camera controls
			ImGui::Separator();
			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
				// Manual orbit angle scrub (most useful when animation is paused)
				ImGui::SliderFloat(
					"Orbit Angle", &sCam.orbitAngle,
					0.0f, 2.0f * std::numbers::pi_v<float>, "%.2f rad"
				);
				ImGui::SliderFloat("Orbit Speed",   &sCam.orbitSpeed,   0.0f, 5.0f,  "%.2f");
				// reverse 1.0 scale to 10.0 and 10.0 to 1.0
				static f32 radiusInverse = sCam.radiusMult;
				ImGui::SliderFloat(
					"Radius Mult",
					&radiusInverse,
					0.1f, 2.0f,
					"%.2f"
				);
				sCam.radiusMult = 2.0f - radiusInverse;
				// static multInverse = 1.0f / sCam.radiusMult;
				// // ImGui::SliderFloat("Radius Mult",   &sCam.radiusMult,   0.5f, 10.0f, "%.2f");
				ImGui::SliderFloat("Height Offset", &sCam.heightOffset, -2.0f, 2.0f, "%.2f");
				ImGui::DragFloat("Pan X", &sCam.panX, 0.01f, -50.0f, 50.0f, "%.2f");
				ImGui::DragFloat("Pan Y", &sCam.panY, 0.01f, -50.0f, 50.0f, "%.2f");
				ImGui::SliderFloat("FoV (deg)", &sCam.fovDeg, 10.0f, 170.0f, "%.1f");
				if (ImGui::Button("Reset Camera")) {
					sCam = CameraState{};
				}
			}

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
