#include "perf-suite.hpp"

#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-image.hpp>
#include <srat/profiler.hpp>
#include <srat/rasterizer-phase-bin.hpp>
#include <srat/rasterizer-phase-rasterize.hpp>
#include <srat/rasterizer-phase-vertex.hpp>
#include <srat/rasterizer-tile-grid.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
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
	std::vector<srat::triangle_color_t> colors;
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
	wl.colors.resize(vertCount);

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
		wl.colors[i] = f32v4 { randf(), randf(), randf(), 1.f };
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
	static std::vector<f32v4> rawColors;
	static std::vector<u32> rawIndices;
	if (rawPositions.empty()) {
		rawPositions.resize(vertCount);
		rawColors.resize(vertCount);
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
			rawColors[i] = f32v4 { randf(), randf(), randf(), 1.f };
			rawIndices[i] = i;
		}
	}

	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	static std::vector<srat::triangle_position_t> outPos(vertCount);
	static std::vector<srat::triangle_depth_t> outDepth(vertCount);
	static std::vector<srat::triangle_perspective_w_t> outPerspW(vertCount);
	static std::vector<srat::triangle_color_t> outColor(vertCount);

	srat::gfx::Viewport const viewport {
		.offset = { 0, 0 },
		.dim = { (u32)config.targetDim.x, (u32)config.targetDim.y },
	};

	srat::gfx::DrawInfo const drawInfo {
		.modelViewProjection = f32m44_identity(),
		.vertexAttributes = {
			.position = {
				.byteStride = sizeof(f32v3),
				.data = vec2slice(rawPositions),
			},
			.color = {
				.byteStride = sizeof(f32v4),
				.data = vec2slice(rawColors),
			},
			.normal = {
				.byteStride = 0,
				.data = srat::slice<u8 const>(),
			},
			.uv = {
				.byteStride = 0,
				.data = srat::slice<u8 const>(),
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
			.outColors = srat::slice(outColor.data(), vertCount),
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
		.imageWidth = (u32)config.targetDim.x,
		.imageHeight = (u32)config.targetDim.y,
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
			.triangleColors = srat::slice(
				wl.colors.data(), wl.colors.size()
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
			.imageWidth = (u32)config.targetDim.x,
			.imageHeight = (u32)config.targetDim.y,
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
			.triangleColors = srat::slice(
				wl.colors.data(), wl.colors.size()
			),
		});
	}

	srat::gfx::Viewport const viewport {
		.offset = { 0, 0 },
		.dim = { (u32)config.targetDim.x, (u32)config.targetDim.y },
	};

	{
		SRAT_PROFILE_SCOPE("raster");
		srat::rasterizer_phase_rasterization(
			srat::RasterizerPhaseRasterizationParams {
				.tileGrid = tileGrid,
				.viewport = viewport,
				.targetColor = config.targetColor,
				.targetDepth = config.targetDepth,
			}
		);
	}
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// -- startup suite implementation
// -----------------------------------------------------------------------------

namespace {

static constexpr u32 kStartupIterations = 100u;

struct PerfResult {
	char const * label;
	f64 avgMs;
	f64 minMs;
	f64 maxMs;
};

// times `iterations` calls to `fn` and returns stats
template <typename Fn>
PerfResult time_test(char const * label, u32 iterations, Fn fn)
{
	f64 total = 0.0;
	f64 minMs = 1e18;
	f64 maxMs = 0.0;
	for (u32 i = 0; i < iterations; ++i) {
		auto const t0 = std::chrono::high_resolution_clock::now();
		fn();
		auto const t1 = std::chrono::high_resolution_clock::now();
		f64 const ms = std::chrono::duration<f64, std::milli>(t1 - t0).count();
		total += ms;
		if (ms < minMs) { minMs = ms; }
		if (ms > maxMs) { maxMs = ms; }
	}
	return PerfResult { label, total / (f64)iterations, minMs, maxMs };
}

// -- vertex output buffers large enough for any workload

struct VertexOutputBuffers {
	std::vector<srat::triangle_position_t>      positions;
	std::vector<srat::triangle_depth_t>         depths;
	std::vector<srat::triangle_perspective_w_t> perspW;
	std::vector<srat::triangle_color_t>         colors;

	void resize(u32 count) {
		positions.resize(count);
		depths.resize(count);
		perspW.resize(count);
		colors.resize(count);
	}
};

srat::TileGrid make_tile_grid(i32v2 const dim)
{
	return srat::tile_grid_create(srat::TileGridCreateInfo {
		.imageWidth  = (u32)dim.x,
		.imageHeight = (u32)dim.y,
	});
}

// bin a VertexOutputBuffers into an existing TileGrid
void bin_vout(
	VertexOutputBuffers & vout,
	u32 triVertCount,
	srat::TileGrid & tg
)
{
	srat::rasterizer_phase_bin(srat::RasterizerPhaseBinParams {
		.tileGrid             = tg,
		.trianglePositions    = srat::slice(vout.positions.data(), triVertCount),
		.triangleDepths       = srat::slice(vout.depths.data(), triVertCount),
		.trianglePerspectiveW = srat::slice(vout.perspW.data(), triVertCount),
		.triangleColors       = srat::slice(vout.colors.data(), triVertCount),
	});
}

// bin the existing PerfWorkload into a TileGrid
void bin_perf_workload(
	PerfWorkload & wl,
	u32 triVertCount,
	srat::TileGrid & tg
)
{
	srat::rasterizer_phase_bin(srat::RasterizerPhaseBinParams {
		.tileGrid             = tg,
		.trianglePositions    = srat::slice(wl.positions.data(), triVertCount),
		.triangleDepths       = srat::slice(wl.depths.data(), triVertCount),
		.trianglePerspectiveW = srat::slice(wl.perspectiveW.data(), triVertCount),
		.triangleColors       = srat::slice(wl.colors.data(), triVertCount),
	});
}

u32 model_total_index_count(std::vector<srat::gfx::DrawInfo> const & meshes)
{
	u32 total = 0;
	for (auto const & di : meshes) { total += di.indexCount; }
	return total;
}

void transform_model(
	std::vector<srat::gfx::DrawInfo> const & meshes,
	i32v2 const dim,
	VertexOutputBuffers & vout
)
{
	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)dim.x, (u32)dim.y },
	};
	u32 const totalVerts = model_total_index_count(meshes);
	u32 offset = 0;
	for (auto const & di : meshes) {
		srat::rasterizer_phase_vertex(srat::RasterizerStageVertexParams {
			.draw            = di,
			.viewport        = vp,
			.outPositions    = srat::slice(vout.positions.data(), totalVerts),
			.outDepth        = srat::slice(vout.depths.data(), totalVerts),
			.outPerspectiveW = srat::slice(vout.perspW.data(), totalVerts),
			.outColors       = srat::slice(vout.colors.data(), totalVerts),
			.attrOffset      = offset,
		});
		offset += di.indexCount;
	}
}

