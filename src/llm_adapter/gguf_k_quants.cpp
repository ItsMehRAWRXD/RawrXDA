// Scalar K-quant dequant helpers: Q4_K / Q2_K logic adapted from llama.cpp ggml-quants.c (MIT, ggml-org).
// https://github.com/ggerganov/llama.cpp

#include "gguf_k_quants.hpp"

#include <cmath>
#include <cstring>

namespace
{
constexpr int QK_K = 256;
constexpr int K_SCALE_SIZE = 12;

static inline float fp16ToFp32(uint16_t h)
{
    uint32_t sign = (h >> 15) & 1u;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x3FFu;
    if (exp == 0)
    {
        if (mant == 0)
        {
            return sign ? -0.0f : 0.0f;
        }
        while ((mant & 0x400u) == 0)
        {
            mant <<= 1u;
            exp--;
        }
        exp++;
        mant &= 0x3FFu;
    }
    else if (exp == 31)
    {
        return mant ? NAN : (sign ? -INFINITY : INFINITY);
    }
    exp = exp - 15u + 127u;
    uint32_t f32 = (sign << 31) | (exp << 23) | (mant << 13);
    float out;
    std::memcpy(&out, &f32, sizeof(float));
    return out;
}

struct BlockQ4_0
{
    uint16_t d;
    uint8_t qs[16];
};

struct BlockQ8_0
{
    uint16_t d;
    int8_t qs[32];
};

void dequantizeRowQ4_0(const void* src, float* dst, size_t n)
{
    const auto* b = static_cast<const BlockQ4_0*>(src);
    const size_t nb = n / 32;
    for (size_t i = 0; i < nb; ++i)
    {
        const float df = fp16ToFp32(b[i].d);
        for (size_t j = 0; j < 16; ++j)
        {
            int vi = static_cast<int>(b[i].qs[j] & 0xFu) - 8;
            dst[i * 32 + j] = vi * df;
        }
        for (size_t j = 0; j < 16; ++j)
        {
            int vi = static_cast<int>(b[i].qs[j] >> 4) - 8;
            dst[i * 32 + j + 16] = vi * df;
        }
    }
}

void dequantizeRowQ8_0(const void* src, float* dst, size_t n)
{
    const auto* b = static_cast<const BlockQ8_0*>(src);
    const size_t nb = n / 32;
    for (size_t i = 0; i < nb; ++i)
    {
        const float df = fp16ToFp32(b[i].d);
        for (size_t j = 0; j < 32; ++j)
        {
            dst[i * 32 + j] = static_cast<float>(b[i].qs[j]) * df;
        }
    }
}

#pragma pack(push, 1)
struct BlockQ4_K
{
    uint16_t d;
    uint16_t dmin;
    uint8_t scales[K_SCALE_SIZE];
    uint8_t qs[QK_K / 2];
};
struct BlockQ2_K
{
    uint8_t scales[QK_K / 16];
    uint8_t qs[QK_K / 4];
    uint16_t d;
    uint16_t dmin;
};
struct BlockQ3_K
{
    uint8_t hmask[QK_K / 8];
    uint8_t qs[QK_K / 4];
    uint8_t scales[12];
    uint16_t d;
};
struct BlockQ5_K
{
    uint16_t d;
    uint16_t dmin;
    uint8_t scales[K_SCALE_SIZE];
    uint8_t qh[QK_K / 8];
    uint8_t qs[QK_K / 2];
};
struct BlockQ6_K
{
    uint8_t ql[QK_K / 2];
    uint8_t qh[QK_K / 4];
    int8_t scales[QK_K / 16];
    uint16_t d;
};
struct BlockQ8_K
{
    float d;
    int8_t qs[QK_K];
    int16_t bsums[QK_K / 16];
};
static_assert(sizeof(BlockQ4_K) == 2u * sizeof(uint16_t) + K_SCALE_SIZE + QK_K / 2, "block_q4_K size");
static_assert(sizeof(BlockQ2_K) == QK_K / 16 + QK_K / 4 + 2u * sizeof(uint16_t), "block_q2_K size");
static_assert(sizeof(BlockQ3_K) == sizeof(uint16_t) + QK_K / 4 + QK_K / 8 + 12, "block_q3_K size");
static_assert(sizeof(BlockQ5_K) == 2u * sizeof(uint16_t) + K_SCALE_SIZE + QK_K / 2 + QK_K / 8, "block_q5_K size");
static_assert(sizeof(BlockQ6_K) == sizeof(uint16_t) + QK_K / 16 + QK_K / 2 + QK_K / 4, "block_q6_K size");
static_assert(sizeof(BlockQ8_K) == sizeof(float) + QK_K + (QK_K / 16) * sizeof(int16_t), "block_q8_K size");
#pragma pack(pop)

static inline void getScaleMinK4(int j, const uint8_t* q, uint8_t* d, uint8_t* m)
{
    if (j < 4)
    {
        *d = static_cast<uint8_t>(q[j] & 63u);
        *m = static_cast<uint8_t>(q[j + 4] & 63u);
    }
    else
    {
        *d = static_cast<uint8_t>((q[j + 4] & 0xFu) | ((q[j - 4] >> 6) << 4));
        *m = static_cast<uint8_t>((q[j + 4] >> 4) | ((q[j - 0] >> 6) << 4));
    }
}

void dequantizeRowQ4_K(const BlockQ4_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    for (int i = 0; i < nb; ++i)
    {
        const uint8_t* q = x[i].qs;
        const float d = fp16ToFp32(x[i].d);
        const float min = fp16ToFp32(x[i].dmin);
        int is = 0;
        for (int j = 0; j < QK_K; j += 64)
        {
            uint8_t sc = 0;
            uint8_t m = 0;
            getScaleMinK4(is + 0, x[i].scales, &sc, &m);
            const float d1 = d * static_cast<float>(sc);
            const float m1 = min * static_cast<float>(m);
            getScaleMinK4(is + 1, x[i].scales, &sc, &m);
            const float d2 = d * static_cast<float>(sc);
            const float m2 = min * static_cast<float>(m);
            for (int l = 0; l < 32; ++l)
            {
                *y++ = d1 * static_cast<float>(q[l] & 0xFu) - m1;
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

void dequantizeRowQ2_K(const BlockQ2_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    for (int i = 0; i < nb; ++i)
    {
        const float d = fp16ToFp32(x[i].d);
        const float min = fp16ToFp32(x[i].dmin);
        const uint8_t* q = x[i].qs;
        int is = 0;
        for (int n = 0; n < QK_K; n += 128)
        {
            int shift = 0;
            for (int j = 0; j < 4; ++j)
            {
                uint8_t sc = x[i].scales[is++];
                float dl = d * static_cast<float>(sc & 0xFu);
                float ml = min * static_cast<float>(sc >> 4);
                for (int l = 0; l < 16; ++l)
                {
                    *y++ = dl * static_cast<float>(static_cast<int8_t>((q[l] >> shift) & 3)) - ml;
                }
                sc = x[i].scales[is++];
                dl = d * static_cast<float>(sc & 0xFu);
                ml = min * static_cast<float>(sc >> 4);
                for (int l = 0; l < 16; ++l)
                {
                    *y++ = dl * static_cast<float>(static_cast<int8_t>((q[l + 16] >> shift) & 3)) - ml;
                }
                shift += 2;
            }
            q += 32;
        }
    }
}

void dequantizeRowQ3_K(const BlockQ3_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    constexpr uint32_t kmask1 = 0x03030303u;
    constexpr uint32_t kmask2 = 0x0f0f0f0fu;
    for (int i = 0; i < nb; ++i)
    {
        const float dAll = fp16ToFp32(x[i].d);
        const uint8_t* q = x[i].qs;
        const uint8_t* hm = x[i].hmask;
        uint8_t m = 1;
        uint32_t aux[4];
        std::memcpy(aux, x[i].scales, 12);
        uint32_t tmp = aux[2];
        aux[2] = ((aux[0] >> 4) & kmask2) | (((tmp >> 4) & kmask1) << 4);
        aux[3] = ((aux[1] >> 4) & kmask2) | (((tmp >> 6) & kmask1) << 4);
        aux[0] = (aux[0] & kmask2) | (((tmp >> 0) & kmask1) << 4);
        aux[1] = (aux[1] & kmask2) | (((tmp >> 2) & kmask1) << 4);
        const auto* scales = reinterpret_cast<const int8_t*>(aux);
        int is = 0;
        for (int n = 0; n < QK_K; n += 128)
        {
            int shift = 0;
            for (int j = 0; j < 4; ++j)
            {
                float dl = dAll * static_cast<float>(static_cast<int>(scales[is++]) - 32);
                for (int l = 0; l < 16; ++l)
                {
                    const int8_t low = static_cast<int8_t>((q[l + 0] >> shift) & 3);
                    const int8_t sub = ((hm[l + 0] & m) != 0) ? static_cast<int8_t>(0) : static_cast<int8_t>(4);
                    *y++ = dl * static_cast<float>(low - sub);
                }
                dl = dAll * static_cast<float>(static_cast<int>(scales[is++]) - 32);
                for (int l = 0; l < 16; ++l)
                {
                    const int8_t low = static_cast<int8_t>((q[l + 16] >> shift) & 3);
                    const int8_t sub = ((hm[l + 16] & m) != 0) ? static_cast<int8_t>(0) : static_cast<int8_t>(4);
                    *y++ = dl * static_cast<float>(low - sub);
                }
                shift += 2;
                m = static_cast<uint8_t>(m << 1);
            }
            q += 32;
        }
    }
}

void dequantizeRowQ5_K(const BlockQ5_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    for (int i = 0; i < nb; ++i)
    {
        const uint8_t* ql = x[i].qs;
        const uint8_t* qh = x[i].qh;
        const float d = fp16ToFp32(x[i].d);
        const float min = fp16ToFp32(x[i].dmin);
        int is = 0;
        uint8_t sc = 0;
        uint8_t m = 0;
        uint8_t u1 = 1;
        uint8_t u2 = 2;
        for (int j = 0; j < QK_K; j += 64)
        {
            getScaleMinK4(is + 0, x[i].scales, &sc, &m);
            const float d1 = d * static_cast<float>(sc);
            const float m1 = min * static_cast<float>(m);
            getScaleMinK4(is + 1, x[i].scales, &sc, &m);
            const float d2 = d * static_cast<float>(sc);
            const float m2 = min * static_cast<float>(m);
            for (int l = 0; l < 32; ++l)
            {
                const float hi = ((qh[l] & u1) != 0) ? 16.0f : 0.0f;
                *y++ = d1 * (static_cast<float>(ql[l] & 0xFu) + hi) - m1;
            }
            for (int l = 0; l < 32; ++l)
            {
                const float hi = ((qh[l] & u2) != 0) ? 16.0f : 0.0f;
                *y++ = d2 * (static_cast<float>(ql[l] >> 4) + hi) - m2;
            }
            ql += 32;
            is += 2;
            u1 = static_cast<uint8_t>(u1 << 2);
            u2 = static_cast<uint8_t>(u2 << 2);
        }
    }
}

void dequantizeRowQ6_K(const BlockQ6_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    for (int i = 0; i < nb; ++i)
    {
        const float d = fp16ToFp32(x[i].d);
        const uint8_t* ql = x[i].ql;
        const uint8_t* qh = x[i].qh;
        const int8_t* sc = x[i].scales;
        for (int n = 0; n < QK_K; n += 128)
        {
            for (int l = 0; l < 32; ++l)
            {
                const int is = l / 16;
                const int8_t q1 = static_cast<int8_t>((ql[l + 0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
                const int8_t q2 = static_cast<int8_t>((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
                const int8_t q3 = static_cast<int8_t>((ql[l + 0] >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
                const int8_t q4 = static_cast<int8_t>((ql[l + 32] >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
                y[l + 0] = d * static_cast<float>(sc[is + 0]) * static_cast<float>(q1);
                y[l + 32] = d * static_cast<float>(sc[is + 2]) * static_cast<float>(q2);
                y[l + 64] = d * static_cast<float>(sc[is + 4]) * static_cast<float>(q3);
                y[l + 96] = d * static_cast<float>(sc[is + 6]) * static_cast<float>(q4);
            }
            y += 128;
            ql += 64;
            qh += 32;
            sc += 8;
        }
    }
}

void dequantizeRowQ8_K(const BlockQ8_K* x, float* y, int64_t k)
{
    const int nb = static_cast<int>(k / QK_K);
    for (int i = 0; i < nb; ++i)
    {
        const float d = x[i].d;
        for (int j = 0; j < QK_K; ++j)
        {
            *y++ = d * static_cast<float>(x[i].qs[j]);
        }
    }
}

}  // namespace

namespace RawrXD
{
namespace GgufTensorBytes
{

bool payloadBytes(uint32_t ggmlTypeU32, size_t nElements, size_t& outBytes)
{
    outBytes = 0;
    switch (ggmlTypeU32)
    {
        case 0:  // F32
            outBytes = nElements * 4;
            return true;
        case 1:  // F16
            outBytes = nElements * 2;
            return true;
        case 2:  // Q4_0
            if (nElements % 32 != 0)
            {
                return false;
            }
            outBytes = (nElements / 32) * 18;
            return true;
        case 8:  // Q8_0
            if (nElements % 32 != 0)
            {
                return false;
            }
            outBytes = (nElements / 32) * 34;
            return true;
        case 10:  // Q2_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ2_K);
            return true;
        case 11:  // Q3_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ3_K);
            return true;
        case 12:  // Q4_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ4_K);
            return true;
        case 13:  // Q5_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ5_K);
            return true;
        case 14:  // Q6_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ6_K);
            return true;
        case 15:  // Q8_K
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            outBytes = (nElements / static_cast<size_t>(QK_K)) * sizeof(BlockQ8_K);
            return true;
        default:
            return false;
    }
}

bool dequantizeToFloat(uint32_t ggmlTypeU32, const uint8_t* src, size_t nElements, float* dst)
{
    switch (ggmlTypeU32)
    {
        case 0:
            std::memcpy(dst, src, nElements * 4);
            return true;
        case 1:
        {
            const auto* h = reinterpret_cast<const uint16_t*>(src);
            for (size_t i = 0; i < nElements; ++i)
            {
                dst[i] = fp16ToFp32(h[i]);
            }
            return true;
        }
        case 2:
            if (nElements % 32 != 0)
            {
                return false;
            }
            dequantizeRowQ4_0(src, dst, nElements);
            return true;
        case 8:
            if (nElements % 32 != 0)
            {
                return false;
            }
            dequantizeRowQ8_0(src, dst, nElements);
            return true;
        case 10:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ2_K(reinterpret_cast<const BlockQ2_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        case 11:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ3_K(reinterpret_cast<const BlockQ3_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        case 12:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ4_K(reinterpret_cast<const BlockQ4_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        case 13:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ5_K(reinterpret_cast<const BlockQ5_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        case 14:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ6_K(reinterpret_cast<const BlockQ6_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        case 15:
            if (nElements % static_cast<size_t>(QK_K) != 0)
            {
                return false;
            }
            dequantizeRowQ8_K(reinterpret_cast<const BlockQ8_K*>(src), dst, static_cast<int64_t>(nElements));
            return true;
        default:
            return false;
    }
}

}  // namespace GgufTensorBytes
}  // namespace RawrXD
