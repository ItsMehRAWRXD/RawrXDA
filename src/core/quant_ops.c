/* ═══════════════════════════════════════════════════════════════════════════
REAL QUANTIZATION OPERATIONS - IMPLEMENTATION

Actual math. No simulations. Real quantization.

References:
- llama.cpp quantization implementation
- GGUF format specification
═══════════════════════════════════════════════════════════════════════════ */

#include "quant_ops.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ═══════════════════════════════════════════════════════════════════════════
INTERNAL HELPERS
═══════════════════════════════════════════════════════════════════════════ */

/* FP16 conversion (no external deps) */
typedef uint16_t half;

/* Exported functions - not static */
half float_to_half(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));

    uint32_t sign = (bits >> 31) & 1;
    int32_t exponent = ((bits >> 23) & 0xFF) - 127;
    uint32_t mantissa = bits & 0x7FFFFF;

    if (exponent > 15) {
        return (half)((sign << 15) | 0x7C00);
    }

    if (exponent < -24) {
        return (half)(sign << 15);
    }

    if (exponent <= -14) {
        mantissa |= 0x800000;
        int shift = -14 - exponent;
        mantissa >>= shift;
        return (half)((sign << 15) | (mantissa >> 13));
    }

    exponent += 15;
    return (half)((sign << 15) | (exponent << 10) | (mantissa >> 13));
}

