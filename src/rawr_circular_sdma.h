// ============================================================================
// rawr_circular_sdma.h — Kinetic Weight Streamer Interface (1.8T to 2.0T)
// ============================================================================
// C++ interface for RAWR_CIRCULAR_SDMA.ASM: dual 8GB VRAM windows with
// predictive expert prefetching driven by EMA residency predictor.
//
// Architecture:
// - WINDOW_A_BASE: First 8GB BAR aperture (experts 0-19 @ 400MB each)
// - WINDOW_B_BASE: Second 8GB BAR aperture (experts 20-39 @ 400MB each)
// - SDMA0: AMD RDNA3 DMA engine (MMIO registers: doorbell, status, fence)
// - EMA Residency Predictor: m_recentExpertPins from SwarmScheduler
//
// Telemetry:
// - g_sdma_flip_count: Window swap count (monotonic, confirms kinetic flow)
// - g_sdma_wait_cycles: RDTSC cycles spent polling SDMA0_STATUS register
// - g_expert_cache_hits: EMA predictor correct (expert already resident)
// - g_expert_cache_misses: EMA predictor miss (SDMA transfer required)
//
// Target Latency: < 32ms per expert swap (76.8M cycles @ 2.4 GHz)
// ============================================================================

#pragma once

#include <cstdint>

namespace RawrXD
{
namespace CircularSDMA
{

// ============================================================================
// Configuration Constants (matched to RAWR_CIRCULAR_SDMA.ASM)
// ============================================================================
constexpr std::uint64_t WINDOW_SIZE_BYTES = 8ull * 1024ull * 1024ull * 1024ull;  // 8GB per window
constexpr std::uint64_t TYPICAL_EXPERT_SIZE = 400ull * 1024ull * 1024ull;        // 400MB per expert
constexpr std::uint32_t MAX_EXPERTS_PER_WINDOW = 20;                             // 8GB / 400MB

// SDMA0 MMIO register offsets (relative to BAR base)
constexpr std::uint32_t SDMA0_DOORBELL_OFFSET = 0xD000;  // Write to initiate transfer
constexpr std::uint32_t SDMA0_STATUS_OFFSET   = 0xD004;  // Poll for completion (0 = busy, 1 = done)
constexpr std::uint32_t SDMA0_FENCE_OFFSET    = 0xD008;  // Hardware fence value

// Latency targets (RDTSC cycles @ 2.4 GHz)
constexpr std::uint64_t MAX_WAIT_CYCLES_32MS = 76'800'000ull;  // 32ms * 2.4 GHz
constexpr std::uint64_t TIMEOUT_CYCLES_100MS = 240'000'000ull; // 100ms safety timeout

// ============================================================================
// Telemetry Counters (exported from RAWR_CIRCULAR_SDMA.ASM)
// ============================================================================
extern "C" {
    // Monotonic window flip count (confirms sustained kinetic flow)
    extern std::uint64_t g_sdma_flip_count;

    // Aggregated RDTSC cycles spent in Rawr_Wait_And_Flip_Window spin-loop
    extern std::uint64_t g_sdma_wait_cycles;

    // EMA prediction accuracy counters
    extern std::uint64_t g_expert_cache_hits;   // Expert already resident (prefetch worked)
    extern std::uint64_t g_expert_cache_misses; // Expert not resident (SDMA transfer needed)

    // Current state (do NOT modify from C++ — read-only visibility)
    extern std::uint32_t CURRENT_WINDOW;        // 0 = WINDOW_A active, 1 = WINDOW_B active
    extern std::uint32_t CURRENT_EXPERT_ID;     // Currently loaded expert (layer-local ordinal)
    extern std::uint32_t NEXT_EXPERT_ID;        // Prefetch target (set by EMA predictor)

    // BAR base address (must be set before first Gemini pulse)
    extern std::uint64_t g_sovereign_bar_base;
}

// ============================================================================
// ASM Procedure Exports (from RAWR_CIRCULAR_SDMA.ASM)
// ============================================================================
extern "C" {
    /// Initialize dual 8GB windows (WINDOW_A_BASE = BAR + 0, WINDOW_B_BASE = BAR + 8GB)
    /// @param bar_base: Physical BAR aperture base address (from PCI config)
    void Rawr_Init_Circular_SDMA(std::uint64_t bar_base);

    /// Execute one Gemini-class inference pulse with predictive prefetching.
    /// @param current_layer: Layer index (0-based)
    /// @param predicted_expert_id: EMA predictor output (NEXT_EXPERT_ID)
    /// @return Pointer to active 8GB window containing current expert weights
    void* Rawr_Execute_Gemini_Pulse(std::uint32_t current_layer, std::uint32_t predicted_expert_id);

    /// Hardware sync barrier: poll SDMA0_STATUS until transfer completes, then flip windows.
    /// @return Pointer to newly active window (WINDOW_A or WINDOW_B)
    void* Rawr_Wait_And_Flip_Window();

    /// Query telemetry counters (safe to call from PowerShell via FFI).
    /// @param out_flip_count: Window swap count
    /// @param out_wait_cycles: Aggregated RDTSC cycles in spin-wait
    /// @param out_cache_hits: EMA predictor correct count
    /// @param out_cache_misses: EMA predictor miss count
    void Rawr_Get_SDMA_Telemetry(std::uint64_t* out_flip_count,
                                 std::uint64_t* out_wait_cycles,
                                 std::uint64_t* out_cache_hits,
                                 std::uint64_t* out_cache_misses);
}

// ============================================================================
// C++ RAII Helper for Kinetic Pulse Execution
// ============================================================================
struct GeminiPulseGuard
{
    std::uint32_t layer_index;
    std::uint32_t predicted_expert;
    void* active_window = nullptr;

