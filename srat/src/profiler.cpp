#include <srat/profiler.hpp>

#include <numeric>
#include <vector>

// -----------------------------------------------------------------------------
// -- internal state
// -----------------------------------------------------------------------------

namespace {

struct ProfilerState {
	std::vector<srat::ProfilerSection> sections;
	std::vector<srat::ProfilerSection> snap;
	f64 snapFrameTotalMs { 0.0 };
};

static ProfilerState & state()
{
	static ProfilerState s;
	return s;
}

// Linear search by name
static srat::ProfilerSection * find_section(
	std::vector<srat::ProfilerSection> & sections,
	char const * const name
) {
	for (srat::ProfilerSection & sec : sections) {
		if (sec.name == name) { return &sec; }
	}
	return nullptr;
}

} // anonymous namespace

// -----------------------------------------------------------------------------
// -- public api
// -----------------------------------------------------------------------------

void srat::Profiler::frame_begin() {
	for (srat::ProfilerSection & sec : state().sections) {
		sec.accumMs = 0.0;
	}
}

void srat::Profiler::frame_end(f64 const frameTotalMs) {
	auto & s = state();

	for (auto & sec : s.sections) {
		sec.lastMs = sec.accumMs;

		// incremental update
		sec.runningSum -= sec.history[sec.histHead];
		sec.runningSum += sec.accumMs;
		sec.history[sec.histHead] = sec.accumMs;
		sec.histHead = (sec.histHead + 1u) % kProfilerHistoryFrames;

		sec.avgMs = sec.runningSum / static_cast<f64>(kProfilerHistoryFrames);
	}

	// copy a snapshot of section
	s.snap  = s.sections;
	s.snapFrameTotalMs = frameTotalMs;
}

void srat::Profiler::record(char const * const name, f64 const ms) {
	auto & s = state();
	srat::ProfilerSection * sec = find_section(s.sections, name);
	if (sec == nullptr) {
		// register section
		s.sections.emplace_back();
		sec = &s.sections.back();
		sec->name = name;
	}
	sec->accumMs += ms;
}

srat::slice<srat::ProfilerSection> srat::Profiler::snapshot() {
	return { state().snap.data(), state().snap.size() };
}

f64 srat::Profiler::snapshot_frame_total_ms() {
	return state().snapFrameTotalMs;
}
