// ============================================================================
// adaptive_credit_governor.hpp — Weight-Aware Multi-Modal Credit Governor
// ============================================================================
// Extends CreditGovernor with workload-type awareness to prevent agentic
// tool-execution spikes from starving IDE rendering threads.
//
// Key additions:
//   - WorkloadType enum: Inference, Parsing, Hotpatching, Mixed
//   - Weight multipliers per workload type adjust target throughput
//   - Rendering thread reserve: guaranteed minimum credit pool for UI
//   - Tool-execution spike detection: temporary credit boost during apply_hotpatch
//
// Integration:
//   - AgenticToolExecutor reports tool execution start/end
//   - KVCacheCreditBridge reports memory pressure
//   - ExecutionScheduler reports phase transitions
// ============================================================================

#pragma once
#include "flow_control/credit_governor.hpp"
#include <cstdint>
#include <atomic>
#include <chrono>

namespace RawrXD {
namespace FlowControl {

// ============================================================================
// Workload Classification
// ============================================================================
enum class WorkloadType : uint8_t {
    Idle        = 0,   // No active workload
    Inference   = 1,   // Token generation, model forward pass
    Parsing     = 2,   // Rust AST, GGUF loader, symbol indexing
    Hotpatching = 3,   // apply_hotpatch, revert_hotpatch, live binary surgery
    Mixed       = 4,   // Concurrent parsing + hotpatching (highest pressure)
};

struct WorkloadTelemetry {
    WorkloadType type = WorkloadType::Idle;
    uint64_t timestampMs = 0;
    
    // Tool execution state (from AgenticToolExecutor)
    bool toolExecutionActive = false;
    uint32_t activeToolCount = 0;
    uint32_t hotpatchToolCount = 0;  // apply/revert/list hotpatch tools
    
    // Rendering thread health (from ExecutionScheduler)
    uint32_t renderFrameDrops = 0;
    double renderFrameTimeMs = 0.0;
    
    // Memory pressure (from KVCacheCreditBridge)
    float kvCacheReductionFactor = 1.0f;  // 1.0 = no reduction
};

// ============================================================================
// Weight-Aware Governor Configuration
// ============================================================================
struct AdaptiveGovernorConfig {
    // Inherit base governor config
    GovernorConfig base;
    
    // Workload weight multipliers (applied to target throughput)
    // <1.0 = reduce target (more conservative), >1.0 = increase target (more aggressive)
    float weightInference   = 1.00f;  // Baseline
    float weightParsing     = 0.85f;  // 15% reduction during heavy parsing
    float weightHotpatching = 0.60f;  // 40% reduction during live binary surgery
    float weightMixed       = 0.45f;  // 55% reduction during concurrent multi-modal
    
    // Rendering thread protection
    uint32_t renderReserveCredits = 50;   // Guaranteed credits for UI rendering
    uint32_t renderDropThreshold  = 3;  // Frame drops before emergency throttle
    
    // Tool-execution spike handling
    uint32_t toolSpikeBoostCredits = 30;   // Temporary credit boost during tool spikes
    uint32_t toolSpikeDurationMs   = 500;  // How long the boost lasts
    
    // Hysteresis to prevent rapid workload oscillation
    uint32_t workloadHysteresisMs = 200;
    
    // Silent mode
    bool silent = false;
};

// ============================================================================
// Adaptive Credit Governor — Weight-Aware Multi-Modal Tuning
// ============================================================================
class AdaptiveCreditGovernor : public CreditGovernor {
public:
    AdaptiveCreditGovernor();
    ~AdaptiveCreditGovernor();
    
    // Initialize with adaptive config (calls base CreditGovernor::Initialize)
    bool Initialize(const CreditConfig& baseConfig, const AdaptiveGovernorConfig& config);
    
    // Feed workload telemetry (thread-safe, called by orchestrator)
    void RecordWorkloadTelemetry(const WorkloadTelemetry& telemetry);
    
    // Get current effective target throughput (after weight adjustment)
    double GetEffectiveTargetThroughput() const;
    
    // Get current workload type
    WorkloadType GetCurrentWorkloadType() const;
    
    // Get rendering reserve status
    bool IsRenderReserveActive() const;
    uint32_t GetRenderReserveCredits() const;
    
    // Emergency throttle (called when rendering thread detects drops)
    void EmergencyThrottle();
    void ClearEmergencyThrottle();
    bool IsEmergencyThrottled() const;
    
    // Print adaptive tuning report (extends base PrintReport)
    void PrintAdaptiveReport() const;
    
private:
    AdaptiveGovernorConfig adaptiveConfig_;
    
    // Current workload state
    std::atomic<WorkloadType> currentWorkload_{WorkloadType::Idle};
    std::atomic<uint64_t> workloadStartMs_{0};
    std::atomic<bool> emergencyThrottled_{false};
    
    // Tool spike tracking
    std::atomic<uint64_t> toolSpikeEndMs_{0};
    std::atomic<uint32_t> activeToolCount_{0};
    
    // Rendering protection
    std::atomic<uint32_t> renderReserveActive_{0};
    std::atomic<uint32_t> consecutiveFrameDrops_{0};
    
    // Effective target (base target * weight multiplier)
    std::atomic<double> effectiveTargetThroughput_{0.0};
    
    void UpdateWorkloadWeights(const WorkloadTelemetry& telemetry);
    void ApplyToolSpikeBoost(uint64_t nowMs);
    void CheckRenderProtection(uint64_t nowMs);
    float GetWeightForWorkload(WorkloadType type) const;
    uint64_t GetCurrentTimeMs() const;
};

} // namespace FlowControl
} // namespace RawrXD
