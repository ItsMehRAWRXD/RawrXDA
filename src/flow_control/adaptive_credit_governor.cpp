// ============================================================================
// adaptive_credit_governor.cpp — Weight-Aware Multi-Modal Credit Governor
// ============================================================================

#include "flow_control/adaptive_credit_governor.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace RawrXD {
namespace FlowControl {

AdaptiveCreditGovernor::AdaptiveCreditGovernor() = default;
AdaptiveCreditGovernor::~AdaptiveCreditGovernor() = default;

bool AdaptiveCreditGovernor::Initialize(const CreditConfig& baseConfig,
                                          const AdaptiveGovernorConfig& config) {
    adaptiveConfig_ = config;
    
    // Initialize base governor with the base config
    if (!CreditGovernor::Initialize(baseConfig, config.base)) {
        return false;
    }
    
    currentWorkload_.store(WorkloadType::Idle, std::memory_order_relaxed);
    workloadStartMs_.store(GetCurrentTimeMs(), std::memory_order_relaxed);
    emergencyThrottled_.store(false, std::memory_order_relaxed);
    toolSpikeEndMs_.store(0, std::memory_order_relaxed);
    activeToolCount_.store(0, std::memory_order_relaxed);
    renderReserveActive_.store(0, std::memory_order_relaxed);
    consecutiveFrameDrops_.store(0, std::memory_order_relaxed);
    effectiveTargetThroughput_.store(config.base.targetThroughput, std::memory_order_relaxed);
    
    if (!config.silent) {
        printf("[AdaptiveCreditGovernor] Initialized\n");
        printf("  Workload weights: Inference=%.2f Parsing=%.2f Hotpatch=%.2f Mixed=%.2f\n",
               config.weightInference, config.weightParsing,
               config.weightHotpatching, config.weightMixed);
        printf("  Render reserve: %u credits, drop threshold: %u\n",
               config.renderReserveCredits, config.renderDropThreshold);
        printf("  Tool spike boost: +%u credits for %u ms\n",
               config.toolSpikeBoostCredits, config.toolSpikeDurationMs);
    }
    
    return true;
}

void AdaptiveCreditGovernor::RecordWorkloadTelemetry(const WorkloadTelemetry& telemetry) {
    uint64_t nowMs = GetCurrentTimeMs();
    
    // Update active tool count
    activeToolCount_.store(telemetry.activeToolCount, std::memory_order_relaxed);
    
    // Detect tool-execution spikes (especially hotpatching tools)
    if (telemetry.toolExecutionActive && telemetry.hotpatchToolCount > 0) {
        ApplyToolSpikeBoost(nowMs);
    }
    
    // Update workload type with hysteresis
    WorkloadType newType = telemetry.type;
    WorkloadType currentType = currentWorkload_.load(std::memory_order_relaxed);
    
    if (newType != currentType) {
        uint64_t elapsedMs = nowMs - workloadStartMs_.load(std::memory_order_relaxed);
        if (elapsedMs >= adaptiveConfig_.workloadHysteresisMs) {
            currentWorkload_.store(newType, std::memory_order_relaxed);
            workloadStartMs_.store(nowMs, std::memory_order_relaxed);
            
            if (!adaptiveConfig_.silent) {
                const char* typeName = "Unknown";
                switch (newType) {
                    case WorkloadType::Idle:        typeName = "Idle"; break;
                    case WorkloadType::Inference:   typeName = "Inference"; break;
                    case WorkloadType::Parsing:     typeName = "Parsing"; break;
                    case WorkloadType::Hotpatching: typeName = "Hotpatching"; break;
                    case WorkloadType::Mixed:       typeName = "Mixed"; break;
                }
                printf("[AdaptiveCreditGovernor] Workload transition: %s\n", typeName);
            }
        }
    }
    
    // Update effective target throughput based on workload weight
    UpdateWorkloadWeights(telemetry);
    
    // Check rendering thread protection
    CheckRenderProtection(nowMs);
}

void AdaptiveCreditGovernor::UpdateWorkloadWeights(const WorkloadTelemetry& telemetry) {
    WorkloadType type = currentWorkload_.load(std::memory_order_relaxed);
    float weight = GetWeightForWorkload(type);
    
    // Apply KV-cache reduction factor (from memory pressure)
    weight *= telemetry.kvCacheReductionFactor;
    
    // Clamp weight to reasonable bounds
    weight = std::clamp(weight, 0.1f, 2.0f);
    
    double baseTarget = adaptiveConfig_.base.targetThroughput;
    double newEffectiveTarget = baseTarget * weight;
    
    effectiveTargetThroughput_.store(newEffectiveTarget, std::memory_order_relaxed);
    
    // Feed adjusted telemetry to base governor
    GovernorTelemetry adjustedTelemetry;
    adjustedTelemetry.timestampMs = telemetry.timestampMs;
    adjustedTelemetry.throughputElemPerSec = newEffectiveTarget;  // Use effective target
    adjustedTelemetry.tokenRate = 0.0;
    adjustedTelemetry.blockedCount = 0;
    adjustedTelemetry.successCount = 0;
    adjustedTelemetry.availableCredits = 0;
    adjustedTelemetry.pendingElements = 0;
    
    CreditGovernor::RecordTelemetry(adjustedTelemetry);
}

void AdaptiveCreditGovernor::ApplyToolSpikeBoost(uint64_t nowMs) {
    uint64_t currentEndMs = toolSpikeEndMs_.load(std::memory_order_relaxed);
    uint64_t newEndMs = nowMs + adaptiveConfig_.toolSpikeDurationMs;
    
    // Extend the spike window
    if (newEndMs > currentEndMs) {
        toolSpikeEndMs_.store(newEndMs, std::memory_order_relaxed);
        
        if (!adaptiveConfig_.silent) {
            printf("[AdaptiveCreditGovernor] Tool spike boost active (+%u credits until %llu ms)\n",
                   adaptiveConfig_.toolSpikeBoostCredits, newEndMs);
        }
    }
}

void AdaptiveCreditGovernor::CheckRenderProtection(uint64_t nowMs) {
    uint32_t frameDrops = consecutiveFrameDrops_.load(std::memory_order_relaxed);
    
    if (frameDrops >= adaptiveConfig_.renderDropThreshold) {
        if (!emergencyThrottled_.load(std::memory_order_relaxed)) {
            emergencyThrottled_.store(true, std::memory_order_relaxed);
            renderReserveActive_.store(adaptiveConfig_.renderReserveCredits, std::memory_order_relaxed);
            
            if (!adaptiveConfig_.silent) {
                printf("[AdaptiveCreditGovernor] EMERGENCY THROTTLE: %u frame drops, reserve=%u credits\n",
                       frameDrops, adaptiveConfig_.renderReserveCredits);
            }
        }
    } else {
        if (emergencyThrottled_.load(std::memory_order_relaxed)) {
            emergencyThrottled_.store(false, std::memory_order_relaxed);
            renderReserveActive_.store(0, std::memory_order_relaxed);
            
            if (!adaptiveConfig_.silent) {
                printf("[AdaptiveCreditGovernor] Emergency throttle cleared\n");
            }
        }
    }
}

float AdaptiveCreditGovernor::GetWeightForWorkload(WorkloadType type) const {
    switch (type) {
        case WorkloadType::Idle:        return 1.0f;
        case WorkloadType::Inference:   return adaptiveConfig_.weightInference;
        case WorkloadType::Parsing:     return adaptiveConfig_.weightParsing;
        case WorkloadType::Hotpatching: return adaptiveConfig_.weightHotpatching;
        case WorkloadType::Mixed:       return adaptiveConfig_.weightMixed;
        default:                        return 1.0f;
    }
}

double AdaptiveCreditGovernor::GetEffectiveTargetThroughput() const {
    return effectiveTargetThroughput_.load(std::memory_order_relaxed);
}

WorkloadType AdaptiveCreditGovernor::GetCurrentWorkloadType() const {
    return currentWorkload_.load(std::memory_order_relaxed);
}

bool AdaptiveCreditGovernor::IsRenderReserveActive() const {
    return renderReserveActive_.load(std::memory_order_relaxed) > 0;
}

uint32_t AdaptiveCreditGovernor::GetRenderReserveCredits() const {
    return renderReserveActive_.load(std::memory_order_relaxed);
}

void AdaptiveCreditGovernor::EmergencyThrottle() {
    emergencyThrottled_.store(true, std::memory_order_relaxed);
    renderReserveActive_.store(adaptiveConfig_.renderReserveCredits, std::memory_order_relaxed);
    
    if (!adaptiveConfig_.silent) {
        printf("[AdaptiveCreditGovernor] Manual emergency throttle activated\n");
    }
}

void AdaptiveCreditGovernor::ClearEmergencyThrottle() {
    emergencyThrottled_.store(false, std::memory_order_relaxed);
    renderReserveActive_.store(0, std::memory_order_relaxed);
    
    if (!adaptiveConfig_.silent) {
        printf("[AdaptiveCreditGovernor] Manual emergency throttle cleared\n");
    }
}

bool AdaptiveCreditGovernor::IsEmergencyThrottled() const {
    return emergencyThrottled_.load(std::memory_order_relaxed);
}

uint64_t AdaptiveCreditGovernor::GetCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

void AdaptiveCreditGovernor::PrintAdaptiveReport() const {
    CreditGovernor::PrintReport();
    
    printf("\n");
    printf("=== Adaptive Governor Extension ===\n");
    
    WorkloadType type = GetCurrentWorkloadType();
    const char* typeName = "Unknown";
    switch (type) {
        case WorkloadType::Idle:        typeName = "Idle"; break;
        case WorkloadType::Inference:   typeName = "Inference"; break;
        case WorkloadType::Parsing:     typeName = "Parsing"; break;
        case WorkloadType::Hotpatching: typeName = "Hotpatching"; break;
        case WorkloadType::Mixed:       typeName = "Mixed"; break;
    }
    
    printf("Current workload:   %s\n", typeName);
    printf("Effective target:   %.2f B elem/s (weight=%.2f)\n",
           GetEffectiveTargetThroughput() / 1e9,
           GetWeightForWorkload(type));
    printf("Render reserve:     %s (%u credits)\n",
           IsRenderReserveActive() ? "ACTIVE" : "inactive",
           GetRenderReserveCredits());
    printf("Emergency throttle: %s\n",
           IsEmergencyThrottled() ? "ACTIVE" : "inactive");
    printf("Active tools:       %u\n",
           activeToolCount_.load(std::memory_order_relaxed));
    printf("===================================\n");
}

} // namespace FlowControl
} // namespace RawrXD
