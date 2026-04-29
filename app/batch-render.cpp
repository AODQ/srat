#include "batch-render.hpp"
#include "model-loader.hpp"

#include <srat/core-math.hpp>
#include <srat/core-types.hpp>
#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-device.hpp>
#include <srat/gfx-image.hpp>

#include <raylib.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <numbers>
#include <string>

// -----------------------------------------------------------------------------
// -- output resolution
// -----------------------------------------------------------------------------

static constexpr i32v2 kBatchDim = { 512, 512 };

// -----------------------------------------------------------------------------
// -- model list
// -----------------------------------------------------------------------------

// NOLINTBEGIN(*)
static char const * const kModelNames[] = {
	"Cube",

	// "ABeautifulGame",
	// "AlphaBlendModeTest",
	// "AnimatedColorsCube",
	// "AnimatedCube",
	// "AnimatedMorphCube",
	// "AnimatedTriangle",
	// "AnimationPointerUVs",
	// "AnisotropyBarnLamp",
	// "AnisotropyDiscTest",
	// "AnisotropyRotationTest",
	// "AnisotropyStrengthTest",
	// "AntiqueCamera",
	// "AttenuationTest",
	// "Avocado",
	// "BarramundiFish",
	// "BoomBox",
	// "BoomBoxWithAxes",
	// "Box With Spaces",
	// "Box",
	// "BoxAnimated",
	// "BoxInterleaved",
	// "BoxTextured",
	// "BoxTexturedNonPowerOfTwo",
	// "BoxVertexColors",
	// "BrainStem",
	// "Cameras",
	// "CarConcept",
	// "CarbonFibre",
	// "CesiumMan",
	// "CesiumMilkTruck",
	// "ChairDamaskPurplegold",
	// "ChronographWatch",
	// "ClearCoatCarPaint",
	// "ClearCoatTest",
	// "ClearcoatWicker",
	// "CommercialRefrigerator",
	// "CompareAlphaCoverage",
	// "CompareAmbientOcclusion",
	// "CompareAnisotropy",
	// "CompareBaseColor",
	// "CompareClearcoat",
	// "CompareDispersion",
	// "CompareEmissiveStrength",
	// "CompareIor",
	// "CompareIridescence",
	// "CompareMetallic",
	// "CompareNormal",
	// "CompareRoughness",
	// "CompareSheen",
	// "CompareSpecular",
	// "CompareTransmission",
	// "CompareVolume",
	// "Corset",
	// "Cube",
	// "CubeVisibility",
	// "DamagedHelmet",
	// "DiffuseTransmissionPlant",
	// "DiffuseTransmissionTeacup",
	// "DiffuseTransmissionTest",
	// "DirectionalLight",
	// "DispersionTest",
	// "DragonAttenuation",
	// "DragonDispersion",
	// "Duck",
	// "EmissiveStrengthTest",
	// "EnvironmentTest",
	// "FlightHelmet",
	// "Fox",
	// "GlamVelvetSofa",
	// "GlassBrokenWindow",
	// "GlassHurricaneCandleHolder",
	// "GlassVaseFlowers",
	// "IORTestGrid",
	// "InterpolationTest",
	// "IridescenceAbalone",
	// "IridescenceDielectricSpheres",
	// "IridescenceLamp",
	// "IridescenceMetallicSpheres",
	// "IridescenceSuzanne",
	// "IridescentDishWithOlives",
	// "Lantern",
	// "LightVisibility",
	// "LightsPunctualLamp",
	// "MandarinOrange",
	// "MaterialsVariantsShoe",
	// "MeshPrimitiveModes",
	// "MeshoptCubeTest",
	// "MetalRoughSpheres",
	// "MetalRoughSpheresNoTextures",
	// "MorphPrimitivesTest",
	// "MorphStressTest",
	// "MosquitoInAmber",
	// "MultiUVTest",
	// "MultipleScenes",
	// "NegativeScaleTest",
	// "NodePerformanceTest",
	// "NormalTangentMirrorTest",
	// "NormalTangentTest",
	// "OrientationTest",
	// "PlaysetLightTest",
	// "PointLightIntensityTest",
	// "PotOfCoals",
	// "PotOfCoalsAnimationPointer",
	// "PrimitiveModeNormalsTest",
	// "RecursiveSkeletons",
	// "RiggedFigure",
	// "RiggedSimple",
	// "ScatteringSkull",
	// "SciFiHelmet",
	// "SheenChair",
	// "SheenCloth",
	// "SheenTestGrid",
	// "SheenWoodLeatherSofa",
	// "SimpleInstancing",
	// "SimpleMaterial",
	// "SimpleMeshes",
	// "SimpleMorph",
	// "SimpleSkin",
	// "SimpleSparseAccessor",
	// "SimpleTexture",
	// "SpecGlossVsMetalRough",
	// "SpecularSilkPouf",
	// "SpecularTest",
	// "Sponza",
	// "StainedGlassLamp",
	// "SunglassesKhronos",
	// "Suzanne",
	// "TextureCoordinateTest",
	// "TextureEncodingTest",
	// "TextureLinearInterpolationTest",
	// "TextureSettingsTest",
	// "TextureTransformMultiTest",
	// "TextureTransformTest",
	// "ToyCar",
	// "TransmissionOrderTest",
	// "TransmissionRoughnessTest",
	// "TransmissionTest",
	// "TransmissionThinwallTestGrid",
	// "Triangle",
	// "TriangleWithoutIndices",
	// "TwoSidedPlane",
	// "USDShaderBallForGltf",
	// "Unicode\u2764\u267BTest",
	// "UnlitTest",
	// "VertexColorTest",
	// "VirtualCity",
	// "WaterBottle",
	// "XmpMetadataRoundedCube",
};
// NOLINTEND(*)

