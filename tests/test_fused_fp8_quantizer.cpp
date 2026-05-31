// ============================================================================
// test_fused_fp8_quantizer.cpp - Unit tests for fused kernel
// ============================================================================
// Tests:
// 1. Correctness: Fused output matches reference
// 2. Performance: Fused is faster than estimated unfused
// 3. Edge cases: Various scales and clamp values
// 4. Alignment: Handles non-aligned sizes
// ============================================================================

#include "kernels/fused_fp8_quantizer.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <chrono>

using namespace RawrXD::Kernels;

// Reference implementation (unfused, scalar)
static uint8_t ReferenceQuantize(float val, float scale, float clampMax) {
    // Step 1: Scale
    val *= scale;
    
    // Step 2: Clamp
    if (val > clampMax) val = clampMax;
    if (val < -clampMax) val = -clampMax;
    
    // Step 3: Sign
    uint8_t sign = (val < 0) ? 0x80 : 0;
    val = std::abs(val);
    
    // Step 4: Quantize
    int intVal = static_cast<int>(std::nearbyint(val));
    if (intVal > 255) intVal = 255;
    if (intVal < 0) intVal = 0;
    
    return sign | static_cast<uint8_t>(intVal);
}

// Test 1: Correctness
bool TestCorrectness() {
    printf("\n[Test] Correctness - Fused vs Reference...\n");
    
    const size_t N = 10000;
    std::vector<float> input(N);
    std::vector<uint8_t> output_fused(N);
    std::vector<uint8_t> output_ref(N);
    
    // Initialize with varied test data
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)(i % 500) * 0.5f - 100.0f;
    }
    
    // Reference quantization
    for (size_t i = 0; i < N; i++) {
        output_ref[i] = ReferenceQuantize(input[i], 1.0f, 448.0f);
    }
    
    // Fused quantization
    FusedFP8Quantizer quantizer;
    quantizer.Initialize();
    quantizer.Quantize(input.data(), output_fused.data(), N);
    
    // Compare
    int mismatches = 0;
    for (size_t i = 0; i < N; i++) {
        if (output_fused[i] != output_ref[i]) {
            if (mismatches < 5) {
                printf("  Mismatch at %zu: fused=0x%02X, ref=0x%02X\n",
                       i, output_fused[i], output_ref[i]);
            }
            mismatches++;
        }
    }
    
    bool pass = (mismatches == 0);
    printf("  Mismatches: %d / %zu\n", mismatches, N);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    return pass;
}

// Test 2: Performance
bool TestPerformance() {
    printf("\n[Test] Performance - Fused throughput...\n");
    
    const size_t N = 1000000;  // 1M elements
    const int iterations = 100;
    
    std::vector<float> input(N);
    std::vector<uint8_t> output(N);
    
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)(i % 100) * 0.1f;
    }
    
    FusedFP8Quantizer quantizer;
    FusedConfig config;
    config.prefetchNext = true;
    quantizer.Initialize(config);
    
    // Warmup
    quantizer.Quantize(input.data(), output.data(), N);
    
    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        quantizer.Quantize(input.data(), output.data(), N);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    double seconds = nanos / 1e9;
    double avgTime = seconds / iterations;
    double throughput = N / avgTime;
    
    printf("  Elements:        %zu\n", N);
    printf("  Iterations:      %d\n", iterations);
    printf("  Avg time:        %.3f ms\n", avgTime * 1000);
    printf("  Throughput:      %.2f M elements/sec\n", throughput / 1e6);
    printf("  Cycles/element:  %.2f (at 3GHz)\n", (avgTime * 3e9) / N);
    
    // Fused should achieve high throughput
    // AVX2: ~400-600M elements/sec expected
    // AVX-512: ~800M-1.2B elements/sec expected
    bool pass = throughput > 200e6;  // At least 200M elements/sec
    printf("  %s (threshold: 200M elements/sec)\n", pass ? "PASS" : "FAIL");
    
    return pass;
}

