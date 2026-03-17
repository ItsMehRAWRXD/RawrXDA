// ============================================================================
// perf_telemetry.cpp — Centralized Per-Kernel Performance Telemetry
// ============================================================================
// Full implementation of the C++ bridge to RawrXD_PerfCounters.asm.
// Provides percentile computation from histogram buckets, baseline drift
// detection, structured export, and RAII ScopedMeasurement support.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "perf_telemetry.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {
namespace Perf {

// ============================================================================
// ScopedMeasurement — read elapsed without ending
// ============================================================================

uint64_t ScopedMeasurement::elapsed() const {
    uint64_t now;
    // Inline RDTSCP read
    uint32_t aux;
    now = __rdtscp(&aux);
    return now - m_startTSC;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

PerfTelemetry::PerfTelemetry()
    : m_initialized(false)
    , m_baselineCaptured(false)
{
    m_kernelNames.fill(nullptr);
    m_baselineMedians.fill(0.0);
}

PerfTelemetry::~PerfTelemetry() {
    shutdown();
}

// ============================================================================
// Singleton
// ============================================================================

PerfTelemetry& PerfTelemetry::instance() {
    static PerfTelemetry s_instance;
    return s_instance;
}

// ============================================================================
// Lifecycle
// ============================================================================

PerfResult PerfTelemetry::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) return PerfResult::ok("Already initialized");

    asm_perf_init();
    registerDefaultKernels();
    m_initialized = true;

    std::cout << "[PerfTelemetry] Initialized — " << MAX_PERF_SLOTS
              << " measurement slots available" << std::endl;

    return PerfResult::ok("Initialized");
}

PerfResult PerfTelemetry::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return PerfResult::ok("Not initialized");

    m_initialized = false;
    return PerfResult::ok("Shutdown");
}

bool PerfTelemetry::isInitialized() const {
    return m_initialized;
}

// ============================================================================
// Slot Registration
// ============================================================================

void PerfTelemetry::registerKernel(KernelSlot slot, const char* name) {
    registerKernel(static_cast<uint32_t>(slot), name);
}

void PerfTelemetry::registerKernel(uint32_t slotIndex, const char* name) {
    if (slotIndex < MAX_PERF_SLOTS) {
        m_kernelNames[slotIndex] = name;
    }
}

const char* PerfTelemetry::getKernelName(uint32_t slotIndex) const {
    if (slotIndex < MAX_PERF_SLOTS && m_kernelNames[slotIndex]) {
        return m_kernelNames[slotIndex];
    }
    return "(unregistered)";
}

