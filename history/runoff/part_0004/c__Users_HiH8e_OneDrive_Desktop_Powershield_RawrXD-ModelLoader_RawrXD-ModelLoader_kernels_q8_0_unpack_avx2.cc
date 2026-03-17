#include <immintrin.h>
#include <cstdint>
#include <cstring>

// Unpack 64x64 tile from Q8_0 (int8) to FP32 scratch buffer
// Input: 4096 int8 values (64 rows × 64 cols), 1 scale
// Output: 4096 FP32 values (dequantized)
extern "C" void q8_0_unpack_64x64(const int8_t* q8, float* fp32, float scale) {
#if defined(__AVX2__)
    constexpr int TILE_SIZE = 64 * 64;
    __m256 scale_vec = _mm256_set1_ps(scale);
    
    // Process 4096 int8 values in chunks of 8
    for (int i = 0; i < TILE_SIZE; i += 8) {
        // Load 8 int8 values
        __m128i q8_i8 = _mm_loadl_epi64((__m128i*)&q8[i]);
        
        // Convert to int32
        __m256i q8_i32 = _mm256_cvtepi8_epi32(q8_i8);
        
        // Convert to float
        __m256 q8_fp = _mm256_cvtepi32_ps(q8_i32);
        
        // Scale and store
        __m256 result = _mm256_mul_ps(q8_fp, scale_vec);
        _mm256_storeu_ps(&fp32[i], result);
    }
#else
    // Scalar fallback
    for (int i = 0; i < 64 * 64; ++i) {
        fp32[i] = (float)q8[i] * scale;
    }
#endif
}
