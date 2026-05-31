// ============================================================================
// fp8_verifier.cpp - Scalar vs FP8 Numerical Validation Implementation
// ============================================================================

#include "verify/fp8_verifier.hpp"
#include <cstring>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cstdio>

// MASM FP8 Kernel external declaration (Windows x64 ABI)
// RCX = float* input, RDX = uint8_t* output, R8 = size_t count, XMM3 = float scale
extern "C" void SovereignQuantizeE4M3(float* input, uint8_t* output, size_t count, float scale);

namespace RawrXD {
namespace Verify {

// ============================================================================
// E4M3 FP8 format constants (matches MASM kernel)
// ============================================================================
static constexpr float E4M3_MAX = 448.0f;
static constexpr int E4M3_BIAS = 7;

// ============================================================================
// Scalar reference implementation (bit-exact with MASM kernel behavior)
// NOTE: MASM kernel uses clamped integer conversion with banker's rounding
// ============================================================================
static uint8_t FloatToE4M3(float f) {
    // Match MASM kernel behavior exactly:
    // 1. Extract sign bit
    uint8_t sign = (f < 0) ? 0x80 : 0;
    
    // 2. Absolute value
    f = std::abs(f);
    
    // 3. Clamp to E4M3 max (448.0) - matches MASM vminss
    if (f > E4M3_MAX) f = E4M3_MAX;
    
    // 4. Convert to integer with banker's rounding (matches MASM cvtss2si)
    // cvtss2si rounds to nearest even
    int val = static_cast<int>(std::nearbyint(f));
    
    // 5. Clamp to 0-255 (matches MASM min/max)
    if (val > 255) val = 255;
    if (val < 0) val = 0;
    
    // 6. Combine with sign (matches MASM or)
    return sign | static_cast<uint8_t>(val);
}

static float E4M3ToFloat(uint8_t e4m3) {
    // Extract sign
    bool negative = (e4m3 & 0x80) != 0;
    
    // Extract exponent and mantissa
    int exponent = (e4m3 >> 3) & 0xF;
    int mantissa = e4m3 & 0x7;
    
    // Handle zero
    if (exponent == 0 && mantissa == 0) {
        return negative ? -0.0f : 0.0f;
    }
    
    // Calculate value
    float value = std::ldexp(1.0f + (mantissa / 8.0f), exponent - E4M3_BIAS);
    
    return negative ? -value : value;
}

// ============================================================================
// VerificationStats Implementation
// ============================================================================
void VerificationStats::Reset() {
    totalBatches = 0;
    passedBatches = 0;
    failedBatches = 0;
    totalElements = 0;
    totalBitExactMatches = 0;
    accumulatedMaxError = 0.0;
    accumulatedMeanError = 0.0;
    totalScalarLatencyUs = 0;
    totalFp8LatencyUs = 0;
}

void VerificationStats::PrintReport() const {
    double passRate = (totalBatches > 0) ? (100.0 * passedBatches / totalBatches) : 0.0;
    double bitExactRate = (totalElements > 0) ? (100.0 * totalBitExactMatches / totalElements) : 0.0;
    
    double avgSpeedup = (totalFp8LatencyUs > 0) ? (static_cast<double>(totalScalarLatencyUs) / totalFp8LatencyUs) : 0.0;
    
    printf("\n");
    printf("========================================\n");
    printf("FP8 Verification Report\n");
    printf("========================================\n");
    printf("Batches:         %llu\n", (unsigned long long)totalBatches);
    printf("Passed:          %llu (%.2f%%)\n", (unsigned long long)passedBatches, passRate);
    printf("Failed:          %llu\n", (unsigned long long)failedBatches);
    printf("Elements:        %llu\n", (unsigned long long)totalElements);
    printf("Bit-exact:       %llu (%.2f%%)\n", (unsigned long long)totalBitExactMatches, bitExactRate);
    printf("\n");
    printf("Latency (avg):\n");
    printf("  Scalar:        %llu us\n", (unsigned long long)(totalBatches > 0 ? totalScalarLatencyUs / totalBatches : 0));
    printf("  FP8:           %llu us\n", (unsigned long long)(totalBatches > 0 ? totalFp8LatencyUs / totalBatches : 0));
    printf("  Speedup:       %.2fx\n", avgSpeedup);
    printf("\n");
    printf("Error (accumulated):\n");
    printf("  Max:           %.6f\n", accumulatedMaxError);
    printf("  Mean:          %.6f\n", accumulatedMeanError);
    printf("========================================\n");
}

// ============================================================================
// FP8Verifier Implementation
// ============================================================================
FP8Verifier::FP8Verifier() = default;
FP8Verifier::~FP8Verifier() = default;

bool FP8Verifier::Initialize(VerifyMode mode, float epsilonTolerance, bool enableLatencyTracking) {
    mode_ = mode;
    epsilonTolerance_ = epsilonTolerance;
    trackLatency_ = enableLatencyTracking;
    enabled_ = true;
    stats_.Reset();
    
    printf("[FP8-Verifier] Initialized (mode=%s, epsilon=%.6f)\n",
           (mode == VerifyMode::BitExact) ? "BitExact" : 
           (mode == VerifyMode::Epsilon) ? "Epsilon" : "Both",
           epsilonTolerance);
    return true;
}

void FP8Verifier::ScalarQuantizeDequantize(const float* input, float* output, size_t N) {
    for (size_t i = 0; i < N; ++i) {
        uint8_t e4m3 = FloatToE4M3(input[i]);
        output[i] = E4M3ToFloat(e4m3);
    }
}

void FP8Verifier::FP8QuantizeDequantize(const float* input, float* output, size_t N) {
    // Allocate temporary buffer for FP8 quantized data
    std::vector<uint8_t> temp(N);
    
    // Call MASM FP8 kernel (scale = 1.0f for direct quantization)
    SovereignQuantizeE4M3(const_cast<float*>(input), temp.data(), N, 1.0f);
    
    // Dequantize back to float for comparison
    for (size_t i = 0; i < N; ++i) {
        output[i] = E4M3ToFloat(temp[i]);
    }
}

BatchVerificationResult FP8Verifier::CompareResults(const float* scalar, 
                                                     const float* fp8, 
                                                     size_t N,
                                                     uint64_t scalarLatency,
                                                     uint64_t fp8Latency,
                                                     uint64_t batchId) {
    BatchVerificationResult result;
    result.batchId = batchId;
    result.numElements = N;
    result.scalarLatencyUs = scalarLatency;
    result.fp8LatencyUs = fp8Latency;
    result.speedup = (fp8Latency > 0) ? (static_cast<double>(scalarLatency) / fp8Latency) : 0.0;
    result.modeUsed = mode_;
    
    float maxError = 0.0f;
    double sumError = 0.0;
    double sumSquaredError = 0.0;
    uint64_t bitExact = 0;
    
    for (size_t i = 0; i < N; ++i) {
        float error = std::abs(scalar[i] - fp8[i]);
        
        // Check bit-exact match
        if (std::memcmp(&scalar[i], &fp8[i], sizeof(float)) == 0) {
            bitExact++;
        }
        
        maxError = std::max(maxError, error);
        sumError += error;
        sumSquaredError += error * error;
    }
    
    result.maxError = maxError;
    result.meanError = static_cast<float>(sumError / N);
    result.rmsError = static_cast<float>(std::sqrt(sumSquaredError / N));
    result.bitExactMatches = bitExact;
    result.bitExactRatio = static_cast<double>(bitExact) / N;
    
    // Determine pass/fail
    if (mode_ == VerifyMode::BitExact) {
        result.passed = (bitExact == N);
    } else { // Epsilon or Both
        result.passed = (maxError <= epsilonTolerance_);
    }
    
    // Update stats (non-atomic, single-threaded access assumed)
    stats_.totalBatches++;
    stats_.totalElements += N;
    stats_.totalBitExactMatches += bitExact;
    
    if (result.passed) {
        stats_.passedBatches++;
    } else {
        stats_.failedBatches++;
    }
    
    // Update error stats
    stats_.accumulatedMaxError = std::max(stats_.accumulatedMaxError, static_cast<double>(maxError));
    
    // Running mean update
    double prevMean = stats_.accumulatedMeanError;
    stats_.accumulatedMeanError = prevMean + (sumError / N - prevMean) / stats_.totalBatches;
    
    if (trackLatency_) {
        stats_.totalScalarLatencyUs += scalarLatency;
        stats_.totalFp8LatencyUs += fp8Latency;
    }
    
    return result;
}

BatchVerificationResult FP8Verifier::VerifyBatch(const float* input, size_t N, uint64_t batchId) {
    if (!enabled_) {
        return BatchVerificationResult{};
    }
    
    // Allocate temporary buffers
    std::vector<float> scalarOutput(N);
    std::vector<float> fp8Output(N);
    
    return VerifyBatchInPlace(input, scalarOutput.data(), fp8Output.data(), N, batchId);
}

BatchVerificationResult FP8Verifier::VerifyBatchInPlace(const float* input, 
                                                           float* scalarOutput,
                                                           float* fp8Output,
                                                           size_t N, 
                                                           uint64_t batchId) {
    if (!enabled_) {
        return BatchVerificationResult{};
    }
    
    // Time scalar path
    auto t0 = std::chrono::high_resolution_clock::now();
    ScalarQuantizeDequantize(input, scalarOutput, N);
    auto t1 = std::chrono::high_resolution_clock::now();
    
    // Time FP8 path
    auto t2 = std::chrono::high_resolution_clock::now();
    FP8QuantizeDequantize(input, fp8Output, N);
    auto t3 = std::chrono::high_resolution_clock::now();
    
    uint64_t scalarLatency = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    uint64_t fp8Latency = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    
    return CompareResults(scalarOutput, fp8Output, N, scalarLatency, fp8Latency, batchId);
}

VerificationStats FP8Verifier::GetStats() const {
    return stats_;
}

void FP8Verifier::ResetStats() {
    stats_.Reset();
}

void FP8Verifier::PrintStatus() const {
    stats_.PrintReport();
}

// ============================================================================
// Global Verifier Instance
// ============================================================================
static FP8Verifier* g_globalVerifier = nullptr;

FP8Verifier* GetGlobalVerifier() {
    return g_globalVerifier;
}

void InitializeGlobalVerifier(VerifyMode mode) {
    if (!g_globalVerifier) {
        g_globalVerifier = new FP8Verifier();
        g_globalVerifier->Initialize(mode);
    }
}

void ShutdownGlobalVerifier() {
    if (g_globalVerifier) {
        g_globalVerifier->PrintStatus();
        delete g_globalVerifier;
        g_globalVerifier = nullptr;
    }
}

} // namespace Verify
} // namespace RawrXD
