#include "agent/agentic_deep_thinking_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// Standalone inference target does not link the full agent implementation TU.
AgenticDeepThinkingEngine::~AgenticDeepThinkingEngine() = default;

extern "C" {

int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
                       uint32_t M, uint32_t N, uint32_t K) {
    if (!A || !B || !C || M == 0 || N == 0 || K == 0) return -1;
    const size_t m_sz = static_cast<size_t>(M);
    const size_t n_sz = static_cast<size_t>(N);
    const size_t k_sz = static_cast<size_t>(K);
    if (m_sz > (std::numeric_limits<size_t>::max() / n_sz)) return -1;
    if (m_sz > (std::numeric_limits<size_t>::max() / k_sz)) return -1;
    if (k_sz > (std::numeric_limits<size_t>::max() / n_sz)) return -1;
    const size_t mn = m_sz * n_sz;
    if (mn > (std::numeric_limits<size_t>::max() / sizeof(float))) return -1;
    std::memset(C, 0, mn * sizeof(float));
    constexpr uint32_t kBlock = 32;
    for (uint32_t i0 = 0; i0 < M; i0 += kBlock) {
        const uint32_t iMax = std::min(M, i0 + kBlock);
        for (uint32_t k0 = 0; k0 < K; k0 += kBlock) {
            const uint32_t kMax = std::min(K, k0 + kBlock);
            for (uint32_t j0 = 0; j0 < N; j0 += kBlock) {
                const uint32_t jMax = std::min(N, j0 + kBlock);
                for (uint32_t i = i0; i < iMax; ++i) {
                    for (uint32_t k = k0; k < kMax; ++k) {
                        const float a = A[static_cast<size_t>(i) * K + k];
                        for (uint32_t j = j0; j < jMax; ++j) {
                            C[static_cast<size_t>(i) * N + j] += a * B[static_cast<size_t>(k) * N + j];
                        }
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < mn; ++i) {
        if (!std::isfinite(C[i])) return -1;
    }
    return 0;
}

int asm_pyre_gemv_fp32(const float* A, const float* x, float* y,
                       uint32_t M, uint32_t K) {
    if (!A || !x || !y || M == 0 || K == 0) return -1;
    const size_t m_sz = static_cast<size_t>(M);
    const size_t k_sz = static_cast<size_t>(K);
    if (m_sz > (std::numeric_limits<size_t>::max() / k_sz)) return -1;
    for (uint32_t i = 0; i < M; ++i) {
        double sum = 0.0;
        uint32_t k = 0;
        for (; k + 3 < K; k += 4) {
            sum += static_cast<double>(A[static_cast<size_t>(i) * K + (k + 0)]) * x[k + 0];
            sum += static_cast<double>(A[static_cast<size_t>(i) * K + (k + 1)]) * x[k + 1];
            sum += static_cast<double>(A[static_cast<size_t>(i) * K + (k + 2)]) * x[k + 2];
            sum += static_cast<double>(A[static_cast<size_t>(i) * K + (k + 3)]) * x[k + 3];
        }
        for (; k < K; ++k) {
            sum += static_cast<double>(A[static_cast<size_t>(i) * K + k]) * x[k];
        }
        const float out = static_cast<float>(sum);
        y[i] = std::isfinite(out) ? out : 0.0f;
    }
    return 0;
}

int asm_pyre_rmsnorm(const float* input, const float* weight, float* output,
                     uint32_t dim, float eps) {
    if (!input || !weight || !output || dim == 0) return -1;
    if (!std::isfinite(eps) || eps < 1e-12f) eps = 1e-12f;
    double ss = 0.0;
    for (uint32_t i = 0; i < dim; ++i) {
        const float x = input[i];
        if (!std::isfinite(x)) return -1;
        ss += static_cast<double>(x) * static_cast<double>(x);
    }
    if (!std::isfinite(ss)) return -1;
    const float invRms = 1.0f / std::sqrt(static_cast<float>(ss / static_cast<double>(dim)) + eps);
    if (!std::isfinite(invRms)) return -1;
    for (uint32_t i = 0; i < dim; ++i) {
        const float w = std::isfinite(weight[i]) ? weight[i] : 0.0f;
        const float out = input[i] * invRms * w;
        output[i] = std::isfinite(out) ? out : 0.0f;
    }
    return 0;
}

int asm_pyre_silu(float* inout, uint32_t count) {
    if (!inout || count == 0) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        const float x = std::isfinite(inout[i]) ? inout[i] : 0.0f;
        const float clamped = std::clamp(x, -80.0f, 80.0f);
        const float out = x / (1.0f + std::exp(-clamped));
        inout[i] = std::isfinite(out) ? out : 0.0f;
    }
    return 0;
}

int asm_pyre_softmax(float* inout, uint32_t count) {
    if (!inout || count == 0) return -1;
    float maxVal = -std::numeric_limits<float>::infinity();
    for (uint32_t i = 0; i < count; ++i) {
        const float v = inout[i];
        if (std::isfinite(v)) maxVal = std::max(maxVal, v);
    }
    if (!std::isfinite(maxVal)) return -1;

    double sum = 0.0;
    for (uint32_t i = 0; i < count; ++i) {
        const float v = std::isfinite(inout[i]) ? inout[i] : -std::numeric_limits<float>::infinity();
        const float shifted = std::clamp(v - maxVal, -80.0f, 80.0f);
        inout[i] = std::exp(shifted);
        sum += inout[i];
    }
    if (sum <= 0.0 || !std::isfinite(sum)) return -1;
    const float inv = static_cast<float>(1.0 / sum);
    for (uint32_t i = 0; i < count; ++i) {
        const float out = inout[i] * inv;
        inout[i] = std::isfinite(out) ? out : 0.0f;
    }
    return 0;
}

int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim,
                  uint32_t seqOffset, float theta) {
    if (!data || seqLen == 0 || headDim < 2 || !std::isfinite(theta) || theta <= 1.0f) return -1;
    const size_t seq_sz = static_cast<size_t>(seqLen);
    const size_t dim_sz = static_cast<size_t>(headDim);
    if (seq_sz > (std::numeric_limits<size_t>::max() / dim_sz)) return -1;
    const uint32_t half = headDim / 2;
    if (half == 0) return -1;
    constexpr float kTwoPi = 6.2831853071795864769f;
    for (uint32_t s = 0; s < seqLen; ++s) {
        float* row = data + static_cast<size_t>(s) * headDim;
        for (uint32_t i = 0; i < half; ++i) {
            const float freq = std::pow(theta, -2.0f * static_cast<float>(i) / static_cast<float>(headDim));
            if (!std::isfinite(freq)) return -1;
            const float rawAngle = static_cast<float>(seqOffset + s) * freq;
            const float angle = std::fmod(rawAngle, kTwoPi);
            const float c = std::cos(angle);
            const float sn = std::sin(angle);
            const float x0 = row[i];
            const float x1 = row[i + half];
            const float y0 = x0 * c - x1 * sn;
            const float y1 = x0 * sn + x1 * c;
            row[i] = std::isfinite(y0) ? y0 : 0.0f;
            row[i + half] = std::isfinite(y1) ? y1 : 0.0f;
        }
    }
    return 0;
}

int asm_pyre_add_fp32(const float* a, const float* b, float* out, uint32_t count) {
    if (!a || !b || !out || count == 0) return -1;
    for (uint32_t i = 0; i < count; ++i) out[i] = a[i] + b[i];
    return 0;
}

int asm_pyre_mul_fp32(const float* a, const float* b, float* out, uint32_t count) {
    if (!a || !b || !out || count == 0) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        const float v = a[i] * b[i];
        out[i] = std::isfinite(v) ? v : 0.0f;
    }
    return 0;
}

int asm_pyre_embedding_lookup(const float* table, const uint32_t* ids,
                              float* output, uint32_t count, uint32_t dim) {
    if (!table || !ids || !output || count == 0 || dim == 0) return -1;
    constexpr uint32_t kMaxReasonableVocab = 4'000'000;
    const size_t elem_size = sizeof(float);
    if (dim > (std::numeric_limits<size_t>::max() / elem_size)) return -1;
    const size_t row_bytes = static_cast<size_t>(dim) * elem_size;
    if (static_cast<size_t>(count) > (std::numeric_limits<size_t>::max() / static_cast<size_t>(dim))) return -1;
    for (uint32_t i = 0; i < count; ++i) {
        if (ids[i] >= kMaxReasonableVocab) return -1;
        if (static_cast<size_t>(ids[i]) > (std::numeric_limits<size_t>::max() / static_cast<size_t>(dim))) return -1;
        const size_t row_index = static_cast<size_t>(ids[i]) * static_cast<size_t>(dim);
        if (row_index > (std::numeric_limits<size_t>::max() / elem_size)) return -1;
        const float* row = table + row_index;
        float* out = output + static_cast<size_t>(i) * dim;
        std::memcpy(out, row, row_bytes);
    }
    return 0;
}

} // extern "C"
