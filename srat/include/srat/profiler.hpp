#pragma once

// Lightweight per-frame performance profiler.
//
// Usage:
//   1. Call Profiler::frame_begin() at the start of each frame.
//   2. Wrap sections with SRAT_PROFILE_SCOPE("my section") or manual
//      ScopedTimer objects.
//   3. Call Profiler::frame_end(frameTotalMs) at the end of each frame.
//   4. Read Profiler::snapshot() for ImGui rendering.
//
// Design:
//   - Uses std::chrono::high_resolution_clock for sub-millisecond resolution.
//   - Zero heap allocation in the hot path after the first frame (names are
//     registered on first use and reused every subsequent frame).
//   - All profiling calls are expected from a single thread (the main/render
//     thread) at the phase level; internal parallel work is timed as a whole.

#include <srat/core-types.hpp>

#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace srat {

static constexpr u32 kProfilerHistoryFrames = 64u;

// Per-section timing data.
struct ProfilerSection {
    std::string name;
    double lastMs  { 0.0 };  // duration during the most-recently completed frame
    double avgMs   { 0.0 };  // rolling average over kProfilerHistoryFrames frames
    std::array<double, kProfilerHistoryFrames> history {};
    u32    histHead { 0u };

    // Internal: accumulator for the frame currently in progress.
    double accumMs { 0.0 };
};

class Profiler {
public:
    // Reset all per-section accumulators.  Call once per frame before any work.
    static void frame_begin();

    // Snapshot accumulated data and compute rolling averages.
    // frameTotalMs — total wall-clock frame duration in milliseconds,
    //               used to derive percentage breakdowns in the UI.
    static void frame_end(double frameTotalMs);

    // Accumulate elapsed time into a named section.
    // Called automatically by ScopedTimer; may also be called manually.
    static void record(char const * name, double ms);

    // Stable read-only view of the most recently completed frame's data.
    // Safe to call at any point after frame_end().
    static std::vector<ProfilerSection> const & snapshot();

    // Total frame duration supplied to the most recent frame_end() call.
    static double snapshot_frame_total_ms();
};

// RAII helper that records elapsed time from construction to destruction.
struct ScopedTimer {
    char const * name;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    explicit ScopedTimer(char const * n) noexcept
        : name(n)
        , startTime(std::chrono::high_resolution_clock::now())
    {}

    ~ScopedTimer() noexcept {
        auto const endTime = std::chrono::high_resolution_clock::now();
        double const ms = (
            std::chrono::duration<double, std::milli>(endTime - startTime).count()
        );
        Profiler::record(name, ms);
    }

    ScopedTimer(ScopedTimer const &) = delete;
    ScopedTimer & operator=(ScopedTimer const &) = delete;
    ScopedTimer(ScopedTimer &&) = delete;
    ScopedTimer & operator=(ScopedTimer &&) = delete;
};

} // namespace srat

// Convenience macro: declares a ScopedTimer for the current scope.
// The __LINE__ suffix prevents name collisions when used multiple times
// within the same function.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SRAT_PROFILE_SCOPE(sectionName) \
    ::srat::ScopedTimer const _srat_prof_##__LINE__ { sectionName }
