// q4k_dequant_parity_test.c
//
// Phase 1 falsifiable check for the GLSL Q4_K dequant in
//   d:\rawrxd\src\gpu\shaders\fused_q4k_tile_gemm_vec.comp
//
// We compare:
//   (a) ggml's reference dequantize_row_q4_K (transcribed verbatim from
//       ggml-org/llama.cpp ggml-quants.c)
//   (b) The exact same loop / packing the GLSL shader executes per block.
//
// If (a) and (b) agree bit-for-bit (modulo float ordering of additions, which
// is identical here), we know the *math* in the GPU shader is correct, and any
// later GPU-vs-CPU discrepancy must be in:
//   - struct memory layout (already verified via spirv-dis: 144B stride),
//   - dispatch wiring,
//   - or activation buffer indexing.
//
// Build (Windows, MSVC dev shell):
//   cl /O2 /DNDEBUG d:\rawrxd\tests\gpu\q4k_dequant_parity_test.c
//
// Build (mingw / clang):
//   clang -O2 -o q4k_parity d:\rawrxd\tests\gpu\q4k_dequant_parity_test.c
//
// No external deps. Pure C. Self-contained Q4_K block synthesis from a known
// float row -> ggml ref dequant -> shader-mirror dequant -> compare.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define QK_K 256

// 144-byte block, byte-exact ggml layout.
#pragma pack(push, 1)
typedef struct {
    uint16_t d;          // half-precision bits
    uint16_t dmin;       // half-precision bits
    uint8_t  scales[12];
    uint8_t  qs[128];
} block_q4_K;
#pragma pack(pop)

typedef char _q4k_size_check[(sizeof(block_q4_K) == 144) ? 1 : -1];

// ---------------------------------------------------------------------------
// half<->float helpers (no FP16 hw assumed; bit-exact IEEE conversion).
// ---------------------------------------------------------------------------
static float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 0x1u;
    uint32_t e = (h >> 10) & 0x1Fu;
    uint32_t m = h & 0x3FFu;
    uint32_t f;
    if (e == 0) {
        if (m == 0) {
            f = s << 31;
        } else {
            // subnormal -> normalize
            while ((m & 0x400u) == 0) { m <<= 1; e -= 1; }
            e += 1;
            m &= 0x3FFu;
            f = (s << 31) | ((e + 112u) << 23) | (m << 13);
        }
    } else if (e == 31) {
        f = (s << 31) | 0x7F800000u | (m << 13);
    } else {
        f = (s << 31) | ((e + 112u) << 23) | (m << 13);
    }
    float out;
    memcpy(&out, &f, 4);
    return out;
}

static uint16_t float_to_half(float v) {
    uint32_t f;
    memcpy(&f, &v, 4);
    uint32_t s = (f >> 31) & 0x1u;
    int32_t  e = (int32_t)((f >> 23) & 0xFFu) - 127 + 15;
    uint32_t m = f & 0x7FFFFFu;
    uint16_t h;
    if (e <= 0) {
        if (e < -10) { h = (uint16_t)(s << 15); }
        else {
            m |= 0x800000u;
            uint32_t shift = (uint32_t)(14 - e);
            uint32_t round = (m >> (shift - 1)) & 1u;
            h = (uint16_t)((s << 15) | ((m >> shift) + round));
        }
    } else if (e >= 31) {
        h = (uint16_t)((s << 15) | 0x7C00u | (m ? 1u : 0u));
    } else {
        uint32_t round = (m >> 12) & 1u;
        h = (uint16_t)((s << 15) | ((uint32_t)e << 10) | ((m >> 13) + round));
    }
    return h;
}

// ---------------------------------------------------------------------------
// ggml reference: get_scale_min_k4
// (ggml/src/ggml-quants.c, public-domain port)
// ---------------------------------------------------------------------------
static inline void ggml_get_scale_min_k4(int j, const uint8_t *q, uint8_t *d, uint8_t *m) {
    if (j < 4) {
        *d = q[j]     & 63;
        *m = q[j + 4] & 63;
    } else {
        *d = (q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4);
        *m = (q[j + 4] >>  4) | ((q[j    ] >> 6) << 4);
    }
}

