// ============================================================================
// fp8_verifier.hpp - Scalar vs FP8 Numerical Validation Harness
// ============================================================================
// Drop-in verifier for Stage 3 of the 3-stage pipeline
// Compares scalar reference against FP8 kernel output
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>

namespace RawrXD {
namespace Verify {

// Verification mode
enum class VerifyMode {
    BitExact,      // Require identical bits (strict)
    Epsilon,       // Allow small numerical error (tolerant)
    Both           // Run both modes and report
};

// Per-batch verification result
struct BatchVerificationResult {
    uint64_t batchId = 0;
    size_t numElements = 0;
    
    // Error metrics
    float maxError = 0.0f;
    float meanError = 0.0f;
    float rmsError = 0.0f;
    uint64_t bitExactMatches = 0;
    double bitExactRatio = 0.0;
    
    // Latency (microseconds)
    uint64_t scalarLatencyUs = 0;
    uint64_t fp8LatencyUs = 0;
    double speedup = 0.0;
    
    // Status
    bool passed = false;
    VerifyMode modeUsed = VerifyMode::Epsilon;
};

// Accumulated statistics across all batches (non-atomic for copying)
struct VerificationStats {
    uint64_t totalBatches = 0;
    uint64_t passedBatches = 0;
    uint64_t failedBatches = 0;
    
    uint64_t totalElements = 0;
    uint64_t totalBitExactMatches = 0;
    
    double accumulatedMaxError = 0.0;
    double accumulatedMeanError = 0.0;
    uint64_t totalScalarLatencyUs = 0;
    uint64_t totalFp8LatencyUs = 0;
    
    void Reset();
    void PrintReport() const;
};

// FP8 Verifier class - plugs into Stage 3
class FP8Verifier {
public:
    FP8Verifier();
    ~FP8Verifier();
    
    // Initialize with verification parameters
    bool Initialize(VerifyMode mode = VerifyMode::Epsilon, 
                  float epsilonTolerance = 0.001f,
                  bool enableLatencyTracking = true);
    
    // Verify a single batch
    // Input: float array of size N
    // Output: FP8 quantized then dequantized (for comparison)
    BatchVerificationResult VerifyBatch(const float* input, size_t N, uint64_t batchId = 0);
    
    // Verify using pre-allocated output buffers (zero-copy path)
    BatchVerificationResult VerifyBatchInPlace(const float* input, 
                                                float* scalarOutput,
                                                float* fp8Output,
                                                size_t N, 
                                                uint64_t batchId = 0);
    
    // Get accumulated statistics
    VerificationStats GetStats() const;
    void ResetStats();
    
    // Print current status
    void PrintStatus() const;
    
    // Check if verification is enabled
    bool IsEnabled() const { return enabled_; }
    
    // Disable verification (for production runs)
    void Disable() { enabled_ = false; }
    void Enable() { enabled_ = true; }

private:
    bool enabled_ = true;
    VerifyMode mode_ = VerifyMode::Epsilon;
    float epsilonTolerance_ = 0.001f;
    bool trackLatency_ = true;
    
    VerificationStats stats_;
    
    // Internal implementations
    void ScalarQuantizeDequantize(const float* input, float* output, size_t N);
    void FP8QuantizeDequantize(const float* input, float* output, size_t N);
    
    BatchVerificationResult CompareResults(const float* scalar, 
                                           const float* fp8, 
                                           size_t N,
                                           uint64_t scalarLatency,
                                           uint64_t fp8Latency,
                                           uint64_t batchId);
};

// Global verifier instance for pipeline integration
FP8Verifier* GetGlobalVerifier();
void InitializeGlobalVerifier(VerifyMode mode = VerifyMode::Epsilon);
void ShutdownGlobalVerifier();

// Convenience macro for pipeline integration
#define VERIFY_BATCH(input, N, batchId) \
    do { \
        if (auto* verifier = RawrXD::Verify::GetGlobalVerifier()) { \
            if (verifier->IsEnabled()) { \
                verifier->VerifyBatch(input, N, batchId); \
            } \
        } \
    } while(0)

} // namespace Verify
} // namespace RawrXD
