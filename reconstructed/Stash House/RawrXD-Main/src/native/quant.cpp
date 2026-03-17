#pragma once
#include <windows.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdlib.h>

// Native quantized math implementations (Q4_0, Q8_0)
// Replaces ggml quantization with pure C++20/MASM

// Q4_0 quantization (4-bit)
struct block_q4_0 {
    float d;
    uint8_t qs[16];
};

void quantize_q4_0(const float* src, block_q4_0* dst, int n) {
    for (int i = 0; i < n; i += 32) {
        float amax = 0.0f;
        for (int j = 0; j < 32; j++) {
            amax = max(amax, fabsf(src[i + j]));
        }
        const float d = amax / 7.0f;
        dst[i/32].d = d;

        for (int j = 0; j < 32; j += 2) {
            const uint8_t q1 = (uint8_t)roundf(src[i + j] / d) + 8;
            const uint8_t q2 = (uint8_t)roundf(src[i + j + 1] / d) + 8;
            dst[i/32].qs[j/2] = (q1 & 0x0F) | ((q2 & 0x0F) << 4);
        }
    }
}

void dequantize_q4_0(const block_q4_0* src, float* dst, int n) {
    for (int i = 0; i < n; i += 32) {
        const float d = src[i/32].d;
        for (int j = 0; j < 16; j++) {
            const uint8_t q = src[i/32].qs[j];
            dst[i + j*2] = (q & 0x0F) * d;
            dst[i + j*2 + 1] = ((q >> 4) & 0x0F) * d;
        }
    }
}

// Q8_0 quantization (8-bit)
struct block_q8_0 {
    float d;
    int8_t qs[32];
};

void quantize_q8_0(const float* src, block_q8_0* dst, int n) {
    for (int i = 0; i < n; i += 32) {
        float amax = 0.0f;
        for (int j = 0; j < 32; j++) {
            amax = max(amax, fabsf(src[i + j]));
        }
        const float d = amax / 127.0f;
        dst[i/32].d = d;

        for (int j = 0; j < 32; j++) {
            dst[i/32].qs[j] = (int8_t)roundf(src[i + j] / d);
        }
    }
}

void dequantize_q8_0(const block_q8_0* src, float* dst, int n) {
    for (int i = 0; i < n; i += 32) {
        const float d = src[i/32].d;
        for (int j = 0; j < 32; j++) {
            dst[i + j] = src[i/32].qs[j] * d;
        }
    }
}

// AVX-512 accelerated matrix multiplication for quantized tensors
void ggml_vec_dot_q4_0_q8_0(const int n, float* restrict s, const void* restrict vx, const void* restrict vy) {
    const block_q4_0* x = (const block_q4_0*)vx;
    const block_q8_0* y = (const block_q8_0*)vy;

    float sum = 0.0f;
    for (int i = 0; i < n / 32; i++) {
        const float d1 = x[i].d;
        const float d2 = y[i].d;

        // Simple scalar implementation - replace with AVX-512 in MASM
        for (int j = 0; j < 16; j++) {
            const uint8_t q4 = x[i].qs[j];
            const int8_t q8_1 = y[i].qs[j*2];
            const int8_t q8_2 = y[i].qs[j*2 + 1];

            sum += (q4 & 0x0F) * q8_1 * d1 * d2;
            sum += ((q4 >> 4) & 0x0F) * q8_2 * d1 * d2;
        }
    }
    *s = sum;
}

// AVX-512 accelerated Q4_0 x Q8_0 dot product
void ggml_vec_dot_q4_0_q8_0_avx512(const int n, float* restrict s, const void* restrict vx, const void* restrict vy) {
#ifdef __AVX512F__
    const block_q4_0* x = (const block_q4_0*)vx;
    const block_q8_0* y = (const block_q8_0*)vy;
    const int nb = n / 32;

    __m512 acc = _mm512_setzero_ps();
    const __m512i lowMask = _mm512_set1_epi8(0x0F);

    for (int i = 0; i < nb; i += 1) {
        // Broadcast scale factors
        __m512 d0 = _mm512_set1_ps(x[i].d);
        __m512 d1 = _mm512_set1_ps(y[i].d);
        __m512 dProd = _mm512_mul_ps(d0, d1);

        // Load 16 bytes of Q4 quantized data -> expand to 32 int8 values
        __m128i qx_raw = _mm_loadu_si128((__m128i*)x[i].qs);
        // Low nibbles
        __m128i qx_lo = _mm_and_si128(qx_raw, _mm_set1_epi8(0x0F));
        // High nibbles
        __m128i qx_hi = _mm_and_si128(_mm_srli_epi16(qx_raw, 4), _mm_set1_epi8(0x0F));
        // Interleave: lo0, hi0, lo1, hi1, ...
        __m256i qx_32 = _mm256_set_m128i(qx_hi, qx_lo);

        // Load 32 bytes of Q8 quantized data
        __m256i qy_32 = _mm256_loadu_si256((__m256i*)y[i].qs);

        // Convert to int16 and multiply-add pairs
        // Q4 values are unsigned 0-15, subtract 8 to center
        __m256i qx_16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(qx_32));
        __m256i qy_16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(qy_32));
        __m256i qx_16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(qx_32, 1));
        __m256i qy_16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(qy_32, 1));

        // Subtract 8 from Q4 values to center around zero
        __m256i offset = _mm256_set1_epi16(8);
        qx_16_lo = _mm256_sub_epi16(qx_16_lo, offset);
        qx_16_hi = _mm256_sub_epi16(qx_16_hi, offset);

        // Integer multiply and horizontal add pairs -> int32
        __m256i prod_lo = _mm256_madd_epi16(qx_16_lo, qy_16_lo);
        __m256i prod_hi = _mm256_madd_epi16(qx_16_hi, qy_16_hi);

        // Sum all int32 lanes
        __m256i sum32 = _mm256_add_epi32(prod_lo, prod_hi);
        // Horizontal reduce: 8 x int32 -> scalar
        __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(sum32), _mm256_extracti128_si256(sum32, 1));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0x4E));
        sum128 = _mm_add_epi32(sum128, _mm_shuffle_epi32(sum128, 0xB1));
        float isum = (float)_mm_cvtsi128_si32(sum128);

        // Accumulate with scale
        *s += isum * x[i].d * y[i].d;
    }
#else
    // Fallback to scalar
    ggml_vec_dot_q4_0_q8_0(n, s, vx, vy);
#endif
}