#if !defined(_MSC_VER)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace RawrXD::NativeSpeed {

extern "C" void native_vdot_avx512(const float* a, const float* b, int n, float* result) {
    if (!result) {
        return;
    }
    if (!a || !b || n <= 0) {
        *result = 0.0f;
        return;
    }
    double accum = 0.0;
    for (int i = 0; i < n; ++i) {
        accum += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    }
    *result = static_cast<float>(accum);
}

extern "C" void dequant_q4_0_avx512(const void* src, float* dst, uint64_t nblocks) {
    if (!src || !dst) {
        return;
    }
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint64_t block = 0; block < nblocks; ++block) {
        for (int i = 0; i < 32; ++i) {
            const uint8_t packed = in[block * 16 + static_cast<uint64_t>(i / 2)];
            const int nibble = (i & 1) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
            dst[block * 32 + static_cast<uint64_t>(i)] = static_cast<float>(nibble - 8) * 0.125f;
        }
    }
}

extern "C" void dequant_q4_0_avx2(const void* src, float* dst, uint64_t nblocks) {
    dequant_q4_0_avx512(src, dst, nblocks);
}

extern "C" void dequant_q2k_avx2(const void* src, float* dst, uint64_t nblocks) {
    if (!src || !dst) {
        return;
    }
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint64_t block = 0; block < nblocks; ++block) {
        for (int i = 0; i < 128; ++i) {
            const uint8_t packed = in[block * 32 + static_cast<uint64_t>(i / 4)];
            const int shift = (i & 3) * 2;
            const int q = (packed >> shift) & 0x03;
            dst[block * 128 + static_cast<uint64_t>(i)] = static_cast<float>(q) - 1.5f;
        }
    }
}

extern "C" void dequant_q8_0_avx512(const void* src, float* dst, uint64_t nblocks) {
    if (!src || !dst) {
        return;
    }
    const auto* in = static_cast<const int8_t*>(src);
    for (uint64_t i = 0; i < nblocks * 32; ++i) {
        dst[i] = static_cast<float>(in[i]) * (1.0f / 127.0f);
    }
}

extern "C" void dequant_q8_0_avx2(const void* src, float* dst, uint64_t nblocks) {
    dequant_q8_0_avx512(src, dst, nblocks);
}

extern "C" void native_rmsnorm_avx2(const float* x, const float* weight, float* y, int dim, float eps) {
    if (!x || !weight || !y || dim <= 0) {
        return;
    }
    double sumSq = 0.0;
    for (int i = 0; i < dim; ++i) {
        sumSq += static_cast<double>(x[i]) * static_cast<double>(x[i]);
    }
    const float inv = 1.0f / std::sqrt(static_cast<float>(sumSq / static_cast<double>(dim)) + eps);
    for (int i = 0; i < dim; ++i) {
        y[i] = x[i] * inv * weight[i];
    }
}

extern "C" void native_softmax_avx2(float* x, int n) {
    if (!x || n <= 0) {
        return;
    }
    float maxv = x[0];
    for (int i = 1; i < n; ++i) {
        maxv = std::max(maxv, x[i]);
    }
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        x[i] = std::exp(x[i] - maxv);
        sum += x[i];
    }
    if (sum <= std::numeric_limits<double>::min()) {
        return;
    }
    const float inv = static_cast<float>(1.0 / sum);
    for (int i = 0; i < n; ++i) {
        x[i] *= inv;
    }
}

extern "C" void native_rope_avx2(float* q, float* k, int headDim, int nHeads, int nKVHeads, int pos, float theta) {
    if (!q || !k || headDim <= 1 || nHeads <= 0 || nKVHeads <= 0) {
        return;
    }
    const int pairs = headDim / 2;
    for (int h = 0; h < nHeads; ++h) {
        float* qh = q + static_cast<size_t>(h) * headDim;
        for (int i = 0; i < pairs; ++i) {
            const float freq = std::pow(theta, -2.0f * static_cast<float>(i) / static_cast<float>(headDim));
            const float angle = static_cast<float>(pos) * freq;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float a = qh[2 * i];
            const float b = qh[2 * i + 1];
            qh[2 * i] = a * c - b * s;
            qh[2 * i + 1] = a * s + b * c;
        }
    }
    for (int h = 0; h < nKVHeads; ++h) {
        float* kh = k + static_cast<size_t>(h) * headDim;
        for (int i = 0; i < pairs; ++i) {
            const float freq = std::pow(theta, -2.0f * static_cast<float>(i) / static_cast<float>(headDim));
            const float angle = static_cast<float>(pos) * freq;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float a = kh[2 * i];
            const float b = kh[2 * i + 1];
            kh[2 * i] = a * c - b * s;
            kh[2 * i + 1] = a * s + b * c;
        }
    }
}

extern "C" void native_nt_memcpy(void* dst, const void* src, size_t bytes) {
    if (!dst || !src || bytes == 0) {
        return;
    }
    std::memcpy(dst, src, bytes);
}

