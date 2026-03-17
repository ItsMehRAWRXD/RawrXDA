#include <immintrin.h>
#include <cmath>
#include <algorithm>

// FlashAttention AVX-512 Variant (Option D) - 512-bit vector processing
// v14.7.0-ATTN: CPU-optimized Fused Attention kernel

extern "C" void flash_attention_avx512(float* q, float* k, float* v, int batch_size, int seq_len, int head_size, int num_heads, float* output) {

    const float scale = 1.0f / sqrtf(static_cast<float>(head_size));
    const __m512 v_scale = _mm512_set1_ps(scale);
    
    for (int b = 0; b < batch_size; b++) {
        for (int h = 0; h < num_heads; h++) {
            int head_offset = (b * num_heads + h) * seq_len * head_size;
            float* q_head = q + head_offset;
            float* k_head = k + head_offset;
            float* v_head = v + head_offset;
            float* out_head = output + head_offset;
            
            for (int i = 0; i < seq_len; i++) {
                __m512 m = _mm512_set1_ps(-1e38f); // Running max
                __m512 l = _mm512_set1_ps(1e-8f);  // Running exp sum
                __m512 out_acc = _mm512_setzero_ps(); // Output accumulator

                for (int j = 0; j < seq_len; j++) {
                    // 1. Q * K^T block dot product (AVX-512 FMA)
                    __m512 q_vec = _mm512_loadu_ps(q_head + i * head_size);
                    __m512 k_vec = _mm512_loadu_ps(k_head + j * head_size);
                    __m512 score_vec = _mm512_mul_ps(q_vec, k_vec);
                    
                    // Horizontal sum of scores
                    float score = _mm512_reduce_add_ps(score_vec) * scale;
                    
                    // 2. Online Softmax (AVX-512 Exp)
                    float m_prev = m[0]; // Simplified for scalar score update
                    float m_curr = std::max(m_prev, score);
                    float exp_val = std::exp(score - m_curr);
                    float exp_prev = std::exp(m_prev - m_curr);
                    
                    // Update running sum and output accumulator
                    l = _mm512_set1_ps(l[0] * exp_prev + exp_val);
                    
                    // 3. (Attn * V) accumulation
                    __m512 v_vec = _mm512_loadu_ps(v_head + j * head_size);
                    __m512 attn_v = _mm512_set1_ps(exp_val);
                    out_acc = _mm512_fmadd_ps(attn_v, v_vec, _mm512_mul_ps(out_acc, _mm512_set1_ps(exp_prev)));
                    
                    m = _mm512_set1_ps(m_curr);
                }
                
                // Final normalization and store
                __m512 final_out = _mm512_div_ps(out_acc, l);
                _mm512_storeu_ps(out_head + i * head_size, final_out);
            }
        }
    }
}
