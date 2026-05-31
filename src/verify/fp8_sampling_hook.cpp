// ============================================================================
// fp8_sampling_hook.cpp - Live Pipeline Shadow Sampling Verifier
// ============================================================================
// Samples batches during live Stage 3 execution for continuous correctness
// monitoring without blocking the main pipeline.
//
// Usage: Wire into Stage 3 egress to verify 1 in N batches
//        Reports drift in real-time without pipeline interruption
// ============================================================================

#include "verify/fp8_sampling_hook.hpp"
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cstdio>

// MASM FP8 Kernel external declaration
extern "C" void SovereignQuantizeE4M3(float* input, uint8_t* output, size_t count, float scale);

namespace RawrXD {
namespace Verify {

// ============================================================================
// E4M3 FP8 format constants (matches MASM kernel)
// ============================================================================
static constexpr float E4M3_MAX = 448.0f;
static constexpr int E4M3_BIAS = 7;

static uint8_t FloatToE4M3(float f) {
    uint8_t sign = (f < 0) ? 0x80 : 0;
    f = std::abs(f);
    if (f > E4M3_MAX) f = E4M3_MAX;
    int val = static_cast<int>(std::nearbyint(f));
    if (val > 255) val = 255;
    if (val < 0) val = 0;
    return sign | static_cast<uint8_t>(val);
}

static float E4M3ToFloat(uint8_t e4m3) {
    bool negative = (e4m3 & 0x80) != 0;
    int exponent = (e4m3 >> 3) & 0xF;
    int mantissa = e4m3 & 0x7;
    if (exponent == 0 && mantissa == 0) {
        return negative ? -0.0f : 0.0f;
    }
    float value = std::ldexp(1.0f + (mantissa / 8.0f), exponent - E4M3_BIAS);
    return negative ? -value : value;
}

// ============================================================================
// SamplingVerifier Implementation
// ============================================================================
SamplingVerifier::SamplingVerifier() = default;
SamplingVerifier::~SamplingVerifier() = default;

bool SamplingVerifier::Initialize(const SamplingConfig& config) {
    config_ = config;
    enabled_ = true;
    batchCounter_ = 0;
    samplesTaken_ = 0;
    driftDetected_ = false;
    consecutiveDrifts_ = 0;
    
    // Pre-allocate shadow buffers to avoid allocation during hot path
    if (config_.shadowBufferSize > 0) {
        shadowInput_.resize(config_.shadowBufferSize);
        shadowScalar_.resize(config_.shadowBufferSize);
        shadowFp8_.resize(config_.shadowBufferSize);
    }
    
    printf("[SamplingVerifier] Initialized (sampleRate=1/%zu, threshold=%.6f, mode=%s)\n",
           config_.sampleInterval,
           config_.driftThreshold,
           config_.mode == VerifyMode::BitExact ? "BitExact" : 
           config_.mode == VerifyMode::Epsilon ? "Epsilon" : "Both");
    return true;
}

void SamplingVerifier::Shutdown() {
    if (enabled_) {
        PrintReport();
        enabled_ = false;
    }
}

SamplingResult SamplingVerifier::MaybeVerifyBatch(const float* pipelineInput, size_t N) {
    SamplingResult result;
    result.wasSampled = false;
    result.driftDetected = false;
    
    if (!enabled_ || N == 0) {
        return result;
    }
    
    // Increment batch counter
    uint64_t currentBatch = batchCounter_.fetch_add(1, std::memory_order_relaxed);
    
    // Check if this batch should be sampled
    if (currentBatch % config_.sampleInterval != 0) {
        return result;
    }
    
    // Limit sample size to configured shadow buffer
    size_t sampleSize = std::min(N, config_.shadowBufferSize);
    if (sampleSize == 0) {
        return result;
    }
    
    // Take the sample
    result = SampleBatch(pipelineInput, sampleSize, currentBatch);
    
    // Handle drift detection
    if (result.driftDetected) {
        HandleDrift(result);
    }
    
    return result;
}

SamplingResult SamplingVerifier::SampleBatch(const float* input, size_t N, uint64_t batchId) {
    SamplingResult result;
    result.batchId = batchId;
    result.wasSampled = true;
    result.numElements = N;
    result.sampleTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    samplesTaken_++;
    
    // Copy input to shadow buffer (non-blocking)
    std::memcpy(shadowInput_.data(), input, N * sizeof(float));
    
    // Run scalar reference path
    auto t0 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        uint8_t e4m3 = FloatToE4M3(shadowInput_[i]);
        shadowScalar_[i] = E4M3ToFloat(e4m3);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    
    // Run FP8 kernel path
    auto t2 = std::chrono::high_resolution_clock::now();
    std::vector<uint8_t> temp(N);
    SovereignQuantizeE4M3(shadowInput_.data(), temp.data(), N, 1.0f);
    for (size_t i = 0; i < N; ++i) {
        shadowFp8_[i] = E4M3ToFloat(temp[i]);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    
    // Calculate latencies
    result.scalarLatencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    result.fp8LatencyUs = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    result.speedup = (result.fp8LatencyUs > 0) ? 
        (static_cast<double>(result.scalarLatencyUs) / result.fp8LatencyUs) : 0.0;
    
    // Compare results
    float maxError = 0.0f;
    double sumError = 0.0;
    uint64_t bitExact = 0;
    
    for (size_t i = 0; i < N; ++i) {
        float error = std::abs(shadowScalar_[i] - shadowFp8_[i]);
        
        if (std::memcmp(&shadowScalar_[i], &shadowFp8_[i], sizeof(float)) == 0) {
            bitExact++;
        }
        
        maxError = std::max(maxError, error);
        sumError += error;
    }
    
    result.maxError = maxError;
    result.meanError = static_cast<float>(sumError / N);
    result.bitExactMatches = bitExact;
    result.bitExactRatio = static_cast<double>(bitExact) / N;
    
    // Determine drift
    if (config_.mode == VerifyMode::BitExact) {
        result.driftDetected = (bitExact != N);
    } else {
        result.driftDetected = (maxError > config_.driftThreshold);
    }
    
    // Update statistics
    totalSamples_++;
    totalElementsSampled_ += N;
    accumulatedMaxError_ = std::max(accumulatedMaxError_, static_cast<double>(maxError));
    
    if (result.driftDetected) {
        driftEvents_++;
        consecutiveDrifts_++;
    } else {
        consecutiveDrifts_ = 0;
    }
    
    // Log if configured
    if (config_.logSamples || result.driftDetected) {
        LogSample(result);
    }
    
    return result;
}

void SamplingVerifier::HandleDrift(const SamplingResult& result) {
    driftDetected_.store(true, std::memory_order_relaxed);
    
    // Log drift event
    printf("[SamplingVerifier] ⚠️  DRIFT DETECTED in batch %llu\n", 
           (unsigned long long)result.batchId);
    printf("  Max error: %.6f (threshold: %.6f)\n", result.maxError, config_.driftThreshold);
    printf("  Bit-exact: %llu/%llu (%.2f%%)\n",
           (unsigned long long)result.bitExactMatches,
           (unsigned long long)result.numElements,
           100.0 * result.bitExactRatio);
    
    // Check for consecutive drift escalation
    if (consecutiveDrifts_ >= config_.consecutiveDriftLimit) {
        printf("[SamplingVerifier] 🚨 ESCALATION: %zu consecutive drifts detected!\n",
               consecutiveDrifts_);
        
        if (config_.escalationCallback) {
            config_.escalationCallback(result);
        }
    }
}

void SamplingVerifier::LogSample(const SamplingResult& result) {
    const char* status = result.driftDetected ? "DRIFT" : "OK";
    printf("[SamplingVerifier] Sample #%llu (batch %llu): %s | "
           "error=%.6f/%.6f (max/mean) | bit-exact=%.1f%% | speedup=%.2fx\n",
           (unsigned long long)totalSamples_,
           (unsigned long long)result.batchId,
           status,
           result.maxError,
           result.meanError,
           100.0 * result.bitExactRatio,
           result.speedup);
}

void SamplingVerifier::PrintReport() const {
    double driftRate = (totalSamples_ > 0) ? 
        (100.0 * driftEvents_ / totalSamples_) : 0.0;
    
    printf("\n");
    printf("========================================\n");
    printf("FP8 Sampling Verifier Report\n");
    printf("========================================\n");
    printf("Total batches processed:  %llu\n", (unsigned long long)batchCounter_.load());
    printf("Samples taken:            %llu\n", (unsigned long long)totalSamples_);
    printf("Elements sampled:         %llu\n", (unsigned long long)totalElementsSampled_);
    printf("Drift events:             %llu (%.4f%%)\n", 
           (unsigned long long)driftEvents_, driftRate);
    printf("Accumulated max error:    %.6f\n", accumulatedMaxError_);
    printf("Consecutive drifts:       %zu\n", consecutiveDrifts_);
    printf("Drift detected:           %s\n", driftDetected_.load() ? "YES" : "NO");
    printf("========================================\n");
}

// ============================================================================
// Global Sampling Verifier Instance
// ============================================================================
static SamplingVerifier* g_samplingVerifier = nullptr;

SamplingVerifier* GetGlobalSamplingVerifier() {
    return g_samplingVerifier;
}

void InitializeGlobalSamplingVerifier(const SamplingConfig& config) {
    if (!g_samplingVerifier) {
        g_samplingVerifier = new SamplingVerifier();
        g_samplingVerifier->Initialize(config);
    }
}

void ShutdownGlobalSamplingVerifier() {
    if (g_samplingVerifier) {
        g_samplingVerifier->Shutdown();
        delete g_samplingVerifier;
        g_samplingVerifier = nullptr;
    }
}

} // namespace Verify
} // namespace RawrXD
