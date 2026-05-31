#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef __AVX2__
#  include <immintrin.h>
#endif

namespace RawrXD::Kernels {

// ----------------------------------------------------------------------------
// Flash Attention 2: O(N) memory instead of O(N²).
// Fuses online softmax + weighted V accumulation so the full NxN matrix is
// never materialised.  Supports Grouped-Query Attention (GQA).
//
// Template parameter HEAD_DIM must be a multiple of 8 (AVX2 lane width).
// Instantiations for HEAD_DIM=64 and HEAD_DIM=128 are provided below.
// ----------------------------------------------------------------------------
template<int HEAD_DIM>
void flash_attention_2(
    float* __restrict__ output,       // [n_heads, HEAD_DIM]
    const float* __restrict__ q,      // [n_heads, HEAD_DIM]
    const float* __restrict__ k_cache,// [seq_len, kv_n_heads, HEAD_DIM]
    const float* __restrict__ v_cache,// [seq_len, kv_n_heads, HEAD_DIM]
    size_t n_heads,
    size_t seq_len,
    float  scale,
    size_t kv_n_heads,                // GQA: n_heads / kv_n_heads = head_groups
    int    thread_id,
    int    n_threads
) {
    static_assert(HEAD_DIM % 8 == 0, "HEAD_DIM must be a multiple of 8");

    const size_t head_groups      = n_heads / kv_n_heads;
    const size_t heads_per_thread = (n_heads + static_cast<size_t>(n_threads) - 1)
                                     / static_cast<size_t>(n_threads);
    const size_t head_start = static_cast<size_t>(thread_id) * heads_per_thread;
    const size_t head_end   = std::min(head_start + heads_per_thread, n_heads);

    // Tunable block size — keep QK block in L1
    constexpr size_t BLOCK_SIZE = 64;

    for (size_t h = head_start; h < head_end; ++h) {
        const size_t kv_head = h / head_groups;

        const float* q_h  = q      + h * HEAD_DIM;
        float*       out_h = output + h * HEAD_DIM;

        // Online softmax state
        float global_max = -1e30f;
        float global_sum =  0.0f;

        // Running weighted-V accumulator
        float acc[HEAD_DIM];
        std::memset(acc, 0, sizeof(acc));

        for (size_t blk = 0; blk < seq_len; blk += BLOCK_SIZE) {
            const size_t blk_end  = std::min(blk + BLOCK_SIZE, seq_len);
            const size_t blk_len  = blk_end - blk;

            // ---- QK^T for this block ----------------------------------------
            float qk[BLOCK_SIZE];
            for (size_t p = 0; p < blk_len; ++p) {
                const float* k_p = k_cache + (blk + p) * kv_n_heads * HEAD_DIM
                                           + kv_head * HEAD_DIM;
                float dot = 0.0f;
#ifdef __AVX2__
                for (int d = 0; d < HEAD_DIM; d += 8) {
                    __m256 qv = _mm256_loadu_ps(q_h + d);
                    __m256 kv = _mm256_loadu_ps(k_p + d);
                    __m256 pr = _mm256_mul_ps(qv, kv);
                    __m128 lo = _mm256_castps256_ps128(pr);
                    __m128 hi = _mm256_extractf128_ps(pr, 1);
                    __m128 s  = _mm_add_ps(lo, hi);
                    s = _mm_hadd_ps(s, s);
                    s = _mm_hadd_ps(s, s);
                    dot += _mm_cvtss_f32(s);
                }
#else
                for (int d = 0; d < HEAD_DIM; ++d) dot += q_h[d] * k_p[d];
#endif
                qk[p] = dot * scale;
            }

            // ---- Block max ---------------------------------------------------
            float blk_max = qk[0];
            for (size_t p = 1; p < blk_len; ++p)
                blk_max = std::max(blk_max, qk[p]);

            // ---- Online softmax update --------------------------------------
            const float new_max   = std::max(global_max, blk_max);
            const float scale_old = std::exp(global_max - new_max);

            // Rescale running accumulator
            for (int d = 0; d < HEAD_DIM; ++d) acc[d] *= scale_old;
            global_sum *= scale_old;

            // Exponentiate block scores and accumulate weighted V
            float blk_exp[BLOCK_SIZE];
            float blk_sum = 0.0f;
            for (size_t p = 0; p < blk_len; ++p) {
                blk_exp[p] = std::exp(qk[p] - new_max);
                blk_sum   += blk_exp[p];
            }

            for (size_t p = 0; p < blk_len; ++p) {
                const float  a   = blk_exp[p];
                const float* v_p = v_cache + (blk + p) * kv_n_heads * HEAD_DIM
                                           + kv_head * HEAD_DIM;
                for (int d = 0; d < HEAD_DIM; ++d) acc[d] += a * v_p[d];
            }

            global_sum += blk_sum;
            global_max  = new_max;
        }

        // ---- Normalise -------------------------------------------------------
        const float inv_sum = (global_sum > 0.0f) ? (1.0f / global_sum) : 0.0f;
        for (int d = 0; d < HEAD_DIM; ++d) out_h[d] = acc[d] * inv_sum;
    }
}

// Explicit instantiation declarations (definitions in flash_attention.cpp if LTO disabled)
extern template void flash_attention_2< 64>(float*, const float*, const float*, const float*,
                                            size_t, size_t, float, size_t, int, int);
extern template void flash_attention_2<128>(float*, const float*, const float*, const float*,
                                            size_t, size_t, float, size_t, int, int);

} // namespace RawrXD::Kernels
