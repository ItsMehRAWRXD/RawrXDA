#include "quantized_loaders.hpp"

#include <algorithm>
#include <immintrin.h>

namespace weights
{

std::vector<float> dequantize_q4_k(const Q4_KBlock* blocks, int n_blocks)
{
    std::vector<float> out(static_cast<size_t>(n_blocks) * 256u, 0.0f);
    for (int b = 0; b < n_blocks; ++b)
    {
        const auto& blk = blocks[b];
        const float d = static_cast<float>(blk.d);

        for (int i = 0; i < 256; ++i)
        {
            const int scale_idx = i / 16;
            const float s = static_cast<float>(blk.scales[scale_idx] & 0x3F);
            const uint8_t q = (i & 1) ? static_cast<uint8_t>(blk.qs[i / 2] >> 4) : static_cast<uint8_t>(blk.qs[i / 2] & 0x0F);
            out[static_cast<size_t>(b) * 256u + static_cast<size_t>(i)] = (static_cast<float>(q) - 8.0f) * s * d;
        }
    }
    return out;
}

std::vector<float> dequantize_q5_k(const Q5_KBlock* blocks, int n_blocks)
{
    std::vector<float> out(static_cast<size_t>(n_blocks) * 256u, 0.0f);
    for (int b = 0; b < n_blocks; ++b)
    {
        const auto& blk = blocks[b];
        const float d = static_cast<float>(blk.d);

        for (int i = 0; i < 256; ++i)
        {
            const int scale_idx = (i / 16) % 12;
            const float s = static_cast<float>(blk.scales[scale_idx] & 0x3F);
            const uint8_t low = (i & 1) ? static_cast<uint8_t>(blk.qs[i / 2] >> 4) : static_cast<uint8_t>(blk.qs[i / 2] & 0x0F);
            const uint8_t high = static_cast<uint8_t>((blk.qh[i / 8] >> (i % 8)) & 0x01u);
            const uint8_t q = static_cast<uint8_t>(low | (high << 4));
            out[static_cast<size_t>(b) * 256u + static_cast<size_t>(i)] = (static_cast<float>(q) - 16.0f) * s * d;
        }
    }
    return out;
}

std::vector<float> dequantize_q6_k(const Q6_KBlock* blocks, int n_blocks)
{
    std::vector<float> out(static_cast<size_t>(n_blocks) * 256u, 0.0f);
    for (int b = 0; b < n_blocks; ++b)
    {
        const auto& blk = blocks[b];
        const float d = static_cast<float>(blk.d);

        for (int i = 0; i < 256; ++i)
        {
            const int scale_idx = (i / 16) % 12;
            const float s = static_cast<float>(blk.scales[scale_idx] & 0x3F);
            const uint8_t low = (i & 1) ? static_cast<uint8_t>(blk.ql[i / 2] >> 4) : static_cast<uint8_t>(blk.ql[i / 2] & 0x0F);
            const uint8_t high = static_cast<uint8_t>((blk.qh[i / 4] >> ((i % 4) * 2)) & 0x03u);
            const uint8_t q = static_cast<uint8_t>(low | (high << 4));
            out[static_cast<size_t>(b) * 256u + static_cast<size_t>(i)] = (static_cast<float>(q) - 32.0f) * s * d;
        }
    }
    return out;
}

float matvec_q4_k(const Q4_KBlock* weights, const float* input, int rows, int cols)
{
    const int n_blocks = (cols + 255) / 256;
    float acc = 0.0f;
    for (int r = 0; r < rows; ++r)
    {
        auto row = dequantize_q4_k(weights + static_cast<size_t>(r) * static_cast<size_t>(n_blocks), n_blocks);
        float row_acc = 0.0f;
        for (int c = 0; c < cols; ++c)
        {
            row_acc += row[static_cast<size_t>(c)] * input[c];
        }
        acc += row_acc;
    }
    return acc;
}

float matvec_q5_k(const Q5_KBlock* weights, const float* input, int rows, int cols)
{
    const int n_blocks = (cols + 255) / 256;
    float acc = 0.0f;
    for (int r = 0; r < rows; ++r)
    {
        auto row = dequantize_q5_k(weights + static_cast<size_t>(r) * static_cast<size_t>(n_blocks), n_blocks);
        float row_acc = 0.0f;
        for (int c = 0; c < cols; ++c)
        {
            row_acc += row[static_cast<size_t>(c)] * input[c];
        }
        acc += row_acc;
    }
    return acc;
}

float matvec_q6_k(const Q6_KBlock* weights, const float* input, int rows, int cols)
{
    const int n_blocks = (cols + 255) / 256;
    float acc = 0.0f;
    for (int r = 0; r < rows; ++r)
    {
        auto row = dequantize_q6_k(weights + static_cast<size_t>(r) * static_cast<size_t>(n_blocks), n_blocks);
        float row_acc = 0.0f;
        for (int c = 0; c < cols; ++c)
        {
            row_acc += row[static_cast<size_t>(c)] * input[c];
        }
        acc += row_acc;
    }
    return acc;
}

float matvec_q4_k_avx2(const Q4_KBlock* weights, const float* input, int rows, int cols)
{
#if defined(__AVX2__)
    const int n_blocks = (cols + 255) / 256;
    float acc = 0.0f;
    for (int r = 0; r < rows; ++r)
    {
        auto row = dequantize_q4_k(weights + static_cast<size_t>(r) * static_cast<size_t>(n_blocks), n_blocks);
        __m256 sum = _mm256_setzero_ps();
        int c = 0;
        for (; c + 8 <= cols; c += 8)
        {
            __m256 w = _mm256_loadu_ps(row.data() + c);
            __m256 x = _mm256_loadu_ps(input + c);
            sum = _mm256_add_ps(sum, _mm256_mul_ps(w, x));
        }

        alignas(32) float lane[8];
        _mm256_store_ps(lane, sum);
        float row_acc = lane[0] + lane[1] + lane[2] + lane[3] + lane[4] + lane[5] + lane[6] + lane[7];
        for (; c < cols; ++c)
        {
            row_acc += row[static_cast<size_t>(c)] * input[c];
        }
        acc += row_acc;
    }
    return acc;
#else
    return matvec_q4_k(weights, input, rows, cols);
#endif
}

float matvec_q5_k_avx2(const Q5_KBlock* weights, const float* input, int rows, int cols)
{
    return matvec_q5_k(weights, input, rows, cols);
}

float matvec_q6_k_avx2(const Q6_KBlock* weights, const float* input, int rows, int cols)
{
    return matvec_q6_k(weights, input, rows, cols);
}

} // namespace weights
