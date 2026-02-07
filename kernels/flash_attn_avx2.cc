// flash_attn_avx2.cc — Flash-Attention with True Online Softmax (C+intrinsics)
// Phase 4: O(n²) → O(n) memory for long-context inference
// Target: ≥10× speedup on seq=4096, head_dim=64

#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Baseline scalar flash-attention with true online softmax
static void flash_attn_scalar(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim
) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    
    for (int q_idx = 0; q_idx < seq_len; ++q_idx) {
        const float* q_row = Q + q_idx * head_dim;
        float* out_row = O + q_idx * head_dim;
        
        float running_max = -std::numeric_limits<float>::infinity();
        float running_sum = 0.0f;
        std::memset(out_row, 0, head_dim * sizeof(float));
        
        // Process all K/V rows for this query
        for (int k_idx = 0; k_idx < seq_len; ++k_idx) {
            const float* k_row = K + k_idx * head_dim;
            const float* v_row = V + k_idx * head_dim;
            
            // Compute QK^T score
            float qk_score = 0.0f;
            for (int d = 0; d < head_dim; ++d) {
                qk_score += q_row[d] * k_row[d];
            }
            qk_score *= scale;
            
            // Update running max
            float old_max = running_max;
            float new_max = std::max(old_max, qk_score);
            float correction = std::exp(old_max - new_max);
            
            // Rescale previous contributions
            for (int d = 0; d < head_dim; ++d) {
                out_row[d] *= correction;
            }
            running_sum *= correction;
            
            // Add new contribution
            float p = std::exp(qk_score - new_max);
            running_sum += p;
            for (int d = 0; d < head_dim; ++d) {
                out_row[d] += p * v_row[d];
            }
            
            running_max = new_max;
        }
        
        // Final normalization
        float inv_sum = 1.0f / running_sum;
        for (int d = 0; d < head_dim; ++d) {
            out_row[d] *= inv_sum;
        }
    }
}

#ifdef __AVX2__
// AVX2-accelerated flash-attention with online softmax
static void flash_attn_avx2_impl(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim
) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    const __m256 vscale = _mm256_set1_ps(scale);
    
    for (int q_idx = 0; q_idx < seq_len; ++q_idx) {
        const float* q_row = Q + q_idx * head_dim;
        float* out_row = O + q_idx * head_dim;
        
        __m256 vrunning_max = _mm256_set1_ps(-std::numeric_limits<float>::infinity());
        __m256 vrunning_sum = _mm256_setzero_ps();
        
        // Zero output
        for (int d = 0; d < head_dim; d += 8) {
            _mm256_storeu_ps(out_row + d, _mm256_setzero_ps());
        }
        
        float running_max = -std::numeric_limits<float>::infinity();
        float running_sum = 0.0f;
        
        // Process all K/V rows for this query
        for (int k_idx = 0; k_idx < seq_len; ++k_idx) {
            const float* k_row = K + k_idx * head_dim;
            const float* v_row = V + k_idx * head_dim;
            
            // Compute QK^T score using AVX2
            __m256 vdot = _mm256_setzero_ps();
            for (int d = 0; d < head_dim; d += 8) {
                __m256 vq = _mm256_loadu_ps(q_row + d);
                __m256 vk = _mm256_loadu_ps(k_row + d);
                vdot = _mm256_fmadd_ps(vq, vk, vdot);
            }
            
            // Horizontal sum
            __m128 vlow = _mm256_castps256_ps128(vdot);
            __m128 vhigh = _mm256_extractf128_ps(vdot, 1);
            vlow = _mm_add_ps(vlow, vhigh);
            vlow = _mm_hadd_ps(vlow, vlow);
            vlow = _mm_hadd_ps(vlow, vlow);
            float qk_score = _mm_cvtss_f32(vlow) * scale;
            
            // Update running max
            float old_max = running_max;
            float new_max = std::max(old_max, qk_score);
            float correction = std::exp(old_max - new_max);
            
            // Rescale previous contributions
            __m256 vcorrection = _mm256_set1_ps(correction);
            for (int d = 0; d < head_dim; d += 8) {
                __m256 vout = _mm256_loadu_ps(out_row + d);
                vout = _mm256_mul_ps(vout, vcorrection);
                _mm256_storeu_ps(out_row + d, vout);
            }
            running_sum *= correction;
            
            // Add new contribution
            float p = std::exp(qk_score - new_max);
            running_sum += p;
            __m256 vp = _mm256_set1_ps(p);
            for (int d = 0; d < head_dim; d += 8) {
                __m256 vv = _mm256_loadu_ps(v_row + d);
                __m256 vout = _mm256_loadu_ps(out_row + d);
                vout = _mm256_fmadd_ps(vp, vv, vout);
                _mm256_storeu_ps(out_row + d, vout);
            }
            
            running_max = new_max;
        }
        
        // Final normalization
        float inv_sum = 1.0f / running_sum;
        __m256 vinv_sum = _mm256_set1_ps(inv_sum);
        for (int d = 0; d < head_dim; d += 8) {
            __m256 vout = _mm256_loadu_ps(out_row + d);
            vout = _mm256_mul_ps(vout, vinv_sum);
            _mm256_storeu_ps(out_row + d, vout);
        }
    }
}
#endif

// Public API with runtime dispatch
extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar
) {
#ifdef __AVX2__
    if (!force_scalar) {
        flash_attn_avx2_impl(Q, K, V, O, seq_len, head_dim);
        return;
    }
#endif
    flash_attn_scalar(Q, K, V, O, seq_len, head_dim);
}