void PerfTelemetry::registerDefaultKernels() {
    // Camellia-256 core
    registerKernel(KernelSlot::Camellia_Init,           "camellia256_init");
    registerKernel(KernelSlot::Camellia_SetKey,          "camellia256_set_key");
    registerKernel(KernelSlot::Camellia_EncryptBlock,    "camellia256_encrypt_block");
    registerKernel(KernelSlot::Camellia_DecryptBlock,    "camellia256_decrypt_block");
    registerKernel(KernelSlot::Camellia_EncryptCTR,      "camellia256_encrypt_ctr");
    registerKernel(KernelSlot::Camellia_DecryptCTR,      "camellia256_decrypt_ctr");
    registerKernel(KernelSlot::Camellia_EncryptFile,     "camellia256_encrypt_file");
    registerKernel(KernelSlot::Camellia_DecryptFile,     "camellia256_decrypt_file");
    registerKernel(KernelSlot::Camellia_SelfTest,        "camellia256_self_test");
    registerKernel(KernelSlot::Camellia_GetHmacKey,      "camellia256_get_hmac_key");

    // Camellia-256 Authenticated
    registerKernel(KernelSlot::CamAuth_EncryptFile,      "cam_auth_encrypt_file");
    registerKernel(KernelSlot::CamAuth_DecryptFile,      "cam_auth_decrypt_file");
    registerKernel(KernelSlot::CamAuth_EncryptBuf,       "cam_auth_encrypt_buf");
    registerKernel(KernelSlot::CamAuth_DecryptBuf,       "cam_auth_decrypt_buf");

    // K-Quant Kernels
    registerKernel(KernelSlot::KQuant_Q4K_AVX2,          "kquant_q4k_avx2");
    registerKernel(KernelSlot::KQuant_Q4K_AVX512,        "kquant_q4k_avx512");
    registerKernel(KernelSlot::KQuant_Q4K_Batch,         "kquant_q4k_batch");
    registerKernel(KernelSlot::KQuant_CpuidCheck,        "kquant_cpuid_check");
    registerKernel(KernelSlot::KQuant_Q4K_Dispatch,      "kquant_q4k_dispatch");
    registerKernel(KernelSlot::KQuant_Q5K_Dispatch,      "kquant_q5k_dispatch");
    registerKernel(KernelSlot::KQuant_Q6K_Dispatch,      "kquant_q6k_dispatch");
    registerKernel(KernelSlot::KQuant_Q2K_Dispatch,      "kquant_q2k_dispatch");
    registerKernel(KernelSlot::KQuant_Q3K_Dispatch,      "kquant_q3k_dispatch");
    registerKernel(KernelSlot::KQuant_F16_Dispatch,      "kquant_f16_dispatch");

    // Watchdog
    registerKernel(KernelSlot::Watchdog_Init,            "watchdog_init");
    registerKernel(KernelSlot::Watchdog_Verify,          "watchdog_verify");
    registerKernel(KernelSlot::Watchdog_GetBaseline,     "watchdog_get_baseline");
    registerKernel(KernelSlot::Watchdog_GetStatus,       "watchdog_get_status");

    // DirectML Bridge
    registerKernel(KernelSlot::DML_MemcpyH2D,           "dml_memcpy_h2d");
    registerKernel(KernelSlot::DML_MemcpyD2H,           "dml_memcpy_d2h");
    registerKernel(KernelSlot::DML_FenceWait,            "dml_fence_wait");
    registerKernel(KernelSlot::DML_VTableDispatch,       "dml_vtable_dispatch");
    registerKernel(KernelSlot::DML_DequantQ4_0,          "dml_dequant_q4_0");
    registerKernel(KernelSlot::DML_DequantQ8_0,          "dml_dequant_q8_0");
    registerKernel(KernelSlot::DML_RopeRotateFP16,       "dml_rope_fp16");
    registerKernel(KernelSlot::DML_RopeRotateFP32,       "dml_rope_fp32");
    registerKernel(KernelSlot::DML_PrefetchTensor,       "dml_prefetch_tensor");
    registerKernel(KernelSlot::DML_ComputeOpHash,        "dml_compute_op_hash");

    // Spillover
    registerKernel(KernelSlot::Spillover_LoadWithSpillover, "spillover_load");
    registerKernel(KernelSlot::Spillover_SpillToHost,       "spillover_spill_to_host");
    registerKernel(KernelSlot::Spillover_UploadFromHost,    "spillover_upload");
    registerKernel(KernelSlot::Spillover_Materialize,       "spillover_materialize");
    registerKernel(KernelSlot::Spillover_ReleaseVRAM,       "spillover_release_vram");
    registerKernel(KernelSlot::Spillover_Prefetch,          "spillover_prefetch");

    // Snapshot
    registerKernel(KernelSlot::Snapshot_Capture,         "snapshot_capture");
    registerKernel(KernelSlot::Snapshot_Restore,         "snapshot_restore");
    registerKernel(KernelSlot::Snapshot_Verify,          "snapshot_verify");
}

// ============================================================================
// Measurement API
// ============================================================================

uint64_t PerfTelemetry::begin(KernelSlot slot) {
    return asm_perf_begin(static_cast<uint32_t>(slot));
}

