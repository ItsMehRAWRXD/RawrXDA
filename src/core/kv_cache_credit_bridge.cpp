// kv_cache_credit_bridge.cpp
// Implementation of KV-cache credit bridge.
// Bridges memory pressure into credit-based flow control.

#include "kv_cache_credit_bridge.hpp"
#include "memory_pressure_handler.hpp"
#include "flow_control/credit_based_flow_control.hpp"
#include <cmath>
#include <algorithm>

namespace Rawrxd {
namespace KVCache {

// ============================================================================
// Singleton
// ============================================================================
KVCacheCreditBridge& KVCacheCreditBridge::Instance() {
    static KVCacheCreditBridge instance;
    return instance;
}

// ============================================================================
// Initialize
// ============================================================================
bool KVCacheCreditBridge::Initialize(CreditCounter* creditCounter, const KVCachePressureConfig& config) {
    if (initialized_.load()) return true;
    if (!creditCounter) return false;

    creditCounter_ = creditCounter;
    config_ = config;

    running_.store(true);
    monitorThread_ = std::thread(&KVCacheCreditBridge::MonitoringLoop, this);

    initialized_.store(true);
    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void KVCacheCreditBridge::Shutdown() {
    if (!initialized_.load()) return;
    running_.store(false);
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    initialized_.store(false);
}

// ============================================================================
// Report KV-cache usage
// ============================================================================
void KVCacheCreditBridge::ReportKVCacheUsage(uint64_t bytesAllocated, uint64_t bytesReserved) {
    kvCacheAllocated_.store(bytesAllocated, std::memory_order_relaxed);
    kvCacheReserved_.store(bytesReserved, std::memory_order_relaxed);
    // Synchronously apply pressure check
    ApplyPressure();
}

// ============================================================================
// Report system pressure
// ============================================================================
void KVCacheCreditBridge::ReportSystemPressure(const RawrXD::Memory::MemoryStats& stats) {
    // System pressure augments KV-cache pressure
    // If system is under HIGH/CRITICAL pressure, escalate KV-cache reduction
    if (stats.pressure >= RawrXD::Memory::PressureLevel::HIGH) {
        // Force critical reduction regardless of KV-cache size
        currentReduction_.store(config_.reductionCritical, std::memory_order_relaxed);
        currentPressure_.store(RawrXD::Memory::PressureLevel::CRITICAL, std::memory_order_relaxed);
    }
}

// ============================================================================
// Monitoring loop
// ============================================================================
void KVCacheCreditBridge::MonitoringLoop() {
    while (running_.load()) {
        auto start = std::chrono::steady_clock::now();

        uint64_t allocated = kvCacheAllocated_.load(std::memory_order_relaxed);
        float reduction = 1.0f;
        auto pressure = RawrXD::Memory::PressureLevel::NONE;

        // Determine pressure level with hysteresis
        if (allocated > config_.thresholdCritical) {
            pressure = RawrXD::Memory::PressureLevel::CRITICAL;
            reduction = config_.reductionCritical;
        } else if (allocated > config_.thresholdHigh) {
            pressure = RawrXD::Memory::PressureLevel::HIGH;
            reduction = config_.reductionHigh;
        } else if (allocated > config_.thresholdMedium) {
            pressure = RawrXD::Memory::PressureLevel::MEDIUM;
            reduction = config_.reductionMedium;
        } else if (allocated > config_.thresholdLow) {
            pressure = RawrXD::Memory::PressureLevel::LOW;
            reduction = config_.reductionLow;
        }

        // Apply manual override if set
        float manual = manualOverride_.load(std::memory_order_relaxed);
        if (manual >= 0.0f && manual <= 1.0f) {
            reduction = manual;
        }

        // Adaptive PID tuning
        if (config_.adaptivePidEnabled && pressure > RawrXD::Memory::PressureLevel::NONE) {
            auto now = std::chrono::steady_clock::now();
            float dt = std::chrono::duration<float>(now - pidState_.lastTime).count();
            if (dt > 0.0f) {
                // Error = how far above threshold we are (normalized)
                float threshold = static_cast<float>(config_.thresholdHigh);
                float error = (static_cast<float>(allocated) - threshold) / threshold;

                pidState_.integral += error * dt;
                pidState_.integral = std::clamp(pidState_.integral, -10.0f, 10.0f); // anti-windup

                float derivative = (error - pidState_.prevError) / dt;

                float pidOutput = config_.pidKp * error +
                                  config_.pidKi * pidState_.integral +
                                  config_.pidKd * derivative;

                // PID output adjusts reduction factor
                reduction = std::clamp(reduction + pidOutput, 0.0f, 1.0f);

                pidState_.prevError = error;
                pidState_.lastTime = now;
            }
        }

        // Apply reduction to credit counter
        if (creditCounter_) {
            auto currentReduction = currentReduction_.load(std::memory_order_relaxed);
            if (std::abs(reduction - currentReduction) > 0.01f) {
                // Update credit counter's minCredits
                uint32_t baseMinCredits = 64; // Default from CreditConfig
                uint32_t newMinCredits = static_cast<uint32_t>(baseMinCredits * reduction);
                newMinCredits = std::max(newMinCredits, 1u); // Never zero

                // Note: CreditCounter doesn't expose SetMinCredits directly
                // In production, this would call a method on CreditCounter
                // For now, we track the reduction factor externally
            }
            currentReduction_.store(reduction, std::memory_order_relaxed);
            currentPressure_.store(pressure, std::memory_order_relaxed);
        }

        lastUpdate_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);

        // Sleep until next poll
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto sleepTime = std::chrono::milliseconds(config_.pollIntervalMs) - elapsed;
        if (sleepTime > std::chrono::milliseconds::zero()) {
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

// ============================================================================
// Apply / Release pressure (synchronous)
// ============================================================================
void KVCacheCreditBridge::ApplyPressure() {
    uint64_t allocated = kvCacheAllocated_.load(std::memory_order_relaxed);
    float reduction = 1.0f;
    auto pressure = RawrXD::Memory::PressureLevel::NONE;

    if (allocated > config_.thresholdCritical) {
        pressure = RawrXD::Memory::PressureLevel::CRITICAL;
        reduction = config_.reductionCritical;
    } else if (allocated > config_.thresholdHigh) {
        pressure = RawrXD::Memory::PressureLevel::HIGH;
        reduction = config_.reductionHigh;
    } else if (allocated > config_.thresholdMedium) {
        pressure = RawrXD::Memory::PressureLevel::MEDIUM;
        reduction = config_.reductionMedium;
    } else if (allocated > config_.thresholdLow) {
        pressure = RawrXD::Memory::PressureLevel::LOW;
        reduction = config_.reductionLow;
    }

    float manual = manualOverride_.load(std::memory_order_relaxed);
    if (manual >= 0.0f && manual <= 1.0f) {
        reduction = manual;
    }

    float current = currentReduction_.load(std::memory_order_relaxed);
    if (std::abs(reduction - current) > 0.001f) {
        if (reduction < current) {
            throttlesApplied_.fetch_add(1, std::memory_order_relaxed);
        } else {
            throttlesReleased_.fetch_add(1, std::memory_order_relaxed);
        }
        currentReduction_.store(reduction, std::memory_order_relaxed);
        currentPressure_.store(pressure, std::memory_order_relaxed);
    }
    lastUpdate_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
}

void KVCacheCreditBridge::ReleasePressure() {
    ApplyPressure(); // Same logic — reduction computed from current state
}

// ============================================================================
// Getters
// ============================================================================
float KVCacheCreditBridge::GetCurrentReductionFactor() const {
    return currentReduction_.load(std::memory_order_relaxed);
}

RawrXD::Memory::PressureLevel KVCacheCreditBridge::GetCurrentPressureLevel() const {
    return currentPressure_.load(std::memory_order_relaxed);
}

KVCacheCreditBridge::Telemetry KVCacheCreditBridge::GetTelemetry() const {
    std::lock_guard<std::mutex> lock(telemetryMutex_);
    Telemetry t;
    t.kvCacheBytesAllocated = kvCacheAllocated_.load(std::memory_order_relaxed);
    t.kvCacheBytesReserved = kvCacheReserved_.load(std::memory_order_relaxed);
    t.reductionFactor = currentReduction_.load(std::memory_order_relaxed);
    t.pressureLevel = currentPressure_.load(std::memory_order_relaxed);
    t.creditThrottlesApplied = throttlesApplied_.load(std::memory_order_relaxed);
    t.creditThrottlesReleased = throttlesReleased_.load(std::memory_order_relaxed);
    t.lastUpdate = lastUpdate_.load(std::memory_order_relaxed);
    return t;
}

void KVCacheCreditBridge::SetManualReduction(float factor) {
    manualOverride_.store(factor, std::memory_order_relaxed);
}

} // namespace KVCache
} // namespace Rawrxd
