// kv_cache_credit_bridge.hpp
// Bridges KV-cache memory pressure into the credit-based flow control system.
// When KV-cache grows (e.g., during 70B model prefill), credits are throttled
// to prevent OOM while maintaining throughput for critical paths.
//
// Integration:
//   - MemoryPressureHandler: system RAM monitoring
//   - CreditCounter: token budget management
//   - UnifiedOverclockGovernor: thermal/frequency coordination
//   - ExecutionGovernor: task scheduling
//
// Compile with: -I d:\rawrxd\include -I d:\rawrxd\src\core

#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include "flow_control/credit_based_flow_control.hpp"

namespace RawrXD {
namespace Memory {
    struct MemoryStats;
    enum class PressureLevel : uint8_t;
}
}

namespace Rawrxd {
namespace KVCache {

using namespace RawrXD::FlowControl;
using namespace RawrXD::Memory;

// ============================================================================
// KV-Cache Pressure Configuration
// ============================================================================
struct KVCachePressureConfig {
    // Pressure thresholds (bytes of KV-cache allocated)
    uint64_t thresholdLow      = 2ULL * 1024 * 1024 * 1024;   // 2GB
    uint64_t thresholdMedium   = 4ULL * 1024 * 1024 * 1024;   // 4GB
    uint64_t thresholdHigh     = 6ULL * 1024 * 1024 * 1024;   // 6GB
    uint64_t thresholdCritical = 8ULL * 1024 * 1024 * 1024;   // 8GB

    // Credit reduction factors (applied to minCredits)
    float reductionLow      = 0.90f;  // 90% of normal
    float reductionMedium  = 0.70f;  // 70% of normal
    float reductionHigh    = 0.40f;  // 40% of normal
    float reductionCritical = 0.10f; // 10% of normal (near-halt)

    // Hysteresis to prevent oscillation
    float hysteresisBytes = 256ULL * 1024 * 1024; // 256MB

    // Polling interval
    uint32_t pollIntervalMs = 100;

    // Enable adaptive PID tuning of credit reduction
    bool adaptivePidEnabled = true;
    float pidKp = 0.5f;
    float pidKi = 0.05f;
    float pidKd = 0.1f;
};

// ============================================================================
// KV-Cache Credit Bridge
// ============================================================================
class KVCacheCreditBridge {
public:
    static KVCacheCreditBridge& Instance();

    // Initialize with credit counter to control
    bool Initialize(CreditCounter* creditCounter, const KVCachePressureConfig& config = {});

    // Shutdown monitoring thread
    void Shutdown();

    // Report KV-cache allocation (called by inference engine)
    void ReportKVCacheUsage(uint64_t bytesAllocated, uint64_t bytesReserved);

    // Report system memory pressure (called by MemoryPressureHandler)
    void ReportSystemPressure(const MemoryStats& stats);

    // Get current credit reduction factor (0.0-1.0)
    float GetCurrentReductionFactor() const;

    // Get current pressure level
    PressureLevel GetCurrentPressureLevel() const;

    // Telemetry
    struct Telemetry {
        uint64_t kvCacheBytesAllocated;
        uint64_t kvCacheBytesReserved;
        float reductionFactor;
        PressureLevel pressureLevel;
        uint64_t creditThrottlesApplied;
        uint64_t creditThrottlesReleased;
        std::chrono::steady_clock::time_point lastUpdate;
    };
    Telemetry GetTelemetry() const;

    // Manual override (for testing/emergency)
    void SetManualReduction(float factor); // 0.0-1.0, or -1.0 to disable

private:
    KVCacheCreditBridge() = default;
    ~KVCacheCreditBridge() { Shutdown(); }

    void MonitoringLoop();
    void ApplyPressure();
    void ReleasePressure();

    CreditCounter* creditCounter_{nullptr};
    KVCachePressureConfig config_;

    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    std::thread monitorThread_;

    // State
    std::atomic<uint64_t> kvCacheAllocated_{0};
    std::atomic<uint64_t> kvCacheReserved_{0};
    std::atomic<float> currentReduction_{1.0f};
    std::atomic<PressureLevel> currentPressure_{static_cast<PressureLevel>(0)};
    std::atomic<float> manualOverride_{-1.0f};

    // PID state for adaptive tuning
    struct {
        float integral = 0.0f;
        float prevError = 0.0f;
        std::chrono::steady_clock::time_point lastTime;
    } pidState_;

    // Telemetry counters
    std::atomic<uint64_t> throttlesApplied_{0};
    std::atomic<uint64_t> throttlesReleased_{0};
    std::atomic<std::chrono::steady_clock::time_point> lastUpdate_;

    mutable std::mutex telemetryMutex_;
};

} // namespace KVCache
} // namespace Rawrxd
