// Scalar K-quant dequant — layout-compatible with ggml block_q4_K / block_q2_K (QK_K = 256).
// Derived from llama.cpp ggml-quants.c (dequantize_row_q4_K / dequantize_row_q2_K).

#include "llm_adapter/GGUFRunner_kdequant.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{

constexpr int QK_K = 256;

inline float fp16ToFp32(std::uint16_t h)
{
    std::uint32_t sign = (h >> 15) & 1U;
    std::uint32_t exp = (h >> 10) & 0x1FU;
    std::uint32_t mant = h & 0x3FFU;

    if (exp == 0)
    {
        if (mant == 0)
        {
            return sign ? -0.0f : 0.0f;
        }
        while ((mant & 0x400U) == 0)
        {
            mant <<= 1U;
            exp--;
        }
        exp++;
        mant &= 0x3FFU;
    }
    else if (exp == 31)
    {
        return mant ? std::nanf("") : (sign ? -INFINITY : INFINITY);
    }

    exp = exp - 15 + 127;
    std::uint32_t f32 = (sign << 31) | (exp << 23) | (mant << 13);
    float result;
    std::memcpy(&result, &f32, sizeof(float));
    return result;
}

inline void get_scale_min_k4(int j, const std::uint8_t* q, std::uint8_t* d, std::uint8_t* m)
{
    if (j < 4)
    {
        *d = q[j] & 63;
        *m = q[j + 4] & 63;
    }
    else
    {
        *d = static_cast<std::uint8_t>((q[j + 4] & 0xF) | ((q[j - 4] >> 6) << 4));
        *m = static_cast<std::uint8_t>((q[j + 4] >> 4) | ((q[j - 0] >> 6) << 4));
    }
}

#pragma pack(push, 1)
struct BlockQ4K
{
    std::uint16_t d;
    std::uint16_t dmin;
    std::uint8_t scales[12];
    std::uint8_t qs[128];
};

struct BlockQ2K
{
    std::uint8_t scales[16];
    std::uint8_t qs[64];
    std::uint16_t d;
    std::uint16_t dmin;
};
#pragma pack(pop)

static_assert(sizeof(BlockQ4K) == 144, "block_q4_K size");
static_assert(sizeof(BlockQ2K) == 84, "block_q2_K size");

}  // namespace

void rawrxd_dequantize_row_q4_K(const void* rawBlocks, float* y, std::int64_t k)
{
    if (!rawBlocks || !y || k <= 0 || (k % QK_K) != 0)
    {
        return;
    }
    const auto* x = static_cast<const BlockQ4K*>(rawBlocks);
    const int nb = static_cast<int>(k / QK_K);

    for (int i = 0; i < nb; i++)
    {
        const std::uint8_t* q = x[i].qs;
        const float d = fp16ToFp32(x[i].d);
        const float min = fp16ToFp32(x[i].dmin);

        int is = 0;
        std::uint8_t sc = 0;
        std::uint8_t m = 0;
        for (int j = 0; j < QK_K; j += 64)
        {
            get_scale_min_k4(is + 0, x[i].scales, &sc, &m);
            const float d1 = d * static_cast<float>(sc);
            const float m1 = min * static_cast<float>(m);
            get_scale_min_k4(is + 1, x[i].scales, &sc, &m);
            const float d2 = d * static_cast<float>(sc);
            const float m2 = min * static_cast<float>(m);
            for (int l = 0; l < 32; ++l)
            {
                *y++ = d1 * static_cast<float>(q[l] & 0xF) - m1;
            }
            for (int l = 0; l < 32; ++l)
            {
                *y++ = d2 * static_cast<float>(q[l] >> 4) - m2;
            }
            q += 32;
            is += 2;
        }
    }
}

void rawrxd_dequantize_row_q2_K(const void* rawBlocks, float* y, std::int64_t k)
{
    if (!rawBlocks || !y || k <= 0 || (k % QK_K) != 0)
    {
        return;
    }
    const auto* x = static_cast<const BlockQ2K*>(rawBlocks);
    const int nb = static_cast<int>(k / QK_K);

    for (int i = 0; i < nb; i++)
    {
        const float d = fp16ToFp32(x[i].d);
        const float min = fp16ToFp32(x[i].dmin);
        const std::uint8_t* q = x[i].qs;

        int is = 0;
        float dl = 0.0f;
        float ml = 0.0f;
        for (int n = 0; n < QK_K; n += 128)
        {
            int shift = 0;
            for (int j = 0; j < 4; ++j)
            {
                std::uint8_t sc = x[i].scales[is++];
                dl = d * static_cast<float>(sc & 0xF);
                ml = min * static_cast<float>(sc >> 4);
                for (int l = 0; l < 16; ++l)
                {
                    *y++ = dl * static_cast<float>(static_cast<std::int8_t>((q[l] >> shift) & 3)) - ml;
                }

                sc = x[i].scales[is++];
                dl = d * static_cast<float>(sc & 0xF);
                ml = min * static_cast<float>(sc >> 4);
                for (int l = 0; l < 16; ++l)
                {
                    *y++ = dl * static_cast<float>(static_cast<std::int8_t>((q[l + 16] >> shift) & 3)) - ml;
                }

                shift += 2;
            }
            q += 32;
        }
    }
}