extern "C" void sgemm_avx512(const float* A, const float* B, float* C, int M, int N, int K) {
    if (!A || !B || !C || M <= 0 || N <= 0 || K <= 0) {
        return;
    }
    for (int m = 0; m < M; ++m) {
        for (int n = 0; n < N; ++n) {
            double sum = 0.0;
            for (int k = 0; k < K; ++k) {
                sum += static_cast<double>(A[m * K + k]) * static_cast<double>(B[k * N + n]);
            }
            C[m * N + n] = static_cast<float>(sum);
        }
    }
}

extern "C" void sgemv_avx512(const float* A, const float* x, float* y, int M, int K) {
    if (!A || !x || !y || M <= 0 || K <= 0) {
        return;
    }
    for (int m = 0; m < M; ++m) {
        double sum = 0.0;
        for (int k = 0; k < K; ++k) {
            sum += static_cast<double>(A[m * K + k]) * static_cast<double>(x[k]);
        }
        y[m] = static_cast<float>(sum);
    }
}

extern "C" void sgemv_avx2(const float* A, const float* x, float* y, int M, int K) {
    sgemv_avx512(A, x, y, M, K);
}

extern "C" void native_fused_mlp_avx2(const float* x, const float* W1,
                                      const float* b1, const float* W2,
                                      const float* b2, float* out,
                                      int seqLen, int hiddenDim, int ffnDim) {
    if (!x || !W1 || !W2 || !out || seqLen <= 0 || hiddenDim <= 0 || ffnDim <= 0) {
        return;
    }

    auto gelu = [](float v) -> float {
        constexpr float kAlpha = 0.7978845608f;   // sqrt(2/pi)
        constexpr float kBeta = 0.044715f;
        const float t = kAlpha * (v + kBeta * v * v * v);
        return 0.5f * v * (1.0f + std::tanh(t));
    };

    std::vector<float> hidden(static_cast<size_t>(ffnDim), 0.0f);
    for (int s = 0; s < seqLen; ++s) {
        const float* xs = x + static_cast<size_t>(s) * hiddenDim;

        for (int j = 0; j < ffnDim; ++j) {
            double sum = (b1 ? static_cast<double>(b1[j]) : 0.0);
            for (int i = 0; i < hiddenDim; ++i) {
                sum += static_cast<double>(xs[i]) *
                       static_cast<double>(W1[static_cast<size_t>(i) * ffnDim + j]);
            }
            hidden[static_cast<size_t>(j)] = gelu(static_cast<float>(sum));
        }

        float* ys = out + static_cast<size_t>(s) * hiddenDim;
        for (int i = 0; i < hiddenDim; ++i) {
            double sum = (b2 ? static_cast<double>(b2[i]) : 0.0);
            for (int j = 0; j < ffnDim; ++j) {
                sum += static_cast<double>(hidden[static_cast<size_t>(j)]) *
                       static_cast<double>(W2[static_cast<size_t>(j) * hiddenDim + i]);
            }
            ys[i] = static_cast<float>(sum);
        }
    }
}

extern "C" void qgemv_q4_0_avx2(const void* A, const float* x, float* y, int M, int K) {
    if (!A || !x || !y || M <= 0 || K <= 0) {
        return;
    }

    const auto* q = static_cast<const uint8_t*>(A);
    const int rowBytes = (K + 1) / 2;
    for (int m = 0; m < M; ++m) {
        double sum = 0.0;
        const uint8_t* row = q + static_cast<size_t>(m) * rowBytes;
        for (int k = 0; k < K; ++k) {
            const uint8_t packed = row[k / 2];
            const int nibble = ((k & 1) == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
            const int qv = nibble - 8;
            sum += static_cast<double>(qv) * 0.125 * static_cast<double>(x[k]);
        }
        y[m] = static_cast<float>(sum);
    }
}

extern "C" void qgemv_q8_0_avx2(const void* A, const float* x, float* y, int M, int K) {
    if (!A || !x || !y || M <= 0 || K <= 0) {
        return;
    }

    const auto* q = static_cast<const int8_t*>(A);
    for (int m = 0; m < M; ++m) {
        double sum = 0.0;
        const int8_t* row = q + static_cast<size_t>(m) * K;
        for (int k = 0; k < K; ++k) {
            sum += static_cast<double>(row[k]) * (1.0 / 127.0) * static_cast<double>(x[k]);
        }
        y[m] = static_cast<float>(sum);
    }
}

extern "C" void native_rmsnorm_avx512(const float* x, const float* weight, float* y, int dim, float eps) {
    native_rmsnorm_avx2(x, weight, y, dim, eps);
}

extern "C" void native_softmax_avx512(float* x, int n) {
    native_softmax_avx2(x, n);
}

extern "C" void sgemm_avx2(const float* A, const float* B, float* C, int M, int N, int K) {
    sgemm_avx512(A, B, C, M, N, K);
}

}  // namespace RawrXD::NativeSpeed

#endif  // !defined(_MSC_VER)
