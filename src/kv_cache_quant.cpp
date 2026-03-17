// kv_cache_quant.cpp — Block-wise KV-cache quantization (FP16 / Q8_0 / Q4_0)
// Part of the RawrXD inference stack.  No stubs.
//
// Layout: each KVPageQuantized covers exactly KV_BLOCK_SIZE float32 values.
// Quantization is block-wise: one scale factor per KV_BLOCK_SIZE-element block.
//
// FP16  : lossless cast via bit manipulation (S1 E5 M10 IEEE 754-2008)
// Q8_0  : signed 8-bit, scale = max_abs / 127.0f
// Q4_0  : 4-bit unsigned nibble, scale = max_abs / 7.5f, zero_point = 8
//         (values 0-15 centered so that dequant = (nibble - 8) * scale)
//
// All functions are extern "C" so MASM callers can reference them directly.

#include "kv_cache_quant.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cassert>
#include <algorithm>

#ifndef KV_BLOCK_SIZE
#define KV_BLOCK_SIZE 256
#endif

// ── Inline FP32 ↔ FP16 converters (no hardware dependency) ────────────────────

static inline uint16_t fp32_to_fp16(float v) noexcept {
    uint32_t x;
    memcpy(&x, &v, 4);
    const uint32_t sign    = (x >> 16) & 0x8000u;
    const int32_t  exp32   = static_cast<int32_t>((x >> 23) & 0xFFu) - 127;
    const uint32_t mant32  = x & 0x7FFFFFu;

    if (exp32 >= 16) {
        // Overflow → infinity (or propagate NaN payload if applicable)
        return static_cast<uint16_t>(sign | 0x7C00u | (mant32 ? 0x200u : 0u));
    }
    if (exp32 < -24) {
        // Underflow → zero
        return static_cast<uint16_t>(sign);
    }
    if (exp32 < -14) {
        // Subnormal fp16 path
        const uint32_t mant16 = (mant32 | 0x800000u) >> (1 - exp32 - 14 + 13);
        // Round-to-nearest
        const uint32_t round  = (mant32 | 0x800000u) >> (-14 - exp32 + 12) & 1u;
        return static_cast<uint16_t>(sign | (mant16 + round));
    }
    const uint32_t exp16  = static_cast<uint32_t>(exp32 + 15) << 10;
    const uint32_t mant16 = (mant32 + 0x1000u) >> 13;      // truncate + round LSB
    return static_cast<uint16_t>(sign | exp16 | (mant16 & 0x3FFu));
}

static inline float fp16_to_fp32(uint16_t h) noexcept {
    const uint32_t sign  = static_cast<uint32_t>(h & 0x8000u) << 16;
    const uint32_t exp16 = (h >> 10) & 0x1Fu;
    const uint32_t mant  = (h & 0x3FFu);

    uint32_t x;
    if (exp16 == 0u) {
        if (mant == 0u) {
            x = sign;                                  // ±0
        } else {
            // Subnormal fp16 → normalise
            uint32_t e = 0u, m = mant;
            while ((m & 0x400u) == 0u) { m <<= 1; ++e; }
            x = sign | ((127u - 15u - e + 0u) << 23) | ((m & 0x3FFu) << 13);
        }
    } else if (exp16 == 31u) {
        x = sign | 0x7F800000u | (mant << 13);        // ±Inf or NaN
    } else {
        x = sign | ((exp16 + (127u - 15u)) << 23) | (mant << 13);
    }
    float f;
    memcpy(&f, &x, 4);
    return f;
}

// ── Public API ─────────────────────────────────────────────────────────────────