// -- individual test functions

// 1. model vertex phase only
PerfResult test_model_vertex(
	PerfSuiteStartupConfig const & cfg,
	std::vector<srat::gfx::DrawInfo> const & meshes
)
{
	u32 const totalVerts = model_total_index_count(meshes);
	VertexOutputBuffers vout;
	vout.resize(totalVerts);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	return time_test("model-vertex", kStartupIterations, [&]() {
		u32 offset = 0;
		for (auto const & di : meshes) {
			srat::rasterizer_phase_vertex(srat::RasterizerStageVertexParams {
				.draw            = di,
				.viewport        = vp,
				.outPositions    = srat::slice(vout.positions.data(), totalVerts),
				.outDepth        = srat::slice(vout.depths.data(), totalVerts),
				.outPerspectiveW = srat::slice(vout.perspW.data(), totalVerts),
				.outColors       = srat::slice(vout.colors.data(), totalVerts),
				.attrOffset      = offset,
			});
			offset += di.indexCount;
		}
	});
}

// 2. model binning phase only (vertex output pre-computed)
PerfResult test_model_binning(
	PerfSuiteStartupConfig const & cfg,
	std::vector<srat::gfx::DrawInfo> const & meshes
)
{
	u32 const totalVerts = model_total_index_count(meshes);
	VertexOutputBuffers vout;
	vout.resize(totalVerts);
	transform_model(meshes, cfg.targetDim, vout);

	return time_test("model-binning", kStartupIterations, [&]() {
		srat::TileGrid tg = make_tile_grid(cfg.targetDim);
		bin_vout(vout, totalVerts, tg);
		srat::tile_grid_destroy(tg);
	});
}

