#pragma once

#include "replay_core.hpp"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace RawrXD::Replay {

class PhaseTraceValidator {
  public:
    struct Result {
        uint64_t hash = 0;
        bool crash_isolated = false;
        bool deterministic = false;
        bool causal_integrity_ok = false;
        std::string detail;
    };

    using ExecutionFn = std::function<void()>;

    static Result RunSingleTest(const ExecutionFn& execute)
    {
        DrainBus();
        execute();

        const auto events = ReplayCore::DrainTraceBusSnapshot();
        const ReplayGraph graph = ReplayCore::Build(events);

        Result out;
        out.hash = graph.canonical_hash;
        out.crash_isolated = ContainsNoErrorEvents(events);
        out.causal_integrity_ok = graph.causal_integrity_ok;
        out.deterministic = out.causal_integrity_ok;
        out.detail = out.crash_isolated ? "ok" : "error_event_detected";
        return out;
    }

    static Result RunDeterminismPair(const ExecutionFn& execute_a, const ExecutionFn& execute_b)
    {
        DrainBus();
        execute_a();
        const auto first_events = ReplayCore::DrainTraceBusSnapshot();
        const ReplayGraph first_graph = ReplayCore::Build(first_events);

        DrainBus();
        execute_b();
        const auto second_events = ReplayCore::DrainTraceBusSnapshot();
        const ReplayGraph second_graph = ReplayCore::Build(second_events);

        const auto validation = ReplayCore::ValidateDeterminism(first_graph, second_graph);

        Result out;
        out.hash = first_graph.canonical_hash;
        out.causal_integrity_ok = validation.causal_integrity_ok;
        out.deterministic = validation.deterministic;
        out.crash_isolated = ContainsNoErrorEvents(first_events) && ContainsNoErrorEvents(second_events);
        out.detail = validation.detail;
        return out;
    }

  private:
    static bool ContainsNoErrorEvents(const std::vector<RawrXD::Trace::TraceEvent>& events)
    {
        for (const auto& e : events)
        {
            if (e.type == RawrXD::Trace::TraceType::Error)
            {
                return false;
            }
        }
        return true;
    }

    static void DrainBus()
    {
        RawrXD::Trace::TraceEvent ev{};
        while (RawrXD::Trace::TraceBus::Pop(ev))
        {
        }
    }
};

}  // namespace RawrXD::Replay
