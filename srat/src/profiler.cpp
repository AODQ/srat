#include <srat/profiler.hpp>

#include <numeric>

// -----------------------------------------------------------------------------
// -- internal state
// -----------------------------------------------------------------------------

namespace {

struct ProfilerState {
    // Insertion-ordered list of sections; new entries are appended on first use.
    std::vector<srat::ProfilerSection> sections;

    // Snapshot written by frame_end() and read by the ImGui panel.
    std::vector<srat::ProfilerSection> snap;

    double snapFrameTotalMs { 0.0 };
};

static ProfilerState & state()
{
    static ProfilerState s;
    return s;
}

// Linear search by name; fine for N < 20.
static srat::ProfilerSection * find_section(
    std::vector<srat::ProfilerSection> & sections,
    char const * const name
) {
    for (auto & sec : sections) {
        if (sec.name == name) {
            return &sec;
        }
    }
    return nullptr;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::Profiler::frame_begin()
{
    for (auto & sec : state().sections) {
        sec.accumMs = 0.0;
    }
}

void srat::Profiler::frame_end(double const frameTotalMs)
{
    auto & s = state();

    for (auto & sec : s.sections) {
        sec.lastMs                     = sec.accumMs;
        sec.history[sec.histHead]      = sec.accumMs;
        sec.histHead                   = (sec.histHead + 1u) % kProfilerHistoryFrames;

        double sum = 0.0;
        for (double const v : sec.history) {
            sum += v;
        }
        sec.avgMs = sum / static_cast<double>(kProfilerHistoryFrames);
    }

    // Publish a copy so the UI thread sees a consistent, fully-updated snapshot.
    s.snap              = s.sections;
    s.snapFrameTotalMs  = frameTotalMs;
}

void srat::Profiler::record(char const * const name, double const ms)
{
    auto & s = state();
    srat::ProfilerSection * sec = find_section(s.sections, name);
    if (sec == nullptr) {
        // First time this section is seen: register it.
        s.sections.emplace_back();
        sec       = &s.sections.back();
        sec->name = name;
    }
    sec->accumMs += ms;
}

std::vector<srat::ProfilerSection> const & srat::Profiler::snapshot()
{
    return state().snap;
}

double srat::Profiler::snapshot_frame_total_ms()
{
    return state().snapFrameTotalMs;
}
