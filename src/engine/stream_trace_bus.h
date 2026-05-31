#pragma once

#include "stream_trace_epoch.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <thread>

namespace RawrXD::Trace {

enum class TraceType : uint8_t {
    Phase,
    TensorOp,
    KernelDispatch,
    MemoryAlloc,
    MemoryFree,
    AttentionStep,
    Error,
};

struct TraceEvent {
    uint64_t epoch_id = 0;
    uint64_t causal_parent = 0;
    uint64_t logical_tick = 0;
    uint64_t thread_id = 0;
    TraceType type = TraceType::Phase;
    uint32_t node_id = 0;
    uint32_t op_id = 0;
    uint64_t payload_a = 0;
    uint64_t payload_b = 0;
    uint64_t hash = 0;
};

class TraceBus {
public:
    static constexpr uint32_t kCapacity = 1u << 18;

    static void InitFromEnv() {
        const char* enabledVar = std::getenv("RAWRXD_TRACE_STREAM");
        const char* levelVar = std::getenv("RAWRXD_TRACE_LEVEL");

        const bool explicitEnable = IsTruthy(enabledVar);
        const bool fullLevel = levelVar && std::string_view(levelVar) == "full";
        enabled_.store(explicitEnable || fullLevel, std::memory_order_release);
    }

    static bool Enabled() {
        return enabled_.load(std::memory_order_acquire);
    }

    static void Emit(TraceEvent event) {
        if (!Enabled()) {
            return;
        }

        if (event.epoch_id == 0) {
            event.epoch_id = TraceEpochAuthority::CurrentEpochId();
        }
        if (event.causal_parent == 0) {
            event.causal_parent = TraceEpochAuthority::CurrentThreadCausalRoot();
        }
        if (event.logical_tick == 0) {
            event.logical_tick = TraceEpochAuthority::NextGlobalTick();
        }
        event.thread_id = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        event.hash = RollingHash(event);
        TraceEpochAuthority::ObserveEventHash(event.hash);

        const uint32_t idx = write_idx_.fetch_add(1, std::memory_order_acq_rel);
        buffer_[idx & (kCapacity - 1)] = event;
    }

    static bool Pop(TraceEvent& out) {
        const uint32_t r = read_idx_.load(std::memory_order_acquire);
        const uint32_t w = write_idx_.load(std::memory_order_acquire);
        if (r >= w) {
            return false;
        }

        out = buffer_[r & (kCapacity - 1)];
        read_idx_.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    static bool IsTruthy(const char* v) {
        if (!v) {
            return false;
        }
        return std::strcmp(v, "1") == 0 ||
               std::strcmp(v, "true") == 0 ||
               std::strcmp(v, "TRUE") == 0 ||
               std::strcmp(v, "on") == 0 ||
               std::strcmp(v, "ON") == 0;
    }

    static uint64_t Mix(uint64_t h, uint64_t v) {
        h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    }

    static uint64_t RollingHash(const TraceEvent& e) {
        uint64_t seed = chain_.load(std::memory_order_relaxed);
        uint64_t h = seed;
        h = Mix(h, e.epoch_id);
        h = Mix(h, e.causal_parent);
        h = Mix(h, e.logical_tick);
        h = Mix(h, e.thread_id);
        h = Mix(h, static_cast<uint64_t>(e.type));
        h = Mix(h, e.node_id);
        h = Mix(h, e.op_id);
        h = Mix(h, e.payload_a);
        h = Mix(h, e.payload_b);
        chain_.store(h, std::memory_order_relaxed);
        return h;
    }

    inline static std::array<TraceEvent, kCapacity> buffer_{};
    inline static std::atomic<uint32_t> write_idx_{0};
    inline static std::atomic<uint32_t> read_idx_{0};
    inline static std::atomic<uint64_t> chain_{0x9e3779b97f4a7c15ull};
    inline static std::atomic<bool> enabled_{false};
};

inline void EmitPhase(uint32_t from, uint32_t to) {
    TraceBus::Emit(TraceEvent{0, 0, 0, 0, TraceType::Phase, from, to, 0, 0, 0});
}

inline void EmitTensorOp(uint32_t op, uint64_t a, uint64_t b) {
    TraceBus::Emit(TraceEvent{0, 0, 0, 0, TraceType::TensorOp, op, 0, a, b, 0});
}

inline void EmitKernelDispatch(uint32_t kernelId, uint64_t flags) {
    TraceBus::Emit(TraceEvent{0, 0, 0, 0, TraceType::KernelDispatch, kernelId, 0, flags, 0, 0});
}

inline void EmitAttentionStep(uint32_t head, uint32_t seqLen) {
    TraceBus::Emit(TraceEvent{0, 0, 0, 0, TraceType::AttentionStep, head, seqLen, 0, 0, 0});
}

}  // namespace RawrXD::Trace

#define RAWRXD_TRACE_INIT() RawrXD::Trace::TraceBus::InitFromEnv()
#define RAWRXD_TRACE_PHASE(from, to) RawrXD::Trace::EmitPhase(static_cast<uint32_t>(from), static_cast<uint32_t>(to))
#define RAWRXD_TRACE_TENSOR(op, a, b) RawrXD::Trace::EmitTensorOp(static_cast<uint32_t>(op), static_cast<uint64_t>(a), static_cast<uint64_t>(b))
#define RAWRXD_TRACE_KERNEL(kernel, flags) RawrXD::Trace::EmitKernelDispatch(static_cast<uint32_t>(kernel), static_cast<uint64_t>(flags))
#define RAWRXD_TRACE_ATTN(head, seq) RawrXD::Trace::EmitAttentionStep(static_cast<uint32_t>(head), static_cast<uint32_t>(seq))
