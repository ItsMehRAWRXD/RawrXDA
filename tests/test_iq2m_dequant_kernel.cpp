// ============================================================================
// test_iq2m_dequant_kernel.cpp
// Unit Test: IQ2_M 2.5-bit Dequantization Kernel Correctness
// ============================================================================
// Validates:
//   - Scalar fallback correctness (reference implementation)
//   - AVX2 kernel output matches scalar (if available)
//   - AVX-512 kernel output matches scalar (if available)
//   - Batch dequantization produces correct total count
//   - Memory-mapped pointer safety (read-only, no internal buffers)
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <math>
#include <vector>
#include <chrono>

#include "compression/iq2m_dequant_kernel.h"

using namespace RawrXD::Compression;

// ============================================================================
// Test Helpers
// ============================================================================

static bool FloatsEqual(float a, float b, float tolerance = 0.001f) {
    if (std::isnan(a) && std::isnan(b)) return true;
    if (std::isinf(a) && std::isinf(b)) return (a > 0) == (b > 0);
    return std::fabs(a - b) <= tolerance;
}

static bool BuffersEqual(const float* a, const float* b, uint32_t count, float tolerance = 0.001f) {
    for (uint32_t i = 0; i < count; ++i) {
        if (!FloatsEqual(a[i], b[i], tolerance)) {
            fprintf(stderr, "  Mismatch at index %u: expected %.6f, got %.6f\n", i, a[i], b[i]);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Generate a test block with known values
// ============================================================================

static void GenerateTestBlock(uint8_t* block, float scale) {
    // Scale factor (first 4 bytes)
    std::memcpy(block, &scale, sizeof(float));

    // 256 weights as 2-bit values packed into 64 bytes
    // Pattern: 0,1,2,3,0,1,2,3,... (repeating)
    for (uint32_t i = 0; i < 64; ++i) {
        uint8_t packed = 0;
        for (uint32_t j = 0; j < 4; ++j) {
            uint32_t val = (i * 4 + j) % 4;  // 0,1,2,3 repeating
            packed |= (val << (j * 2));
        }
        block[4 + i] = packed;
    }
}

static void GenerateRandomBlock(uint8_t* block, float scale) {
    std::memcpy(block, &scale, sizeof(float));
    for (uint32_t i = 0; i < 64; ++i) {
        block[4 + i] = static_cast<uint8_t>(rand() & 0xFF);
    }
}

// ============================================================================
// Test 1: Scalar Reference Correctness
// ============================================================================

static bool Test_ScalarReference() {
    printf("TEST: Scalar Reference Correctness\n");

    uint8_t block[68];
    float output[256];
    float expected[256];

    const float scale = 2.5f;
    GenerateTestBlock(block, scale);

    // Expected values for pattern 0,1,2,3 repeating
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t val = i % 4;
        switch (val) {
            case 0: expected[i] = -1.0f * scale; break;
            case 1: expected[i] = -0.33333333f * scale; break;
            case 2: expected[i] = +0.33333333f * scale; break;
            case 3: expected[i] = +1.0f * scale; break;
        }
    }

    uint32_t written = IQ2M_DequantBlock_Scalar(block, output);
    if (written != 256) {
        fprintf(stderr, "  FAIL: Expected 256 floats, got %u\n", written);
        return false;
    }

    if (!BuffersEqual(expected, output, 256)) {
        fprintf(stderr, "  FAIL: Output does not match expected values\n");
        return false;
    }

    printf("  PASS\n");
    return true;
}

// ============================================================================
// Test 2: AVX2 vs Scalar Parity
// ============================================================================

static bool Test_AVX2Parity() {
    printf("TEST: AVX2 vs Scalar Parity\n");

    uint32_t features = IQ2M_Init();
    if (!(features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX2))) {
        printf("  SKIP: AVX2 not available on this CPU\n");
        return true;  // Not a failure, just not testable
    }

    uint8_t block[68];
    float scalarOutput[256];
    float avx2Output[256];

    const float scale = 1.337f;
    GenerateRandomBlock(block, scale);

    uint32_t scalarWritten = IQ2M_DequantBlock_Scalar(block, scalarOutput);
    uint32_t avx2Written = IQ2M_DequantBlock_AVX2(block, avx2Output);

    if (scalarWritten != 256 || avx2Written != 256) {
        fprintf(stderr, "  FAIL: scalar=%u, avx2=%u (expected 256)\n", scalarWritten, avx2Written);
        return false;
    }

    if (!BuffersEqual(scalarOutput, avx2Output, 256)) {
        fprintf(stderr, "  FAIL: AVX2 output differs from scalar reference\n");
        return false;
    }

    printf("  PASS\n");
    return true;
}

// ============================================================================
// Test 3: AVX-512 vs Scalar Parity
// ============================================================================

static bool Test_AVX512Parity() {
    printf("TEST: AVX-512 vs Scalar Parity\n");

    uint32_t features = IQ2M_Init();
    if (!(features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX512))) {
        printf("  SKIP: AVX-512 not available on this CPU\n");
        return true;
    }

    uint8_t block[68];
    float scalarOutput[256];
    float avx512Output[256];

    const float scale = -0.75f;
    GenerateRandomBlock(block, scale);

    uint32_t scalarWritten = IQ2M_DequantBlock_Scalar(block, scalarOutput);
    uint32_t avx512Written = IQ2M_DequantBlock_AVX512(block, avx512Output);

    if (scalarWritten != 256 || avx512Written != 256) {
        fprintf(stderr, "  FAIL: scalar=%u, avx512=%u (expected 256)\n", scalarWritten, avx512Written);
        return false;
    }

    if (!BuffersEqual(scalarOutput, avx512Output, 256)) {
        fprintf(stderr, "  FAIL: AVX-512 output differs from scalar reference\n");
        return false;
    }

    printf("  PASS\n");
    return true;
}

