// sovereign_stats_block_v2.h — Stub for build compatibility
#pragma once
#include <cstdint>
#include <string>

namespace sov {

struct SovereignStatsBlockV2 {
    uint64_t timestamp = 0;
    uint64_t cycles = 0;
    uint64_t instructions = 0;
    uint64_t cache_misses = 0;
    uint64_t branch_mispredicts = 0;
    uint64_t memory_bandwidth = 0;
    uint64_t tps = 0;
    uint64_t latency_ns = 0;

    void reset() {
        timestamp = cycles = instructions = cache_misses = branch_mispredicts = memory_bandwidth = tps = latency_ns = 0;
    }

    std::string toJson() const {
        return "{}";
    }
};

} // namespace sov