// 3. model rasterize phase only (bin result cached, not timed)
PerfResult test_model_raster(
	PerfSuiteStartupConfig const & cfg,
	std::vector<srat::gfx::DrawInfo> const & meshes
)
{
	u32 const totalVerts = model_total_index_count(meshes);
	VertexOutputBuffers vout;
	vout.resize(totalVerts);
	transform_model(meshes, cfg.targetDim, vout);

	srat::TileGrid tg = make_tile_grid(cfg.targetDim);
	bin_vout(vout, totalVerts, tg);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	PerfResult res = time_test("model-raster", kStartupIterations, [&]() {
		srat::rasterizer_phase_rasterization(
			srat::RasterizerPhaseRasterizationParams {
				.tileGrid    = tg,
				.viewport    = vp,
				.targetColor = cfg.targetColor,
				.targetDepth = cfg.targetDepth,
			}
		);
	});

	srat::tile_grid_destroy(tg);
	return res;
}

// 4. full-screen triangle rasterize phase
PerfResult test_fullscreen_triangle_raster(PerfSuiteStartupConfig const & cfg)
{
	// one triangle covering the whole viewport in screen space
	static constexpr u32 kVc = 3u;
	VertexOutputBuffers vout;
	vout.resize(kVc);

	f32 const w = (f32)cfg.targetDim.x;
	f32 const h = (f32)cfg.targetDim.y;
	vout.positions[0] = i32v2 { 0,              0              };
	vout.positions[1] = i32v2 { (i32)(w * 2.f), 0              };
	vout.positions[2] = i32v2 { 0,              (i32)(h * 2.f) };
	vout.depths[0]    = 0.5f; vout.depths[1]    = 0.5f; vout.depths[2]    = 0.5f;
	vout.perspW[0]    = 1.f;  vout.perspW[1]    = 1.f;  vout.perspW[2]    = 1.f;
	vout.colors[0]    = f32v4 { 1.f, 0.f, 0.f, 1.f };
	vout.colors[1]    = f32v4 { 0.f, 1.f, 0.f, 1.f };
	vout.colors[2]    = f32v4 { 0.f, 0.f, 1.f, 1.f };

	srat::TileGrid tg = make_tile_grid(cfg.targetDim);
	bin_vout(vout, kVc, tg);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	PerfResult res = time_test(
		"fullscreen-triangle-raster", kStartupIterations, [&]() {
			srat::rasterizer_phase_rasterization(
				srat::RasterizerPhaseRasterizationParams {
					.tileGrid    = tg,
					.viewport    = vp,
					.targetColor = cfg.targetColor,
					.targetDepth = cfg.targetDepth,
				}
			);
		}
	);

	srat::tile_grid_destroy(tg);
	return res;
}

// build a workload where many small triangles cluster into very few tiles
void build_dense_bin_workload(
	i32v2 const dim,
	u32 const triCount,
	VertexOutputBuffers & vout
)
{
	vout.resize(triCount * 3u);

	i32 const cx = dim.x / 2;
	i32 const cy = dim.y / 2;
	static constexpr i32 kPatchRadius = 16;

	srand(1337u);
	for (u32 t = 0; t < triCount; ++t) {
		for (u32 v = 0; v < 3u; ++v) {
			u32 const idx = t * 3u + v;
			i32 const ox = (rand() % (kPatchRadius * 2 + 1)) - kPatchRadius;
			i32 const oy = (rand() % (kPatchRadius * 2 + 1)) - kPatchRadius;
			vout.positions[idx] = i32v2 { cx + ox, cy + oy };
			vout.depths[idx]    = 0.5f;
			vout.perspW[idx]    = 1.f;
			vout.colors[idx]    = f32v4 {
				(f32)(rand() % 256) / 255.f,
				(f32)(rand() % 256) / 255.f,
				(f32)(rand() % 256) / 255.f,
				1.f,
			};
		}
	}
}

