#include \"flash_attention.h\"
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>

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
                                float score = 0.0f;
                                for (int d = 0; d < head_size; d++) {
                                    score += q_head[qi * head_size + d] * k_head[ki * head_size + d];
                                }
                                scores[(qi - i) * (block_j_end - j) + (ki - j)] = score * scale;
                            }
                            
                            // Apply softmax to this query's scores
                            softmax_inplace(&scores[(qi - i) * (block_j_end - j)], block_j_end - j);
                            
                            // Multiply by values and accumulate
                            for (int ki = j; ki < block_j_end; ki++) {
                                float attn_weight = scores[(qi - i) * (block_j_end - j) + (ki - j)];
                                for (int d = 0; d < head_size; d++) {
                                    out_head[qi * head_size + d] += attn_weight * v_head[ki * head_size + d];
                                }
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
                        float score = 0.0f;
                        for (int d = 0; d < head_size; d++) {
                            score += q_head[i * head_size + d] * k_head[j * head_size + d];
                        }
                        attn[i * seq_len + j] = score * scale;
                    }
                    
                    // Apply softmax row-wise
                    softmax_inplace(&attn[i * seq_len], seq_len);
                }
                
                // Multiply by V
                for (int i = 0; i < seq_len; i++) {
                    for (int d = 0; d < head_size; d++) {
                        float sum = 0.0f;
                        for (int j = 0; j < seq_len; j++) {
                            sum += attn[i * seq_len + j] * v_head[j * head_size + d];
                        }
                        out_head[i * head_size + d] = sum;
                    }
                }
            }
        }
    }
}