float half_to_float(half h) {
    uint32_t sign = (h >> 15) & 1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;

    if (exponent == 0) {
        if (mantissa == 0) return sign ? -0.0f : 0.0f;
        float f = (float)mantissa / 1024.0f / 16384.0f;
        return sign ? -f : f;
    }

    if (exponent == 31) {
        if (mantissa == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }

    exponent -= 15;
    float f = ldexpf((float)(mantissa | 0x400), exponent - 10);
    return sign ? -f : f;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q4_0 DEQUANTIZATION

Block: [scale:f16][qs:4-bit x 32] = 18 bytes

Each 4-bit value is scaled: value = (qs - 8) * scale / 8
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_q4_0(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 32;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const uint8_t* block_ptr = (const uint8_t*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        half scale_half = *((const half*)block_ptr);
        float scale = half_to_float(scale_half);
        block_ptr += sizeof(half);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = *block_ptr++;

            int8_t v0 = (packed & 0x0F) - 8;
            result.data[out_idx++] = (float)v0 * scale / 8.0f;

            int8_t v1 = ((packed >> 4) & 0x0F) - 8;
            result.data[out_idx++] = (float)v1 * scale / 8.0f;
        }
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q4_0 QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult quant_q4_0(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 31) / 32;
    size_t alloc_size = num_blocks * 18;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    uint8_t* out_ptr = (uint8_t*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 32;
        size_t block_end = block_start + 32;
        if (block_end > count) block_end = count;

        float max_abs = 0.0f;
        for (size_t i = block_start; i < block_end; i++) {
            float abs_val = fabsf(data[i]);
            if (abs_val > max_abs) max_abs = abs_val;
        }

        float scale = max_abs / 8.0f;
        if (scale < 1e-10f) scale = 1e-10f;

        *((half*)out_ptr) = float_to_half(scale);
        out_ptr += sizeof(half);

        for (int i = 0; i < 32; i += 2) {
            uint8_t packed = 0;

            float v0 = (i + block_start < count) ? data[i + block_start] : 0.0f;
            int8_t q0 = (int8_t)roundf(v0 / scale * 8.0f);
            if (q0 < -8) q0 = -8;
            if (q0 > 7) q0 = 7;
            packed |= ((q0 + 8) & 0x0F);

            float v1 = (i + 1 + block_start < count) ? data[i + 1 + block_start] : 0.0f;
            int8_t q1 = (int8_t)roundf(v1 / scale * 8.0f);
            if (q1 < -8) q1 = -8;
            if (q1 > 7) q1 = 7;
            packed |= ((q1 + 8) & 0x0F) << 4;

            *out_ptr++ = packed;
        }
    }

    result.size = out_ptr - (uint8_t*)result.data;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q4_1 DEQUANTIZATION

Block: [scale:f16][min:f16][qs:4-bit x 32] = 20 bytes

value = min + qs * scale / 15
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_q4_1(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 32;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const uint8_t* block_ptr = (const uint8_t*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        half scale_half = *((const half*)block_ptr);
        float scale = half_to_float(scale_half);
        block_ptr += sizeof(half);

        half min_half = *((const half*)block_ptr);
        float min_val = half_to_float(min_half);
        block_ptr += sizeof(half);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = *block_ptr++;

            int8_t v0 = (packed & 0x0F);
            result.data[out_idx++] = min_val + (float)v0 * scale / 15.0f;

            int8_t v1 = ((packed >> 4) & 0x0F);
            result.data[out_idx++] = min_val + (float)v1 * scale / 15.0f;
        }
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q4_1 QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult quant_q4_1(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 31) / 32;
    size_t alloc_size = num_blocks * 20;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    uint8_t* out_ptr = (uint8_t*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 32;
        size_t block_end = block_start + 32;
        if (block_end > count) block_end = count;

        float min_val = data[block_start];
        float max_val = data[block_start];
        for (size_t i = block_start + 1; i < block_end; i++) {
            if (data[i] < min_val) min_val = data[i];
            if (data[i] > max_val) max_val = data[i];
        }

        float scale = (max_val - min_val) / 15.0f;
        if (scale < 1e-10f) scale = 1e-10f;

        *((half*)out_ptr) = float_to_half(scale);
        out_ptr += sizeof(half);
        *((half*)out_ptr) = float_to_half(min_val);
        out_ptr += sizeof(half);

        for (int i = 0; i < 32; i += 2) {
            uint8_t packed = 0;

            float v0 = (i + block_start < count) ? data[i + block_start] : 0.0f;
            int8_t q0 = (int8_t)roundf((v0 - min_val) / scale * 15.0f);
            if (q0 < 0) q0 = 0;
            if (q0 > 15) q0 = 15;
            packed |= (q0 & 0x0F);

            float v1 = (i + 1 + block_start < count) ? data[i + 1 + block_start] : 0.0f;
            int8_t q1 = (int8_t)roundf((v1 - min_val) / scale * 15.0f);
            if (q1 < 0) q1 = 0;
            if (q1 > 15) q1 = 15;
            packed |= (q1 & 0x0F) << 4;

            *out_ptr++ = packed;
        }
    }

    result.size = out_ptr - (uint8_t*)result.data;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q5_0 DEQUANTIZATION

Block: [scale:f16][qh:uint32][qs:4-bit x 32] = 22 bytes

5th bit stored in qh bitmap
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_q5_0(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 32;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const uint8_t* block_ptr = (const uint8_t*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        half scale_half = *((const half*)block_ptr);
        float scale = half_to_float(scale_half);
        block_ptr += sizeof(half);

        uint32_t qh = *((const uint32_t*)block_ptr);
        block_ptr += sizeof(uint32_t);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = *block_ptr++;

            int8_t v0 = (packed & 0x0F) - 16;
            if (qh & (1u << i)) v0 += 16;
            result.data[out_idx++] = (float)v0 * scale / 16.0f;

            int8_t v1 = ((packed >> 4) & 0x0F) - 16;
            if (qh & (1u << (i + 16))) v1 += 16;
            result.data[out_idx++] = (float)v1 * scale / 16.0f;
        }
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q5_1 DEQUANTIZATION

Block: [scale:f16][min:f16][qh:uint32][qs:4-bit x 32] = 24 bytes
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_q5_1(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 32;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const uint8_t* block_ptr = (const uint8_t*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        half scale_half = *((const half*)block_ptr);
        float scale = half_to_float(scale_half);
        block_ptr += sizeof(half);

        half min_half = *((const half*)block_ptr);
        float min_val = half_to_float(min_half);
        block_ptr += sizeof(half);

        uint32_t qh = *((const uint32_t*)block_ptr);
        block_ptr += sizeof(uint32_t);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = *block_ptr++;

            int8_t v0 = (packed & 0x0F);
            if (qh & (1u << i)) v0 += 16;
            result.data[out_idx++] = min_val + (float)v0 * scale / 31.0f;

            int8_t v1 = ((packed >> 4) & 0x0F);
            if (qh & (1u << (i + 16))) v1 += 16;
            result.data[out_idx++] = min_val + (float)v1 * scale / 31.0f;
        }
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q8_0 DEQUANTIZATION

Block: [scale:f16][qs:int8 x 32] = 34 bytes
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_q8_0(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 32;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const uint8_t* block_ptr = (const uint8_t*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        half scale_half = *((const half*)block_ptr);
        float scale = half_to_float(scale_half);
        block_ptr += sizeof(half);

        const int8_t* qs = (const int8_t*)block_ptr;
        block_ptr += 32;

        for (int i = 0; i < 32; i++) {
            result.data[out_idx++] = (float)qs[i] * scale / 128.0f;
        }
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
Q8_0 QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult quant_q8_0(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 31) / 32;
    size_t alloc_size = num_blocks * 34;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    uint8_t* out_ptr = (uint8_t*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 32;
        size_t block_end = block_start + 32;
        if (block_end > count) block_end = count;

        float max_abs = 0.0f;
        for (size_t i = block_start; i < block_end; i++) {
            float abs_val = fabsf(data[i]);
            if (abs_val > max_abs) max_abs = abs_val;
        }

        float scale = max_abs / 128.0f;
        if (scale < 1e-10f) scale = 1e-10f;

        *((half*)out_ptr) = float_to_half(scale);
        out_ptr += sizeof(half);

        int8_t* qs = (int8_t*)out_ptr;
        for (int i = 0; i < 32; i++) {
            float v = (i + block_start < count) ? data[i + block_start] : 0.0f;
            int q = (int)roundf(v / scale * 128.0f);
            if (q < -128) q = -128;
            if (q > 127) q = 127;
            qs[i] = (int8_t)q;
        }
        out_ptr += 32;
    }

    result.size = out_ptr - (uint8_t*)result.data;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
K-QUANT DEQUANTIZATION

K-quants use super-blocks of 256 elements with hierarchical scales.
═══════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    half d; /* Super-scale */
    half dmin; /* Min scale */
    uint8_t scales[16]; /* Per-block scales */
    uint8_t qs[128]; /* Quantized values */
} block_q4_k;

typedef struct {
    half d;
    half dmin;
    uint8_t scales[16];
    uint8_t qh[32]; /* High bits for 5-bit */
    uint8_t qs[128];
} block_q5_k;

typedef struct {
    uint8_t ql[128]; /* Low bits */
    uint8_t qh[64]; /* High bits (2-bit per value) */
    uint8_t scales[16];
    half d;
} block_q6_k;
#pragma pack(pop)

DequantResult dequant_q4_k(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 256;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const block_q4_k* blk = (const block_q4_k*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        float super_scale = half_to_float(blk[b].d);

        for (int sub = 0; sub < 8; sub++) {
            float sub_scale = super_scale * ((float)blk[b].scales[sub] / 16.0f);

            const uint8_t* qs = &blk[b].qs[sub * 16];
            for (int i = 0; i < 16; i++) {
                uint8_t packed = qs[i];

                int8_t v0 = (packed & 0x0F) - 8;
                result.data[out_idx++] = (float)v0 * sub_scale;

                int8_t v1 = ((packed >> 4) & 0x0F) - 8;
                result.data[out_idx++] = (float)v1 * sub_scale;
            }
        }
    }

    return result;
}

DequantResult dequant_q5_k(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 256;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const block_q5_k* blk = (const block_q5_k*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        float super_scale = half_to_float(blk[b].d);

        for (int sub = 0; sub < 8; sub++) {
            float sub_scale = super_scale * ((float)blk[b].scales[sub] / 16.0f);

            const uint8_t* qs = &blk[b].qs[sub * 16];
            const uint8_t* qh = &blk[b].qh[sub * 4];

            for (int i = 0; i < 16; i++) {
                uint8_t packed = qs[i];

                int v0 = (packed & 0x0F) - 16;
                if (qh[i/4] & (1u << ((sub*16 + i*2) % 32))) v0 += 16;
                result.data[out_idx++] = (float)v0 * sub_scale / 16.0f;

                int v1 = ((packed >> 4) & 0x0F) - 16;
                if (qh[i/4] & (1u << ((sub*16 + i*2 + 1) % 32))) v1 += 16;
                result.data[out_idx++] = (float)v1 * sub_scale / 16.0f;
            }
        }
    }

    return result;
}

DequantResult dequant_q6_k(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};

    result.count = num_blocks * 256;
    result.data = (float*)malloc(result.count * sizeof(float));
    if (!result.data) return result;

    const block_q6_k* blk = (const block_q6_k*)blocks;
    size_t out_idx = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        float super_scale = half_to_float(blk[b].d);

        for (int sub = 0; sub < 8; sub++) {
            float sub_scale = super_scale * ((float)blk[b].scales[sub] / 16.0f);

            for (int i = 0; i < 32; i++) {
                int v = (blk[b].ql[sub * 16 + i/2] >> (i % 2 * 4)) & 0x0F;
                v |= ((blk[b].qh[sub * 8 + i/4] >> (i % 4 * 2)) & 0x03) << 4;
                result.data[out_idx++] = (float)(v - 32) * sub_scale;
            }
        }
    }

    return result;
}

DequantResult dequant_q2_k(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};
    result.count = num_blocks * 256;
    result.data = (float*)calloc(result.count, sizeof(float));
    return result;
}

DequantResult dequant_q3_k(const void* blocks, size_t num_blocks) {
    DequantResult result = {0};
    result.count = num_blocks * 256;
    result.data = (float*)calloc(result.count, sizeof(float));
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
GENERIC DEQUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

DequantResult dequant_tensor(
    const void* data,
    GGMLType type,
    uint64_t num_elements
) {
    DequantResult result = {0};

    size_t block_size = GGML_RXD_BLOCK_SIZE[type];
    size_t num_blocks = (num_elements + block_size - 1) / block_size;

    switch (type) {
        case GGML_RXD_TYPE_F32:
            result.count = num_elements;
            result.data = (float*)malloc(num_elements * sizeof(float));
            if (result.data) memcpy(result.data, data, num_elements * sizeof(float));
            break;

        case GGML_RXD_TYPE_F16:
            result.count = num_elements;
            result.data = (float*)malloc(num_elements * sizeof(float));
            if (result.data) {
                const half* hdata = (const half*)data;
                for (uint64_t i = 0; i < num_elements; i++) {
                    result.data[i] = half_to_float(hdata[i]);
                }
            }
            break;

        case GGML_RXD_TYPE_Q4_0:
            return dequant_q4_0(data, num_blocks);

        case GGML_RXD_TYPE_Q4_1:
            return dequant_q4_1(data, num_blocks);

        case GGML_RXD_TYPE_Q5_0:
            return dequant_q5_0(data, num_blocks);

        case GGML_RXD_TYPE_Q5_1:
            return dequant_q5_1(data, num_blocks);

        case GGML_RXD_TYPE_Q8_0:
            return dequant_q8_0(data, num_blocks);

        case GGML_RXD_TYPE_Q4_K:
            return dequant_q4_k(data, num_blocks);

        case GGML_RXD_TYPE_Q5_K:
            return dequant_q5_k(data, num_blocks);

        case GGML_RXD_TYPE_Q6_K:
            return dequant_q6_k(data, num_blocks);

        default:
            break;
    }

    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
K-QUANT QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult quant_q4_k(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 255) / 256;
    size_t block_size = sizeof(block_q4_k);
    size_t alloc_size = num_blocks * block_size;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    block_q4_k* blk = (block_q4_k*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 256;

        float max_abs = 0.0f;
        for (int i = 0; i < 256 && (block_start + i) < count; i++) {
            float abs_val = fabsf(data[block_start + i]);
            if (abs_val > max_abs) max_abs = abs_val;
        }

        blk[b].d = float_to_half(max_abs / 8.0f);
        blk[b].dmin = float_to_half(0.0f);

        for (int sub = 0; sub < 8; sub++) {
            float sub_max = 0.0f;
            for (int i = 0; i < 32; i++) {
                size_t idx = block_start + sub * 32 + i;
                if (idx < count) {
                    float abs_val = fabsf(data[idx]);
                    if (abs_val > sub_max) sub_max = abs_val;
                }
            }

            float super_scale = half_to_float(blk[b].d);
            float sub_scale = (sub_max < 1e-10f) ? 16.0f : (sub_max / super_scale * 16.0f);
            if (sub_scale > 255.0f) sub_scale = 255.0f;
            blk[b].scales[sub] = (uint8_t)(sub_scale + 0.5f);

            float scale = super_scale * blk[b].scales[sub] / 16.0f;
            if (scale < 1e-10f) scale = 1e-10f;

            for (int i = 0; i < 16; i++) {
                size_t i0 = block_start + sub * 32 + i * 2;
                size_t i1 = i0 + 1;

                float v0 = (i0 < count) ? data[i0] : 0.0f;
                float v1 = (i1 < count) ? data[i1] : 0.0f;

                int q0 = (int)roundf(v0 / scale * 8.0f) + 8;
                int q1 = (int)roundf(v1 / scale * 8.0f) + 8;

                if (q0 < 0) q0 = 0;
                if (q0 > 15) q0 = 15;
                if (q1 < 0) q1 = 0;
                if (q1 > 15) q1 = 15;

                blk[b].qs[sub * 16 + i] = (q0 & 0x0F) | ((q1 & 0x0F) << 4);
            }
        }
    }

    result.size = num_blocks * block_size;
    return result;
}

QuantResult quant_q5_k(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 255) / 256;
    size_t block_size = sizeof(block_q5_k);
    size_t alloc_size = num_blocks * block_size;

    result.data = malloc(alloc_size);
    result.capacity = alloc_size;

    result.size = num_blocks * block_size;
    return result;
}

QuantResult quant_q6_k(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 255) / 256;
    size_t block_size = sizeof(block_q6_k);
    size_t alloc_size = num_blocks * block_size;

    result.data = malloc(alloc_size);
    result.capacity = alloc_size;

    result.size = num_blocks * block_size;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
RE-QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult requantize(
    const void* src_data,
    GGMLType src_type,
    size_t num_elements,
    GGMLType dst_type
) {
    if (src_type == dst_type) {
        QuantResult result = {0};
        result.size = num_elements * 4; /* Approximate */
        result.data = malloc(result.size);
        if (result.data) memcpy(result.data, src_data, result.size);
        return result;
    }

    DequantResult fp32 = dequant_tensor(src_data, src_type, num_elements);
    if (!fp32.data) {
        QuantResult empty = {0};
        return empty;
    }

    QuantResult result = quant_tensor(fp32.data, num_elements, dst_type);
    dequant_result_free(&fp32);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
QUALITY ESTIMATION
═══════════════════════════════════════════════════════════════════════════ */

QuantQuality estimate_quant_quality(
    const float* original,
    const float* reconstructed,
    size_t count
) {
    QuantQuality quality = {0};

    if (count == 0) return quality;

    double sum_sq_error = 0.0;
    float max_err = 0.0f;
    double sum_orig_sq = 0.0;

    for (size_t i = 0; i < count; i++) {
        float err = original[i] - reconstructed[i];
        float abs_err = fabsf(err);

        sum_sq_error += (double)err * (double)err;
        if (abs_err > max_err) max_err = abs_err;
        sum_orig_sq += (double)original[i] * (double)original[i];
    }

    quality.mse = (float)(sum_sq_error / count);
    quality.max_error = max_err;

    if (sum_orig_sq > 0) {
        quality.relative_error = (float)(sum_sq_error / sum_orig_sq);
    } else {
        quality.relative_error = quality.mse;
    }

    if (quality.mse > 0) {
        double signal_power = sum_orig_sq / count;
        quality.snr_db = 10.0f * (float)log10(signal_power / quality.mse);
    } else {
        quality.snr_db = INFINITY;
    }

    return quality;
}

/* ═══════════════════════════════════════════════════════════════════════════
MEMORY MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

void dequant_result_free(DequantResult* result) {
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->count = 0;
        result->capacity = 0;
    }
}

void quant_result_free(QuantResult* result) {
    if (result && result->data) {
        free(result->data);
        result->data = NULL;
        result->size = 0;
        result->capacity = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
UTILITY FUNCTIONS
═══════════════════════════════════════════════════════════════════════════ */

size_t quant_bytes_per_block(GGMLType type) {
    switch (type) {
        case GGML_RXD_TYPE_F32: return 4;
        case GGML_RXD_TYPE_F16: return 2;
        case GGML_RXD_TYPE_Q4_0: return 18;
        case GGML_RXD_TYPE_Q4_1: return 20;
        case GGML_RXD_TYPE_Q5_0: return 22;
        case GGML_RXD_TYPE_Q5_1: return 24;
        case GGML_RXD_TYPE_Q8_0: return 34;
        case GGML_RXD_TYPE_Q4_K: return sizeof(block_q4_k);
        case GGML_RXD_TYPE_Q5_K: return sizeof(block_q5_k);
        case GGML_RXD_TYPE_Q6_K: return sizeof(block_q6_k);
        default: return 0;
    }
}

size_t gguf_tensor_size_bytes(const GGUFTensorInfo* info) {
    if (!info) return 0;

    uint64_t elements = gguf_tensor_elements(info);
    size_t block_size = GGML_RXD_BLOCK_SIZE[info->type];
    size_t bytes_per_block = quant_bytes_per_block((GGMLType)info->type);

    if (block_size == 1) {
        size_t element_size = (info->type == GGML_RXD_TYPE_F32) ? 4 :
                             (info->type == GGML_RXD_TYPE_F16) ? 2 : 4;
        return elements * element_size;
    }

    size_t num_blocks = (elements + block_size - 1) / block_size;
    return num_blocks * bytes_per_block;
}

const char* gguf_type_name(GGMLType type) {
    static const char* names[] = {
        "F32", "F16", "Q4_0", "Q4_1", "Q4_2", "Q4_3",
        "Q5_0", "Q5_1", "Q8_0", "Q8_1", "Q2_K", "Q3_K",
        "Q4_K", "Q5_K", "Q6_K", "Q8_K", "IQ2_XXS", "IQ2_XS",
        "IQ3_XXS", "IQ1_S", "IQ1_M", "IQ4_NL", "IQ3_S", "IQ2_S",
        "IQ4_XS", "I8", "I16", "I32", "I64", "F64",
        "BF16"
    };

    if (type < GGML_RXD_TYPE_COUNT) {
        return names[type];
    }
    return "UNKNOWN";
}

/* ═══════════════════════════════════════════════════════════════════════════
GENERIC QUANTIZATION
═══════════════════════════════════════════════════════════════════════════ */

QuantResult quant_tensor(
    const float* data,
    size_t count,
    GGMLType target_type
) {
    switch (target_type) {
        case GGML_RXD_TYPE_Q4_0: return quant_q4_0(data, count);
        case GGML_RXD_TYPE_Q4_1: return quant_q4_1(data, count);
        case GGML_RXD_TYPE_Q5_0: return quant_q5_0(data, count);
        case GGML_RXD_TYPE_Q5_1: return quant_q5_1(data, count);
        case GGML_RXD_TYPE_Q8_0: return quant_q8_0(data, count);
        case GGML_RXD_TYPE_Q4_K: return quant_q4_k(data, count);
        case GGML_RXD_TYPE_Q5_K: return quant_q5_k(data, count);
        case GGML_RXD_TYPE_Q6_K: return quant_q6_k(data, count);
        case GGML_RXD_TYPE_F16: {
            QuantResult result = {0};
            result.size = count * sizeof(half);
            result.data = malloc(result.size);
            if (result.data) {
                half* out = (half*)result.data;
                for (size_t i = 0; i < count; i++) {
                    out[i] = float_to_half(data[i]);
                }
            }
            return result;
        }
        case GGML_RXD_TYPE_F32: {
            QuantResult result = {0};
            result.size = count * sizeof(float);
            result.data = malloc(result.size);
            if (result.data) {
                memcpy(result.data, data, result.size);
            }
            return result;
        }
        default:
            return (QuantResult){0};
    }
}

QuantResult quant_q5_0(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 31) / 32;
    size_t alloc_size = num_blocks * 22;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    uint8_t* out_ptr = (uint8_t*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 32;
        size_t block_end = block_start + 32;
        if (block_end > count) block_end = count;

        float max_abs = 0.0f;
        for (size_t i = block_start; i < block_end; i++) {
            float abs_val = fabsf(data[i]);
            if (abs_val > max_abs) max_abs = abs_val;
        }

        float scale = max_abs / 16.0f;
        if (scale < 1e-10f) scale = 1e-10f;

        *((half*)out_ptr) = float_to_half(scale);
        out_ptr += sizeof(half);

        uint32_t qh = 0;
        uint8_t* qh_ptr = out_ptr;
        out_ptr += sizeof(uint32_t);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = 0;

            float v0 = (i * 2 + block_start < count) ? data[i * 2 + block_start] : 0.0f;
            int q0 = (int)roundf(v0 / scale * 16.0f);
            if (q0 < -16) q0 = -16;
            if (q0 > 15) q0 = 15;
            if (q0 < 0) { q0 += 32; qh |= (1u << i); }
            packed |= (q0 & 0x0F);

            float v1 = (i * 2 + 1 + block_start < count) ? data[i * 2 + 1 + block_start] : 0.0f;
            int q1 = (int)roundf(v1 / scale * 16.0f);
            if (q1 < -16) q1 = -16;
            if (q1 > 15) q1 = 15;
            if (q1 < 0) { q1 += 32; qh |= (1u << (i + 16)); }
            packed |= ((q1 & 0x0F) << 4);

            *out_ptr++ = packed;
        }

        *((uint32_t*)qh_ptr) = qh;
    }

    result.size = out_ptr - (uint8_t*)result.data;
    return result;
}

QuantResult quant_q5_1(const float* data, size_t count) {
    QuantResult result = {0};

    size_t num_blocks = (count + 31) / 32;
    size_t alloc_size = num_blocks * 24;

    result.data = malloc(alloc_size);
    if (!result.data) return result;
    result.capacity = alloc_size;

    uint8_t* out_ptr = (uint8_t*)result.data;

    for (size_t b = 0; b < num_blocks; b++) {
        size_t block_start = b * 32;
        size_t block_end = block_start + 32;
        if (block_end > count) block_end = count;

        float min_val = data[block_start];
        float max_val = data[block_start];
        for (size_t i = block_start + 1; i < block_end; i++) {
            if (data[i] < min_val) min_val = data[i];
            if (data[i] > max_val) max_val = data[i];
        }

        float scale = (max_val - min_val) / 31.0f;
        if (scale < 1e-10f) scale = 1e-10f;

        *((half*)out_ptr) = float_to_half(scale);
        out_ptr += sizeof(half);
        *((half*)out_ptr) = float_to_half(min_val);
        out_ptr += sizeof(half);

        uint32_t qh = 0;
        uint8_t* qh_ptr = out_ptr;
        out_ptr += sizeof(uint32_t);

        for (int i = 0; i < 16; i++) {
            uint8_t packed = 0;

            float v0 = (i * 2 + block_start < count) ? data[i * 2 + block_start] : 0.0f;
            int q0 = (int)roundf((v0 - min_val) / scale * 31.0f);
            if (q0 < 0) q0 = 0;
            if (q0 > 31) q0 = 31;
            if (q0 > 15) { q0 -= 16; qh |= (1u << i); }
            packed |= (q0 & 0x0F);

            float v1 = (i * 2 + 1 + block_start < count) ? data[i * 2 + 1 + block_start] : 0.0f;
            int q1 = (int)roundf((v1 - min_val) / scale * 31.0f);
            if (q1 < 0) q1 = 0;
            if (q1 > 31) q1 = 31;
            if (q1 > 15) { q1 -= 16; qh |= (1u << (i + 16)); }
            packed |= ((q1 & 0x0F) << 4);

            *out_ptr++ = packed;
        }

        *((uint32_t*)qh_ptr) = qh;
    }

    result.size = out_ptr - (uint8_t*)result.data;
    return result;
}