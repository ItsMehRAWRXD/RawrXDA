#include <immintrin.h>
#include <cstdint>

extern "C" float q4_0_dot_avx2(const uint8_t* q4, const float* a, int n, float scale) {
#if defined(__AVX2__)
    __m256 acc = _mm256_setzero_ps();
    
    for (int i = 0; i < n; i += 16) {
        alignas(32) int8_t nibbles[16];
        for (int j = 0; j < 16; j += 2) {
            int idx = i + j;
            uint8_t byte = q4[idx >> 1];
            nibbles[j] = (byte & 0xF);
            nibbles[j + 1] = (byte >> 4) & 0xF;
        }
        
        __m128i nib_lo = _mm_loadu_si128((__m128i*)nibbles);
        __m128i nib_hi = _mm_loadu_si128((__m128i*)(nibbles + 8));
        
        __m256i w_i32 = _mm256_cvtepi8_epi32(nib_lo);
        __m256i offset = _mm256_set1_epi32(8);
        __m256i centered = _mm256_sub_epi32(w_i32, offset);
        __m256 w_fp = _mm256_cvtepi32_ps(centered);
        __m256 w_scaled = _mm256_mul_ps(w_fp, _mm256_set1_ps(scale));
        
        __m256 a_vec = _mm256_loadu_ps(&a[i]);
        acc = _mm256_fmadd_ps(w_scaled, a_vec, acc);
        
        w_i32 = _mm256_cvtepi8_epi32(nib_hi);
        centered = _mm256_sub_epi32(w_i32, offset);
        w_fp = _mm256_cvtepi32_ps(centered);
        w_scaled = _mm256_mul_ps(w_fp, _mm256_set1_ps(scale));
        
        a_vec = _mm256_loadu_ps(&a[i + 8]);
        acc = _mm256_fmadd_ps(w_scaled, a_vec, acc);
    }
    
    alignas(32) float temp[8];
    _mm256_storeu_ps(temp, acc);
    return temp[0] + temp[1] + temp[2] + temp[3] + temp[4] + temp[5] + temp[6] + temp[7];
#else
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        int byteIndex = i >> 1;
        bool hi = (i & 1) != 0;
        uint8_t byte = q4[byteIndex];
        int8_t nib = hi ? ((byte >> 4) & 0xF) : (byte & 0xF);
        float w = (float)(nib - 8) * scale;
        sum += a[i] * w;
    }
    return sum;
#endif
}
