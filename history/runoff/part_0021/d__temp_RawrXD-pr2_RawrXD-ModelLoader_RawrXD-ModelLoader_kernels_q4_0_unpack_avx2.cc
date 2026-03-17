#include <immintrin.h>
#include <cstdint>
#include <cstring>

extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
#if defined(__AVX2__)
    constexpr int N = 64;
    constexpr int K = 64;
    
    for (int k = 0; k < K; ++k) {
        for (int n = 0; n < N; n += 8) {
            __m256i indices = _mm256_setr_epi32(
                (k * N + n) >> 1,
                (k * N + n + 1) >> 1,
                (k * N + n + 2) >> 1,
                (k * N + n + 3) >> 1,
                (k * N + n + 4) >> 1,
                (k * N + n + 5) >> 1,
                (k * N + n + 6) >> 1,
                (k * N + n + 7) >> 1
            );
            
            alignas(32) uint8_t bytes[8];
            for (int i = 0; i < 8; ++i) {
                int idx = k * N + n + i;
                bytes[i] = q4[idx >> 1];
            }
            
            alignas(32) int8_t nibbles[8];
            for (int i = 0; i < 8; ++i) {
                int idx = k * N + n + i;
                bool hi = (idx & 1) != 0;
                nibbles[i] = hi ? ((bytes[i] >> 4) & 0xF) : (bytes[i] & 0xF);
            }
            
            __m256i offset = _mm256_set1_epi32(8);
            __m256i nib_vec = _mm256_cvtepi8_epi32(_mm_loadl_epi64((__m128i*)nibbles));
            __m256i centered = _mm256_sub_epi32(nib_vec, offset);
            __m256 fp = _mm256_cvtepi32_ps(centered);
            __m256 scaled = _mm256_mul_ps(fp, _mm256_set1_ps(scale));
            
            _mm256_storeu_ps(&fp32[k * N + n], scaled);
        }
    }
#else
    for (int k = 0; k < 64; ++k) {
        for (int n = 0; n < 64; ++n) {
            int idx = k * 64 + n;
            int byteIndex = idx >> 1;
            bool hi = (idx & 1) != 0;
            uint8_t byte = q4[byteIndex];
            int8_t nib = hi ? ((byte >> 4) & 0xF) : (byte & 0xF);
            fp32[idx] = (float)(nib - 8) * scale;
        }
    }
#endif
}
