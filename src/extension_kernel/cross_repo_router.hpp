#pragma once

#include "repo_symbol_index.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace RawrXD::ExtensionKernel {

struct RouteContext {
    uint64_t active_file = 0;
    uint64_t active_symbol = 0;
    uint64_t signature_hint = 0;
    uint32_t edit_tick = 0;
};

struct RoutedSymbol {
    uint64_t repo_id = 0;
    uint64_t symbol_id = 0;
    uint64_t file_id = 0;
    uint64_t signature_hash = 0;
    float score = 0.0f;
};

struct RouterLimits {
    uint32_t max_repos = 4;
    uint32_t max_symbols = 32;
    uint32_t max_symbols_per_repo = 12;
    float min_score = 0.65f;
};

inline float scoreSymbol(const RepoSymbol& node, const RepoSymbolIndex& repo, const RouteContext& ctx) {
    const uint32_t age = (ctx.edit_tick > node.last_seen_tick) ? (ctx.edit_tick - node.last_seen_tick) : 0;
    const float recency = 1.0f / static_cast<float>(1u + age);
    const float usage = std::min(2.5f, static_cast<float>(node.usage_count) * 0.08f);
    const float proximity = (node.file_id == ctx.active_file) ? 1.25f : 0.0f;
    const float symbol_match = (node.symbol_id == ctx.active_symbol && ctx.active_symbol != 0) ? 1.5f : 0.0f;
    const float shape = (ctx.signature_hint != 0 && node.signature_hash == ctx.signature_hint) ? 1.1f : 0.0f;
    const float edge_penalty = std::min(0.4f, static_cast<float>(node.edge_count) * 0.025f);
    const float repo_bias = (repo.repo_id != 0) ? 0.15f : 0.0f;
    return recency + usage + proximity + symbol_match + shape + repo_bias - edge_penalty;
}

inline void insertTopK(std::vector<RoutedSymbol>& out, const RoutedSymbol& cand, uint32_t k) {
    if (k == 0) return;
    if (out.size() < k) {
        out.push_back(cand);
        std::push_heap(out.begin(), out.end(), [](const RoutedSymbol& a, const RoutedSymbol& b) {
            return a.score > b.score;
        });
        return;
    }
    if (!out.empty() && cand.score > out.front().score) {
        std::pop_heap(out.begin(), out.end(), [](const RoutedSymbol& a, const RoutedSymbol& b) {
            return a.score > b.score;
        });
        out.back() = cand;
        std::push_heap(out.begin(), out.end(), [](const RoutedSymbol& a, const RoutedSymbol& b) {
            return a.score > b.score;
        });
    }
}

inline std::vector<RoutedSymbol> routeCrossRepoSymbols(
    std::span<const RepoSymbolIndex* const> repos,
    const RouteContext& ctx,
    RouterLimits limits = {})
{
    std::vector<RoutedSymbol> routed;
    routed.reserve(std::min<uint32_t>(limits.max_symbols, 64));
    std::array<uint32_t, 8> per_repo{};
    const uint32_t repo_count = std::min<uint32_t>(static_cast<uint32_t>(repos.size()), std::min<uint32_t>(limits.max_repos, 8));

    for (uint32_t r = 0; r < repo_count; ++r) {
        const RepoSymbolIndex* repo = repos[r];
        if (!repo || !repo->nodes || repo->node_count == 0) continue;
        uint32_t accepted_for_repo = 0;
        for (uint32_t i = 0; i < repo->node_count; ++i) {
            const RepoSymbol& node = repo->nodes[i];
            const float score = scoreSymbol(node, *repo, ctx);
            if (score < limits.min_score) continue;
            if (accepted_for_repo >= limits.max_symbols_per_repo) break;
            insertTopK(routed, RoutedSymbol{repo->repo_id, node.symbol_id, node.file_id, node.signature_hash, score}, limits.max_symbols);
            ++accepted_for_repo;
        }
        per_repo[r] = accepted_for_repo;
    }

    std::sort(routed.begin(), routed.end(), [](const RoutedSymbol& a, const RoutedSymbol& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.repo_id != b.repo_id) return a.repo_id < b.repo_id;
        return a.symbol_id < b.symbol_id;
    });
    (void)per_repo;
    return routed;
}

} // namespace RawrXD::ExtensionKernel
