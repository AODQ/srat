#include <cstdio>

#include <srat/command-buffer.hpp>
#include <srat/image.hpp>
#include <srat/rasterizer-binning.hpp>
#include <srat/types.hpp>
#include <srat/virtual-range-allocator.hpp>

#include <raylib.h>
#include <rlImGui.h>
#include <imgui.h>

#include <array>
#include <cstdint>

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

// just a placeholder function
void draw_scene(
	f32 const deltaTime,
	srat::Image const & target,
	srat::Image const & depthTarget
)
{
	// -- clear image
	for (u64 i = 0; i < (u64)kWindowDim.x * (u64)kWindowDim.y; ++i) {
		u8 * pixel = srat::image_data(target) + i * 4;
		pixel[0] = 0; // r
		pixel[1] = 0; // g
		pixel[2] = 0; // b
		pixel[3] = 255; // a
	}

	// -- clear depth
	for (u64 i = 0; i < (u64)kWindowDim.x * (u64)kWindowDim.y; ++i) {
		u16 * depth = (u16 *)srat::image_data(depthTarget) + i;
		*depth = UINT16_MAX; // max depth
	}

	static constexpr f32v3 kCubeVerts[8] = {
		{ -1.f, -1.f, -1.f, },
		{  1.f, -1.f, -1.f, },
		{  1.f,  1.f, -1.f, },
		{ -1.f,  1.f, -1.f, },
		{ -1.f, -1.f,  1.f, },
		{  1.f, -1.f,  1.f, },
		{  1.f,  1.f,  1.f, },
		{ -1.f,  1.f,  1.f, },
	};

	static constexpr f32v3 kCubeTris[] = {
		{ 1.f, 0.f, 0.f, }, { 1.f, 0.f, 0.f, }, { 1.f, 0.f, 0.f, },
		{ 0.f, 1.f, 0.f, }, { 0.f, 1.f, 0.f, }, { 0.f, 1.f, 0.f, },
		{ 0.f, 0.f, 1.f, }, { 0.f, 0.f, 1.f, }, { 0.f, 0.f, 1.f, },
		{ 1.f, 1.f, 0.f, }, { 1.f, 1.f, 0.f, }, { 1.f, 1.f, 0.f, },
	};

	static constexpr u32 kCubeTriInds[12*3] = {
		0, 1, 2, 0, 2, 3, // back
		4, 6, 5, 4, 7, 6, // front
		0, 4, 5, 0, 5, 1, // bottom
		3, 2, 6, 3, 6, 7, // top
		1, 5, 6, 1, 6, 2, // right
		0, 3, 7, 0, 7, 4, // left
	};

	// build modelviewproj
	f32 const time = animationEnabled ? fmodf(deltaTime, 1000.f) : 0.f;
	srat::CommandBuffer cmdBuf = srat::command_buffer_create();
	srat::command_buffer_bind_framebuffer(cmdBuf, target, depthTarget);
	static constexpr i32 kGridX = 1;
	static constexpr i32 kGridY = 1;
	static constexpr i32 kGridZ = 1;
	// kGridX * kGridY * kGridZ = 16384

	for (i32 cube = 0; cube < kGridX * kGridY * kGridZ; ++cube) {
		i32 const cx = cube % kGridX;
		i32 const cy = (cube / kGridX) % kGridY;
		i32 const cz = cube / (kGridX * kGridY);
		f32m44 const model = (
		  f32m44_rotate_y(time * 0.5f)
		* f32m44_rotate_x(time * 0.25f)
		* f32m44_translate(
			(cx - kGridX * 0.5f) * 2.5f,
			(cy - kGridY * 0.5f) * 2.5f,
			(cz - kGridZ * 0.5f) * 2.5f
		)
	);
	f32m44 const view = f32m44_translate(0.f, 0.f, -10.0f);
	f32m44 const proj = f32m44_perspective(
		90.f * (3.14159265f / 180.f), /*aspect=*/ 1.0f, 0.1f, 5000.0f
	);
	f32m44 const modelViewProj = proj * view * model;

	// -- rasterize 12 triangles
	srat::VertexAttributes const attributes = {
		.position = {
			.byteStride = sizeof(f32v4),
			.data = (u8 *)kCubeVerts,
		},
		.color = {
			.byteStride = sizeof(f32v4),
			.data = (u8 *)kCubeTris,
		},
		.normal = {},
		.uv = {},
	};

	srat::command_buffer_draw(cmdBuf, srat::DrawInfo {
		.modelViewProjection = modelViewProj,
		.vertexAttributes = attributes,
		.indices = (u32 *)kCubeTriInds,
		.indexCount = 12*3,
	});
	}
	srat::command_buffer_submit(cmdBuf);
	srat::command_buffer_destroy(cmdBuf);
}

// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------

i32 main(i32 const argc, char const * const * argv)
{
	unit_tests(argc, argv);

	raylib_init();

	Texture2D const tex = (
		LoadTextureFromImage(GenImageColor(kWindowDim.x, kWindowDim.y, BLACK))
	);

	srat::Image const imageColor = (
		srat::image_create(srat::ImageCreateInfo {
			.dim = { kWindowDim.x, kWindowDim.y },
			.layout = srat::Layout::Linear,
			.format = srat::Format::r8g8b8a8_unorm,
		})
	);

	srat::Image const sratImageDepth = (
		srat::image_create(srat::ImageCreateInfo {
			.dim = { kWindowDim.x, kWindowDim.y },
			.layout = srat::Layout::Linear,
			.format = srat::Format::depth16_unorm,
		})
	);

	while (!WindowShouldClose())
	{
		BeginDrawing();
		ClearBackground(RAYWHITE);

		// -- here is the srat hookup
		// just draw triangle from 0,0->kWindowDim,0->256,kWindowDim
		draw_scene(GetTime(), imageColor, sratImageDepth);

		// lastly copy srat data into raylib texture
		UpdateTexture(tex, srat::image_data(imageColor));

		static std::array<float, 16> timings {};
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
// 			u32v2 const targetDim = srat::image_dim(imageColor);
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
				timings.data(),
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
					/*min=*/ 2,
					/*max=*/ 64,
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

	srat::image_destroy(imageColor);
	srat::image_destroy(sratImageDepth);
	SRAT_CLEAN_EXIT();

	// raylib destroy
	UnloadTexture(tex);
	raylib_shutdown();
	rlImGuiShutdown();
	CloseWindow();
	return 0;
}
