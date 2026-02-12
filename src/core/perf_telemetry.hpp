// ============================================================================
// perf_telemetry.hpp — Centralized Per-Kernel Performance Telemetry
// ============================================================================
//
// PURPOSE:
//   C++ bridge to the MASM PerfCounters kernel (RawrXD_PerfCounters.asm).
//   Provides RDTSC-based cycle-accurate latency histograms for every MASM
//   kernel entrypoint, with percentile computation, drift detection, and
//   structured export (JSON / CSV / debug log).
//
// DESIGN:
//   - 64 measurement slots mapped to named kernel functions
//   - ScopedMeasurement RAII helper for automatic begin/end
//   - Percentile engine (P50/P90/P99/P999) from histogram buckets
//   - Drift detector: alerts when median latency shifts > 2× baseline
//   - Thread-safe: MASM kernel uses lock xadd, C++ reads are snapshot-based
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <mutex>

// ============================================================================
//  MASM extern declarations (RawrXD_PerfCounters.asm)
// ============================================================================

extern "C" {
    int      asm_perf_init();
    uint64_t asm_perf_begin(uint32_t slotIndex);
    uint64_t asm_perf_end(uint32_t slotIndex, uint64_t startTSC);
    int      asm_perf_read_slot(uint32_t slotIndex, void* buffer128);
    int      asm_perf_reset_slot(uint32_t slotIndex);
    uint32_t asm_perf_get_slot_count();
    void*    asm_perf_get_table_base();
}

namespace RawrXD {
namespace Perf {

// ============================================================================
//  Constants
// ============================================================================

static constexpr uint32_t MAX_PERF_SLOTS = 64;
static constexpr uint32_t HISTOGRAM_BUCKETS = 8;

// Well-known slot assignments for kernel entrypoints
// These must be used consistently by the stress harness & production code
enum class KernelSlot : uint32_t {
    // Camellia-256 core
    Camellia_Init               = 0,
    Camellia_SetKey             = 1,
    Camellia_EncryptBlock       = 2,
    Camellia_DecryptBlock       = 3,
    Camellia_EncryptCTR         = 4,
    Camellia_DecryptCTR         = 5,
    Camellia_EncryptFile        = 6,
    Camellia_DecryptFile        = 7,
    Camellia_SelfTest           = 8,
    Camellia_GetHmacKey         = 9,

    // Camellia-256 Authenticated
    CamAuth_EncryptFile         = 10,
    CamAuth_DecryptFile         = 11,
    CamAuth_EncryptBuf          = 12,
    CamAuth_DecryptBuf          = 13,

    // K-Quant Kernels
    KQuant_Q4K_AVX2             = 14,
    KQuant_Q4K_AVX512           = 15,
    KQuant_Q4K_Batch            = 16,
    KQuant_CpuidCheck           = 17,
    KQuant_Q4K_Dispatch         = 18,
    KQuant_Q5K_Dispatch         = 19,
    KQuant_Q6K_Dispatch         = 20,
    KQuant_Q2K_Dispatch         = 21,
    KQuant_Q3K_Dispatch         = 22,
    KQuant_F16_Dispatch         = 23,

    // Watchdog
    Watchdog_Init               = 24,
    Watchdog_Verify             = 25,
    Watchdog_GetBaseline        = 26,
    Watchdog_GetStatus          = 27,

    // DirectML Bridge
    DML_MemcpyH2D               = 28,
    DML_MemcpyD2H               = 29,
    DML_FenceWait                = 30,
    DML_VTableDispatch           = 31,
    DML_DequantQ4_0              = 32,
    DML_DequantQ8_0              = 33,
    DML_RopeRotateFP16           = 34,
    DML_RopeRotateFP32           = 35,
    DML_PrefetchTensor           = 36,
    DML_ComputeOpHash            = 37,

    // Spillover
    Spillover_LoadWithSpillover  = 38,
    Spillover_SpillToHost        = 39,
    Spillover_UploadFromHost     = 40,
    Spillover_Materialize        = 41,
    Spillover_ReleaseVRAM        = 42,
    Spillover_Prefetch           = 43,

    // Snapshot
    Snapshot_Capture             = 44,
    Snapshot_Restore             = 45,
    Snapshot_Verify              = 46,

    // Reserved for future kernels
    Reserved_47                  = 47,
    Reserved_48                  = 48,

