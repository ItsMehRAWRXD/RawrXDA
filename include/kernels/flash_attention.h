#pragma once

#include <vector>
#include <math>
#include <functional>

/**
 * @brief Flash attention implementation for efficient transformer attention
 * @param Q Query matrix (seq_len x d_model)
 * @param K Key matrix (seq_len x d_model) 
 * @param V Value matrix (seq_len x d_model)
 * @param output Output matrix (seq_len x d_model)
 * @param seq_len Sequence length
 * @param d_model Model dimension
 * @param block_size Block size for tiling
 */
void flash_attention(const float* Q, const float* K, const float* V, 
                     float* output, int seq_len, int d_model, int block_size = 64);

/**
 * @brief Baseline attention implementation for comparison
 * @param Q Query matrix (seq_len x d_model)
 * @param K Key matrix (seq_len x d_model)
 * @param V Value matrix (seq_len x d_model)
 * @param output Output matrix (seq_len x d_model)
 * @param seq_len Sequence length
 * @param d_model Model dimension
 */
void attention_baseline(const float* Q, const float* K, const float* V,
                        float* output, int seq_len, int d_model);