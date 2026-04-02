// Reference FP32 helpers for weighted multi-expert FFN down-project accumulation (MoE).
// Intended for: (1) CI parity between looped vs grouped paths, (2) future grouped-GEMM integration.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

namespace RawrXD
{
namespace MoEAccumRef
{

/// Row-major GEMM accumulate: \p C += \p A * \p B with A [M×K], B [K×N], C [M×N].
inline void gemmRowMajorAccumF32(std::size_t M, std::size_t N, std::size_t K, const float* A, const float* B, float* C)
{
    for (std::size_t m = 0; m < M; ++m)
    {
        for (std::size_t n = 0; n < N; ++n)
        {
            double sum = 0;
            for (std::size_t k = 0; k < K; ++k)
                sum += static_cast<double>(A[m * K + k]) * static_cast<double>(B[k * N + n]);
            C[m * N + n] += static_cast<float>(sum);
        }
    }
}

/// \p hidden [inDim], each \p WDown[k] row-major [inDim × outDim]: row i is contiguous outDim floats.
/// Computes acc[j] += sum_k weights[k] * (hidden · WDown[k][*,j]).
inline void moeDownProjectLoopedF32(const float* hidden, std::size_t inDim, std::size_t outDim, int numExperts,
                                    const float* const* WDown, const float* weights, float* acc)
{
    std::vector<float> tmp(outDim);
    for (int e = 0; e < numExperts; ++e)
    {
        const float w = weights[static_cast<std::size_t>(e)];
        if (w == 0.f)
            continue;
        std::fill(tmp.begin(), tmp.end(), 0.f);
        const float* W = WDown[static_cast<std::size_t>(e)];
        for (std::size_t i = 0; i < inDim; ++i)
        {
            const float hi = hidden[i];
            const float* row = W + i * outDim;
            for (std::size_t j = 0; j < outDim; ++j)
                tmp[j] += hi * row[j];
        }
        for (std::size_t j = 0; j < outDim; ++j)
            acc[j] += w * tmp[j];
    }
}

/// Pack expert weights side-by-side: packed row-major [inDim × (numExperts * outDim)].
inline void packExpertDownWeightsF32(std::size_t inDim, std::size_t outDim, int numExperts, const float* const* WDown,
                                     float* packed)
{
    const std::size_t stride = static_cast<std::size_t>(numExperts) * outDim;
    for (std::size_t i = 0; i < inDim; ++i)
    {
        for (int e = 0; e < numExperts; ++e)
        {
            const float* src = WDown[static_cast<std::size_t>(e)] + i * outDim;
            float* dst = packed + i * stride + static_cast<std::size_t>(e) * outDim;
            std::memcpy(dst, src, outDim * sizeof(float));
        }
    }
}

/// Grouped path: one small GEMM (1×inDim)·(inDim×(K·outDim)) then scale each expert block by \p weights and sum into \p
/// acc.
inline void moeDownProjectGroupedF32(const float* hidden, std::size_t inDim, std::size_t outDim, int numExperts,
                                     const float* const* WDown, const float* weights, float* acc,
                                     std::vector<float>& workspace)
{
    const std::size_t Ntot = static_cast<std::size_t>(numExperts) * outDim;
    workspace.resize(Ntot);
    std::fill(workspace.begin(), workspace.end(), 0.f);
    std::vector<float> packed(inDim * Ntot);
    packExpertDownWeightsF32(inDim, outDim, numExperts, WDown, packed.data());
    // C[1×Ntot] += (1×inDim)·(inDim×Ntot)
    gemmRowMajorAccumF32(1, Ntot, inDim, hidden, packed.data(), workspace.data());
    for (int e = 0; e < numExperts; ++e)
    {
        const float w = weights[static_cast<std::size_t>(e)];
        if (w == 0.f)
            continue;
        const float* block = workspace.data() + static_cast<std::size_t>(e) * outDim;
        for (std::size_t j = 0; j < outDim; ++j)
            acc[j] += w * block[j];
    }
}

[[nodiscard]] inline float maxAbsDiffF32(std::size_t n, const float* a, const float* b)
{
    float m = 0.f;
    for (std::size_t i = 0; i < n; ++i)
        m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

}  // namespace MoEAccumRef
}  // namespace RawrXD
