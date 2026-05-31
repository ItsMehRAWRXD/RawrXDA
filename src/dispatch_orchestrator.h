#pragma once
// =============================================================================
// dispatch_orchestrator.h — Tiered Dispatch: GPU Vulkan + CPU AVX-512
// =============================================================================
// Routes inference work to the optimal compute backend based on tensor size
// and layer position:
//
//   Layers 0-N (FFN up-projection, >100KB):  GPU Vulkan   [throughput]
//   Layers N+  (sampling/penalties, <1MB):    CPU AVX-512  [latency]
//   Consensus voting / swarm aggregation:     CPU AVX-512  [deterministic]
//
// Integrates:
//   - mxcsr_determinism.asm  (FP mode bracketing)
//   - zerocopy_ring_bridge.asm  (tool→GPU zero-copy IPC)
//   - ums_micro_scheduler.asm  (non-preemptive kernel scheduling)
// =============================================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace RawrXD {

// ── MASM kernel imports ────────────────────────────────────────────────────
extern "C" {
// MXCSR determinism (mxcsr_determinism.asm)
void     RawrXD_MXCSR_LockDeterministic();
void     RawrXD_MXCSR_LockPerformance();
void     RawrXD_MXCSR_Save(uint32_t* pSlot);
void     RawrXD_MXCSR_Restore(const uint32_t* pSlot);
uint32_t RawrXD_MXCSR_GetMode();

// Zero-copy ring buffer (zerocopy_ring_bridge.asm)
uint32_t RawrXD_Ring_Init(void* pBuffer, uint64_t capacity);
uint64_t RawrXD_Ring_Produce(void* pRing, const void* pData, uint64_t nBytes);
uint64_t RawrXD_Ring_Available(const void* pRing);
uint64_t RawrXD_Ring_Free(const void* pRing);
void     RawrXD_Ring_ConsumerAdvance(void* pRing, uint64_t nBytes);
void     RawrXD_Ring_Reset(void* pRing);

// UMS micro-scheduler (ums_micro_scheduler.asm)
uint32_t RawrXD_Sched_Init();
void     RawrXD_Sched_Shutdown();
uint32_t RawrXD_Sched_SubmitKernel(void(*pfnKernel)(void*), void* pContext);
void     RawrXD_Sched_SetAffinity(uint64_t coreMask);
void     RawrXD_Sched_GetStats(uint64_t* pStats);
uint32_t RawrXD_Sched_IsReady();

// CPU feature detection (rawr_cpu_features.asm)
uint32_t rawr_cpu_has_avx512();
uint32_t rawr_cpu_has_avx2();

// Inference core GEMM/GEMV (inference_core.asm)
void InferenceCore_Init();
}

// ── Dispatch tier classification ───────────────────────────────────────────
enum class DispatchTier : uint8_t {
    GPU_Vulkan  = 0,  // Throughput: large matmuls (>100KB), FFN, attention QKV
    CPU_AVX512  = 1,  // Latency: sampling, penalties, small tensors (<1MB)
    CPU_AVX2    = 2,  // Fallback: no AVX-512 support
    CPU_Scalar  = 3,  // Last resort: no SIMD
};

// ── Dispatch decision for a single operation ───────────────────────────────
struct DispatchDecision {
    DispatchTier    tier;
    uint32_t        layer_index;     // Which transformer layer
    uint64_t        tensor_bytes;    // Size of the primary tensor
    bool            deterministic;   // Requires IEEE-754 strict mode
};

// ── Dispatch statistics ────────────────────────────────────────────────────
struct DispatchStats {
    uint64_t gpu_dispatches;
    uint64_t cpu_avx512_dispatches;
    uint64_t cpu_avx2_dispatches;
    uint64_t cpu_scalar_dispatches;
    uint64_t mxcsr_transitions;
    uint64_t ring_bytes_produced;
    uint64_t ring_drops;
    uint64_t sched_submitted;
    uint64_t sched_completed;
    uint64_t sched_errors;
};

