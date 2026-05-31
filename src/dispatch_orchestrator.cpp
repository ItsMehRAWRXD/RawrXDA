// =============================================================================
// dispatch_orchestrator.cpp — Tiered GPU/CPU Dispatch Implementation
// =============================================================================
#include "dispatch_orchestrator.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cinttypes>
#include <cstring>

namespace RawrXD {

DispatchOrchestrator& DispatchOrchestrator::instance()
{
    static DispatchOrchestrator s_instance;
    return s_instance;
}

DispatchOrchestrator::~DispatchOrchestrator()
{
    shutdown();
}

bool DispatchOrchestrator::initialize(const DispatchConfig& config)
{
    if (m_ready.load(std::memory_order_acquire))
        return true; // already initialized

    m_config = config;

    // ── Detect CPU capabilities ────────────────────────────────────────
    m_hasAVX512 = (rawr_cpu_has_avx512() != 0);
    m_hasAVX2   = (rawr_cpu_has_avx2() != 0);

    // Initialize GEMM/GEMV dispatch pointers based on detected features
    InferenceCore_Init();

    // ── Initialize UMS scheduler ───────────────────────────────────────
    if (!RawrXD_Sched_Init())
    {
        OutputDebugStringA("[DispatchOrchestrator] WARNING: UMS scheduler init failed, using inline dispatch\n");
        // Non-fatal: kernels will run inline on caller thread
    }

    // Set CPU affinity if configured
    if (config.cpu_affinity_mask != 0)
    {
        RawrXD_Sched_SetAffinity(config.cpu_affinity_mask);
    }

    // ── Allocate ring buffer ───────────────────────────────────────────
    m_ringCapacity = config.ring_capacity;
    if (m_ringCapacity < 4096)
        m_ringCapacity = 4096;

    // Ensure power of 2
    uint64_t cap = 1;
    while (cap < m_ringCapacity)
        cap <<= 1;
    m_ringCapacity = cap;

#ifdef _WIN32
    // Use VirtualAlloc for page-aligned memory — compatible with Vulkan
    // host-visible mapping if the GPU driver supports external memory.
    const uint64_t totalSize = 64 + m_ringCapacity; // header + data
    m_ringBuffer = VirtualAlloc(nullptr, totalSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    m_ringBuffer = nullptr;
#endif

    if (m_ringBuffer)
    {
        if (!RawrXD_Ring_Init(m_ringBuffer, m_ringCapacity))
        {
            OutputDebugStringA("[DispatchOrchestrator] WARNING: Ring buffer init failed\n");
#ifdef _WIN32
            VirtualFree(m_ringBuffer, 0, MEM_RELEASE);
#endif
            m_ringBuffer = nullptr;
        }
    }

    m_ready.store(true, std::memory_order_release);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "[DispatchOrchestrator] Ready: AVX-512=%s AVX2=%s UMS=%s Ring=%s (%" PRIu64 "KB)\n",
             m_hasAVX512 ? "YES" : "NO",
             m_hasAVX2 ? "YES" : "NO",
             RawrXD_Sched_IsReady() ? "YES" : "NO",
             m_ringBuffer ? "YES" : "NO",
             m_ringCapacity / 1024);
    OutputDebugStringA(msg);

    return true;
}

void DispatchOrchestrator::shutdown()
{
    if (!m_ready.load(std::memory_order_acquire))
        return;

    m_ready.store(false, std::memory_order_release);

    // Shutdown scheduler (waits for pending work)
    RawrXD_Sched_Shutdown();

    // Free ring buffer
    if (m_ringBuffer)
    {
#ifdef _WIN32
        VirtualFree(m_ringBuffer, 0, MEM_RELEASE);
#endif
        m_ringBuffer = nullptr;
    }

    OutputDebugStringA("[DispatchOrchestrator] Shutdown complete\n");
}

// ── Tier classification ────────────────────────────────────────────────────
DispatchTier DispatchOrchestrator::classify(uint32_t layerIndex, uint64_t tensorBytes,
                                             bool needsDeterminism) const
{
    // Deterministic operations (consensus voting, swarm aggregation) always
    // go to CPU with IEEE-754 strict mode, regardless of tensor size.
    if (needsDeterminism || m_config.force_deterministic)
    {
        return m_hasAVX512 ? DispatchTier::CPU_AVX512 : DispatchTier::CPU_AVX2;
    }

    // GPU path: early layers with large tensors, and GPU is available
    if (m_config.enable_gpu &&
        layerIndex < m_config.gpu_layer_cutoff &&
        tensorBytes > m_config.gpu_tensor_threshold)
    {
        return DispatchTier::GPU_Vulkan;
    }

    // CPU path: late layers, small tensors, or GPU disabled
    if (m_hasAVX512) return DispatchTier::CPU_AVX512;
    if (m_hasAVX2)   return DispatchTier::CPU_AVX2;
    return DispatchTier::CPU_Scalar;
}

// ── Kernel submission ──────────────────────────────────────────────────────
bool DispatchOrchestrator::submitCPUKernel(void(*pfnKernel)(void*), void* ctx)
{
    if (!pfnKernel)
        return false;

    DispatchTier tier = m_hasAVX512 ? DispatchTier::CPU_AVX512 : DispatchTier::CPU_AVX2;

    // Try UMS scheduler first (non-preemptive, MXCSR-bracketed)
    if (RawrXD_Sched_IsReady())
    {
        if (RawrXD_Sched_SubmitKernel(pfnKernel, ctx))
        {
            if (tier == DispatchTier::CPU_AVX512)
                m_stats.cpu_avx512_dispatches.fetch_add(1, std::memory_order_relaxed);
            else
                m_stats.cpu_avx2_dispatches.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }

    // Fallback: run inline with MXCSR bracketing
    uint32_t savedMxcsr = 0;
    RawrXD_MXCSR_Save(&savedMxcsr);
    RawrXD_MXCSR_LockPerformance();
    pfnKernel(ctx);
    RawrXD_MXCSR_Restore(&savedMxcsr);
    m_stats.mxcsr_transitions.fetch_add(1, std::memory_order_relaxed);

    if (tier == DispatchTier::CPU_AVX512)
        m_stats.cpu_avx512_dispatches.fetch_add(1, std::memory_order_relaxed);
    else if (tier == DispatchTier::CPU_AVX2)
        m_stats.cpu_avx2_dispatches.fetch_add(1, std::memory_order_relaxed);
    else
        m_stats.cpu_scalar_dispatches.fetch_add(1, std::memory_order_relaxed);

    return true;
}

// ── Ring buffer operations ─────────────────────────────────────────────────
uint64_t DispatchOrchestrator::ringProduce(const void* data, uint64_t bytes)
{
    if (!m_ringBuffer || !data || bytes == 0)
        return 0;

    uint64_t written = RawrXD_Ring_Produce(m_ringBuffer, data, bytes);
    if (written > 0)
        m_stats.ring_bytes_produced.fetch_add(written, std::memory_order_relaxed);
    else
        m_stats.ring_drops.fetch_add(1, std::memory_order_relaxed);
    return written;
}

uint64_t DispatchOrchestrator::ringAvailable() const
{
    if (!m_ringBuffer) return 0;
    return RawrXD_Ring_Available(m_ringBuffer);
}

uint64_t DispatchOrchestrator::ringFree() const
{
    if (!m_ringBuffer) return 0;
    return RawrXD_Ring_Free(m_ringBuffer);
}

void DispatchOrchestrator::ringConsumerAdvance(uint64_t bytes)
{
    if (m_ringBuffer)
        RawrXD_Ring_ConsumerAdvance(m_ringBuffer, bytes);
}

void DispatchOrchestrator::ringReset()
{
    if (m_ringBuffer)
        RawrXD_Ring_Reset(m_ringBuffer);
}

// ── Stats ──────────────────────────────────────────────────────────────────
DispatchStats DispatchOrchestrator::getStats() const
{
    DispatchStats s;
    s.gpu_dispatches        = m_stats.gpu_dispatches.load(std::memory_order_relaxed);
    s.cpu_avx512_dispatches = m_stats.cpu_avx512_dispatches.load(std::memory_order_relaxed);
    s.cpu_avx2_dispatches   = m_stats.cpu_avx2_dispatches.load(std::memory_order_relaxed);
    s.cpu_scalar_dispatches = m_stats.cpu_scalar_dispatches.load(std::memory_order_relaxed);
    s.mxcsr_transitions     = m_stats.mxcsr_transitions.load(std::memory_order_relaxed);
    s.ring_bytes_produced   = m_stats.ring_bytes_produced.load(std::memory_order_relaxed);
    s.ring_drops            = m_stats.ring_drops.load(std::memory_order_relaxed);

    // Pull scheduler stats
    uint64_t schedStats[3] = {};
    RawrXD_Sched_GetStats(schedStats);
    s.sched_submitted = schedStats[0];
    s.sched_completed = schedStats[1];
    s.sched_errors    = schedStats[2];
    return s;
}

} // namespace RawrXD
