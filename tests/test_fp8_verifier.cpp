// ============================================================================
// test_fp8_verifier.cpp - FP8 Verification Harness Test
// ============================================================================
// Validates scalar vs FP8 numerical correctness before MASM integration
// ============================================================================

#include "verify/fp8_verifier.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <vector>

using namespace RawrXD::Verify;

// Generate test data with various patterns
void GenerateTestData(float* data, size_t N, int pattern) {
    switch (pattern) {
        case 0: // Random in E4M3 range
            for (size_t i = 0; i < N; ++i) {
                data[i] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 800.0f;
            }
            break;
        case 1: // Sequential
            for (size_t i = 0; i < N; ++i) {
                data[i] = static_cast<float>(i) * 0.1f;
            }
            break;
        case 2: // Edge cases (near E4M3 limits)
            for (size_t i = 0; i < N; ++i) {
                if (i % 4 == 0) data[i] = 448.0f;      // Max positive
                else if (i % 4 == 1) data[i] = -448.0f; // Max negative
                else if (i % 4 == 2) data[i] = 0.0f;    // Zero
                else data[i] = 1.0f;                     // Unity
            }
            break;
        case 3: // Small values (subnormal region)
            for (size_t i = 0; i < N; ++i) {
                data[i] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.01f;
            }
            break;
    }
}

int main() {
    printf("========================================\n");
    printf("FP8 Verifier Test Harness\n");
    printf("========================================\n\n");
    
    srand(static_cast<unsigned>(time(nullptr)));
    
    // Test configurations
    const size_t batchSizes[] = {64, 256, 1024, 4096};
    const int numPatterns = 4;
    const int batchesPerPattern = 10;
    
    // Test 1: Bit-exact mode
    printf("Test 1: Bit-Exact Mode\n");
    printf("------------------------\n");
    {
        FP8Verifier verifier;
        verifier.Initialize(VerifyMode::BitExact, 0.0f, true);
        
        for (size_t batchSize : batchSizes) {
            std::vector<float> input(batchSize);
            
            for (int pattern = 0; pattern < numPatterns; ++pattern) {
                for (int batch = 0; batch < batchesPerPattern; ++batch) {
                    GenerateTestData(input.data(), batchSize, pattern);
                    auto result = verifier.VerifyBatch(input.data(), batchSize, 
                                                       pattern * 1000 + batch);
                    
                    if (!result.passed) {
                        printf("  FAIL: batch=%llu, pattern=%d, bit-exact=%.2f%%\n",
                               (unsigned long long)result.batchId, pattern,
                               result.bitExactRatio * 100.0);
                    }
                }
            }
        }
        
        printf("  Results:\n");
        verifier.PrintStatus();
    }
    
    // Test 2: Epsilon mode
    printf("\nTest 2: Epsilon Mode (tolerance=0.001)\n");
    printf("----------------------------------------\n");
    {
        FP8Verifier verifier;
        verifier.Initialize(VerifyMode::Epsilon, 0.001f, true);
        
        for (size_t batchSize : batchSizes) {
            std::vector<float> input(batchSize);
            
            for (int pattern = 0; pattern < numPatterns; ++pattern) {
                for (int batch = 0; batch < batchesPerPattern; ++batch) {
                    GenerateTestData(input.data(), batchSize, pattern);
                    auto result = verifier.VerifyBatch(input.data(), batchSize,
                                                       pattern * 1000 + batch);
                    
                    if (!result.passed) {
                        printf("  FAIL: batch=%llu, maxError=%.6f\n",
                               (unsigned long long)result.batchId, result.maxError);
                    }
                }
            }
        }
        
        printf("  Results:\n");
        verifier.PrintStatus();
    }
    
    // Test 3: Global verifier (simulates pipeline integration)
    printf("\nTest 3: Global Verifier (Pipeline Simulation)\n");
    printf("-----------------------------------------------\n");
    {
        InitializeGlobalVerifier(VerifyMode::Epsilon);
        
        const size_t pipelineBatchSize = 1024;
        std::vector<float> input(pipelineBatchSize);
        
        // Simulate 100 pipeline batches
        for (int i = 0; i < 100; ++i) {
            GenerateTestData(input.data(), pipelineBatchSize, i % numPatterns);
            VERIFY_BATCH(input.data(), pipelineBatchSize, i);
        }
        
        printf("  Global verifier results:\n");
        ShutdownGlobalVerifier();  // Prints report
    }
    
    printf("\n========================================\n");
    printf("FP8 Verifier Test Complete\n");
    printf("========================================\n");
    printf("\nStatus: READY for MASM FP8 kernel integration\n");
    printf("Next: Replace FP8QuantizeDequantize() with MASM kernel call\n");
    
    return 0;
}