uint64_t PerfTelemetry::end(KernelSlot slot, uint64_t startTSC) {
    return asm_perf_end(static_cast<uint32_t>(slot), startTSC);
}

// ============================================================================
// Read & Report
// ============================================================================

PerfSlotData PerfTelemetry::readSlotRaw(uint32_t slotIndex) const {
    PerfSlotData data = {};
    if (slotIndex < MAX_PERF_SLOTS) {
        asm_perf_read_slot(slotIndex, &data);
    }
    return data;
}

PerfPercentiles PerfTelemetry::computePercentiles(const PerfSlotData& data) const {
    PerfPercentiles p = {};
    if (data.count == 0) return p;

    // Mean
    p.mean = static_cast<double>(data.totalCycles) / static_cast<double>(data.count);

    // Approximate variance from bucket distribution
    // Each bucket covers a range; use geometric mean of bucket bounds as representative
    double bucketMids[HISTOGRAM_BUCKETS] = {
        128.0,      // [0, 256)
        640.0,      // [256, 1024)
        2560.0,     // [1024, 4096)
        10240.0,    // [4096, 16384)
        40960.0,    // [16384, 65536)
        163840.0,   // [65536, 262144)
        655360.0,   // [262144, 1048576)
        2097152.0   // [1048576, ∞)  — capped estimate
    };

    double sumSqDev = 0.0;
    for (uint32_t b = 0; b < HISTOGRAM_BUCKETS; ++b) {
        if (data.buckets[b] > 0) {
            double dev = bucketMids[b] - p.mean;
            sumSqDev += static_cast<double>(data.buckets[b]) * dev * dev;
        }
    }
    p.stddev = (data.count > 1) ? sqrt(sumSqDev / static_cast<double>(data.count - 1)) : 0.0;

    // Percentile computation from histogram CDF
    // Walk buckets accumulating counts until we reach each percentile threshold
    auto computePercentile = [&](double fraction) -> double {
        uint64_t target = static_cast<uint64_t>(fraction * static_cast<double>(data.count));
        if (target == 0) target = 1;

        uint64_t cumulative = 0;
        for (uint32_t b = 0; b < HISTOGRAM_BUCKETS; ++b) {
            cumulative += data.buckets[b];
            if (cumulative >= target) {
                // Linear interpolation within bucket
                uint64_t prevCumulative = cumulative - data.buckets[b];
                double lowerBound = (b == 0) ? 0.0 : static_cast<double>(BUCKET_BOUNDS[b - 1]);
                double upperBound = static_cast<double>(BUCKET_BOUNDS[b]);
                if (upperBound > 10000000.0) upperBound = bucketMids[b] * 2.0;

                double fraction_within = (data.buckets[b] > 0)
                    ? static_cast<double>(target - prevCumulative) / static_cast<double>(data.buckets[b])
                    : 0.5;

                return lowerBound + fraction_within * (upperBound - lowerBound);
            }
        }
        return bucketMids[HISTOGRAM_BUCKETS - 1];
    };

    p.p50  = computePercentile(0.50);
    p.p90  = computePercentile(0.90);
    p.p95  = computePercentile(0.95);
    p.p99  = computePercentile(0.99);
    p.p999 = computePercentile(0.999);

    return p;
}

PerfSlotReport PerfTelemetry::generateReport(uint32_t slotIndex) const {
    PerfSlotReport r = {};
    if (slotIndex >= MAX_PERF_SLOTS) return r;

    r.kernelName = getKernelName(slotIndex);
    r.slotIndex = slotIndex;

    PerfSlotData data = readSlotRaw(slotIndex);
    r.count = data.count;
    r.minCycles = data.minCycles;
    r.maxCycles = data.maxCycles;
    r.meanCycles = (data.count > 0)
        ? static_cast<double>(data.totalCycles) / static_cast<double>(data.count)
        : 0.0;

    memcpy(r.buckets, data.buckets, sizeof(r.buckets));

    r.percentiles = computePercentiles(data);

    // Drift detection
    if (m_baselineCaptured && m_baselineMedians[slotIndex] > 0.0) {
        r.driftRatio = r.percentiles.p50 / m_baselineMedians[slotIndex];
        r.driftDetected = (r.driftRatio > 2.0 || r.driftRatio < 0.5);
    } else {
        r.driftRatio = 1.0;
        r.driftDetected = false;
    }

    return r;
}