static constexpr u32 kModelCount = (
	sizeof(kModelNames) / sizeof(kModelNames[0])
);

// -----------------------------------------------------------------------------
// -- helpers
// -----------------------------------------------------------------------------

// prefer Models/Name/glTF/Name.gltf; fall back to any .gltf in subtree.
// returns empty string if nothing found.
static std::string find_gltf_path(std::string const & modelDir)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	// -- preferred: glTF/ subdirectory only (not glTF-Embedded)
	std::string const preferredDir = modelDir + "/glTF";
	if (fs::is_directory(preferredDir, ec)) {
		for (
			auto const & entry :
			fs::directory_iterator(preferredDir, ec)
		) {
			if (ec) { break; }
			if (entry.path().extension() == ".gltf") {
				return entry.path().string();
			}
		}
	}

	// -- fallback: scan full subtree
	ec.clear();
	for (
		auto const & entry :
		fs::recursive_directory_iterator(modelDir, ec)
	) {
		if (ec) { break; }
		if (entry.path().extension() == ".gltf") {
			return entry.path().string();
		}
	}
	return {};
}

// destroy every per-mesh texture created by the model loader.
// the image pool only has 128 slots, so this must be called after
// each model render to avoid exhausting it.
static void destroy_model_materials(SratModel const & model)
{
	for (auto const & mesh : model.meshes) {
		srat::gfx::material_destroy(mesh.drawInfo.boundMaterial);
	}
}

// simple fixed orbit camera: places eye at 45° azimuth, 25° elevation,
// at 1.5× the model's max bounding dimension, looking at the center.
static f32m44 make_orbit_view(SratModel const & model)
{
	f32v3 const center = model.center();
	f32v3 const eye = center + f32v3(0.f, 0.f, model.boundsMax.z * 1.5f);
	printf("EYE: %f %f %f\n", eye.x, eye.y, eye.z);
	printf("CTR: %f %f %f\n", center.x, center.y, center.z);
	return f32m44_lookat(eye, center, {0.f, 1.f, 0.f});
}

// -----------------------------------------------------------------------------
// -- clear helpers
// -----------------------------------------------------------------------------

static void clear_color(srat::gfx::Image const & img, u8 r, u8 g, u8 b)
{
	srat::slice<u8> data = srat::gfx::image_data8(img);
	u64 const pixelCount = (u64)kBatchDim.x * kBatchDim.y;
	for (u64 i = 0; i < pixelCount; ++i) {
		data[i*4 + 0] = r;
		data[i*4 + 1] = g;
		data[i*4 + 2] = b;
		data[i*4 + 3] = 255;
	}
}

