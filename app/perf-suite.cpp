#include "perf-suite.hpp"

#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-image.hpp>
#include <srat/profiler.hpp>
#include <srat/rasterizer-phase-bin.hpp>
#include <srat/rasterizer-phase-rasterize.hpp>
#include <srat/rasterizer-phase-vertex.hpp>
#include <srat/rasterizer-tile-grid.hpp>

#include <cstdlib>
#include <vector>

// -----------------------------------------------------------------------------
// -- helpers
// -----------------------------------------------------------------------------

char const * perf_suite_mode_label(PerfSuiteMode const mode)
{
	switch (mode) {
	case PerfSuiteMode::Vertex:  return "perf vertex";
	case PerfSuiteMode::Binning: return "perf binning";
	case PerfSuiteMode::Raster:  return "perf raster";
	default: return "unknown";
	}
}

// -----------------------------------------------------------------------------
// -- synthetic workload generation
// -----------------------------------------------------------------------------

namespace {

// number of synthetic triangles for perf runs
static constexpr u32 kPerfTriangleCount = 8192;

struct PerfWorkload {
	std::vector<srat::triangle_position_t> positions;
	std::vector<srat::triangle_depth_t> depths;
	std::vector<srat::triangle_perspective_w_t> perspectiveW;
	std::vector<f32v2> uvs;
	bool initialised { false };
};

static PerfWorkload & workload()
{
	static PerfWorkload wl;
	return wl;
}

// builds randomised screen-space triangle data that is viewport-aware
void ensure_workload(i32v2 const dim)
{
	PerfWorkload & wl = workload();
	if (wl.initialised) { return; }
	wl.initialised = true;

	u32 const vertCount = kPerfTriangleCount * 3u;
	wl.positions.resize(vertCount);
	wl.depths.resize(vertCount);
	wl.perspectiveW.resize(vertCount);
	wl.uvs.resize(vertCount);

	srand(42); // deterministic
	auto randf = []() -> f32 {
		return (f32)rand() / (f32)RAND_MAX;
	};

	for (u32 i = 0; i < vertCount; ++i) {
		wl.positions[i] = i32v2 {
			(i32)(randf() * (f32)dim.x),
			(i32)(randf() * (f32)dim.y),
		};
		wl.depths[i] = randf();
		wl.perspectiveW[i] = 0.5f + randf() * 0.5f;
		wl.uvs[i] = f32v2 { randf(), randf() };
	}
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// -- individual phase runners
// -----------------------------------------------------------------------------

namespace {

void run_perf_vertex(PerfSuiteConfig const & config)
{
	// build a simple draw call with synthetic vertex data
	u32 const triCount = kPerfTriangleCount;
	u32 const vertCount = triCount * 3u;

	// -- positions and colors as raw bytes for the DrawInfo
	static std::vector<f32v3> rawPositions;
	static std::vector<f32v2> rawUvs;
	static std::vector<u32> rawIndices;
	if (rawPositions.empty()) {
		rawPositions.resize(vertCount);
		rawUvs.resize(vertCount);
		rawIndices.resize(vertCount);
		srand(42);
		auto randf = []() -> f32 {
			return (f32)rand() / (f32)RAND_MAX;
		};
		for (u32 i = 0; i < vertCount; ++i) {
			rawPositions[i] = f32v3 {
				randf() * 2.f - 1.f,
				randf() * 2.f - 1.f,
				randf() * 2.f - 1.f,
			};
			rawUvs[i] = f32v2 { randf(), randf() };
			rawIndices[i] = i;
		}
	}

	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	static std::vector<srat::triangle_position_t> outPos(vertCount);
	static std::vector<srat::triangle_depth_t> outDepth(vertCount);
	static std::vector<srat::triangle_perspective_w_t> outPerspW(vertCount);
	static std::vector<f32v2> outUv(vertCount);

	srat::gfx::Viewport const viewport {
		.offset = { 0, 0 },
		.dim = { config.targetDim.x, config.targetDim.y },
	};

	srat::gfx::DrawInfo const drawInfo {
		.boundTexture = srat::gfx::Image { 0 }, // not used in shader
		.modelViewProjection = f32m44_identity(),
		.vertexAttributes = {
			.position = {
				.byteStride = sizeof(f32v3),
				.data = vec2slice(rawPositions),
			},
			.normal = {
				.byteStride = 0,
				.data = srat::slice<u8 const>(),
			},
			.uv = {
				.byteStride = sizeof(f32v2),
				.data = vec2slice(rawUvs),
			},
		},
		.indices = srat::slice(rawIndices.data(), rawIndices.size()),
		.indexCount = vertCount,
	};

	{
		SRAT_PROFILE_SCOPE("vtx");
		srat::rasterizer_phase_vertex(srat::RasterizerStageVertexParams {
			.draw = drawInfo,
			.viewport = viewport,
			.outPositions = srat::slice(outPos.data(), vertCount),
			.outDepth = srat::slice(outDepth.data(), vertCount),
			.outPerspectiveW = srat::slice(outPerspW.data(), vertCount),
			.outUvs = srat::slice(outUv.data(), vertCount),
			.attrOffset = 0,
		});
	}
}

void run_perf_binning(PerfSuiteConfig const & config)
{
	ensure_workload(config.targetDim);
	PerfWorkload & wl = workload();

	// create a fresh tile grid each frame so binning always does real work
	srat::TileGrid tileGrid = srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth = config.targetDim.x,
		.imageHeight = config.targetDim.y,
	});

	{
		SRAT_PROFILE_SCOPE("bin");
		srat::rasterizer_phase_bin(srat::RasterizerPhaseBinParams {
			.tileGrid = tileGrid,
			.trianglePositions = srat::slice(
				wl.positions.data(), wl.positions.size()
			),
			.triangleDepths = srat::slice(
				wl.depths.data(), wl.depths.size()
			),
			.trianglePerspectiveW = srat::slice(
				wl.perspectiveW.data(), wl.perspectiveW.size()
			),
			.triangleUvs = srat::slice(
				wl.uvs.data(), wl.uvs.size()
			),
		});
	}

	srat::tile_grid_destroy(tileGrid);
}

void run_perf_raster(PerfSuiteConfig const & config)
{
	ensure_workload(config.targetDim);
	PerfWorkload & wl = workload();

	// set up tile grid and run binning so the raster phase has real input
	static bool binningDone = false;
	static srat::TileGrid tileGrid = (
		srat::tile_grid_create(srat::TileGridCreateInfo {
			.imageWidth = config.targetDim.x,
			.imageHeight = config.targetDim.y,
		})
	);

	if (!binningDone) {
		binningDone = true;
		srat::rasterizer_phase_bin(srat::RasterizerPhaseBinParams {
			.tileGrid = tileGrid,
			.trianglePositions = srat::slice(
				wl.positions.data(), wl.positions.size()
			),
			.triangleDepths = srat::slice(
				wl.depths.data(), wl.depths.size()
			),
			.trianglePerspectiveW = srat::slice(
				wl.perspectiveW.data(), wl.perspectiveW.size()
			),
			.triangleUvs = srat::slice(
				wl.uvs.data(), wl.uvs.size()
			),
		});
	}

	srat::gfx::Viewport const viewport {
		.offset = { 0, 0 },
		.dim = { config.targetDim.x, config.targetDim.y },
	};

	{
		SRAT_PROFILE_SCOPE("raster");
		srat::rasterizer_phase_rasterization(
			srat::RasterizerPhaseRasterizationParams {
				.tileGrid = tileGrid,
				.viewport = viewport,
				.targetColor = config.targetColor,
				.targetDepth = config.targetDepth,
				.boundTexture = srat::gfx::Image { 0 }, // not used in shader
			}
		);
	}
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void perf_suite_run(PerfSuiteMode const mode, PerfSuiteConfig const & config)
{
	switch (mode) {
	case PerfSuiteMode::Vertex:  run_perf_vertex(config);  break;
	case PerfSuiteMode::Binning: run_perf_binning(config); break;
	case PerfSuiteMode::Raster:  run_perf_raster(config);  break;
	default: break;
	}
}
