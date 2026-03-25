// win32ide_symbol_impls_F.cpp — RawrXD IDE debug agentic symbol implementations

#include <cstdint>
#include <cstring>
#include <cmath>
#include <immintrin.h>

extern "C" {

// 1. General matrix multiply: C = A * B
//    A: MxK, B: KxN, C: MxN (row-major)
int asm_pyre_gemm_fp32(const float* A, const float* B, float* C,
                       uint32_t M, uint32_t N, uint32_t K)
{
    for (uint32_t i = 0; i < M; ++i) {
        for (uint32_t j = 0; j < N; ++j) {
            __m256 acc = _mm256_setzero_ps();
            const float* rowA = A + i * K;
            uint32_t k = 0;
            for (; k + 8 <= K; k += 8) {
                __m256 av = _mm256_loadu_ps(rowA + k);
                // Gather B[k..k+7][j] — stride N, so scalar gather
                // For perf with small N: build b_vec manually
                float bvals[8];
                for (int ii = 0; ii < 8; ++ii)
                    bvals[ii] = B[(k + ii) * N + j];
                __m256 bv = _mm256_loadu_ps(bvals);
                acc = _mm256_fmadd_ps(av, bv, acc);
            }
            // Horizontal sum of acc
            __m128 lo = _mm256_castps256_ps128(acc);
            __m128 hi = _mm256_extractf128_ps(acc, 1);
            __m128 sum4 = _mm_add_ps(lo, hi);
            __m128 shuf = _mm_movehdup_ps(sum4);
            __m128 sums = _mm_add_ps(sum4, shuf);
            shuf = _mm_movehl_ps(shuf, sums);
            sums = _mm_add_ss(sums, shuf);
            float result = _mm_cvtss_f32(sums);
            // Scalar remainder
            for (; k < K; ++k)
                result += rowA[k] * B[k * N + j];
            C[i * N + j] = result;
        }
    }
    return 0;
}

// 2. Matrix-vector multiply: y = A * x
//    A: MxK, x: Kx1, y: Mx1
int asm_pyre_gemv_fp32(const float* A, const float* x, float* y,
                       uint32_t M, uint32_t K)
{
    for (uint32_t i = 0; i < M; ++i) {
        const float* rowA = A + i * K;
        __m256 acc = _mm256_setzero_ps();
        uint32_t k = 0;
        for (; k + 8 <= K; k += 8) {
            __m256 av = _mm256_loadu_ps(rowA + k);
            __m256 xv = _mm256_loadu_ps(x + k);
            acc = _mm256_fmadd_ps(av, xv, acc);
        }
        // Horizontal sum
        __m128 lo = _mm256_castps256_ps128(acc);
        __m128 hi = _mm256_extractf128_ps(acc, 1);
        __m128 sum4 = _mm_add_ps(lo, hi);
        __m128 shuf = _mm_movehdup_ps(sum4);
        __m128 sums = _mm_add_ps(sum4, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        float result = _mm_cvtss_f32(sums);
        for (; k < K; ++k)
            result += rowA[k] * x[k];
        y[i] = result;
    }
    return 0;
}

// 3. RMS normalisation
//    ss = mean(input^2), scale = 1 / sqrt(ss + eps)
//    output[i] = input[i] * scale * weight[i]
int asm_pyre_rmsnorm(const float* input, const float* weight,
                     float* output, uint32_t dim, float eps)
{
    __m256 vss = _mm256_setzero_ps();
    uint32_t i = 0;
    for (; i + 8 <= dim; i += 8) {
        __m256 v = _mm256_loadu_ps(input + i);
        vss = _mm256_fmadd_ps(v, v, vss);
    }
    // Horizontal sum of vss
    __m128 lo = _mm256_castps256_ps128(vss);
    __m128 hi = _mm256_extractf128_ps(vss, 1);
    __m128 sum4 = _mm_add_ps(lo, hi);
    __m128 shuf = _mm_movehdup_ps(sum4);
    __m128 sums = _mm_add_ps(sum4, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    float ss = _mm_cvtss_f32(sums);
    // Scalar remainder
    for (; i < dim; ++i)
        ss += input[i] * input[i];

    ss = ss / static_cast<float>(dim);
    float scale = 1.0f / sqrtf(ss + eps);

    for (uint32_t j = 0; j < dim; ++j)
        output[j] = input[j] * scale * weight[j];
    return 0;
}

// 4. SiLU activation: inout[i] = inout[i] * sigmoid(inout[i])
//    sigmoid(x) = 1 / (1 + exp(-x))
int asm_pyre_silu(float* inout, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        float x = inout[i];
        inout[i] = x / (1.0f + expf(-x));
    }
    return 0;
}

// 5. Numerically stable softmax in-place
int asm_pyre_softmax(float* inout, uint32_t count)
{
    if (count == 0) return 0;

    // Find max
    float maxval = inout[0];
    for (uint32_t i = 1; i < count; ++i)
        if (inout[i] > maxval) maxval = inout[i];

    // exp(x - max) and accumulate sum
    float sum = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        inout[i] = expf(inout[i] - maxval);
        sum += inout[i];
    }

    // Normalise
    float inv_sum = 1.0f / sum;
    for (uint32_t i = 0; i < count; ++i)
        inout[i] *= inv_sum;

    return 0;
}

// 6. Rotary Position Embedding (RoPE)
//    data layout: [seqLen, headDim] row-major
//    For each position p, rotate pairs (data[p*headDim + i],
//                                       data[p*headDim + i + headDim/2])
int asm_pyre_rope(float* data, uint32_t seqLen, uint32_t headDim,
                  uint32_t seqOffset, float theta)
{
    uint32_t half = headDim / 2;
    for (uint32_t p = 0; p < seqLen; ++p) {
        float* row = data + p * headDim;
        uint32_t absPos = seqOffset + p;
        for (uint32_t i = 0; i < half; ++i) {
            float freq = 1.0f / powf(theta, (2.0f * static_cast<float>(i)) /
                                                static_cast<float>(headDim));
            float angle = static_cast<float>(absPos) * freq;
            float c = cosf(angle);
            float s = sinf(angle);
            float x = row[i];
            float y = row[i + half];
            row[i]        = c * x - s * y;
            row[i + half] = s * x + c * y;
        }
    }
    return 0;
}

// 7. Element-wise addition: out[i] = a[i] + b[i]
int asm_pyre_add_fp32(const float* a, const float* b, float* out,
                      uint32_t count)
{
    uint32_t i = 0;
    for (; i + 8 <= count; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        _mm256_storeu_ps(out + i, _mm256_add_ps(va, vb));
    }
    for (; i < count; ++i)
        out[i] = a[i] + b[i];
    return 0;
}

} // extern "C"
