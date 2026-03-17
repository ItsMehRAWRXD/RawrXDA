// flash_attn_neon.cc — ARM-NEON Flash-Attention kernel  
// Phase 5: Port AVX2 Flash-Attention to ARM64 for Apple Silicon parity
// Target: ≥3× speedup on M1/M2/M3 vs scalar on 4K context

#include <arm_neon.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <limits>

// Scalar baseline with true online softmax
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

#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
// ARM-NEON accelerated flash-attention with online softmax
static void flash_attn_neon_impl(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim
) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    const float32x4_t vscale = vdupq_n_f32(scale);
    
    for (int q_idx = 0; q_idx < seq_len; ++q_idx) {
        const float* q_row = Q + q_idx * head_dim;
        float* out_row = O + q_idx * head_dim;
        
        float running_max = -std::numeric_limits<float>::infinity();
        float running_sum = 0.0f;
        
        // Zero output
        for (int d = 0; d < head_dim; d += 4) {
            vst1q_f32(out_row + d, vdupq_n_f32(0.0f));
        }
        
        // Process all K/V rows for this query
        for (int k_idx = 0; k_idx < seq_len; ++k_idx) {
            const float* k_row = K + k_idx * head_dim;
            const float* v_row = V + k_idx * head_dim;
            
            // Compute QK^T score using NEON FMA
            float32x4_t vdot = vdupq_n_f32(0.0f);
            for (int d = 0; d < head_dim; d += 4) {
                float32x4_t vq = vld1q_f32(q_row + d);
                float32x4_t vk = vld1q_f32(k_row + d);
                vdot = vfmaq_f32(vdot, vq, vk);  // FMA: vdot += vq * vk
            }
            
            // Horizontal sum: reduce 4×float32 to scalar
            float32x2_t vsum_lo = vget_low_f32(vdot);
            float32x2_t vsum_hi = vget_high_f32(vdot);
            float32x2_t vsum = vpadd_f32(vsum_lo, vsum_hi);  // pairwise add
            vsum = vpadd_f32(vsum, vsum);                    // final reduction
            float qk_score = vget_lane_f32(vsum, 0) * scale;
            
            // Update running max
            float old_max = running_max;
            float new_max = std::max(old_max, qk_score);
            float correction = std::exp(old_max - new_max);
            
            // Rescale previous contributions
            float32x4_t vcorrection = vdupq_n_f32(correction);
            for (int d = 0; d < head_dim; d += 4) {
                float32x4_t vout = vld1q_f32(out_row + d);
                vout = vmulq_f32(vout, vcorrection);
                vst1q_f32(out_row + d, vout);
            }
            running_sum *= correction;
            
            // Add new contribution
            float p = std::exp(qk_score - new_max);
            running_sum += p;
            float32x4_t vp = vdupq_n_f32(p);
            for (int d = 0; d < head_dim; d += 4) {
                float32x4_t vv = vld1q_f32(v_row + d);
                float32x4_t vout = vld1q_f32(out_row + d);
                vout = vfmaq_f32(vout, vp, vv);  // FMA: vout += vp * vv
                vst1q_f32(out_row + d, vout);
            }
            
            running_max = new_max;
        }
        
        // Final normalization
        float inv_sum = 1.0f / running_sum;
        float32x4_t vinv_sum = vdupq_n_f32(inv_sum);
        for (int d = 0; d < head_dim; d += 4) {
            float32x4_t vout = vld1q_f32(out_row + d);
            vout = vmulq_f32(vout, vinv_sum);
            vst1q_f32(out_row + d, vout);
        }
    }
}
#endif

// Public API with runtime dispatch
extern "C" void flash_attn_forward(
    const float* Q, const float* K, const float* V, float* O,
    int seq_len, int head_dim, bool force_scalar
) {
#if defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
    if (!force_scalar) {
        flash_attn_neon_impl(Q, K, V, O, seq_len, head_dim);
        return;
    }
#endif
    flash_attn_scalar(Q, K, V, O, seq_len, head_dim);
}

// Convenience wrapper (matches AVX2 signature)
extern "C" void flash_attention(const float* Q, const float* K, const float* V, 
                                float* O, int seqLen, int headDim) {
    flash_attn_forward(Q, K, V, O, seqLen, headDim, false);
}
