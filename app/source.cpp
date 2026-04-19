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

static constexpr i32v2 kWindowDim = { 1920, 1080 };
static constexpr i32v2 kTargetDim = { 128, 128 };
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

static AppMode sAppMode = AppMode::PerfRaster;

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
	struct Mesh {
		std::vector<u32> indices;
		std::vector<f32v3> positions;
		std::vector<f32v4> colors;
		std::vector<f32v3> normals;
		std::vector<f32v2> uvcoords;
		srat::gfx::DrawInfo drawInfo;
	};
	std::vector<Mesh> meshes;

	f32v3 boundsMin { FLT_MAX, FLT_MAX, FLT_MAX };
	f32v3 boundsMax { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	[[nodiscard]] f32v3 center() const { return (boundsMin + boundsMax) * 0.5f; }
	[[nodiscard]] f32v3 size() const { return boundsMax - boundsMin; }
};

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

	printf("---------\n");
	printf("model info:\n");
	printf("  obj path: %s\n", objPath);
	printf("  material count: %zu\n", reader.GetMaterials().size());
	printf("  vertices: %zu\n", reader.GetAttrib().vertices.size() / 3);
	printf("  normals: %zu\n", reader.GetAttrib().normals.size() / 3);
	printf("  texcoords: %zu\n", reader.GetAttrib().texcoords.size() / 2);
	printf("  texcoords ws: %zu\n", reader.GetAttrib().texcoord_ws.size() / 2);
	printf("  shapes: %zu\n", reader.GetShapes().size());

	auto & attrib = reader.GetAttrib();
	auto & shapes = reader.GetShapes();
	[[maybe_unused]] auto & materials = reader.GetMaterials();

	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	for (auto const & shape : shapes) {
		SratModel::Mesh mesh{};

		std::map<std::tuple<i32, i32, i32>, u32> seen;

		for (tinyobj::index_t const & idx : shape.mesh.indices) {
			std::tuple<int, i32, i32> const key = {
				idx.vertex_index,
				idx.normal_index,
				idx.texcoord_index,
			};
			auto [it, inserted] = seen.emplace(key, (u32)mesh.positions.size());
			if (inserted) {
				i32 const vi = idx.vertex_index;
				mesh.positions.push_back(f32v3 {
					attrib.vertices[3*vi+0],
					attrib.vertices[3*vi+1],
					attrib.vertices[3*vi+2],
				});
				i32 const ni = idx.normal_index;
				mesh.normals.push_back(
					ni >= 0
					? f32v3 {
						attrib.normals[3*ni+0],
						attrib.normals[3*ni+1],
						attrib.normals[3*ni+2],
					}
					: f32v3 { 0.0f, 0.0f, 0.0f }
				);
				i32 const ti = idx.texcoord_index;
				mesh.uvcoords.push_back(
					ti >= 0
					? f32v2 {
						attrib.texcoords[2*ti+0],
						// OBJ V=0 is bottom; flip to V=0 top
						1.0f - attrib.texcoords[2*ti+1],
					}
					: f32v2 { 0.0f, 0.0f }
				);
			}
			mesh.indices.push_back(it->second);
		}

		srat::slice<u8 const> const posSlice = vec2slice(mesh.positions);
		srat::slice<u8 const> const nrmSlice = vec2slice(mesh.normals);
		srat::slice<u8 const> const uvSlice  = vec2slice(mesh.uvcoords);

		mesh.drawInfo = srat::gfx::DrawInfo {
			.boundTexture = srat::gfx::Image { 0 },
			.modelViewProjection = f32m44_identity(),
			.vertexAttributes = {
				.position = {
					.byteStride = sizeof(f32v3),
					.data = posSlice,
				},
				.normal = {
					.byteStride = sizeof(f32v3),
					.data = nrmSlice,
				},
				.uv = {
					.byteStride = sizeof(f32v2),
					.data = uvSlice,
				},
			},
			.indices = (
				vec2slice(mesh.indices).cast<u32 const>()
			),
			.indexCount = (u32)mesh.indices.size(),
		};
		model.meshes.emplace_back(std::move(mesh));
	}

	// -- calculate bounds
	for (const auto & mesh : model.meshes) {
		for (const auto & pos : mesh.positions) {
			model.boundsMin = f32v3_min(model.boundsMin, pos);
			model.boundsMax = f32v3_max(model.boundsMax, pos);
		}
	}

	return model;
}

