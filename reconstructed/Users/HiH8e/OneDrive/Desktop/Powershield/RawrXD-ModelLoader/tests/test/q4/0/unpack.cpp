// test_q4_0_unpack.cpp - Benchmark batch dequantization approach
#include <immintrin.h>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>

// Forward declarations
extern "C" void q4_0_unpack_32(const uint8_t* q4_16bytes, float* fp32_32, float scale);
extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale);

// Scalar Q4_0 GEMM reference (no unpacking)
float q4_0_dot_32_scalar(const uint8_t* q4, float scale, const float* x) {
    float sum = 0.0f;
    for (int i = 0; i < 16; ++i) {
        uint8_t byte = q4[i];
        int8_t lo = (int8_t)((byte & 0xF)) - 8;
        int8_t hi = (int8_t)((byte >> 4)) - 8;
        sum += (scale * (float)lo) * x[i*2 + 0];
        sum += (scale * (float)hi) * x[i*2 + 1];
    }
    return sum;
}

// FP32 dot product
float dot_32_fp32(const float* a, const float* b) {
    float sum = 0.0f;
    for (int i = 0; i < 32; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

#ifdef __AVX2__
// FP32 dot product with AVX2
float dot_32_fp32_avx2(const float* a, const float* b) {
    __m256 sum = _mm256_setzero_ps();
    for (int i = 0; i < 4; ++i) {
        __m256 av = _mm256_loadu_ps(a + i*8);
        __m256 bv = _mm256_loadu_ps(b + i*8);
        sum = _mm256_fmadd_ps(av, bv, sum);
    }
    
    // Horizontal add
    __m128 lo = _mm256_castps256_ps128(sum);
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    __m128 s = _mm_add_ps(lo, hi);
    s = _mm_hadd_ps(s, s);
    s = _mm_hadd_ps(s, s);
    return _mm_cvtss_f32(s);
}
#endif

int main() {
    std::cout << "Q4_0 Batch Dequant Benchmark\n";
    std::cout << "============================\n\n";
    
    // Prepare test data
    constexpr int N = 64 * 64;  // 4096 weights
    constexpr int N_blocks = N / 32;  // 128 blocks
    
    std::vector<uint8_t> q4_data(N / 2);  // 2048 bytes
    std::vector<float> x_data(N, 1.0f);
    std::vector<float> fp32_scratch(N);
    
    // Fill with test data
    for (size_t i = 0; i < q4_data.size(); ++i) {
        q4_data[i] = uint8_t(i & 0xFF);
    }
    
    float scale = 0.5f;
    
    // Test 1: Correctness - single block
    std::cout << "TEST 1 - Correctness (32 weights):\n";
    q4_0_unpack_32(q4_data.data(), fp32_scratch.data(), scale);
    float scalar_result = q4_0_dot_32_scalar(q4_data.data(), scale, x_data.data());
    float unpacked_result = dot_32_fp32(fp32_scratch.data(), x_data.data());
    
    std::cout << "  Scalar Q4_0:  " << scalar_result << "\n";
    std::cout << "  Unpack+FP32:  " << unpacked_result << "\n";
    std::cout << "  Difference:   " << std::abs(scalar_result - unpacked_result) << "\n";
    
    if (std::abs(scalar_result - unpacked_result) < 1e-4f) {
        std::cout << "  ✅ PASS\n\n";
    } else {
        std::cout << "  ❌ FAIL\n\n";
        return 1;
    }
    
    // Test 2: Performance - realistic workload (unpack once, use many times)
    std::cout << "TEST 2 - Performance (amortized unpack cost):\n";
    const int iters = 10000;
    
    // Scalar Q4_0 approach (decode on every access)
    auto t0 = std::chrono::high_resolution_clock::now();
    volatile double s_sum = 0;
    for (int iter = 0; iter < iters; ++iter) {
        for (int b = 0; b < N_blocks; ++b) {
            s_sum += q4_0_dot_32_scalar(q4_data.data() + b * 16, scale, x_data.data() + b * 32);
        }
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double scalar_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    // Unpack ONCE + reuse FP32 for all iterations
    t0 = std::chrono::high_resolution_clock::now();
    q4_0_unpack_64x64(q4_data.data(), fp32_scratch.data(), scale);  // Unpack once
    volatile double u_sum = 0;
    for (int iter = 0; iter < iters; ++iter) {
        for (int b = 0; b < N_blocks; ++b) {
            u_sum += dot_32_fp32(fp32_scratch.data() + b * 32, x_data.data() + b * 32);
        }
    }
    t1 = std::chrono::high_resolution_clock::now();
    double unpack_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    std::cout << "  Scalar Q4_0 (decode every time): " << scalar_ms << " ms\n";
    std::cout << "  Unpack once + FP32:               " << unpack_ms << " ms\n";
    std::cout << "  Speedup:                          " << (scalar_ms / unpack_ms) << "×\n\n";
    
#ifdef __AVX2__
    // Unpack ONCE + AVX2 FP32
    t0 = std::chrono::high_resolution_clock::now();
    q4_0_unpack_64x64(q4_data.data(), fp32_scratch.data(), scale);  // Unpack once
    volatile double a_sum = 0;
    for (int iter = 0; iter < iters; ++iter) {
        for (int b = 0; b < N_blocks; ++b) {
            a_sum += dot_32_fp32_avx2(fp32_scratch.data() + b * 32, x_data.data() + b * 32);
        }
    }
    t1 = std::chrono::high_resolution_clock::now();
    double avx2_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    
    std::cout << "  Unpack once + AVX2 FP32:          " << avx2_ms << " ms\n";
    std::cout << "  AVX2 Speedup:                     " << (scalar_ms / avx2_ms) << "×\n\n";
    
    if (scalar_ms / avx2_ms >= 2.0) {
        std::cout << "✅ SUCCESS: AVX2 approach is 2× faster!\n";
        return 0;
    } else if (scalar_ms / avx2_ms >= 1.5) {
        std::cout << "⚠️  PARTIAL: Speedup is " << (scalar_ms / avx2_ms) << "× (target: 2×)\n";
        return 0;
    } else {
        std::cout << "❌ NEED WORK: Speedup only " << (scalar_ms / avx2_ms) << "×\n";
        return 1;
    }
#else
    if (scalar_ms / unpack_ms >= 1.2) {
        std::cout << "✅ Unpack approach helps!\n";
        return 0;
    } else {
        std::cout << "⚠️  No clear win without AVX2\n";
        return 1;
    }
#endif
}
