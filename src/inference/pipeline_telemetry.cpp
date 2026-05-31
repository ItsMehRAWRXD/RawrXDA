// ============================================================================
// pipeline_telemetry.cpp - Live Inference Telemetry Collector Implementation
// ============================================================================

#include "inference/pipeline_telemetry.hpp"
#include <cstdio>

namespace RawrXD {
namespace Inference {

// ============================================================================
// Singleton
// ============================================================================
PipelineTelemetryCollector* PipelineTelemetryCollector::Instance() {
    static PipelineTelemetryCollector instance;
    return &instance;
}

// ============================================================================
// Governor attachment
// ============================================================================
void PipelineTelemetryCollector::AttachGovernor(
    RawrXD::FlowControl::CreditGovernor* governor) {
    governor_ = governor;
}

void PipelineTelemetryCollector::DetachGovernor() {
    governor_ = nullptr;
}

// ============================================================================
// Record a completed token batch
// ============================================================================
void PipelineTelemetryCollector::RecordTokenBatch(size_t tokens, double elapsedMs) {
    if (tokens == 0 || elapsedMs <= 0.0) return;
    
    // Update counters
    totalTokens_.fetch_add(tokens, std::memory_order_relaxed);
    totalBatches_.fetch_add(1, std::memory_order_relaxed);
    totalLatencyNs_.fetch_add(
        static_cast<uint64_t>(elapsedMs * 1e6), 
        std::memory_order_relaxed);
    
    // Compute instantaneous throughput (tokens/sec)
    double instantThroughput = (tokens / elapsedMs) * 1000.0;
    
    // Update EMA
    double oldEma = emaThroughput_.load(std::memory_order_relaxed);
    double newEma = (EMA_ALPHA * instantThroughput) + 
                    ((1.0 - EMA_ALPHA) * oldEma);
    emaThroughput_.store(newEma, std::memory_order_relaxed);
    
    // Store for snapshot
    lastTokensPerSec_.store(newEma, std::memory_order_relaxed);
    
    // Compute average latency
    uint64_t batches = totalBatches_.load(std::memory_order_relaxed);
    uint64_t totalNs = totalLatencyNs_.load(std::memory_order_relaxed);
    double avgLatencyMs = (batches > 0) ? (totalNs / batches) / 1e6 : 0.0;
    lastAvgLatencyMs_.store(avgLatencyMs, std::memory_order_relaxed);
    
    // Feed governor if attached
    if (governor_) {
        RawrXD::FlowControl::GovernorTelemetry tel;
        tel.throughputElemPerSec = newEma;  // tokens/sec as element throughput
        tel.tokenRate = instantThroughput;
        tel.timestampMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        
        // Get credit stats from governor's current config
        auto cfg = governor_->GetCurrentConfig();
        tel.availableCredits = cfg.maxCredits - cfg.minCredits;  // Approximate
        tel.pendingElements = static_cast<uint32_t>(
            totalTokens_.load(std::memory_order_relaxed));
        
        governor_->RecordTelemetry(tel);
    }
}

// ============================================================================
// Record a blocked/backpressure event
// ============================================================================
void PipelineTelemetryCollector::RecordBlocked() {
    blockedBatches_.fetch_add(1, std::memory_order_relaxed);
}

// ============================================================================
// Get snapshot for dashboard
// ============================================================================
PipelineTelemetrySnapshot PipelineTelemetryCollector::GetSnapshot() const {
    PipelineTelemetrySnapshot snap;
    snap.tokensPerSec = lastTokensPerSec_.load(std::memory_order_relaxed);
    snap.avgLatencyMs = lastAvgLatencyMs_.load(std::memory_order_relaxed);
    snap.totalTokens = totalTokens_.load(std::memory_order_relaxed);
    snap.totalBatches = totalBatches_.load(std::memory_order_relaxed);
    snap.blockedBatches = blockedBatches_.load(std::memory_order_relaxed);
    snap.timestampMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    
    if (governor_) {
        snap.lastError = governor_->GetLastError();
        auto cfg = governor_->GetCurrentConfig();
        snap.minCredits = cfg.minCredits;
        snap.availableCredits = cfg.maxCredits;  // Simplified
    }
    
    return snap;
}

// ============================================================================
// Reset all counters
// ============================================================================
void PipelineTelemetryCollector::Reset() {
    totalTokens_.store(0, std::memory_order_relaxed);
    totalBatches_.store(0, std::memory_order_relaxed);
    blockedBatches_.store(0, std::memory_order_relaxed);
    totalLatencyNs_.store(0, std::memory_order_relaxed);
    emaThroughput_.store(0.0, std::memory_order_relaxed);
    lastTokensPerSec_.store(0.0, std::memory_order_relaxed);
    lastAvgLatencyMs_.store(0.0, std::memory_order_relaxed);
}

} // namespace Inference
} // namespace RawrXD
