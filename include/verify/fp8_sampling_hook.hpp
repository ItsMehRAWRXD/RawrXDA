// ============================================================================
// fp8_sampling_hook.hpp - Live Pipeline Shadow Sampling Verifier
// ============================================================================
// Samples batches during live Stage 3 execution for continuous correctness
// monitoring without blocking the main pipeline.
//
// Integration: Call MaybeVerifyBatch() from Stage 3 egress for every batch
//              Only 1 in N batches are actually verified (configurable)
//              Zero pipeline interruption - shadow execution in parallel
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#include <functional>
#include "verify/fp8_verifier.hpp"  // For VerifyMode

namespace RawrXD {
namespace Verify {

// Sampling configuration
struct SamplingConfig {
    // Sample 1 batch every N batches (e.g., 100 = 1% sampling)
    size_t sampleInterval = 100;
    
    // Maximum elements to sample per batch (truncates if larger)
    size_t shadowBufferSize = 1024;
    
    // Drift detection threshold
    float driftThreshold = 0.001f;
    
    // Verification mode
    VerifyMode mode = VerifyMode::Epsilon;
    
    // Log every sample (not just drifts)
    bool logSamples = false;
    
    // Consecutive drift limit before escalation
    size_t consecutiveDriftLimit = 3;
    
    // Callback for drift escalation (optional)
    std::function<void(const struct SamplingResult&)> escalationCallback;
};

// Result of a sampling operation
struct SamplingResult {
    uint64_t batchId = 0;
    size_t numElements = 0;
    bool wasSampled = false;
    bool driftDetected = false;
    
    // Error metrics
    float maxError = 0.0f;
    float meanError = 0.0f;
    uint64_t bitExactMatches = 0;
    double bitExactRatio = 0.0;
    
    // Latency (microseconds)
    uint64_t scalarLatencyUs = 0;
    uint64_t fp8LatencyUs = 0;
    double speedup = 0.0;
    
    // Timestamp
    uint64_t sampleTimestamp = 0;
};

// Sampling verifier - non-blocking shadow execution
class SamplingVerifier {
public:
    SamplingVerifier();
    ~SamplingVerifier();
    
    // Initialize with sampling configuration
    bool Initialize(const SamplingConfig& config);
    void Shutdown();
    
    // Check if a batch should be sampled and verify if so
    // Call this from Stage 3 egress for every batch
    // Returns immediately with wasSampled=false if not sampling this batch
    SamplingResult MaybeVerifyBatch(const float* pipelineInput, size_t N);
    
    // Force a sample (bypasses sampling interval)
    SamplingResult ForceSample(const float* pipelineInput, size_t N, uint64_t batchId);
    
    // Get current statistics
    uint64_t GetTotalBatches() const { return batchCounter_.load(); }
    uint64_t GetSamplesTaken() const { return totalSamples_; }
    uint64_t GetDriftEvents() const { return driftEvents_; }
    bool IsDriftDetected() const { return driftDetected_.load(); }
    size_t GetConsecutiveDrifts() const { return consecutiveDrifts_; }
    
    // Check if verifier is enabled
    bool IsEnabled() const { return enabled_; }
    void Disable() { enabled_ = false; }
    void Enable() { enabled_ = true; }
    
    // Print report
    void PrintReport() const;
    
    // Reset statistics
    void ResetStats() {
        batchCounter_ = 0;
        totalSamples_ = 0;
        totalElementsSampled_ = 0;
        driftEvents_ = 0;
        consecutiveDrifts_ = 0;
        driftDetected_ = false;
        accumulatedMaxError_ = 0.0;
    }

private:
    SamplingResult SampleBatch(const float* input, size_t N, uint64_t batchId);
    void HandleDrift(const SamplingResult& result);
    void LogSample(const SamplingResult& result);
    
    bool enabled_ = false;
    SamplingConfig config_;
    
    // Batch counter (incremented for every batch, not just samples)
    std::atomic<uint64_t> batchCounter_{0};
    
    // Statistics
    uint64_t samplesTaken_ = 0;
    uint64_t totalSamples_ = 0;
    uint64_t totalElementsSampled_ = 0;
    uint64_t driftEvents_ = 0;
    std::atomic<bool> driftDetected_{false};
    size_t consecutiveDrifts_ = 0;
    double accumulatedMaxError_ = 0.0;
    
    // Shadow buffers (pre-allocated)
    std::vector<float> shadowInput_;
    std::vector<float> shadowScalar_;
    std::vector<float> shadowFp8_;
};

// Global sampling verifier instance
SamplingVerifier* GetGlobalSamplingVerifier();
void InitializeGlobalSamplingVerifier(const SamplingConfig& config = SamplingConfig{});
void ShutdownGlobalSamplingVerifier();

// Convenience macro for Stage 3 integration
// Usage: SAMPLE_BATCH_VERIFY(input, N) in Stage 3 egress
#define SAMPLE_BATCH_VERIFY(input, N) \
    do { \
        if (auto* sampler = RawrXD::Verify::GetGlobalSamplingVerifier()) { \
            if (sampler->IsEnabled()) { \
                sampler->MaybeVerifyBatch(input, N); \
            } \
        } \
    } while(0)

// Drift-aware macro - returns true if drift detected
#define SAMPLE_BATCH_CHECK_DRIFT(input, N, resultVar) \
    do { \
        resultVar.wasSampled = false; \
        resultVar.driftDetected = false; \
        if (auto* sampler = RawrXD::Verify::GetGlobalSamplingVerifier()) { \
            if (sampler->IsEnabled()) { \
                resultVar = sampler->MaybeVerifyBatch(input, N); \
            } \
        } \
    } while(0)

} // namespace Verify
} // namespace RawrXD
