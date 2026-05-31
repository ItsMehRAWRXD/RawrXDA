#pragma once

#include "stream_trace_bus.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RawrXD::Replay {

struct ReplayNode {
    uint64_t id = 0;
    uint64_t epoch = 0;
    uint64_t parent = 0;
    RawrXD::Trace::TraceType type = RawrXD::Trace::TraceType::Phase;
    uint64_t tick = 0;
    uint32_t node_id = 0;
    uint32_t op_id = 0;
    uint64_t payload_a = 0;
    uint64_t payload_b = 0;
    uint64_t event_hash = 0;
};

struct ReplayEdge {
    uint64_t from = 0;
    uint64_t to = 0;
};

struct ReplayGraph {
    std::vector<ReplayNode> nodes;
    std::vector<ReplayEdge> edges;
    uint64_t canonical_hash = 0;
    bool causal_integrity_ok = true;
};

struct DeterminismValidation {
    bool deterministic = false;
    bool causal_integrity_ok = false;
    uint64_t left_hash = 0;
    uint64_t right_hash = 0;
    std::string detail;
};

class ReplayCore {
  public:
    static ReplayGraph Build(const std::vector<RawrXD::Trace::TraceEvent>& input_events)
    {
        std::vector<RawrXD::Trace::TraceEvent> events = input_events;
        sortEvents(events);

        ReplayGraph graph;
        graph.nodes.reserve(events.size());
        graph.edges.reserve(events.size());

        std::unordered_map<uint64_t, uint64_t> hash_to_node;
        hash_to_node.reserve(events.size());

        uint64_t next_id = 1;
        for (const auto& ev : events)
        {
            ReplayNode n;
            n.id = next_id++;
            n.epoch = ev.epoch_id;
            n.parent = ev.causal_parent;
            n.type = ev.type;
            n.tick = ev.logical_tick;
            n.node_id = ev.node_id;
            n.op_id = ev.op_id;
            n.payload_a = ev.payload_a;
            n.payload_b = ev.payload_b;
            n.event_hash = ev.hash;

            graph.nodes.push_back(n);
            hash_to_node[ev.hash] = n.id;
        }

        graph.causal_integrity_ok = buildEdges(graph, hash_to_node);
        graph.canonical_hash = HashGraph(graph);
        return graph;
    }

    static uint64_t HashGraph(const ReplayGraph& g)
    {
        uint64_t h = 0xcbf29ce484222325ull;

        for (const auto& n : g.nodes)
        {
            h = mix(h, n.id);
            h = mix(h, n.epoch);
            h = mix(h, n.parent);
            h = mix(h, static_cast<uint64_t>(n.type));
            h = mix(h, n.tick);
            h = mix(h, n.node_id);
            h = mix(h, n.op_id);
            h = mix(h, n.payload_a);
            h = mix(h, n.payload_b);
            h = mix(h, n.event_hash);
        }

        for (const auto& e : g.edges)
        {
            h = mix(h, e.from);
            h = mix(h, e.to);
        }

        h = mix(h, g.causal_integrity_ok ? 1ull : 0ull);
        return h;
    }

    static DeterminismValidation ValidateDeterminism(const ReplayGraph& left, const ReplayGraph& right)
    {
        DeterminismValidation out;
        out.left_hash = left.canonical_hash;
        out.right_hash = right.canonical_hash;
        out.causal_integrity_ok = left.causal_integrity_ok && right.causal_integrity_ok;
        out.deterministic = out.causal_integrity_ok && left.canonical_hash == right.canonical_hash &&
                            left.nodes.size() == right.nodes.size() && left.edges.size() == right.edges.size();

        if (out.deterministic)
        {
            out.detail = "deterministic";
            return out;
        }

        if (!out.causal_integrity_ok)
        {
            out.detail = "causal_integrity_violation";
            return out;
        }

        if (left.nodes.size() != right.nodes.size())
        {
            out.detail = "node_count_mismatch";
            return out;
        }

        if (left.edges.size() != right.edges.size())
        {
            out.detail = "edge_count_mismatch";
            return out;
        }

        out.detail = "hash_mismatch";
        return out;
    }

    static std::vector<RawrXD::Trace::TraceEvent> DrainTraceBusSnapshot()
    {
        std::vector<RawrXD::Trace::TraceEvent> out;
        RawrXD::Trace::TraceEvent ev{};
        while (RawrXD::Trace::TraceBus::Pop(ev))
        {
            out.push_back(ev);
        }
        return out;
    }

  private:
    static bool buildEdges(ReplayGraph& g, const std::unordered_map<uint64_t, uint64_t>& hash_to_node)
    {
        bool ok = true;
        for (const auto& n : g.nodes)
        {
            if (n.parent == 0)
            {
                continue;
            }

            const auto it = hash_to_node.find(n.parent);
            if (it == hash_to_node.end())
            {
                ok = false;
                continue;
            }

            g.edges.push_back(ReplayEdge{it->second, n.id});
        }
        return ok;
    }

    static void sortEvents(std::vector<RawrXD::Trace::TraceEvent>& events)
    {
        std::sort(events.begin(), events.end(), [](const auto& a, const auto& b)
                  {
                      if (a.epoch_id != b.epoch_id)
                          return a.epoch_id < b.epoch_id;
                      if (a.causal_parent != b.causal_parent)
                          return a.causal_parent < b.causal_parent;
                      if (a.logical_tick != b.logical_tick)
                          return a.logical_tick < b.logical_tick;
                      if (a.thread_id != b.thread_id)
                          return a.thread_id < b.thread_id;
                      if (a.hash != b.hash)
                          return a.hash < b.hash;
                      return static_cast<uint64_t>(a.type) < static_cast<uint64_t>(b.type);
                  });
    }

    static uint64_t mix(uint64_t seed, uint64_t value)
    {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    }
};

}  // namespace RawrXD::Replay
