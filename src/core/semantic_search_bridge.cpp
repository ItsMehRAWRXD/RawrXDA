// semantic_search_bridge.cpp — SemanticSearcher + scalar fallbacks

#include "core/semantic_search_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#ifdef _MSC_VER
#include <immintrin.h>
#endif

namespace RawrXD::Search
{

namespace
{

float dotF32(const float* a, const float* b, std::size_t n)
{
    float s = 0.f;
    for (std::size_t i = 0; i < n; ++i)
    {
        s += a[i] * b[i];
    }
    return s;
}

float dotQ8F32(const std::int8_t* q, const float* f, std::size_t n, float qScale)
{
    float s = 0.f;
    for (std::size_t i = 0; i < n; ++i)
    {
        s += (qScale * static_cast<float>(q[i])) * f[i];
    }
    return s;
}

float halfBitsToFloat(std::uint16_t h)
{
#ifdef _MSC_VER
    const __m128i v = _mm_cvtsi32_si128(static_cast<int>(h));
    const __m128 f = _mm_cvtph_ps(v);
    return _mm_cvtss_f32(f);
#else
    const std::uint32_t sign = static_cast<std::uint32_t>(h & 0x8000u) << 16;
    const std::uint32_t mant = h & 0x03FFu;
    const std::uint32_t exp = (h >> 10) & 0x1Fu;
    if (exp == 0u)
    {
        if (mant == 0u)
        {
            float z = 0.f;
            std::uint32_t bits = sign;
            std::memcpy(&z, &bits, sizeof(z));
            return z;
        }
        return std::ldexp(static_cast<float>(mant), -24 - 14);
    }
    if (exp == 31u)
    {
        float inf = std::numeric_limits<float>::infinity();
        if (sign != 0u)
        {
            inf = -inf;
        }
        return inf;
    }
    const std::uint32_t fbits = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    float x = 0.f;
    std::memcpy(&x, &fbits, sizeof(x));
    return x;
#endif
}

float dotF16F32Scalar(const std::uint16_t* v, const float* q, std::size_t n)
{
    float s = 0.f;
    for (std::size_t i = 0; i < n; ++i)
    {
        s += halfBitsToFloat(v[i]) * q[i];
    }
    return s;
}

}  // namespace

SemanticSearcher::SemanticSearcher(void* dmaRingBuffer) : m_dmaBuffer(dmaRingBuffer)
{
    (void)m_dmaBuffer;
}

float SemanticSearcher::similarity(const IndexSlice& vec, const QueryContext& query)
{
    const auto* base = vec.data + vec.offset;

    switch (vec.format)
    {
        case QuantFormat::Q8_0:
        {
            const auto* qb = reinterpret_cast<const std::int8_t*>(base);
            if (vec.length >= 256 && query.dim >= 256)
            {
                return RawrXD_Q8_Q8_Dot256(qb, query.q8Query, vec.scale * query.q8Scale);
            }
            const std::size_t n = std::min(vec.length, query.dim);
            return dotQ8F32(qb, query.normalizedQuery, n, vec.scale);
        }
        case QuantFormat::F16:
        {
            const auto* hf = reinterpret_cast<const std::uint16_t*>(base);
            const std::size_t n = std::min<std::size_t>(vec.length / 2u, query.dim);
            if (n >= 16 && (n % 16u) == 0)
            {
                return RawrXD_F16F32_Dot(hf, query.normalizedQuery, n);
            }
            return dotF16F32Scalar(hf, query.normalizedQuery, n);
        }
        case QuantFormat::F32:
        {
            const std::size_t n = std::min(vec.length / sizeof(float), query.dim);
            return dotF32(reinterpret_cast<const float*>(base), query.normalizedQuery, n);
        }
        default:
            break;
    }

    return 0.f;
}

void SemanticSearcher::batchScan(std::span<const IndexSlice> candidates, const QueryContext& query, float* outScores,
                                 std::uint32_t* outIndices)
{
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        outScores[i] = similarity(candidates[i], query);
        outIndices[i] = static_cast<std::uint32_t>(i);
    }
#ifdef _MSC_VER
    _mm_sfence();
#endif
}

std::vector<std::pair<std::uint32_t, float>> SemanticSearcher::topK(const float* scores, const std::uint32_t* indices,
                                                                    std::size_t count, std::size_t k)
{
    if (count == 0 || k == 0)
    {
        return {};
    }
    k = std::min(k, count);
    std::vector<std::pair<std::uint32_t, float>> pairs;
    pairs.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        pairs.emplace_back(indices[i], scores[i]);
    }
    const auto mid = static_cast<std::ptrdiff_t>(k);
    std::partial_sort(pairs.begin(), pairs.begin() + mid, pairs.end(),
                      [](const auto& a, const auto& b) { return a.second > b.second; });
    pairs.resize(static_cast<std::size_t>(mid));
    std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return pairs;
}

}  // namespace RawrXD::Search
