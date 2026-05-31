// ============================================================================
// credit_governor.hpp - Auto-Tuning Credit Threshold Governor
// ============================================================================
// Dynamically adjusts CreditConfig.minCredits based on real-time throughput
// telemetry to maintain optimal pipeline pressure without deadlock.
//
// Control strategy: PID-like with exponential smoothing
//   - P: React to throughput deviation from target
//   - I: Accumulate sustained pressure to shift baseline
//   - D: Dampen oscillation from burst traffic
// ============================================================================

#pragma once

#include "flow_control/credit_based_flow_control.hpp"
#include <cstdint>
#include <chrono>

namespace RawrXD {
namespace FlowControl {

struct GovernorTelemetry {
    uint64_t timestampMs = 0;
    double throughputElemPerSec = 0.0;   // From FP8 quantizer
    double tokenRate = 0.0;              // From stream callback
    uint32_t blockedCount = 0;
    uint32_t successCount = 0;
    uint32_t availableCredits = 0;
    uint32_t pendingElements = 0;
};

struct GovernorConfig {
    // Target throughput (elements/sec) — governor maintains this
    double targetThroughput = 12.0e9;    // 12B elem/s baseline from AVX-512 test
    
    // PID coefficients
    double kp = 0.3;                     // Proportional: react to deviation
    double ki = 0.05;                    // Integral: correct sustained drift
    double kd = 0.1;                     // Derivative: dampen oscillation
    
    // Bounds for minCredits (safety clamps)
    uint32_t minCreditsFloor = 10;       // Never go below this
    uint32_t minCreditsCeiling = 500;    // Never go above this
    
    // Smoothing factor for exponential moving average (0.0-1.0)
    double emaAlpha = 0.3;
    
    // Update interval (ms)
    uint32_t updateIntervalMs = 100;
    
    // Burst tolerance: allow temporary overshoot before throttling
    double burstTolerance = 1.2;         // 120% of target
    
    // Silent mode: suppress diagnostic printf output (for TUI/dashboard use)
    bool silent = false;
};

// ============================================================================
// Credit Governor — auto-tunes thresholds based on live telemetry
// ============================================================================
class CreditGovernor {
public:
    CreditGovernor();
    ~CreditGovernor();
    
    // Initialize with base config and governor parameters
    bool Initialize(const CreditConfig& baseConfig, const GovernorConfig& govConfig);
    void Shutdown();
    
    // Feed telemetry from pipeline stages (thread-safe)
    void RecordTelemetry(const GovernorTelemetry& telemetry);
    
    // Get current (possibly auto-tuned) credit config
    CreditConfig GetCurrentConfig() const;
    
    // Force a manual override (disables auto-tuning until Reset)
    void OverrideMinCredits(uint32_t minCredits);
    void ResetOverride();
    
    // Governor status
    bool IsAutoTuning() const { return autoTuningEnabled_ && !overrideActive_; }
    
    // Get last computed metrics
    double GetLastError() const { return lastError_; }
    double GetLastAdjustment() const { return lastAdjustment_; }
    
    // Print tuning report
    void PrintReport() const;
    
private:
    bool initialized_ = false;
    bool autoTuningEnabled_ = true;
    bool overrideActive_ = false;
    
    CreditConfig baseConfig_;
    CreditConfig currentConfig_;
    GovernorConfig govConfig_;
    
    // PID state
    double integralError_ = 0.0;
    double lastError_ = 0.0;
    double lastAdjustment_ = 0.0;
    double smoothedThroughput_ = 0.0;
    
    // Timing
    uint64_t lastUpdateMs_ = 0;
    
    // Statistics
    uint64_t totalUpdates_ = 0;
    uint64_t totalTelemetrySamples_ = 0;
    
    void UpdateGovernor(uint64_t nowMs);
    uint64_t GetCurrentTimeMs() const;
};

} // namespace FlowControl
} // namespace RawrXD
