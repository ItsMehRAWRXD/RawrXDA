#include <immintrin.h>
#include <cstdint>
#include <cstring>

// Keep BlockQ8_0 layout consistent with q8_0_avx2.cc
struct BlockQ8_0 {
    uint16_t d;      // delta (float16)
    int8_t qs[32];   // 32 signed bytes
};

static inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp = (h & 0x7C00) >> 10;
    uint32_t mant = (h & 0x03FF);

    if (exp == 0) {
        if (mant == 0) return 0.0f;
        exp = 1;
        while ((mant & 0x0400) == 0) {
            mant <<= 1;
            exp--;
        }
        mant &= 0x03FF;
    }

    uint32_t f32;
    if (exp == 0x1F) {
        f32 = sign | 0x7F800000 | (mant << 13);
    } else {
        f32 = sign | ((exp + 112) << 23) | (mant << 13);
    }

    float result;
    std::memcpy(&result, &f32, sizeof(float));
    return result;
}

// From q8_0_avx2.cc
extern "C" float q8_0_dot_avx2(const int8_t* q8, const float* a, float scale);

// Baseline flash-like kernel using Q8_0 dot products per 32-d block
// This is a simplified compute kernel for benchmarking only.
extern "C" void flash_attn_avx2(
    const float* Q,
    const void*  K,
    const float* V,
    float*       O,
    int          seqLen,
    int          headDim)
{
    const BlockQ8_0* Kb = reinterpret_cast<const BlockQ8_0*>(K);
    const int blocksPerVec = headDim / 32;

    for (int i = 0; i < seqLen; ++i) {
        float* oRow = &O[i * headDim];
        const float* qRow = &Q[i * headDim];
        const float* vRow = &V[i * headDim];

        for (int b = 0; b < blocksPerVec; ++b) {
            const BlockQ8_0& kb = Kb[i * blocksPerVec + b];
            const int8_t* kptr = kb.qs;
            const float scale = fp16_to_fp32(kb.d);

            const float* qptr = qRow + b * 32;
            float dp = q8_0_dot_avx2(kptr, qptr, scale);

            // Write a simple fused output: O = V + dp (broadcast)
            float* out = oRow + b * 32;
#if defined(__AVX2__)
            __m256 addv = _mm256_set1_ps(dp);
            for (int j = 0; j < 32; j += 8) {
                __m256 vv = _mm256_loadu_ps(vRow + b*32 + j);
                __m256 ov = _mm256_add_ps(vv, addv);
                _mm256_storeu_ps(out + j, ov);
            }
#else
            for (int j = 0; j < 32; ++j) {
                out[j] = vRow[b*32 + j] + dp;
            }
#endif
        }
    }
}
