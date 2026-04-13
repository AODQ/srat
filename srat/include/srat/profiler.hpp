#pragma once

// Lightweight per-frame performance profiler.

#include <srat/core-types.hpp>

#include <chrono>
#include <string>

namespace srat {

static constexpr u32 kProfilerHistoryFrames = 64u;

// Per-section timing data.
struct ProfilerSection {
	std::string name;
	f64 lastMs  { 0.0 };
	f64 avgMs { 0.0 };
	srat::array<f64, kProfilerHistoryFrames> history {};
	u32 histHead { 0u };
	f64 runningSum { 0.0 };  // maintained incrementally to avoid O(N) re-sum

	// Internal: accumulator for the frame currently in progress.
	f64 accumMs { 0.0 };
};

namespace Profiler {
	void frame_begin();
	void frame_end(f64 const frameTotalMs);
	void record(char const * const name, f64 const ms);

	// read-only view, call only after frame_end
	[[nodiscard]] srat::slice<ProfilerSection> snapshot();
	[[nodiscard]] f64 snapshot_frame_total_ms();
};

// RAII timer
struct ScopedTimer {
	char const * name;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

	explicit ScopedTimer(char const * n)
		: name(n)
		, startTime(std::chrono::high_resolution_clock::now())
	{}

	~ScopedTimer() noexcept {
		auto const endTime = std::chrono::high_resolution_clock::now();
		f64 const ms = (
			std::chrono::duration<f64, std::milli>(endTime - startTime).count()
		);
		Profiler::record(name, ms);
	}

	ScopedTimer(ScopedTimer const &) = delete;
	ScopedTimer & operator=(ScopedTimer const &) = delete;
	ScopedTimer(ScopedTimer &&) = delete;
	ScopedTimer & operator=(ScopedTimer &&) = delete;
};

} // namespace srat

// declares a ScopedTimer for the current scope.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SRAT_PROFILE_SCOPE(sectionName) \
	::srat::ScopedTimer const _srat_prof_##__COUNTER__ { sectionName }
