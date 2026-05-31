// ============================================================================
// RawrXD Unified Overclock/Underclock Governor v2
// Zero-dependency hardware frequency control system
// Supports: CPU, GPU, RAM, NVMe/HDD, with auto-tune toggle
// Bridges existing OverclockGovernor + OverclockVendor + ThermalGovernor
// ============================================================================
// SCAFFOLD_226: ASM build and ml64/nasm
// SCAFFOLD_196: Toolchain (nasm, masm) and ASM run
// Pure x64 MASM implementation: src/asm/RawrXD_UnifiedOverclockGovernor.asm
// ============================================================================
#pragma once
#ifndef RAWRXD_UNIFIED_OVERCLOCK_H
#define RAWRXD_UNIFIED_OVERCLOCK_H

#include <cstdint>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <array>
#include <functional>
#include <chrono>

struct AppState;

extern "C" {
    __int64 OverclockGov_Initialize(void* appState);
    __int64 OverclockGov_Shutdown();
    __int64 OverclockGov_IsRunning();
    __int64 OverclockGov_ApplyOffset(uint32_t domain, int32_t offsetMhz);
    __int64 OverclockGov_ApplyCpuOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyGpuOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyMemoryOffset(int32_t offsetMhz);
    __int64 OverclockGov_ApplyStorageOffset(int32_t offsetMhz);
    __int64 OverclockGov_ReadTemperature(uint32_t domain);
    __int64 OverclockGov_ReadFrequency(uint32_t domain);
    __int64 OverclockGov_ReadPowerDraw(uint32_t domain);
    __int64 OverclockGov_ReadUtilization(uint32_t domain);
    __int64 OverclockGov_EmergencyThrottleAll();
    __int64 OverclockGov_ResetAllToBaseline();
}

namespace RawrXD {

// ============================================================================
// Result Pattern (shared)
// ============================================================================
struct ClockResult {
    bool success;
    const char* detail;
    int errorCode;

    static ClockResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static ClockResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// Hardware Domain Enums
// ============================================================================
enum class HardwareDomain : uint32_t {
    CPU       = 0,
    GPU       = 1,
    Memory    = 2,  // RAM frequency / timings
    Storage   = 3,  // NVMe/SATA queue depth, power state
    COUNT     = 4
};

const char* HardwareDomainToString(HardwareDomain d);

// ============================================================================
// Clock Direction
// ============================================================================
enum class ClockDirection : uint8_t {
    Overclock  = 0,
    Underclock = 1,
    Stock      = 2   // Reset to baseline
};

// ============================================================================
// Auto-Tune Strategy
// ============================================================================
enum class AutoTuneStrategy : uint8_t {
    Disabled       = 0,
    Conservative   = 1,   // Small steps, wide deadband, safety-first
    Balanced       = 2,   // Medium steps, moderate deadband
    Aggressive     = 3,   // Large steps, tight deadband, perf-first
    AdaptiveML     = 4    // Adaptive PID gain retuning from history
};

// ============================================================================
// Per-Domain Clock Profile
// ============================================================================
struct ClockProfile {
    HardwareDomain domain;
    ClockDirection direction;
    int32_t        offsetMhz;         // Signed: positive = OC, negative = UC
    int32_t        minFreqMhz;        // Floor clamp
    int32_t        maxFreqMhz;        // Ceiling clamp
    float          targetTempC;       // Thermal target
    float          criticalTempC;     // Emergency throttle threshold
    float          hysteresisC;       // Deadband width
    bool           autoTuneEnabled;
    AutoTuneStrategy autoTuneStrategy;
};

// ============================================================================
// PID Controller State (per domain)
// ============================================================================
struct PIDState {
    float kp = 0.5f;      // Proportional gain
    float ki = 0.05f;     // Integral gain
    float kd = 0.1f;      // Derivative gain
    float integral = 0.0f;
    float prevError = 0.0f;
    float output = 0.0f;
    int32_t consecutiveFaults = 0;
    int32_t totalFaults = 0;
    std::chrono::steady_clock::time_point lastFaultTime;
};

// ============================================================================
// Domain Telemetry
// ============================================================================
struct DomainTelemetry {
    HardwareDomain domain;
    float currentTempC;
    float currentFreqMhz;
    float baselineFreqMhz;
    int32_t appliedOffsetMhz;
    float powerDrawWatts;
    float utilizationPct;
    float efficiencyScore;      // ops/watt normalized
    uint64_t adjustmentCount;
    uint64_t faultCount;
    bool autoTuneActive;
    AutoTuneStrategy activeStrategy;
    PIDState pidSnapshot;
};

// ============================================================================
// Aggregate System Telemetry
// ============================================================================
struct SystemClockTelemetry {
    std::array<DomainTelemetry, static_cast<size_t>(HardwareDomain::COUNT)> domains;
    float totalPowerDrawWatts;
    float totalThermalEnvelopeC;
    bool  anyFaulted;
    bool  anyAutoTuning;
    uint64_t totalAdjustments;
    std::chrono::steady_clock::time_point timestamp;
};

// ============================================================================
// Unified Overclock Governor v2
// Bridges: OverclockGovernor, OverclockVendor, ThermalGovernor, GPUAutoTuner
// ============================================================================
class UnifiedOverclockGovernor {
public:
    static UnifiedOverclockGovernor& Instance();