    // User-defined slots (49-63)
    UserSlot_0                   = 49,
    UserSlot_Max                 = 63,
};

// ============================================================================
//  Raw slot data (matches MASM layout, 128 bytes)
// ============================================================================

struct alignas(64) PerfSlotData {
    uint64_t count;
    uint64_t totalCycles;
    uint64_t minCycles;
    uint64_t maxCycles;
    uint64_t buckets[HISTOGRAM_BUCKETS];
    uint64_t lastCycles;
    uint32_t flags;
    uint32_t reserved;
    uint64_t reserved2;
    uint64_t reserved3;
};

static_assert(sizeof(PerfSlotData) == 128, "PerfSlotData must be 128 bytes");

// ============================================================================
//  Computed statistics (from histogram)
// ============================================================================

struct PerfPercentiles {
    double p50;
    double p90;
    double p95;
    double p99;
    double p999;
    double mean;
    double stddev;
};

struct PerfSlotReport {
    const char*    kernelName;
    uint32_t       slotIndex;
    uint64_t       count;
    uint64_t       minCycles;
    uint64_t       maxCycles;
    double         meanCycles;
    PerfPercentiles percentiles;
    uint64_t       buckets[HISTOGRAM_BUCKETS];
    bool           driftDetected;     // true if median shifted > 2× from baseline
    double         driftRatio;        // current_median / baseline_median
};

// ============================================================================
//  PatchResult-compatible result type
// ============================================================================

struct PerfResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static PerfResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static PerfResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
//  ScopedMeasurement — RAII timing helper
// ============================================================================
//  Usage:
//    {
//        Perf::ScopedMeasurement m(KernelSlot::Camellia_EncryptBlock);
//        asm_camellia256_encrypt_block(in, out);
//    }  // delta automatically recorded on scope exit
// ============================================================================

class ScopedMeasurement {
public:
    explicit ScopedMeasurement(KernelSlot slot)
        : m_slot(static_cast<uint32_t>(slot))
        , m_startTSC(asm_perf_begin(m_slot))
    {}

    explicit ScopedMeasurement(uint32_t slot)
        : m_slot(slot)
        , m_startTSC(asm_perf_begin(slot))
    {}

    ~ScopedMeasurement() {
        asm_perf_end(m_slot, m_startTSC);
    }

    // Non-copyable
    ScopedMeasurement(const ScopedMeasurement&) = delete;
    ScopedMeasurement& operator=(const ScopedMeasurement&) = delete;

    uint64_t elapsed() const;   // Read TSC delta so far (without ending)

private:
    uint32_t m_slot;
    uint64_t m_startTSC;
};

// ============================================================================
//  PerfTelemetry — Main telemetry engine
// ============================================================================

class PerfTelemetry {
public:
    static PerfTelemetry& instance();

    // ---- Lifecycle ----
    PerfResult initialize();
    PerfResult shutdown();
    bool isInitialized() const;

    // ---- Slot Registration ----
    // Associate a human-readable name with a slot index
    void registerKernel(KernelSlot slot, const char* name);
    void registerKernel(uint32_t slotIndex, const char* name);
    const char* getKernelName(uint32_t slotIndex) const;

    // ---- Measurement (alternative to ScopedMeasurement) ----
    uint64_t begin(KernelSlot slot);
    uint64_t end(KernelSlot slot, uint64_t startTSC);

    // ---- Read & Report ----
    PerfSlotData readSlotRaw(uint32_t slotIndex) const;
    PerfPercentiles computePercentiles(const PerfSlotData& data) const;
    PerfSlotReport generateReport(uint32_t slotIndex) const;

    // ---- Bulk Operations ----
    void resetAllSlots();
    void resetSlot(uint32_t slotIndex);

    // ---- Baseline & Drift Detection ----
    void captureBaseline();     // Snapshot current medians as baseline
    bool checkDrift(uint32_t slotIndex, double threshold = 2.0) const;
    double getDriftRatio(uint32_t slotIndex) const;

    // ---- Export ----
    std::string exportJSON() const;
    std::string exportCSV() const;
    std::string getDiagnostics() const;

    // ---- Bulk report for all active slots ----
    uint32_t getActiveSlotCount() const;

private:
    PerfTelemetry();
    ~PerfTelemetry();
    PerfTelemetry(const PerfTelemetry&) = delete;
    PerfTelemetry& operator=(const PerfTelemetry&) = delete;

    // Auto-register all well-known kernel names
    void registerDefaultKernels();

    // ---- State ----
    bool m_initialized;
    mutable std::mutex m_mutex;

    // Slot names
    std::array<const char*, MAX_PERF_SLOTS> m_kernelNames;

    // Baseline medians (for drift detection)
    std::array<double, MAX_PERF_SLOTS> m_baselineMedians;
    bool m_baselineCaptured;

    // Histogram bucket upper bounds (for percentile interpolation)
    static constexpr uint64_t BUCKET_BOUNDS[HISTOGRAM_BUCKETS] = {
        256, 1024, 4096, 16384, 65536, 262144, 1048576, UINT64_MAX
    };
};

} // namespace Perf
} // namespace RawrXD
