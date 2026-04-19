#pragma once

#include <srat/core-types.hpp>
#include <srat/gfx-command-buffer.hpp>
#include <srat/gfx-device.hpp>
#include <srat/gfx-image.hpp>

// -----------------------------------------------------------------------------
// -- performance profiler suite
// -----------------------------------------------------------------------------
// runs isolated benchmarks for individual rasterization phases so that tools
// like Tracy can pinpoint bottlenecks without noise from other stages.
//
// only one mode runs at a time; the caller selects which via PerfSuiteMode.

enum struct PerfSuiteMode : u32 {
	Vertex,
	Binning,
	Raster,
	Count,
};

char const * perf_suite_mode_label(PerfSuiteMode mode);

struct PerfSuiteConfig {
	srat::gfx::Device device;
	srat::gfx::Image targetColor;
	srat::gfx::Image targetDepth;
	i32v2 targetDim;
};

// runs a single iteration of the selected phase benchmark.
// call once per frame inside the main loop.
void perf_suite_run(PerfSuiteMode mode, PerfSuiteConfig const & config);

// -----------------------------------------------------------------------------
// -- one-shot startup perf suite
// -----------------------------------------------------------------------------
// runs all 7 benchmark scenarios once at program start and writes results to
// a timestamped file under logs/.

struct PerfSuiteStartupConfig {
	srat::gfx::Device device;
	srat::gfx::Image targetColor;
	srat::gfx::Image targetDepth;
	i32v2 targetDim;
	// model mesh draw infos used for tests 1-3.
	// slices inside each DrawInfo must remain valid for the duration of the call.
	std::vector<srat::gfx::DrawInfo> const * modelMeshes { nullptr };
};

void perf_suite_run_startup(PerfSuiteStartupConfig const & config);