    // Lifecycle
    ClockResult initialize(AppState* state = nullptr);
    ClockResult shutdown();
    bool isRunning() const;

    // Per-domain control
    ClockResult setProfile(HardwareDomain domain, const ClockProfile& profile);
    ClockResult applyOffset(HardwareDomain domain, int32_t offsetMhz);
    ClockResult resetToBaseline(HardwareDomain domain);
    ClockResult resetAllToBaseline();

    // Auto-tune toggle (per-domain)
    ClockResult enableAutoTune(HardwareDomain domain, AutoTuneStrategy strategy);
    ClockResult disableAutoTune(HardwareDomain domain);
    bool isAutoTuneEnabled(HardwareDomain domain) const;
    AutoTuneStrategy getAutoTuneStrategy(HardwareDomain domain) const;

    // Global auto-tune toggle
    ClockResult enableAutoTuneAll(AutoTuneStrategy strategy);
    ClockResult disableAutoTuneAll();

    // Direction control
    ClockResult overclock(HardwareDomain domain, int32_t stepMhz);
    ClockResult underclock(HardwareDomain domain, int32_t stepMhz);

    // Telemetry
    DomainTelemetry getDomainTelemetry(HardwareDomain domain) const;
    SystemClockTelemetry getSystemTelemetry() const;

    // Safety
    ClockResult emergencyThrottleAll();
    bool isEmergencyActive() const;
    ClockResult clearEmergency();

    // Session persistence
    ClockResult saveSession(const std::string& path);
    ClockResult loadSession(const std::string& path);

    // CLI interface
    ClockResult dispatchCLI(const std::string& command, const std::string& args);

private:
    UnifiedOverclockGovernor();
    ~UnifiedOverclockGovernor();
    UnifiedOverclockGovernor(const UnifiedOverclockGovernor&) = delete;
    UnifiedOverclockGovernor& operator=(const UnifiedOverclockGovernor&) = delete;

    // Internal control loop
    void controlLoop();
    void pidUpdate(HardwareDomain domain);
    void autoTuneStep(HardwareDomain domain);

    // Platform-specific apply
    ClockResult applyCpuOffset(int32_t offsetMhz);
    ClockResult applyGpuOffset(int32_t offsetMhz);
    ClockResult applyMemoryOffset(int32_t offsetMhz);
    ClockResult applyStorageOffset(int32_t offsetMhz);

    // Platform-specific read
    float readTemperature(HardwareDomain domain);
    float readFrequency(HardwareDomain domain);
    float readPowerDraw(HardwareDomain domain);
    float readUtilization(HardwareDomain domain);

    // Safety checks
    bool checkThermalSafety(HardwareDomain domain);
    void recordFault(HardwareDomain domain);
    bool shouldRollback(HardwareDomain domain) const;

    // State
    mutable std::mutex mutex_;
    std::thread controlThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> emergency_{false};
    AppState* appState_ = nullptr;

    std::array<ClockProfile, static_cast<size_t>(HardwareDomain::COUNT)> profiles_;
    std::array<PIDState, static_cast<size_t>(HardwareDomain::COUNT)> pidStates_;
    std::array<DomainTelemetry, static_cast<size_t>(HardwareDomain::COUNT)> telemetry_;
    std::array<float, static_cast<size_t>(HardwareDomain::COUNT)> baselines_;

    // Auto-tune history (sliding window for adaptive ML)
    struct AutoTuneHistoryEntry {
        float tempC;
        float freqMhz;
        float powerW;
        float utilPct;
        int32_t offsetApplied;
        float resultingEfficiency;
    };
    static constexpr size_t AUTO_TUNE_HISTORY_SIZE = 256;
    std::array<std::array<AutoTuneHistoryEntry, AUTO_TUNE_HISTORY_SIZE>,
               static_cast<size_t>(HardwareDomain::COUNT)> autoTuneHistory_;
    std::array<size_t, static_cast<size_t>(HardwareDomain::COUNT)> historyHead_{};
};

} // namespace RawrXD
#endif // RAWRXD_UNIFIED_OVERCLOCK_H