// ============================================================================
// Bulk Operations
// ============================================================================

void PerfTelemetry::resetAllSlots() {
    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        asm_perf_reset_slot(i);
    }
}

void PerfTelemetry::resetSlot(uint32_t slotIndex) {
    if (slotIndex < MAX_PERF_SLOTS) {
        asm_perf_reset_slot(slotIndex);
    }
}

// ============================================================================
// Baseline & Drift Detection
// ============================================================================

void PerfTelemetry::captureBaseline() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        PerfSlotData data = readSlotRaw(i);
        if (data.count > 0) {
            PerfPercentiles p = computePercentiles(data);
            m_baselineMedians[i] = p.p50;
        } else {
            m_baselineMedians[i] = 0.0;
        }
    }
    m_baselineCaptured = true;

    std::cout << "[PerfTelemetry] Baseline captured across " << getActiveSlotCount()
              << " active slots" << std::endl;
}

bool PerfTelemetry::checkDrift(uint32_t slotIndex, double threshold) const {
    if (!m_baselineCaptured || slotIndex >= MAX_PERF_SLOTS) return false;
    if (m_baselineMedians[slotIndex] <= 0.0) return false;

    PerfSlotData data = readSlotRaw(slotIndex);
    if (data.count == 0) return false;

    PerfPercentiles p = computePercentiles(data);
    double ratio = p.p50 / m_baselineMedians[slotIndex];
    return (ratio > threshold || ratio < 1.0 / threshold);
}

double PerfTelemetry::getDriftRatio(uint32_t slotIndex) const {
    if (!m_baselineCaptured || slotIndex >= MAX_PERF_SLOTS) return 1.0;
    if (m_baselineMedians[slotIndex] <= 0.0) return 1.0;

    PerfSlotData data = readSlotRaw(slotIndex);
    if (data.count == 0) return 1.0;

    PerfPercentiles p = computePercentiles(data);
    return p.p50 / m_baselineMedians[slotIndex];
}

uint32_t PerfTelemetry::getActiveSlotCount() const {
    uint32_t active = 0;
    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        PerfSlotData data = readSlotRaw(i);
        if (data.count > 0) active++;
    }
    return active;
}

// ============================================================================
// Export — JSON
// ============================================================================

std::string PerfTelemetry::exportJSON() const {
    std::ostringstream ss;
    ss << "{\n  \"perf_telemetry\": {\n";
    ss << "    \"active_slots\": " << getActiveSlotCount() << ",\n";
    ss << "    \"baseline_captured\": " << (m_baselineCaptured ? "true" : "false") << ",\n";
    ss << "    \"kernels\": [\n";

    bool first = true;
    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        PerfSlotData data = readSlotRaw(i);
        if (data.count == 0) continue;

        if (!first) ss << ",\n";
        first = false;

        PerfSlotReport r = generateReport(i);

        ss << "      {\n";
        ss << "        \"slot\": " << i << ",\n";
        ss << "        \"name\": \"" << r.kernelName << "\",\n";
        ss << "        \"count\": " << r.count << ",\n";
        ss << "        \"min_cycles\": " << r.minCycles << ",\n";
        ss << "        \"max_cycles\": " << r.maxCycles << ",\n";
        ss << "        \"mean_cycles\": " << std::fixed << std::setprecision(1) << r.meanCycles << ",\n";
        ss << "        \"p50\": " << r.percentiles.p50 << ",\n";
        ss << "        \"p90\": " << r.percentiles.p90 << ",\n";
        ss << "        \"p95\": " << r.percentiles.p95 << ",\n";
        ss << "        \"p99\": " << r.percentiles.p99 << ",\n";
        ss << "        \"p999\": " << r.percentiles.p999 << ",\n";
        ss << "        \"stddev\": " << r.percentiles.stddev << ",\n";
        ss << "        \"drift_ratio\": " << r.driftRatio << ",\n";
        ss << "        \"drift_detected\": " << (r.driftDetected ? "true" : "false") << ",\n";
        ss << "        \"histogram\": [";
        for (uint32_t b = 0; b < HISTOGRAM_BUCKETS; ++b) {
            if (b > 0) ss << ", ";
            ss << r.buckets[b];
        }
        ss << "]\n";
        ss << "      }";
    }

    ss << "\n    ]\n  }\n}\n";
    return ss.str();
}