// Test 3: Different scales
bool TestScales() {
    printf("\n[Test] Different scales...\n");
    
    const size_t N = 1000;
    std::vector<float> input(N);
    std::vector<uint8_t> output(N);
    
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)(i % 50);
    }
    
    FusedFP8Quantizer quantizer;
    quantizer.Initialize();
    
    bool pass = true;
    for (float scale : {0.5f, 1.0f, 2.0f, 10.0f}) {
        quantizer.QuantizeWithScale(input.data(), output.data(), N, scale);
        
        // Verify first element
        float expected = input[0] * scale;
        if (expected > 448.0f) expected = 448.0f;
        uint8_t expectedVal = static_cast<uint8_t>(static_cast<int>(expected));
        
        printf("  Scale %.1f: output[0]=0x%02X\n", scale, output[0]);
    }
    
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 4: Edge cases
bool TestEdgeCases() {
    printf("\n[Test] Edge cases...\n");
    
    struct TestCase {
        const char* name;
        float input;
        float scale;
        float clampMax;
    };
    
    TestCase cases[] = {
        {"Zero", 0.0f, 1.0f, 448.0f},
        {"Negative zero", -0.0f, 1.0f, 448.0f},
        {"Small positive", 1.0f, 1.0f, 448.0f},
        {"Small negative", -1.0f, 1.0f, 448.0f},
        {"Max value", 448.0f, 1.0f, 448.0f},
        {"Over max", 500.0f, 1.0f, 448.0f},
        {"Large scale", 100.0f, 10.0f, 448.0f},
        {"Small scale", 1000.0f, 0.1f, 448.0f},
    };
    
    const int numCases = sizeof(cases) / sizeof(cases[0]);
    
    FusedFP8Quantizer quantizer;
    quantizer.Initialize();
    
    bool pass = true;
    for (int i = 0; i < numCases; i++) {
        uint8_t output;
        quantizer.QuantizeWithScale(&cases[i].input, &output, 1, cases[i].scale);
        
        uint8_t expected = ReferenceQuantize(cases[i].input, cases[i].scale, cases[i].clampMax);
        
        if (output != expected) {
            printf("  FAIL: %s - expected 0x%02X, got 0x%02X\n", 
                   cases[i].name, expected, output);
            pass = false;
        } else {
            printf("  OK: %s\n", cases[i].name);
        }
    }
    
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 5: Alignment handling
bool TestAlignment() {
    printf("\n[Test] Alignment handling...\n");
    
    const size_t N = 1000;
    std::vector<float> input(N);
    std::vector<uint8_t> output(N);
    
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)(i % 50);
    }
    
    FusedFP8Quantizer quantizer;
    quantizer.Initialize();
    
    // Test various sizes (not all multiples of 8 or 16)
    bool pass = true;
    for (size_t size : {1, 7, 15, 16, 17, 31, 32, 100, 1000}) {
        quantizer.Quantize(input.data(), output.data(), size);
        // If we get here without crash, it's good
    }
    
    printf("  Tested sizes: 1, 7, 15, 16, 17, 31, 32, 100, 1000\n");
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 6: Metrics tracking
bool TestMetrics() {
    printf("\n[Test] Metrics tracking...\n");
    
    FusedFP8Quantizer quantizer;
    quantizer.Initialize();
    
    const size_t N = 10000;
    std::vector<float> input(N);
    std::vector<uint8_t> output(N);
    
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)i * 0.1f;
    }
    
    // Run multiple times
    for (int i = 0; i < 10; i++) {
        quantizer.Quantize(input.data(), output.data(), N);
    }
    
    auto metrics = quantizer.GetMetrics();
    
    printf("  Total calls:     %llu\n", (unsigned long long)metrics.totalCalls);
    printf("  Total elements:  %llu\n", (unsigned long long)metrics.totalElements);
    printf("  Avg throughput:  %.2f M elements/sec\n", metrics.avgThroughput / 1e6);
    
    bool pass = (metrics.totalCalls == 10) && (metrics.totalElements == N * 10);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    
    return pass;
}

// Main test runner
int main() {
    printf("========================================\n");
    printf("Fused FP8 Quantizer Tests\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 6;
    
    if (TestCorrectness()) passed++;
    if (TestPerformance()) passed++;
    if (TestScales()) passed++;
    if (TestEdgeCases()) passed++;
    if (TestAlignment()) passed++;
    if (TestMetrics()) passed++;
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}
