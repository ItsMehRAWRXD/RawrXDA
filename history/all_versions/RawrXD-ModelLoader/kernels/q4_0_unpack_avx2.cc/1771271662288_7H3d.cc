#include <immintrin.h>
#include <cstdint>

// Unpack Q4_0 quantized values to float using AVX2
// Q4_0 format: each byte contains two 4-bit values (0-15), mapped to int8 range (-8 to 7)
extern "C" void q4_0_unpack_64x64(const uint8_t* q4, float* fp32, float scale) {
    // Process 64x64 block (4096 elements, 2048 bytes of Q4_0 data)
    // Each AVX2 register can hold 8 floats, so we process 8 elements at a time

    const __m256 scale_vec = _mm256_set1_ps(scale);
    const __m256 offset_vec = _mm256_set1_ps(-8.0f); // Q4_0 maps 0-15 to -8 to 7

    for (int i = 0; i < 4096; i += 8) {
        // Load 4 bytes (8 nibbles) of Q4_0 data
        __m128i q4_bytes = _mm_loadu_si128((__m128i*)(q4 + (i >> 1)));

        // Unpack low and high nibbles
        __m128i low_bytes = _mm_and_si128(q4_bytes, _mm_set1_epi8(0x0F));
        __m128i high_bytes = _mm_and_si128(_mm_srli_epi16(q4_bytes, 4), _mm_set1_epi8(0x0F));

        // Convert to float and apply offset and scale
        __m256 low_floats = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(low_bytes));
        __m256 high_floats = _mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(high_bytes));

        low_floats = _mm256_add_ps(low_floats, offset_vec);
        high_floats = _mm256_add_ps(high_floats, offset_vec);

        low_floats = _mm256_mul_ps(low_floats, scale_vec);
        high_floats = _mm256_mul_ps(high_floats, scale_vec);

        // Store results (interleave low and high)
        _mm256_storeu_ps(fp32 + i, low_floats);
        _mm256_storeu_ps(fp32 + i + 8, high_floats);
    }
}