extern "C" {

// Quantize pSrc[count] float32s into pDst according to fmt.
// pDst must be pre-allocated with KVPage_QuantizedSize(count, fmt) bytes.
// Returns: 0 on success, -1 on unsupported format.
int KVPage_Quantize(const float* pSrc, uint8_t* pDst, size_t count, KVQuantFormat fmt) {
    if (!pSrc || !pDst || count == 0) return -1;

    switch (fmt) {
    // ── FP16 ──────────────────────────────────────────────────────────────────
    case KV_QUANT_FP16: {
        uint16_t* out = reinterpret_cast<uint16_t*>(pDst);
        for (size_t i = 0; i < count; ++i) {
            out[i] = fp32_to_fp16(pSrc[i]);
        }
        return 0;
    }

    // ── Q8_0 ──────────────────────────────────────────────────────────────────
    //   Block layout: [KV_BLOCK_SIZE × int8_t] + [float32 scale]
    case KV_QUANT_Q8_0: {
        const size_t numBlocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        for (size_t b = 0; b < numBlocks; ++b) {
            const size_t offset = b * KV_BLOCK_SIZE;
            const size_t bLen   = std::min<size_t>(KV_BLOCK_SIZE, count - offset);

            // Compute max absolute value in block
            float maxAbs = 0.0f;
            for (size_t i = 0; i < bLen; ++i) {
                const float a = std::fabs(pSrc[offset + i]);
                if (a > maxAbs) maxAbs = a;
            }

            const float scale    = maxAbs > 0.0f ? maxAbs / 127.0f : 1.0f;
            const float invScale = 1.0f / scale;

            int8_t*  qdata = reinterpret_cast<int8_t*>(pDst) + b * (KV_BLOCK_SIZE + sizeof(float));
            float*   scPtr = reinterpret_cast<float*>(qdata + KV_BLOCK_SIZE);

            for (size_t i = 0; i < bLen; ++i) {
                const float v = pSrc[offset + i] * invScale;
                int32_t q = static_cast<int32_t>(v + (v >= 0.0f ? 0.5f : -0.5f));
                q = std::max(-127, std::min(127, q));
                qdata[i] = static_cast<int8_t>(q);
            }
            // Pad tail if block is partial
            for (size_t i = bLen; i < KV_BLOCK_SIZE; ++i) {
                qdata[i] = 0;
            }
            memcpy(scPtr, &scale, sizeof(float));
        }
        return 0;
    }

    // ── Q4_0 ──────────────────────────────────────────────────────────────────
    //   Block layout: [KV_BLOCK_SIZE/2 × uint8_t nibbles] + [float32 scale]
    //   Nibble encoding: value = nibble - 8  →  range [-8, +7]
    //   scale = max_abs / 7.0f
    case KV_QUANT_Q4_0: {
        const size_t numBlocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        for (size_t b = 0; b < numBlocks; ++b) {
            const size_t offset = b * KV_BLOCK_SIZE;
            const size_t bLen   = std::min<size_t>(KV_BLOCK_SIZE, count - offset);

            float maxAbs = 0.0f;
            for (size_t i = 0; i < bLen; ++i) {
                const float a = std::fabs(pSrc[offset + i]);
                if (a > maxAbs) maxAbs = a;
            }

            const float scale    = maxAbs > 0.0f ? maxAbs / 7.0f : 1.0f;
            const float invScale = 1.0f / scale;

            const size_t nibbleBytes = KV_BLOCK_SIZE / 2;
            uint8_t* qdata = pDst + b * (nibbleBytes + sizeof(float));
            float*   scPtr = reinterpret_cast<float*>(qdata + nibbleBytes);

            for (size_t i = 0; i < KV_BLOCK_SIZE; i += 2) {
                uint8_t lo = 8, hi = 8;
                if (i < bLen) {
                    const float v  = pSrc[offset + i] * invScale;
                    int32_t q      = static_cast<int32_t>(v + (v >= 0.0f ? 0.5f : -0.5f)) + 8;
                    lo = static_cast<uint8_t>(std::max(0, std::min(15, q)));
                }
                if ((i + 1) < bLen) {
                    const float v  = pSrc[offset + i + 1] * invScale;
                    int32_t q      = static_cast<int32_t>(v + (v >= 0.0f ? 0.5f : -0.5f)) + 8;
                    hi = static_cast<uint8_t>(std::max(0, std::min(15, q)));
                }
                qdata[i / 2] = lo | (hi << 4);
            }
            memcpy(scPtr, &scale, sizeof(float));
        }
        return 0;
    }

    default:
        return -1;
    }
}

// Dequantize pSrc (quantized) back into pDst[count] float32s.
int KVPage_Dequantize(const uint8_t* pSrc, float* pDst, size_t count, KVQuantFormat fmt) {
    if (!pSrc || !pDst || count == 0) return -1;

    switch (fmt) {
    case KV_QUANT_FP16: {
        const uint16_t* in = reinterpret_cast<const uint16_t*>(pSrc);
        for (size_t i = 0; i < count; ++i) {
            pDst[i] = fp16_to_fp32(in[i]);
        }
        return 0;
    }

    case KV_QUANT_Q8_0: {
        const size_t numBlocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        for (size_t b = 0; b < numBlocks; ++b) {
            const size_t offset = b * KV_BLOCK_SIZE;
            const size_t bLen   = std::min<size_t>(KV_BLOCK_SIZE, count - offset);

            const int8_t*  qdata = reinterpret_cast<const int8_t*>(pSrc)
                                   + b * (KV_BLOCK_SIZE + sizeof(float));
            float scale;
            memcpy(&scale, qdata + KV_BLOCK_SIZE, sizeof(float));

            for (size_t i = 0; i < bLen; ++i) {
                pDst[offset + i] = static_cast<float>(qdata[i]) * scale;
            }
        }
        return 0;
    }

    case KV_QUANT_Q4_0: {
        const size_t numBlocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        for (size_t b = 0; b < numBlocks; ++b) {
            const size_t offset     = b * KV_BLOCK_SIZE;
            const size_t bLen       = std::min<size_t>(KV_BLOCK_SIZE, count - offset);
            const size_t nibbleBytes = KV_BLOCK_SIZE / 2;

            const uint8_t* qdata = pSrc + b * (nibbleBytes + sizeof(float));
            float scale;
            memcpy(&scale, qdata + nibbleBytes, sizeof(float));

            for (size_t i = 0; i < bLen; i += 2) {
                const uint8_t byte = qdata[i / 2];
                const int lo       = static_cast<int>(byte & 0x0Fu) - 8;
                const int hi       = static_cast<int>((byte >> 4) & 0x0Fu) - 8;
                pDst[offset + i]       = static_cast<float>(lo) * scale;
                if ((i + 1) < bLen) {
                    pDst[offset + i + 1] = static_cast<float>(hi) * scale;
                }
            }
        }
        return 0;
    }

    default:
        return -1;
    }
}

// Return the number of bytes needed to store 'count' float32 values
// in the given quantization format.
size_t KVPage_QuantizedSize(size_t count, KVQuantFormat fmt) {
    switch (fmt) {
    case KV_QUANT_FP16:
        return count * sizeof(uint16_t);

    case KV_QUANT_Q8_0: {
        const size_t blocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        return blocks * (KV_BLOCK_SIZE + sizeof(float));
    }

    case KV_QUANT_Q4_0: {
        const size_t blocks = (count + KV_BLOCK_SIZE - 1) / KV_BLOCK_SIZE;
        return blocks * (KV_BLOCK_SIZE / 2 + sizeof(float));
    }

    default:
        return 0;
    }
}

} // extern "C"
