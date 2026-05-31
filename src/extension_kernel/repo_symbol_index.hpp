#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>

namespace RawrXD::ExtensionKernel {

struct RepoSymbol {
    uint64_t symbol_id = 0;
    uint64_t file_id = 0;
    uint64_t signature_hash = 0;
    uint32_t usage_count = 0;
    uint32_t last_seen_tick = 0;
    uint32_t first_edge = 0;
    uint32_t edge_count = 0;
};

struct RepoSymbolEdge {
    uint64_t target_symbol = 0;
    float weight = 0.0f;
    uint8_t type = 0; // 0=import, 1=usage, 2=override
};

struct RepoSymbolIndex {
    uint64_t repo_id = 0;
    RepoSymbol* nodes = nullptr;
    RepoSymbolEdge* edges = nullptr;
    uint32_t node_count = 0;
    uint32_t edge_count = 0;
    std::atomic<uint64_t> version{0};

    std::span<RepoSymbol> symbolSpan() const {
        return nodes ? std::span<RepoSymbol>(nodes, node_count) : std::span<RepoSymbol>();
    }

    std::span<RepoSymbolEdge> edgeSpan(const RepoSymbol& node) const {
        if (!edges || node.first_edge >= edge_count) return {};
        const uint32_t n = std::min(node.edge_count, edge_count - node.first_edge);
        return std::span<RepoSymbolEdge>(edges + node.first_edge, n);
    }
};

inline RepoSymbol* findSymbol(const RepoSymbolIndex& idx, uint64_t symbol_id) {
    if (!idx.nodes) return nullptr;
    for (uint32_t i = 0; i < idx.node_count; ++i) {
        if (idx.nodes[i].symbol_id == symbol_id) return &idx.nodes[i];
    }
    return nullptr;
}

inline void touchSymbol(RepoSymbol& node, uint32_t tick) {
    if (node.usage_count != UINT32_MAX) ++node.usage_count;
    node.last_seen_tick = tick;
}

inline uint64_t normalizeSymbolShape(uint64_t signature_hash, uint32_t dependency_count) {
    return signature_hash ^ (static_cast<uint64_t>(dependency_count) * 0x9E3779B185EBCA87ull);
}

} // namespace RawrXD::ExtensionKernel
