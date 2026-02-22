#include <immintrin.h>
#include <cstdint>
#include <cstring>

// Scalar reference: Q8_0 dequant + dot product for 32 elements
extern "C" float q8_0_dot_scalar(const int8_t* q8, const float* a, float scale) {
    float sum = 0.0f;
    for (int i = 0; i < 32; ++i) {
        sum += a[i] * ((float)q8[i] * scale);
    }
    return sum;
}

// AVX2 optimized: Q8_0 dequant + dot product for 32 elements
extern "C" float q8_0_dot_avx2(const int8_t* q8, const float* a, float scale) {
#if defined(__AVX2__)
    __m256 scale_vec = _mm256_set1_ps(scale);
    __m256 acc = _mm256_setzero_ps();
    
    // Process 32 int8 values in 4 chunks of 8
    for (int i = 0; i < 32; i += 8) {
        // Load 8 int8 values and convert to int32
        __m128i q8_i8 = _mm_loadl_epi64((__m128i*)&q8[i]);
        __m256i q8_i32 = _mm256_cvtepi8_epi32(q8_i8);
        
        // Convert to float
        __m256 q8_fp = _mm256_cvtepi32_ps(q8_i32);
        
        // Scale weights
        __m256 w = _mm256_mul_ps(q8_fp, scale_vec);
        
        // Load activations and FMA
        __m256 a_vec = _mm256_loadu_ps(&a[i]);
        acc = _mm256_fmadd_ps(w, a_vec, acc);
    }
    
    // Horizontal sum
    alignas(32) float temp[8];
    _mm256_storeu_ps(temp, acc);
    return temp[0] + temp[1] + temp[2] + temp[3] + temp[4] + temp[5] + temp[6] + temp[7];
#else
    return q8_0_dot_scalar(q8, a, scale);
#endif
}