// ============================================================================
// Export — CSV
// ============================================================================

std::string PerfTelemetry::exportCSV() const {
    std::ostringstream ss;
    ss << "slot,name,count,min,max,mean,p50,p90,p95,p99,p999,stddev,drift_ratio,drift";
    for (uint32_t b = 0; b < HISTOGRAM_BUCKETS; ++b) {
        ss << ",bucket_" << b;
    }
    ss << "\n";

    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        PerfSlotData data = readSlotRaw(i);
        if (data.count == 0) continue;

        PerfSlotReport r = generateReport(i);

        ss << i << ","
           << r.kernelName << ","
           << r.count << ","
           << r.minCycles << ","
           << r.maxCycles << ","
           << std::fixed << std::setprecision(1)
           << r.meanCycles << ","
           << r.percentiles.p50 << ","
           << r.percentiles.p90 << ","
           << r.percentiles.p95 << ","
           << r.percentiles.p99 << ","
           << r.percentiles.p999 << ","
           << r.percentiles.stddev << ","
           << r.driftRatio << ","
           << (r.driftDetected ? "YES" : "no");

        for (uint32_t b = 0; b < HISTOGRAM_BUCKETS; ++b) {
            ss << "," << r.buckets[b];
        }
        ss << "\n";
    }

    return ss.str();
}

// ============================================================================
// Diagnostics
// ============================================================================

std::string PerfTelemetry::getDiagnostics() const {
    std::ostringstream ss;
    ss << "=== PerfTelemetry Diagnostics ===\n";
    ss << "Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    ss << "Active Slots: " << getActiveSlotCount() << "/" << MAX_PERF_SLOTS << "\n";
    ss << "Baseline: " << (m_baselineCaptured ? "Captured" : "Not captured") << "\n\n";

    ss << std::left << std::setw(32) << "Kernel"
       << std::right
       << std::setw(10) << "Count"
       << std::setw(12) << "Min"
       << std::setw(12) << "P50"
       << std::setw(12) << "P90"
       << std::setw(12) << "P99"
       << std::setw(12) << "Max"
       << std::setw(8) << "Drift"
       << "\n";
    ss << std::string(110, '-') << "\n";

    for (uint32_t i = 0; i < MAX_PERF_SLOTS; ++i) {
        PerfSlotData data = readSlotRaw(i);
        if (data.count == 0) continue;

        PerfSlotReport r = generateReport(i);

        ss << std::left << std::setw(32) << r.kernelName
           << std::right
           << std::setw(10) << r.count
           << std::setw(12) << r.minCycles
           << std::setw(12) << static_cast<uint64_t>(r.percentiles.p50)
           << std::setw(12) << static_cast<uint64_t>(r.percentiles.p90)
           << std::setw(12) << static_cast<uint64_t>(r.percentiles.p99)
           << std::setw(12) << r.maxCycles;

        if (r.driftDetected) {
            ss << std::setw(8) << std::fixed << std::setprecision(2)
               << r.driftRatio << "x!";
        } else {
            ss << std::setw(8) << "-";
        }
        ss << "\n";
    }

    return ss.str();
}

} // namespace Perf
} // namespace RawrXD