// this draws the scene a single time as a unit-test for debugging
void draw_scene_unit_tests(
	srat::gfx::Device const & device,
	SratModel const & model,
	srat::gfx::Image const & mtrlDefTex,
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
		depthPtr[i] = UINT16_MAX; // max depth
	}

	// -- build modelviewproj
	f32m44 const proj = f32m44_perspective(
		sCam.fovDeg * (std::numbers::pi_v<float> / 180.f),
		/*aspect=*/ 1.0f,
		0.1f,
		1'000.0f
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
			.boundTexture = mtrlDefTex,
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
	srat::gfx::Image const & mtrlDefTex,
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
			.boundTexture = mtrlDefTex,
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

	auto const mtrlDefTex = []() -> srat::gfx::Image {
		Image const img = LoadImage("assets/default.png");
		SRAT_ASSERT(img.data);
		SRAT_ASSERT(img.width > 0 && img.height > 0);
		SRAT_ASSERT(
			img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
			|| img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
		);
		std::vector<u8> rgbaData((size_t)img.width * (size_t)img.height * 4);
		if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) {
			// convert RGB to RGBA by adding an opaque alpha channel
			for (int y = 0; y < img.height; ++y) {
				for (int x = 0; x < img.width; ++x) {
					size_t srcIndex = (y * img.width + x) * 3;
					size_t dstIndex = (y * img.width + x) * 4;
					rgbaData[dstIndex + 0] = ((u8 *)img.data)[srcIndex + 0]; // R
					rgbaData[dstIndex + 1] = ((u8 *)img.data)[srcIndex + 1]; // G
					rgbaData[dstIndex + 2] = ((u8 *)img.data)[srcIndex + 2]; // B
					rgbaData[dstIndex + 3] = 255; // A (opaque)
				}
			}
		} else {
			// already RGBA, just copy
			memcpy(rgbaData.data(), img.data, rgbaData.size());
		}
		auto result = srat::gfx::image_create(srat::gfx::ImageCreateInfo {
			.dim = { img.width, img.height },
			.layout = srat::gfx::ImageLayout::Linear,
			.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
			.optInitialData = srat::slice<u8 const>(
				rgbaData.data(),
				rgbaData.size()
			)
		});
		UnloadImage(img);
		return result;
	}();

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
	SratModel model = loadModel("assets/cube.obj");
#else
	// SratModel model = loadModel("assets/blade.obj");
	// SratModel model = loadModel("assets/sphere.obj");
	// SratModel model = loadModel("assets/bunny.obj");
	SratModel model = loadModel("assets/cube.obj");
	// SratModel model = loadModel("assets/teapot.obj");
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
			mtrlDefTex,
			unitTestImageReference,
			unitTestImageReferenceDepth
		);
		draw_scene_unit_tests(
			device,
			model,
			mtrlDefTex,
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

	// -- one-shot performance suite
	{
		std::vector<srat::gfx::DrawInfo> modelMeshDrawInfos;
		modelMeshDrawInfos.reserve(model.meshes.size());
		for (auto const & mesh : model.meshes) {
			modelMeshDrawInfos.push_back(mesh.drawInfo);
		}
		perf_suite_run_startup(PerfSuiteStartupConfig {
			.device      = device,
			.targetColor = imageColor,
			.targetDepth = sratImageDepth,
			.targetDim   = kTargetDim,
			.modelMeshes = &modelMeshDrawInfos,
		});
	}

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
			draw_scene(
				device, model, mtrlDefTex, imageColor, sratImageDepth
			);
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
			ImGui::Image(
			 	ImTextureID(deviceTexOut.id),
				ImVec2(float(deviceTexOut.width)*2, float(deviceTexOut.height)*2)
			);
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
			
			// shader mode
			// NOLINTBEGIN(*)
			{
				const char * shaderModeLabels[] = {
					"Display UV",
					"Display Depth",
					"Display Color",
				};
				int shaderModeInt = (int)srat_shader_mode();
				if (
					ImGui::Combo(
						"shader mode",
						&shaderModeInt,
						shaderModeLabels,
						IM_ARRAYSIZE(shaderModeLabels)
					)
				) {
					srat_shader_mode() = (ShaderMode)shaderModeInt;
				}
			}
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
				ImGui::SliderFloat("Radius Mult",   &sCam.radiusMult,   0.5f, 10.0f, "%.2f");
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
	srat::gfx::device_destroy(deviceReference);
	srat::gfx::image_destroy(imageColor);
	srat::gfx::image_destroy(sratImageDepth);

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
