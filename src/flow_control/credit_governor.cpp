// ============================================================================
// credit_governor.cpp - Auto-Tuning Credit Threshold Implementation
// ============================================================================

#include "flow_control/credit_governor.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace RawrXD {
namespace FlowControl {

CreditGovernor::CreditGovernor() = default;
CreditGovernor::~CreditGovernor() = default;

bool CreditGovernor::Initialize(const CreditConfig& baseConfig, const GovernorConfig& govConfig) {
    baseConfig_ = baseConfig;
    currentConfig_ = baseConfig;
    govConfig_ = govConfig;
    
    integralError_ = 0.0;
    lastError_ = 0.0;
    lastAdjustment_ = 0.0;
    smoothedThroughput_ = govConfig.targetThroughput;
    lastUpdateMs_ = GetCurrentTimeMs();
    totalUpdates_ = 0;
    totalTelemetrySamples_ = 0;
    
    initialized_ = true;
    autoTuningEnabled_ = true;
    overrideActive_ = false;
    
    if (!govConfig_.silent) {
        printf("[CreditGovernor] Initialized\n");
        printf("  Target throughput: %.2f B elem/s\n", govConfig.targetThroughput / 1e9);
        printf("  Base minCredits: %u\n", baseConfig.minCredits);
        printf("  PID: Kp=%.2f Ki=%.2f Kd=%.2f\n", govConfig.kp, govConfig.ki, govConfig.kd);
        printf("  Bounds: [%u, %u]\n", govConfig.minCreditsFloor, govConfig.minCreditsCeiling);
    }
    
    return true;
}

void CreditGovernor::Shutdown() {
    if (initialized_) {
        PrintReport();
        initialized_ = false;
    }
}

void CreditGovernor::RecordTelemetry(const GovernorTelemetry& telemetry) {
    if (!initialized_) return;
    
    totalTelemetrySamples_++;
    
    // Exponential moving average of throughput
    smoothedThroughput_ = govConfig_.emaAlpha * telemetry.throughputElemPerSec +
                        (1.0 - govConfig_.emaAlpha) * smoothedThroughput_;
    
    // Use telemetry timestamp if provided (>0), otherwise wall-clock
    uint64_t nowMs = telemetry.timestampMs > 0 ? telemetry.timestampMs : GetCurrentTimeMs();
    
    // If this is the first telemetry with a valid timestamp, reset baseline
    if (telemetry.timestampMs > 0 && totalTelemetrySamples_ == 1) {
        lastUpdateMs_ = nowMs;
    }
    
    // Ensure monotonic comparison (handle first call or clock resets)
    if (nowMs >= lastUpdateMs_ && (nowMs - lastUpdateMs_) >= govConfig_.updateIntervalMs) {
        UpdateGovernor(nowMs);
    }
}

void CreditGovernor::UpdateGovernor(uint64_t nowMs) {
    if (!autoTuningEnabled_ || overrideActive_) return;
    
    // Calculate error (negative = below target, positive = above target)
    double error = (smoothedThroughput_ - govConfig_.targetThroughput) / govConfig_.targetThroughput;
    
    // PID computation
    integralError_ += error;
    integralError_ = std::clamp(integralError_, -10.0, 10.0);  // Anti-windup
    
    double derivative = error - lastError_;
    
    double adjustment = govConfig_.kp * error +
                        govConfig_.ki * integralError_ +
                        govConfig_.kd * derivative;
    
    // Convert adjustment to credit delta
    // Positive error (above target) → decrease minCredits (more aggressive)
    // Negative error (below target) → increase minCredits (more conservative)
    int creditDelta = -static_cast<int>(adjustment * 50.0);  // Scale factor
    
    uint32_t newMinCredits = static_cast<uint32_t>(
        std::clamp(static_cast<int>(currentConfig_.minCredits) + creditDelta,
                   static_cast<int>(govConfig_.minCreditsFloor),
                   static_cast<int>(govConfig_.minCreditsCeiling))
    );
    
    if (newMinCredits != currentConfig_.minCredits) {
        if (!govConfig_.silent) {
            printf("[CreditGovernor] Tuning: throughput=%.2f B elem/s error=%+.3f → minCredits %u → %u\n",
                   smoothedThroughput_ / 1e9, error, currentConfig_.minCredits, newMinCredits);
        }
        currentConfig_.minCredits = newMinCredits;
    }
    
    lastError_ = error;
    lastAdjustment_ = adjustment;
    lastUpdateMs_ = nowMs;
    totalUpdates_++;
}

CreditConfig CreditGovernor::GetCurrentConfig() const {
    return currentConfig_;
}

void CreditGovernor::OverrideMinCredits(uint32_t minCredits) {
    overrideActive_ = true;
    currentConfig_.minCredits = std::clamp(minCredits, govConfig_.minCreditsFloor, govConfig_.minCreditsCeiling);
    printf("[CreditGovernor] Manual override: minCredits = %u\n", currentConfig_.minCredits);
}

void CreditGovernor::ResetOverride() {
    overrideActive_ = false;
    currentConfig_ = baseConfig_;
    integralError_ = 0.0;
    smoothedThroughput_ = govConfig_.targetThroughput;  // Reset EMA baseline
    lastUpdateMs_ = 0;  // Reset timing baseline
    printf("[CreditGovernor] Override reset, resuming auto-tune\n");
}

uint64_t CreditGovernor::GetCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

void CreditGovernor::PrintReport() const {
    printf("\n");
    printf("========================================\n");
    printf("Credit Governor Report\n");
    printf("========================================\n");
    printf("Auto-tuning:     %s\n", IsAutoTuning() ? "ACTIVE" : "DISABLED");
    printf("Total updates:   %llu\n", (unsigned long long)totalUpdates_);
    printf("Telemetry samples: %llu\n", (unsigned long long)totalTelemetrySamples_);
    printf("\n");
    printf("Current config:\n");
    printf("  minCredits:    %u (base: %u)\n", currentConfig_.minCredits, baseConfig_.minCredits);
    printf("  maxCredits:    %u\n", currentConfig_.maxCredits);
    printf("\n");
    printf("Throughput:\n");
    printf("  Smoothed:      %.2f B elem/s\n", smoothedThroughput_ / 1e9);
    printf("  Target:        %.2f B elem/s\n", govConfig_.targetThroughput / 1e9);
    printf("  Last error:    %+.3f\n", lastError_);
    printf("  Last adjust:   %+.3f\n", lastAdjustment_);
    printf("========================================\n");
}

} // namespace FlowControl
} // namespace RawrXD
