// test_q4_0_kernel.cpp - Standalone test for Q4_0 dot-product kernel
#include <immintrin.h>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <chrono>

// Scalar reference: 32 weights × 32 activations  
float q4_0_dot_32_scalar(const uint8_t* q4_16, float scale, const float* x32) {
    float s = 0.0f;
    for (int i = 0; i < 16; ++i) {  // 16 bytes = 32 nibbles
        uint8_t byte = q4_16[i];
        int8_t lo = (int8_t)((byte & 0xF)) - 8;  // Map 0-15 to -8..7
        int8_t hi = (int8_t)((byte >> 4)) - 8;
        s += (scale * (float)lo) * x32[2 * i + 0];
        s += (scale * (float)hi) * x32[2 * i + 1];
    }
    return s;
}

#ifdef __AVX2__
// AVX2 dot-product: 32 Q4 weights × 32 fp32 activations (optimized - no temp buffer)
float q4_0_dot_32_avx2(const uint8_t* q4, const float* x, float scale) {
    __m256 sum = _mm256_setzero_ps();
    __m256 scale_vec = _mm256_set1_ps(scale);
    __m256 offset_vec = _mm256_set1_ps(-8.0f);  // Map 0-15 to -8..7
    
    for (int i = 0; i < 16; i += 4) {  // Process 4 bytes (8 weights) at a time
        // Load 4 bytes = 8 nibbles
        uint32_t bytes4;
        std::memcpy(&bytes4, &q4[i], 4);
        
        // Extract nibbles using shifts and masks
        alignas(32) uint8_t nibbles[8];
        for (int j = 0; j < 4; ++j) {
            uint8_t byte = (bytes4 >> (j * 8)) & 0xFF;
            nibbles[j * 2 + 0] = byte & 0xF;
            nibbles[j * 2 + 1] = byte >> 4;
        }
        
        // Convert to int32, then float, apply offset and scale
        __m256i nibbles_i32 = _mm256_cvtepu8_epi32(_mm_loadl_epi64((__m128i*)nibbles));
        __m256 nibbles_f32 = _mm256_cvtepi32_ps(nibbles_i32);
        __m256 weights = _mm256_fmadd_ps(_mm256_add_ps(nibbles_f32, offset_vec), scale_vec, _mm256_setzero_ps());
        
        // Load activations and accumulate
        __m256 act = _mm256_loadu_ps(&x[i * 2]);
        sum = _mm256_fmadd_ps(weights, act, sum);
    }
    
    // Horizontal add
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 s  = _mm_add_ps(lo, hi);
    s = _mm_hadd_ps(s, s);
    s = _mm_hadd_ps(s, s);
    return _mm_cvtss_f32(s);
}
#endif

int main() {
    std::cout << "Q4_0 Kernel Test\n";
    std::cout << "================\n\n";
    
    // Test 1: Simple correctness test
    alignas(32) float x[32];
    for (int i = 0; i < 32; ++i) x[i] = float(i + 1);  // 1,2,3...32
    
    uint8_t q4[16];
    for (int i = 0; i < 16; ++i) {
        q4[i] = uint8_t(i | ((i) << 4));  // lo=i, hi=i
    }
    float scale = 0.5f;
    
    float scalar_result = q4_0_dot_32_scalar(q4, scale, x);
    std::cout << "TEST 1 - Correctness:\n";
    std::cout << "  Scalar result: " << scalar_result << "\n";
    
#ifdef __AVX2__
    float avx2_result = q4_0_dot_32_avx2(q4, x, scale);
    std::cout << "  AVX2 result:   " << avx2_result << "\n";
    float diff = std::abs(avx2_result - scalar_result);
    std::cout << "  Difference:    " << diff << "\n";
    
    if (diff < 1e-4f) {
        std::cout << "  ✅ PASS\n\n";
    } else {
        std::cout << "  ❌ FAIL\n\n";
        return 1;
    }
    
    // Test 2: Performance benchmark
    std::cout << "TEST 2 - Performance (1M iterations):\n";
    const int iters = 1000000;
    
    auto t0 = std::chrono::high_resolution_clock::now();
    volatile float s_result = 0;
    for (int i = 0; i < iters; ++i) {
        s_result = q4_0_dot_32_scalar(q4, scale, x);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double scalar_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    t0 = std::chrono::high_resolution_clock::now();
    volatile float a_result = 0;
    for (int i = 0; i < iters; ++i) {
        a_result = q4_0_dot_32_avx2(q4, x, scale);
    }
    t1 = std::chrono::high_resolution_clock::now();
    double avx2_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    std::cout << "  Scalar: " << scalar_ms << " ms\n";
    std::cout << "  AVX2:   " << avx2_ms << " ms\n";
    std::cout << "  Speedup: " << (scalar_ms / avx2_ms) << "×\n";
    
    if (scalar_ms / avx2_ms >= 1.5) {
        std::cout << "  ✅ PASS (AVX2 is faster)\n";
        return 0;
    } else {
        std::cout << "  ⚠️  WARNING: AVX2 not significantly faster\n";
        return 1;
    }
#else
    std::cout << "  ⚠️  AVX2 not available\n";
    return 0;
#endif
}
