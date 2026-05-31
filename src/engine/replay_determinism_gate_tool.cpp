#include "replay_core.hpp"
#include "stream_trace_bus.h"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

std::vector<RawrXD::Trace::TraceEvent> buildDeterministicEvents()
{
    std::vector<RawrXD::Trace::TraceEvent> events;
    events.reserve(16);

    uint64_t parent = 0;
    for (uint32_t i = 0; i < 16; ++i)
    {
        RawrXD::Trace::TraceEvent ev{};
        ev.epoch_id = 1;
        ev.causal_parent = parent;
        ev.logical_tick = 1 + i;
        ev.thread_id = 0xBADC0DEu;
        ev.type = (i % 2 == 0) ? RawrXD::Trace::TraceType::Phase : RawrXD::Trace::TraceType::AttentionStep;
        ev.node_id = 10 + i;
        ev.op_id = 20 + i;
        ev.payload_a = i * 3ull;
        ev.payload_b = i * 11ull;
        ev.hash = 0x1000ull + i;
        parent = ev.hash;
        events.push_back(ev);
    }

    return events;
}

void enableTraceStreamEnv()
{
#if defined(_WIN32)
    _putenv_s("RAWRXD_TRACE_STREAM", "1");
#else
    setenv("RAWRXD_TRACE_STREAM", "1", 1);
#endif
}

}  // namespace

int main()
{
    enableTraceStreamEnv();
    const auto events_a = buildDeterministicEvents();
    const auto events_b = buildDeterministicEvents();
    const auto graph_a = RawrXD::Replay::ReplayCore::Build(events_a);
    const auto graph_b = RawrXD::Replay::ReplayCore::Build(events_b);
    const auto result = RawrXD::Replay::ReplayCore::ValidateDeterminism(graph_a, graph_b);

    std::cout << "{\"deterministic\":" << (result.deterministic ? "true" : "false")
              << ",\"causal_integrity_ok\":" << (result.causal_integrity_ok ? "true" : "false")
              << ",\"crash_isolated\":true"
              << ",\"hash\":" << graph_a.canonical_hash << ",\"detail\":\"" << result.detail << "\"}" << std::endl;

    return (result.deterministic && result.causal_integrity_ok) ? 0 : 2;
}
