#pragma once

#include <atomic>
#include <cstdint>

namespace RawrXD::Trace {

struct TraceEpoch {
    uint64_t epoch_id = 0;
    uint64_t global_tick = 0;
    uint64_t causal_root = 0;
};

class TraceEpochAuthority {
public:
    static uint64_t CurrentEpochId() {
        return epoch_id_.load(std::memory_order_acquire);
    }

    static uint64_t NextGlobalTick() {
        return global_tick_.fetch_add(1, std::memory_order_relaxed);
    }

    static uint64_t CurrentThreadCausalRoot() {
        return thread_causal_root_;
    }

    static void ObserveEventHash(uint64_t hash) {
        thread_causal_root_ = hash;
    }

    static TraceEpoch Snapshot() {
        return TraceEpoch{CurrentEpochId(), NextGlobalTick(), CurrentThreadCausalRoot()};
    }

    static uint64_t AdvanceEpoch() {
        return epoch_id_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

private:
    inline static std::atomic<uint64_t> epoch_id_{1};
    inline static std::atomic<uint64_t> global_tick_{1};
    inline static thread_local uint64_t thread_causal_root_ = 0;
};

}  // namespace RawrXD::Trace
