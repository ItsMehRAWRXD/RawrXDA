#include <immintrin.h>
#include <cstdint>

// Unpack Q4_0 quantized values to float using AVX-512
// Q4_0 format: each byte contains two 4-bit values (0-15), mapped to int8 range (-8 to 7)
extern "C" void q4_0_unpack_64x64_avx512(const uint8_t* q4, float* fp32, float scale) {
#if defined(__AVX512F__)
    // Process 64x64 block (4096 elements, 2048 bytes of Q4_0 data)
    // Each AVX-512 register can hold 16 floats, so we process 16 elements at a time

    const __m512 scale_vec = _mm512_set1_ps(scale);
    const __m512 offset_vec = _mm512_set1_ps(-8.0f); // Q4_0 maps 0-15 to -8 to 7

    for (int i = 0; i < 4096; i += 16) {
        // Load 8 bytes (16 nibbles) of Q4_0 data
        __m128i q4_bytes = _mm_loadu_si128((__m128i*)(q4 + (i >> 1)));

        // Unpack low and high nibbles
        __m512i low_nibbles = _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(_mm512_castsi128_si512(q4_bytes), 0));
        __m512i high_nibbles = _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(_mm512_castsi128_si512(_mm_srli_epi16(q4_bytes, 4)), 0));

        // Convert to float and apply offset and scale
        __m512 low_floats = _mm512_cvtepi32_ps(low_nibbles);
        __m512 high_floats = _mm512_cvtepi32_ps(high_nibbles);

        low_floats = _mm512_add_ps(low_floats, offset_vec);
        high_floats = _mm512_add_ps(high_floats, offset_vec);

        low_floats = _mm512_mul_ps(low_floats, scale_vec);
        high_floats = _mm512_mul_ps(high_floats, scale_vec);

        // Store results
        _mm512_storeu_ps(fp32 + i, low_floats);
        _mm512_storeu_ps(fp32 + i + 16, high_floats);
    }
#else
    // Fallback scalar implementation
    for (int i = 0; i < 4096; ++i) {
        int byte_idx = i >> 1;
        bool is_high = (i & 1) != 0;
        uint8_t byte = q4[byte_idx];
        uint8_t nibble = is_high ? (byte >> 4) & 0xF : byte & 0xF;
        int8_t val = static_cast<int8_t>(nibble) - 8; // Map 0-15 to -8 to 7
        fp32[i] = static_cast<float>(val) * scale;
    }
#endif
}