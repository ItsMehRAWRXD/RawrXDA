#include "extension_kernel/cross_repo_router.hpp"

#include <array>
#include <cstdint>

int main() {
    using namespace RawrXD::ExtensionKernel;

    std::array<RepoSymbol, 4> repoA{{
        {0x101, 0xAAA, 0xD00D, 40, 99, 0, 1},
        {0x102, 0xAAB, 0xBEEF, 2,  10, 0, 0},
        {0x103, 0xAAC, 0xCAFE, 6,  20, 0, 0},
        {0x104, 0xAAD, 0xFEED, 1,   1, 0, 0},
    }};
    std::array<RepoSymbol, 4> repoB{{
        {0x201, 0xBBB, 0xD00D, 12, 98, 0, 1},
        {0x202, 0xBBC, 0x1234, 1,   4, 0, 0},
        {0x203, 0xBBD, 0x5678, 3,  30, 0, 0},
        {0x204, 0xBBE, 0x9999, 1,   2, 0, 0},
    }};
    std::array<RepoSymbolEdge, 2> edges{{
        {0x201, 0.9f, 1},
        {0x101, 0.8f, 0},
    }};

    RepoSymbolIndex idxA{0xA, repoA.data(), edges.data(), static_cast<uint32_t>(repoA.size()), 1};
    RepoSymbolIndex idxB{0xB, repoB.data(), edges.data() + 1, static_cast<uint32_t>(repoB.size()), 1};
    const RepoSymbolIndex* repos[] = {&idxA, &idxB};

    RouteContext ctx{};
    ctx.active_file = 0xAAA;
    ctx.active_symbol = 0x101;
    ctx.signature_hint = 0xD00D;
    ctx.edit_tick = 100;

    RouterLimits limits{};
    limits.max_repos = 2;
    limits.max_symbols = 3;
    limits.max_symbols_per_repo = 2;
    limits.min_score = 0.65f;

    auto routed = routeCrossRepoSymbols(std::span<const RepoSymbolIndex* const>(repos, 2), ctx, limits);
    if (routed.empty()) return 1;
    if (routed.size() > limits.max_symbols) return 2;
    if (routed[0].symbol_id != 0x101) return 3;
    for (const auto& r : routed) {
        if (r.score < limits.min_score) return 4;
    }

    touchSymbol(repoB[0], 101);
    if (repoB[0].last_seen_tick != 101 || repoB[0].usage_count != 13) return 5;
    return 0;
}
