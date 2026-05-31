// ============================================================================
// pipeline_telemetry.hpp - Live Inference Telemetry Collector
// ============================================================================
// Bridges the inference pipeline (llama.cpp native bridge) to the CreditGovernor
// and dashboard. Collects token-level throughput and emits GovernorTelemetry.
//
// Usage:
//   auto* telemetry = PipelineTelemetryCollector::Instance();
//   telemetry->AttachGovernor(&governor);
//   telemetry->RecordTokenBatch(tokensGenerated, elapsedMs);
// ============================================================================

#pragma once

#include "flow_control/credit_governor.hpp"
#include <cstdint>
#include <chrono>
#include <cmath>
#include <atomic>

namespace RawrXD {
namespace Inference {

struct PipelineTelemetrySnapshot {
    double tokensPerSec = 0.0;
    double avgLatencyMs = 0.0;
    uint64_t totalTokens = 0;
    uint64_t totalBatches = 0;
    uint64_t blockedBatches = 0;
    double lastError = 0.0;
    uint32_t minCredits = 0;
    uint32_t availableCredits = 0;
    uint64_t timestampMs = 0;
};

class PipelineTelemetryCollector {
public:
    static PipelineTelemetryCollector* Instance();
    
    // Attach a governor to receive telemetry (optional)
    void AttachGovernor(RawrXD::FlowControl::CreditGovernor* governor);
    void DetachGovernor();
    
    // Record a token batch completion
    void RecordTokenBatch(size_t tokens, double elapsedMs);
    
    // Record a blocked/backpressure event
    void RecordBlocked();
    
    // Get current snapshot (for dashboard polling)
    PipelineTelemetrySnapshot GetSnapshot() const;
    
    // Reset all counters
    void Reset();
    
    // Check if collector has a governor attached
    bool HasGovernor() const { return governor_ != nullptr; }

private:
    PipelineTelemetryCollector() = default;
    ~PipelineTelemetryCollector() = default;
    
    PipelineTelemetryCollector(const PipelineTelemetryCollector&) = delete;
    PipelineTelemetryCollector& operator=(const PipelineTelemetryCollector&) = delete;
    
    RawrXD::FlowControl::CreditGovernor* governor_ = nullptr;
    
    // Running statistics
    std::atomic<uint64_t> totalTokens_{0};
    std::atomic<uint64_t> totalBatches_{0};
    std::atomic<uint64_t> blockedBatches_{0};
    std::atomic<uint64_t> totalLatencyNs_{0};
    
    // EMA for throughput (tokens/sec)
    std::atomic<double> emaThroughput_{0.0};
    static constexpr double EMA_ALPHA = 0.3;
    
    // Last snapshot values
    mutable std::atomic<double> lastTokensPerSec_{0.0};
    mutable std::atomic<double> lastAvgLatencyMs_{0.0};
};

} // namespace Inference
} // namespace RawrXD