// ============================================================================
// Test 4: Batch Dequantization
// ============================================================================

static bool Test_BatchDequant() {
    printf("TEST: Batch Dequantization\n");

    const uint32_t blockCount = 16;
    const uint32_t blockStride = 68;  // 4 bytes scale + 64 bytes packed

    std::vector<uint8_t> compressed(blockCount * blockStride);
    std::vector<float> output(blockCount * 256);
    std::vector<float> expected(blockCount * 256);

    // Generate blocks with varying scales
    for (uint32_t b = 0; b < blockCount; ++b) {
        float scale = 0.5f + b * 0.1f;
        GenerateTestBlock(&compressed[b * blockStride], scale);

        // Compute expected values
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t val = i % 4;
            switch (val) {
                case 0: expected[b * 256 + i] = -1.0f * scale; break;
                case 1: expected[b * 256 + i] = -0.33333333f * scale; break;
                case 2: expected[b * 256 + i] = +0.33333333f * scale; break;
                case 3: expected[b * 256 + i] = +1.0f * scale; break;
            }
        }
    }

    uint32_t totalWritten = IQ2M_DequantBatch(
        compressed.data(),
        output.data(),
        blockCount,
        blockStride);

    if (totalWritten != blockCount * 256) {
        fprintf(stderr, "  FAIL: Expected %u floats, got %u\n", blockCount * 256, totalWritten);
        return false;
    }

    if (!BuffersEqual(expected.data(), output.data(), blockCount * 256)) {
        fprintf(stderr, "  FAIL: Batch output does not match expected\n");
        return false;
    }

    printf("  PASS\n");
    return true;
}

// ============================================================================
// Test 5: Null Pointer Safety
// ============================================================================

static bool Test_NullSafety() {
    printf("TEST: Null Pointer Safety\n");

    float dummy[256];

    uint32_t r1 = IQ2M_DequantBlock_Scalar(nullptr, dummy);
    uint32_t r2 = IQ2M_DequantBlock_Scalar(dummy, nullptr);

    if (r1 != 0 || r2 != 0) {
        fprintf(stderr, "  FAIL: Null pointers should return 0 (got %u, %u)\n", r1, r2);
        return false;
    }

    uint32_t r3 = IQ2M_DequantBatch(nullptr, dummy, 1, 68);
    uint32_t r4 = IQ2M_DequantBatch(dummy, nullptr, 1, 68);
    uint32_t r5 = IQ2M_DequantBatch(dummy, dummy, 0, 68);

    if (r3 != 0 || r4 != 0 || r5 != 0) {
        fprintf(stderr, "  FAIL: Invalid batch params should return 0\n");
        return false;
    }

    printf("  PASS\n");
    return true;
}

// ============================================================================
// Test 6: Performance Benchmark (optional)
// ============================================================================

static bool Test_Performance() {
    printf("TEST: Performance Benchmark\n");

    const uint32_t blockCount = 10000;  // ~10K blocks = ~2.5M weights
    const uint32_t blockStride = 68;

    std::vector<uint8_t> compressed(blockCount * blockStride);
    std::vector<float> output(blockCount * 256);

    for (uint32_t b = 0; b < blockCount; ++b) {
        GenerateRandomBlock(&compressed[b * blockStride], 1.0f);
    }

    // Warmup
    IQ2M_DequantBatch(compressed.data(), output.data(), 100, blockStride);

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    uint32_t total = IQ2M_DequantBatch(compressed.data(), output.data(), blockCount, blockStride);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double weightsPerSec = (total / (elapsed / 1000000.0)) / 1000000.0;  // Millions of weights/sec

    printf("  Dequantized %u weights in %lld us (%.2f M weights/sec)\n",
           total, elapsed, weightsPerSec);
    printf("  PASS\n");
    return true;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("=================================================================\n");
    printf("RawrXD IQ2_M Dequantization Kernel Test Suite\n");
    printf("=================================================================\n\n");

    // Detect features
    uint32_t features = IQ2M_Init();
    printf("CPU Features:\n");
    printf("  AVX2:   %s\n", (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX2)) ? "YES" : "NO");
    printf("  AVX512: %s\n", (features & static_cast<uint32_t>(IQ2MFeatureFlags::AVX512)) ? "YES" : "NO");
    printf("\n");

    int passed = 0;
    int failed = 0;

    #define RUN_TEST(name) \
        if (Test_##name()) { passed++; } else { failed++; }

    RUN_TEST(ScalarReference);
    RUN_TEST(AVX2Parity);
    RUN_TEST(AVX512Parity);
    RUN_TEST(BatchDequant);
    RUN_TEST(NullSafety);
    RUN_TEST(Performance);

    printf("\n=================================================================\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("=================================================================\n");

    return failed > 0 ? 1 : 0;
}