// ── Configuration ──────────────────────────────────────────────────────────
struct DispatchConfig {
    uint32_t gpu_layer_cutoff = 4;         // Layers [0, cutoff) → GPU
    uint64_t gpu_tensor_threshold = 102400; // Tensors > threshold → GPU (bytes)
    uint64_t ring_capacity = 1 << 20;      // 1MB ring buffer
    uint32_t sched_max_threads = 4;        // Pool thread limit
    uint64_t cpu_affinity_mask = 0;        // 0 = no pinning
    bool     force_deterministic = false;  // Force IEEE-754 on all paths
    bool     enable_gpu = true;            // Set false for CPU-only (800B path)
};

// =============================================================================
// DispatchOrchestrator — The central routing layer
// =============================================================================
class DispatchOrchestrator {
public:
    static DispatchOrchestrator& instance();

    // Lifecycle
    bool initialize(const DispatchConfig& config = {});
    void shutdown();
    bool isReady() const { return m_ready.load(std::memory_order_acquire); }

    // ── Tier classification ────────────────────────────────────────────────
    // Given a layer index and tensor size, decide where to route.
    DispatchTier classify(uint32_t layerIndex, uint64_t tensorBytes, bool needsDeterminism = false) const;

    // ── Kernel submission ──────────────────────────────────────────────────
    // Submit a kernel to the appropriate tier.  The callback runs on either
    // the UMS pool (CPU) or returns immediately after GPU queue submit.
    bool submitCPUKernel(void(*pfnKernel)(void*), void* ctx);

    // ── Ring buffer access (for tool→context bridge) ───────────────────────
    uint64_t ringProduce(const void* data, uint64_t bytes);
    uint64_t ringAvailable() const;
    uint64_t ringFree() const;
    void     ringConsumerAdvance(uint64_t bytes);
    void     ringReset();

    // ── MXCSR mode management ──────────────────────────────────────────────
    // Brackets a callable with the appropriate FP mode.
    template<typename Fn>
    void withPerformanceMode(Fn&& fn) {
        uint32_t saved = 0;
        RawrXD_MXCSR_Save(&saved);
        RawrXD_MXCSR_LockPerformance();
        fn();
        RawrXD_MXCSR_Restore(&saved);
        m_stats.mxcsr_transitions.fetch_add(1, std::memory_order_relaxed);
    }

    template<typename Fn>
    void withDeterministicMode(Fn&& fn) {
        uint32_t saved = 0;
        RawrXD_MXCSR_Save(&saved);
        RawrXD_MXCSR_LockDeterministic();
        fn();
        RawrXD_MXCSR_Restore(&saved);
        m_stats.mxcsr_transitions.fetch_add(1, std::memory_order_relaxed);
    }

    // ── Telemetry ──────────────────────────────────────────────────────────
    DispatchStats getStats() const;

    // ── Hardware query ─────────────────────────────────────────────────────
    bool hasAVX512() const { return m_hasAVX512; }
    bool hasAVX2()   const { return m_hasAVX2; }

    const DispatchConfig& config() const { return m_config; }

private:
    DispatchOrchestrator() = default;
    ~DispatchOrchestrator();
    DispatchOrchestrator(const DispatchOrchestrator&) = delete;
    DispatchOrchestrator& operator=(const DispatchOrchestrator&) = delete;

    DispatchConfig  m_config;
    std::atomic<bool> m_ready{false};
    bool m_hasAVX512 = false;
    bool m_hasAVX2   = false;

    // Ring buffer: header + data, allocated via VirtualAlloc
    void*   m_ringBuffer = nullptr;
    uint64_t m_ringCapacity = 0;

    // Atomic stats
    struct AtomicStats {
        std::atomic<uint64_t> gpu_dispatches{0};
        std::atomic<uint64_t> cpu_avx512_dispatches{0};
        std::atomic<uint64_t> cpu_avx2_dispatches{0};
        std::atomic<uint64_t> cpu_scalar_dispatches{0};
        std::atomic<uint64_t> mxcsr_transitions{0};
        std::atomic<uint64_t> ring_bytes_produced{0};
        std::atomic<uint64_t> ring_drops{0};
    };
    mutable AtomicStats m_stats;
};

} // namespace RawrXD