// 5. many triangles per bin - bin phase
PerfResult test_many_tris_bin(PerfSuiteStartupConfig const & cfg)
{
	static constexpr u32 kDenseTriCount = 8192u;
	VertexOutputBuffers vout;
	build_dense_bin_workload(cfg.targetDim, kDenseTriCount, vout);
	u32 const vc = kDenseTriCount * 3u;

	return time_test("many-tris-per-bin-binning", kStartupIterations, [&]() {
		srat::TileGrid tg = make_tile_grid(cfg.targetDim);
		bin_vout(vout, vc, tg);
		srat::tile_grid_destroy(tg);
	});
}

// 6. many triangles per bin - raster phase
PerfResult test_many_tris_raster(PerfSuiteStartupConfig const & cfg)
{
	static constexpr u32 kDenseTriCount = 8192u;
	VertexOutputBuffers vout;
	build_dense_bin_workload(cfg.targetDim, kDenseTriCount, vout);
	u32 const vc = kDenseTriCount * 3u;

	srat::TileGrid tg = make_tile_grid(cfg.targetDim);
	bin_vout(vout, vc, tg);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	PerfResult res = time_test(
		"many-tris-per-bin-raster", kStartupIterations, [&]() {
			srat::rasterizer_phase_rasterization(
				srat::RasterizerPhaseRasterizationParams {
					.tileGrid    = tg,
					.viewport    = vp,
					.targetColor = cfg.targetColor,
					.targetDepth = cfg.targetDepth,
				}
			);
		}
	);

	srat::tile_grid_destroy(tg);
	return res;
}

// 7a. worst-case vertex phase (existing 8192-triangle synthetic workload)
PerfResult test_worstcase_vertex(PerfSuiteStartupConfig const & cfg)
{
	u32 const triCount  = kPerfTriangleCount;
	u32 const vertCount = triCount * 3u;

	static std::vector<f32v3> rawPositions;
	static std::vector<f32v4> rawColors;
	static std::vector<u32>   rawIndices;
	if (rawPositions.empty()) {
		rawPositions.resize(vertCount);
		rawColors.resize(vertCount);
		rawIndices.resize(vertCount);
		srand(42);
		auto randf = []() -> f32 { return (f32)rand() / (f32)RAND_MAX; };
		for (u32 i = 0; i < vertCount; ++i) {
			rawPositions[i] = f32v3 {
				randf() * 2.f - 1.f,
				randf() * 2.f - 1.f,
				randf() * 2.f - 1.f,
			};
			rawColors[i]  = f32v4 { randf(), randf(), randf(), 1.f };
			rawIndices[i] = i;
		}
	}

	auto vec2slice = [](auto & arr) -> srat::slice<u8 const> {
		return srat::slice(arr.data(), arr.size()).template cast<u8 const>();
	};

	static std::vector<srat::triangle_position_t>      outPos(vertCount);
	static std::vector<srat::triangle_depth_t>         outDepth(vertCount);
	static std::vector<srat::triangle_perspective_w_t> outPerspW(vertCount);
	static std::vector<srat::triangle_color_t>         outColor(vertCount);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	srat::gfx::DrawInfo const drawInfo {
		.modelViewProjection = f32m44_identity(),
		.vertexAttributes = {
			.position = { sizeof(f32v3), vec2slice(rawPositions) },
			.color    = { sizeof(f32v4), vec2slice(rawColors)    },
			.normal   = { 0,             srat::slice<u8 const>() },
			.uv       = { 0,             srat::slice<u8 const>() },
		},
		.indices    = srat::slice(rawIndices.data(), rawIndices.size()),
		.indexCount = vertCount,
	};

	return time_test("worstcase-vertex", kStartupIterations, [&]() {
		srat::rasterizer_phase_vertex(srat::RasterizerStageVertexParams {
			.draw            = drawInfo,
			.viewport        = vp,
			.outPositions    = srat::slice(outPos.data(), vertCount),
			.outDepth        = srat::slice(outDepth.data(), vertCount),
			.outPerspectiveW = srat::slice(outPerspW.data(), vertCount),
			.outColors       = srat::slice(outColor.data(), vertCount),
			.attrOffset      = 0,
		});
	});
}