    GeminiPulseGuard(std::uint32_t layer, std::uint32_t expert_pred)
        : layer_index(layer), predicted_expert(expert_pred)
    {
        active_window = Rawr_Execute_Gemini_Pulse(layer_index, predicted_expert);
    }

    ~GeminiPulseGuard()
    {
        // Sync and flip to next window (blocks until SDMA0 completes transfer)
        Rawr_Wait_And_Flip_Window();
    }

    void* GetActiveWindow() const { return active_window; }
};

// ============================================================================
// Telemetry Query Helper (for PowerShell soak test reports)
// ============================================================================
struct SDMATelemetry
{
    std::uint64_t flip_count = 0;
    std::uint64_t wait_cycles = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;

    // Derived metrics
    [[nodiscard]] double CacheHitRate() const noexcept
    {
        const std::uint64_t total = cache_hits + cache_misses;
        return (total > 0) ? (static_cast<double>(cache_hits) / total) : 0.0;
    }

    [[nodiscard]] double AverageWaitCyclesPerFlip() const noexcept
    {
        return (flip_count > 0) ? (static_cast<double>(wait_cycles) / flip_count) : 0.0;
    }

    [[nodiscard]] double AverageWaitMs(double cpu_ghz = 2.4) const noexcept
    {
        const double avg_cycles = AverageWaitCyclesPerFlip();
        return (avg_cycles / (cpu_ghz * 1e6));  // cycles / (GHz * 1e6) = milliseconds
    }

    [[nodiscard]] bool WithinLatencyTarget() const noexcept
    {
        return AverageWaitCyclesPerFlip() < MAX_WAIT_CYCLES_32MS;
    }
};

inline SDMATelemetry QueryTelemetry()
{
    SDMATelemetry t;
    Rawr_Get_SDMA_Telemetry(&t.flip_count, &t.wait_cycles, &t.cache_hits, &t.cache_misses);
    return t;
}

}  // namespace CircularSDMA
}  // namespace RawrXD
