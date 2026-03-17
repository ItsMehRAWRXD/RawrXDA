#include <immintrin.h>
#include <cstdint>
#include <cstring>

// Q4_0 dequantized dot product for 16 elements (packed 4-bit weights)
// Layout: 16 weights packed into 8 bytes (low nibble = w0, high nibble = w1, ...)
// Dequant: y[i] = scale * w4[i]   (zero-point optional; omitted here)
// Returns: sum_i y[i] * x[i]
extern "C" float q4_0_dot_16_scalar(const uint8_t* w4_16, float scale, const float* x16) {
    float s = 0.0f;
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = w4_16[i];
        uint8_t lo = byte & 0x0F;
        uint8_t hi = (byte >> 4) & 0x0F;
        s += (scale * lo) * x16[2 * i + 0];
        s += (scale * hi) * x16[2 * i + 1];
    }
    return s;
}

#ifdef __AVX2__
extern "C" float q4_0_dot_16_avx2(const uint8_t* w4_16, float scale, const float* x16) {
    // Load 8 bytes (16 4-bit values)
    __m128i bytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(w4_16));
    __m128i mask = _mm_set1_epi8(0x0F);

    // Extract low and high nibbles
    __m128i lo8 = _mm_and_si128(bytes, mask);                    // 8 low nibbles [lo0..lo7]
    __m128i hi8 = _mm_and_si128(_mm_srli_epi16(bytes, 4), mask); // 8 high nibbles [hi0..hi7]

    // Interleave to match x order: [lo0,hi0, lo1,hi1, ..., lo7,hi7]
    __m128i inter = _mm_unpacklo_epi8(lo8, hi8); // [lo0,hi0, lo1,hi1, ..., lo7,hi7]

    // Convert bytes -> int32 -> float for both halves (lower 8, then upper 8)
    __m256i i32_0 = _mm256_cvtepu8_epi32(inter);                // elements 0..7
    __m128i inter_hi8 = _mm_srli_si128(inter, 8);               // bytes 8..15
    __m256i i32_1 = _mm256_cvtepu8_epi32(inter_hi8);            // elements 8..15

    __m256 w0 = _mm256_cvtepi32_ps(i32_0);
    __m256 w1 = _mm256_cvtepi32_ps(i32_1);

    // Scale
    __m256 scale_ps = _mm256_set1_ps(scale);
    w0 = _mm256_mul_ps(w0, scale_ps);
    w1 = _mm256_mul_ps(w1, scale_ps);

    // Load activations
    __m256 x0 = _mm256_loadu_ps(x16 + 0);  // corresponds to [x0..x7]
    __m256 x1 = _mm256_loadu_ps(x16 + 8);  // corresponds to [x8..x15]

    // FMA accumulate
    __m256 acc0 = _mm256_mul_ps(w0, x0);
    __m256 acc1 = _mm256_mul_ps(w1, x1);

    __m256 acc = _mm256_add_ps(acc0, acc1);

    // Horizontal sum of 8 floats
    __m128 low = _mm256_castps256_ps128(acc);
    __m128 high = _mm256_extractf128_ps(acc, 1);
    __m128 sum128 = _mm_add_ps(low, high);
    sum128 = _mm_hadd_ps(sum128, sum128);
    sum128 = _mm_hadd_ps(sum128, sum128);
    float result;
    _mm_store_ss(&result, sum128);
    return result;
}
#else
extern "C" float q4_0_dot_16_avx2(const uint8_t* w4_16, float scale, const float* x16) {
    // Fallback to scalar when AVX2 is unavailable
    return q4_0_dot_16_scalar(w4_16, scale, x16);
}
#endif
