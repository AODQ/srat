#include <srat/types.hpp>
#include <srat/image.hpp>
// #include <srat/rasterizer.hpp>
#include <srat/rasterizer-tiled.hpp>
#include <srat/virtual-range-allocator.hpp>

#include <raylib.h>
#include <cstdint>

static constexpr i32v2 kWindowDim = { 512, 512 };

void unit_tests(i32 const argc, char const * const * argv);

void raylib_init()
{
	SetTraceLogLevel(LOG_NONE);
	InitWindow(kWindowDim.x, kWindowDim.y, "srat");
	SetTargetFPS(0);
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

	static constexpr f32v4 kCubeVerts[8] = {
		{ -1.f, -1.f, -1.f, 1.f },
		{  1.f, -1.f, -1.f, 1.f },
		{  1.f,  1.f, -1.f, 1.f },
		{ -1.f,  1.f, -1.f, 1.f },
		{ -1.f, -1.f,  1.f, 1.f },
		{  1.f, -1.f,  1.f, 1.f },
		{  1.f,  1.f,  1.f, 1.f },
		{ -1.f,  1.f,  1.f, 1.f },
	};

	static constexpr f32v4 kCubeTris[] = {
		{ 1.f, 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f, 1.f },
		{ 0.f, 1.f, 0.f, 1.f }, { 0.f, 1.f, 0.f, 1.f }, { 0.f, 1.f, 0.f, 1.f },
		{ 0.f, 0.f, 1.f, 1.f }, { 0.f, 0.f, 1.f, 1.f }, { 0.f, 0.f, 1.f, 1.f },
		{ 1.f, 1.f, 0.f, 1.f }, { 1.f, 1.f, 0.f, 1.f }, { 1.f, 1.f, 0.f, 1.f },
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
	f32 const time = fmodf(deltaTime, 10.f);
	for (i32 cube = 0; cube < 1; ++cube) {
	f32m44 const model = (
		  f32m44_rotate_y(time * 0.5f)
		* f32m44_rotate_x(time * 0.25f)
		* f32m44_translate(
			(cube - 2) * 1.5f, 0.f, 0.f
		)
	);
	f32m44 const view = f32m44_translate(0.f, 0.f, -5.0f);
	f32m44 const proj = f32m44_perspective(
		25.f * (3.14159265f / 180.f), /*aspect=*/ 1.0f, 0.1f, 10.0f
	);
	f32m44 const modelViewProj = proj * view * model;

	// -- rasterize 12 triangles
	srat::VertexAttributes const attributes = {
		.position = {
			.byteOffset = 0,
			.byteStride = sizeof(f32v4),
			.data = (u8 *)kCubeVerts,
		},
		.color = {
			.byteOffset = 0,
			.byteStride = sizeof(f32v4),
			.data = (u8 *)kCubeTris,
		},
		.normal = {},
		.uv = {},
	};

	static srat::TileGrid tileGrid = (
		srat::tile_grid_create(srat::TileGridCreateInfo {
			.imageWidth = kWindowDim.x,
			.imageHeight = kWindowDim.y,
			.maxTriangleIndices = 4096,
		})
	);

	srat::rasterize_tiled(
		srat::DrawInfo {
			.targetColor = target,
			.targetDepth = depthTarget,
			.modelViewProjection = modelViewProj,
			.vertexAttributes = attributes,
			.indices = (u32 *)kCubeTriInds,
			.vertexCount = 12*3,
		},
		tileGrid
	);

	// srat::rasterize(
	// 	/*target=*/ target,
	// 	/*depthTarget=*/ depthTarget,
	// 	/*modelViewProjection=*/ modelViewProj,
	// 	/*attribs=*/ attributes,
	// 	/*indices=*/ (u32 *)kCubeTriInds,
	// 	/*vertexCount=*/ 12*3
	// );
	}
}

i32 main(i32 const argc, char const * const * argv)
{
	unit_tests(argc, argv);

	raylib_init();

	Texture2D const tex = (
		LoadTextureFromImage(GenImageColor(kWindowDim.x, kWindowDim.y, BLACK))
	);

	srat::Image const sratImage = (
		srat::image_create(srat::ImageCreateInfo {
			.dim = kWindowDim,
			.layout = srat::Layout::Linear,
			.format = srat::Format::r8g8b8a8_unorm,
		})
	);

	srat::Image const sratImageDepth = (
		srat::image_create(srat::ImageCreateInfo {
			.dim = kWindowDim,
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
		draw_scene(GetTime(), sratImage, sratImageDepth);

		// lastly copy srat data into raylib texture
		UpdateTexture(tex, srat::image_data(sratImage));

		DrawTexture(tex, 0, 0, WHITE);
		DrawFPS(10, 10);
		EndDrawing();
	}

	// srat destroy
	srat::image_destroy(sratImage);
	srat::image_destroy(sratImageDepth);
	srat::virtual_range_allocator_verify_all_empty();

	// raylib destroy
	UnloadTexture(tex);
	raylib_shutdown();
	return 0;
}