static void clear_depth(srat::gfx::Image const & img)
{
	srat::slice<u16> data = srat::gfx::image_data16(img);
	u64 const pixelCount = (u64)kBatchDim.x * kBatchDim.y;
	for (u64 i = 0; i < pixelCount; ++i) {
		data[i] = UINT16_MAX;
	}
}

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void batch_render_all_models(
	char const * const modelsBasePath,
	char const * const outputDir
) {
	std::filesystem::create_directories(outputDir);

	// -- shared device and render targets (reused across all models)
	srat::gfx::Device const device = (
		srat::gfx::device_create({ .referenceMode = false })
	);

	// NOTE: the image pool (gfx-image.cpp) has 128 slots total.
	// We permanently occupy 2 here. Per-model textures must be
	// destroyed after each render to keep the pool from filling.
	srat::gfx::Image const colorImg = srat::gfx::image_create({
		.dim    = { kBatchDim.x, kBatchDim.y },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::r8g8b8a8_unorm,
	});
	srat::gfx::Image const depthImg = srat::gfx::image_create({
		.dim    = { kBatchDim.x, kBatchDim.y },
		.layout = srat::gfx::ImageLayout::Linear,
		.format = srat::gfx::ImageFormat::depth16_unorm,
	});

	srat::gfx::Viewport const viewport {
		.offset = { 0, 0 },
		.dim    = { kBatchDim.x, kBatchDim.y },
	};

	// wide FOV keeps most models in frame regardless of shape
	f32 const fovRad = (
		60.f * (std::numbers::pi_v<float> / 180.f)
	);
	f32m44 const proj = f32m44_perspective(
		fovRad, /*aspect=*/1.f, 0.001f, 100'000.f
	);

	// -- per-model loop
	for (u32 modelIdx = 0; modelIdx < kModelCount; ++modelIdx) {
		char const * const name = kModelNames[modelIdx];
		std::string const modelDir = (
			std::string(modelsBasePath) + "/" + name
		);
		std::string const gltfPath = find_gltf_path(modelDir);

		if (gltfPath.empty()) {
			printf(
				"[%u/%u] SKIP  %s — no .gltf found\n",
				modelIdx + 1, kModelCount, name
			);
			continue;
		}

		printf(
			"[%u/%u] render %s\n",
			modelIdx + 1, kModelCount, name
		);
		fflush(stdout);

		// load_gltf_model_from_file calls exit() on hard failures,
		// so bad models will abort the batch — that's acceptable
		SratModel const model = (
			load_gltf_model_from_file(gltfPath.c_str())
		);

		if (model.meshes.empty()) {
			printf("       no meshes — skipping\n");
			destroy_model_materials(model);
			continue;
		}

		// -- clear buffers
		clear_color(colorImg, 25, 25, 35); // dark navy background
		clear_depth(depthImg);

		// -- camera
		f32m44 const view = make_orbit_view(model);
		f32m44 const vp   = proj * view;

		// -- record and submit
		srat::gfx::CommandBuffer cmdBuf = (
			srat::gfx::command_buffer_create()
		);
		srat::gfx::command_buffer_bind_framebuffer(
			cmdBuf, viewport, colorImg, depthImg
		);
		for (auto const & mesh : model.meshes) {
			srat::gfx::DrawInfo const & di = mesh.drawInfo;
			srat::gfx::command_buffer_draw(
				cmdBuf,
				srat::gfx::DrawInfo {
					.modelViewProjection = (
						vp * di.modelViewProjection
					),
					.vertexAttributes = di.vertexAttributes,
					.indices = di.indices,
					.indexCount = di.indexCount,
					.boundMaterial = di.boundMaterial
				}
			);
		}
		srat::gfx::command_buffer_submit(device, cmdBuf);
		srat::gfx::command_buffer_destroy(cmdBuf);

		// -- copy CPU buffer → raylib Image → PNG
		Image rlImg = (
			GenImageColor(kBatchDim.x, kBatchDim.y, BLACK)
		);
		std::memcpy(
			rlImg.data,
			srat::gfx::image_data8(colorImg).ptr(),
			(size_t)kBatchDim.x * kBatchDim.y * 4u
		);
		std::string const outPath = (
			std::string(outputDir) + "/" + name + ".png"
		);
		ExportImage(rlImg, outPath.c_str());
		UnloadImage(rlImg);

		printf("       -> %s\n", outPath.c_str());

		// -- free per-model textures before next iteration
		destroy_model_materials(model);
	}

	// -- cleanup shared resources
	srat::gfx::image_destroy(colorImg);
	srat::gfx::image_destroy(depthImg);
	srat::gfx::device_destroy(device);

	printf("\nbatch render complete — %u models attempted\n", kModelCount);
}