// 7b. worst-case binning phase
PerfResult test_worstcase_binning(PerfSuiteStartupConfig const & cfg)
{
	ensure_workload(cfg.targetDim);
	PerfWorkload & wl = workload();
	u32 const vc = kPerfTriangleCount * 3u;

	return time_test("worstcase-binning", kStartupIterations, [&]() {
		srat::TileGrid tg = make_tile_grid(cfg.targetDim);
		bin_perf_workload(wl, vc, tg);
		srat::tile_grid_destroy(tg);
	});
}

// 7c. worst-case raster phase (bin cached)
PerfResult test_worstcase_raster(PerfSuiteStartupConfig const & cfg)
{
	ensure_workload(cfg.targetDim);
	PerfWorkload & wl = workload();
	u32 const vc = kPerfTriangleCount * 3u;

	srat::TileGrid tg = make_tile_grid(cfg.targetDim);
	bin_perf_workload(wl, vc, tg);

	srat::gfx::Viewport const vp {
		.offset = { 0, 0 },
		.dim    = { (u32)cfg.targetDim.x, (u32)cfg.targetDim.y },
	};

	PerfResult res = time_test("worstcase-raster", kStartupIterations, [&]() {
		srat::rasterizer_phase_rasterization(
			srat::RasterizerPhaseRasterizationParams {
				.tileGrid    = tg,
				.viewport    = vp,
				.targetColor = cfg.targetColor,
				.targetDepth = cfg.targetDepth,
			}
		);
	});

	srat::tile_grid_destroy(tg);
	return res;
}

// -- log helpers

std::string make_log_filename()
{
	std::time_t const t = std::time(nullptr);
	std::tm tm {};
#if defined(_WIN32)
	localtime_s(&tm, &t);
#else
	localtime_r(&t, &tm);
#endif
	char buf[64];
	std::snprintf(buf, sizeof(buf),
		"%04d-%02d-%02d-%02d-%02d-%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);
	return std::string("logs/") + buf + ".log";
}

void write_perf_log(
	std::string const & path,
	std::vector<PerfResult> const & results
)
{
	std::filesystem::create_directories("logs");
	std::ofstream f(path);
	if (!f.is_open()) {
		std::fprintf(
			stderr, "perf-suite: could not open log file %s\n", path.c_str()
		);
		return;
	}

	f << "srat performance log\n";
	f << "================================================================================\n";
	f << "iterations per test: " << kStartupIterations << "\n";
	f << "--------------------------------------------------------------------------------\n";
	for (auto const & r : results) {
		char line[256];
		std::snprintf(line, sizeof(line),
			"%-36s  avg %8.4f ms  min %8.4f ms  max %8.4f ms\n",
			r.label, r.avgMs, r.minMs, r.maxMs
		);
		f << line;
	}
	f << "================================================================================\n";

	std::fprintf(stdout, "perf-suite: log written to %s\n", path.c_str());
}

} // anonymous namespace (startup suite)

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

void perf_suite_run_startup(PerfSuiteStartupConfig const & cfg)
{
	std::vector<PerfResult> results;
	results.reserve(9);

	// -- tests 1-3: model phases (only when model meshes are provided)
	if (cfg.modelMeshes && !cfg.modelMeshes->empty()) {
		auto const & meshes = *cfg.modelMeshes;
		results.push_back(test_model_vertex(cfg, meshes));
		results.push_back(test_model_binning(cfg, meshes));
		results.push_back(test_model_raster(cfg, meshes));
	}

	// -- test 4: full-screen triangle raster
	results.push_back(test_fullscreen_triangle_raster(cfg));

	// -- tests 5-6: dense per-bin workload
	results.push_back(test_many_tris_bin(cfg));
	results.push_back(test_many_tris_raster(cfg));

	// -- tests 7a-c: worst-case phases (existing synthetic workload run once)
	results.push_back(test_worstcase_vertex(cfg));
	results.push_back(test_worstcase_binning(cfg));
	results.push_back(test_worstcase_raster(cfg));

	write_perf_log(make_log_filename(), results);
}