// ggml reference dequantize for a single block (256 outputs).
static void ggml_dequantize_block_q4_K(const block_q4_K *x, float *y) {
    const uint8_t *q = x->qs;
    const float d   = half_to_float(x->d);
    const float min = half_to_float(x->dmin);
    int is = 0;
    uint8_t sc, m;
    for (int j = 0; j < QK_K; j += 64) {
        ggml_get_scale_min_k4(is + 0, x->scales, &sc, &m);
        const float d1 = d * sc;  const float m1 = min * m;
        ggml_get_scale_min_k4(is + 1, x->scales, &sc, &m);
        const float d2 = d * sc;  const float m2 = min * m;
        for (int l = 0; l < 32; ++l) y[j + l]      = d1 * (q[l] & 0xF) - m1;
        for (int l = 0; l < 32; ++l) y[j + 32 + l] = d2 * (q[l] >>  4) - m2;
        q += 32;  is += 2;
    }
}

// ---------------------------------------------------------------------------
// Shader-mirror: literal C transcription of fused_q4k_tile_gemm_vec.comp.
// Must produce identical floats to ggml ref.
// ---------------------------------------------------------------------------
static inline void shader_get_scale_min_k4(uint32_t j, const uint8_t *q,
                                           uint32_t *sc, uint32_t *mn) {
    if (j < 4u) {
        *sc = (uint32_t)q[j]     & 63u;
        *mn = (uint32_t)q[j + 4] & 63u;
    } else {
        *sc = ((uint32_t)q[j + 4] & 0xFu) | (((uint32_t)q[j - 4] >> 6) << 4);
        *mn = ((uint32_t)q[j + 4] >>  4)  | (((uint32_t)q[j    ] >> 6) << 4);
    }
}

static void shader_dequantize_block_q4_K(const block_q4_K *blk, float *out256) {
    const float d    = half_to_float(blk->d);
    const float dmin = half_to_float(blk->dmin);
    for (uint32_t seg = 0; seg < 4u; ++seg) {
        uint32_t sc_lo, mn_lo, sc_hi, mn_hi;
        shader_get_scale_min_k4(seg * 2u + 0u, blk->scales, &sc_lo, &mn_lo);
        shader_get_scale_min_k4(seg * 2u + 1u, blk->scales, &sc_hi, &mn_hi);
        const float d1 = d * (float)sc_lo;  const float m1 = dmin * (float)mn_lo;
        const float d2 = d * (float)sc_hi;  const float m2 = dmin * (float)mn_hi;
        const uint32_t qs_off = seg * 32u;
        const uint32_t k_base = seg * 64u;
        for (uint32_t l = 0; l < 32u; ++l) {
            uint32_t qb = blk->qs[qs_off + l];
            out256[k_base + l]       = d1 * (float)(qb & 0xFu) - m1;
            out256[k_base + 32u + l] = d2 * (float)(qb >>  4)  - m2;
        }
    }
}

// ---------------------------------------------------------------------------
// Synthesize a deterministic block for testing (no need to run real ggml
// quantization here -- we only need a syntactically valid Q4_K block).
// ---------------------------------------------------------------------------
static void synth_block(block_q4_K *b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    b->d    = float_to_half(0.0125f);
    b->dmin = float_to_half(0.0040f);
    for (int i = 0; i < 12; ++i) {
        s = s * 1664525u + 1013904223u;
        b->scales[i] = (uint8_t)(s >> 24);
    }
    for (int i = 0; i < 128; ++i) {
        s = s * 1664525u + 1013904223u;
        b->qs[i] = (uint8_t)(s >> 24);
    }
}

int main(void) {
    block_q4_K blk;
    int max_fail = 0;
    float worst_diff = 0.0f;

    for (uint32_t seed = 0; seed < 32; ++seed) {
        synth_block(&blk, seed);

        float ref[QK_K], shd[QK_K];
        ggml_dequantize_block_q4_K(&blk, ref);
        shader_dequantize_block_q4_K(&blk, shd);

        int fails = 0;
        for (int i = 0; i < QK_K; ++i) {
            float diff = fabsf(ref[i] - shd[i]);
            if (diff > worst_diff) worst_diff = diff;
            if (diff > 1e-6f) ++fails;
        }
        if (fails > max_fail) max_fail = fails;
        if (fails) {
            printf("seed=%u  fails=%d  worst=%.9g\n", seed, fails, worst_diff);
        }
    }

    printf("Q4_K dequant parity: max_fail_per_block=%d  worst_abs_diff=%.9g\n",
           max_fail, worst_diff);
    if (max_fail == 0) {
        printf("PASS: shader-mirror == ggml reference (256x32 = 8192 outputs)\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
