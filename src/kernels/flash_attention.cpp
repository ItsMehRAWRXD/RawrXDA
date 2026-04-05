#include \"flash_attention.h\"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <chrono>

#if defined(__AVX512F__)
#include <immintrin.h>
#endif

// Real flash attention implementation with memory-efficient tiling
// Based on "FlashAttention: Fast and Memory-Efficient Exact Attention with IO-Awareness"

static inline void softmax_inplace(float* x, int size) {
    float max_val = x[0];
    for (int i = 1; i < size; i++) {
        max_val = std::max(max_val, x[i]);
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    float inv_sum = 1.0f / (sum + 1e-8f);
    for (int i = 0; i < size; i++) {
        x[i] *= inv_sum;
    }
}

static inline float DotProductF32(const float* a, const float* b, int length) {
#if defined(__AVX512F__)
    int i = 0;
    __m512 acc = _mm512_setzero_ps();
    for (; i + 16 <= length; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);
        acc = _mm512_fmadd_ps(va, vb, acc);
    }
    float sum = _mm512_reduce_add_ps(acc);
    for (; i < length; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#else
    float sum = 0.0f;
    for (int i = 0; i < length; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
#endif
}

static inline void AccumulateScaledF32(float* dst, const float* src, float scale, int length) {
#if defined(__AVX512F__)
    int i = 0;
    const __m512 s = _mm512_set1_ps(scale);
    for (; i + 16 <= length; i += 16) {
        __m512 vd = _mm512_loadu_ps(dst + i);
        __m512 vs = _mm512_loadu_ps(src + i);
        vd = _mm512_fmadd_ps(vs, s, vd);
        _mm512_storeu_ps(dst + i, vd);
    }
    for (; i < length; ++i) {
        dst[i] += src[i] * scale;
    }
#else
    for (int i = 0; i < length; ++i) {
        dst[i] += src[i] * scale;
    }
#endif
}

extern \"C\" {
    void flash_attention(float* q, float* k, float* v, int batch_size, int seq_len, int head_size, int num_heads, float* output) {
        const int block_size = 64;  // Tile size for memory efficiency
        const float scale = 1.0f / sqrtf(static_cast<float>(head_size));
        
        // Process each batch and head
        for (int b = 0; b < batch_size; b++) {
            for (int h = 0; h < num_heads; h++) {
                int offset = (b * num_heads + h) * seq_len * head_size;
                float* q_head = q + offset;
                float* k_head = k + offset;
                float* v_head = v + offset;
                float* out_head = output + offset;
                
                // Initialize output to zero
                std::memset(out_head, 0, seq_len * head_size * sizeof(float));
                
                // Tiled attention computation
                for (int i = 0; i < seq_len; i += block_size) {
                    int block_i_end = std::min(i + block_size, seq_len);
                    
                    for (int j = 0; j < seq_len; j += block_size) {
                        int block_j_end = std::min(j + block_size, seq_len);
                        
                        // Compute attention scores for this tile
                        std::vector<float> scores((block_i_end - i) * (block_j_end - j));
                        
                        for (int qi = i; qi < block_i_end; qi++) {
                            for (int ki = j; ki < block_j_end; ki++) {
                                // Q * K^T
                                const float* q_row = &q_head[qi * head_size];
                                const float* k_row = &k_head[ki * head_size];
                                float score = DotProductF32(q_row, k_row, head_size);
                                scores[(qi - i) * (block_j_end - j) + (ki - j)] = score * scale;
                            }
                            
                            // Apply softmax to this query's scores
                            softmax_inplace(&scores[(qi - i) * (block_j_end - j)], block_j_end - j);
                            
                            // Multiply by values and accumulate
                            for (int ki = j; ki < block_j_end; ki++) {
                                float attn_weight = scores[(qi - i) * (block_j_end - j) + (ki - j)];
                                float* out_row = &out_head[qi * head_size];
                                const float* v_row = &v_head[ki * head_size];
                                AccumulateScaledF32(out_row, v_row, attn_weight, head_size);
                            }
                        }
                    }
                }
            }
        }
    }
    
    void attention_baseline(float* q, float* k, float* v, int batch_size, int seq_len, int head_size, int num_heads, float* output) {
        // Standard non-tiled attention for comparison/testing
        const float scale = 1.0f / sqrtf(static_cast<float>(head_size));
        
        for (int b = 0; b < batch_size; b++) {
            for (int h = 0; h < num_heads; h++) {
                int offset = (b * num_heads + h) * seq_len * head_size;
                float* q_head = q + offset;
                float* k_head = k + offset;
                float* v_head = v + offset;
                float* out_head = output + offset;
                
                // Allocate full attention matrix
                std::vector<float> attn(seq_len * seq_len);
                
                // Compute Q * K^T
                for (int i = 0; i < seq_len; i++) {
                    for (int j = 0; j < seq_len; j++) {
                        const float* q_row = &q_head[i * head_size];
                        const float* k_row = &k_head[j * head_size];
                        float score = DotProductF32(q_row, k_row, head_size);
                        attn[i * seq_len + j] = score * scale;
                    }
                    
                    // Apply softmax row-wise
                    softmax_inplace(&attn[i * seq_len], seq_len);
                }
                
                // Multiply by V
                for (int i = 0; i < seq_len; i++) {
                    float* out_row = &out_head[i * head_size];
                    std::memset(out_row, 0, head_size * sizeof(float));
                    for (int j = 0; j < seq_len; j++) {
                        const float weight = attn[i * seq_len + j];
                        const float* v_row = &v_head[j * head_size];
                        AccumulateScaledF32(out_row, v_row, weight, head_size);
                    }
                }
            }
        }
    }

    // Bench helper: returns average latency in milliseconds and optionally outputs TPS.
    double MeasureAttentionLatency(int batch_size, int seq_len, int head_size, int num_heads,
                                   int warmup_iters, int measure_iters, double* out_tps) {
        if (batch_size <= 0 || seq_len <= 0 || head_size <= 0 || num_heads <= 0 ||
            warmup_iters < 0 || measure_iters <= 0) {
            if (out_tps) {
                *out_tps = 0.0;
            }
            return 0.0;
        }

        const size_t total = static_cast<size_t>(batch_size) * static_cast<size_t>(num_heads) *
                             static_cast<size_t>(seq_len) * static_cast<size_t>(head_size);
        std::vector<float> q(total, 0.125f);
        std::vector<float> k(total, 0.25f);
        std::vector<float> v(total, 0.5f);
        std::vector<float> out(total, 0.0f);

        for (int i = 0; i < warmup_iters; ++i) {
            flash_attention(q.data(), k.data(), v.data(), batch_size, seq_len, head_size, num_heads, out.data());
        }

        const auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < measure_iters; ++i) {
            flash_attention(q.data(), k.data(), v.data(), batch_size, seq_len, head_size, num_heads, out.data());
        }
        const auto end = std::chrono::high_resolution_clock::now();

        const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        const double avg_ms = elapsed_ms / static_cast<double>(measure_iters);
        const double elapsed_s = elapsed_ms / 1000.0;
        const double tokens = static_cast<double>(batch_size) * static_cast<double>(seq_len) * static_cast<double>(measure_iters);

        if (out_tps) {
            *out_tps = (elapsed_s > 0.0) ? (tokens / elapsed_s) : 0.0;
        }
        return avg_ms;
    }
}