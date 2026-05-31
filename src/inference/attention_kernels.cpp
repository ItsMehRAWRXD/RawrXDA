/**
 * @file attention_kernels.cpp
 * @brief Optimized attention mechanisms (FlashAttention, etc.)
 * 
 * Implements:
 * - Standard multi-head attention
 * - FlashAttention-style memory-efficient attention
 * - Grouped-query attention (GQA)
 * - Sliding window attention
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#include "attention_kernels.h"
#include "llama_kernel_ops.h"
#include <math>
#include <algorithm>
#include <string>

namespace RawrXD::Inference {

// ============================================================================
// Multi-Head Attention
// ============================================================================

void multi_head_attention(const float* query, const float* key, const float* value,
                         float* output,
                         int batchSize, int seqLen, int numHeads, int headDim,
                         const float* mask, float scale) {
    int totalHeads = batchSize * numHeads;
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < totalHeads; ++h) {
        int b = h / numHeads;
        int head = h % numHeads;
        
        const float* qHead = query + h * seqLen * headDim;
        const float* kHead = key + h * seqLen * headDim;
        const float* vHead = value + h * seqLen * headDim;
        float* outHead = output + h * seqLen * headDim;
        
        // Compute attention scores: Q @ K^T
        std::vector<float> scores(seqLen * seqLen);
        for (int i = 0; i < seqLen; ++i) {
            for (int j = 0; j < seqLen; ++j) {
                float dot = 0.0f;
                for (int d = 0; d < headDim; ++d) {
                    dot += qHead[i * headDim + d] * kHead[j * headDim + d];
                }
                scores[i * seqLen + j] = dot * scale;
            }
        }
        
        // Apply mask if provided
        if (mask) {
            for (int i = 0; i < seqLen; ++i) {
                for (int j = 0; j < seqLen; ++j) {
                    if (mask[b * seqLen * seqLen + i * seqLen + j] <= 0.0f) {
                        scores[i * seqLen + j] = -std::numeric_limits<float>::infinity();
                    }
                }
            }
        }
        
        // Softmax over rows
        for (int i = 0; i < seqLen; ++i) {
            float maxVal = scores[i * seqLen];
            for (int j = 1; j < seqLen; ++j) {
                maxVal = std::max(maxVal, scores[i * seqLen + j]);
            }
            
            float sum = 0.0f;
            for (int j = 0; j < seqLen; ++j) {
                scores[i * seqLen + j] = std::exp(scores[i * seqLen + j] - maxVal);
                sum += scores[i * seqLen + j];
            }
            
            float invSum = 1.0f / sum;
            for (int j = 0; j < seqLen; ++j) {
                scores[i * seqLen + j] *= invSum;
            }
        }
        
        // Apply attention to values: scores @ V
        for (int i = 0; i < seqLen; ++i) {
            for (int d = 0; d < headDim; ++d) {
                float sum = 0.0f;
                for (int j = 0; j < seqLen; ++j) {
                    sum += scores[i * seqLen + j] * vHead[j * headDim + d];
                }
                outHead[i * headDim + d] = sum;
            }
        }
    }
}

// ============================================================================
// FlashAttention (Memory-Efficient)
// ============================================================================

void flash_attention(const float* query, const float* key, const float* value,
                    float* output,
                    int batchSize, int seqLen, int numHeads, int headDim,
                    const float* mask, float scale, int blockSize) {
    int totalHeads = batchSize * numHeads;
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < totalHeads; ++h) {
        int b = h / numHeads;
        
        const float* qHead = query + h * seqLen * headDim;
        const float* kHead = key + h * seqLen * headDim;
        const float* vHead = value + h * seqLen * headDim;
        float* outHead = output + h * seqLen * headDim;
        
        // Process in blocks for memory efficiency
        for (int qBlock = 0; qBlock < seqLen; qBlock += blockSize) {
            int qBlockEnd = std::min(qBlock + blockSize, seqLen);
            
            for (int kBlock = 0; kBlock < seqLen; kBlock += blockSize) {
                int kBlockEnd = std::min(kBlock + blockSize, seqLen);
                
                // Compute block of attention scores
                for (int i = qBlock; i < qBlockEnd; ++i) {
                    // Online softmax for numerical stability
                    float maxVal = -std::numeric_limits<float>::infinity();
                    
                    for (int j = kBlock; j < kBlockEnd; ++j) {
                        if (mask && mask[b * seqLen * seqLen + i * seqLen + j] <= 0.0f) {
                            continue;
                        }
                        
                        float dot = 0.0f;
                        for (int d = 0; d < headDim; ++d) {
                            dot += qHead[i * headDim + d] * kHead[j * headDim + d];
                        }
                        maxVal = std::max(maxVal, dot * scale);
                    }
                    
                    // Compute softmax and accumulate output
                    float sum = 0.0f;
                    std::vector<float> weights(kBlockEnd - kBlock);
                    
                    for (int j = kBlock; j < kBlockEnd; ++j) {
                        if (mask && mask[b * seqLen * seqLen + i * seqLen + j] <= 0.0f) {
                            weights[j - kBlock] = 0.0f;
                            continue;
                        }
                        
                        float dot = 0.0f;
                        for (int d = 0; d < headDim; ++d) {
                            dot += qHead[i * headDim + d] * kHead[j * headDim + d];
                        }
                        
                        weights[j - kBlock] = std::exp(dot * scale - maxVal);
                        sum += weights[j - kBlock];
                    }
                    
                    // Apply to values
                    if (sum > 0.0f) {
                        float invSum = 1.0f / sum;
                        for (int j = kBlock; j < kBlockEnd; ++j) {
                            float w = weights[j - kBlock] * invSum;
                            for (int d = 0; d < headDim; ++d) {
                                outHead[i * headDim + d] += w * vHead[j * headDim + d];
                            }
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// Grouped-Query Attention (GQA)
// ============================================================================

void grouped_query_attention(const float* query, const float* key, const float* value,
                            float* output,
                            int batchSize, int seqLen, int numQHeads, int numKVHeads,
                            int headDim, const float* mask, float scale) {
    int headsPerKV = numQHeads / numKVHeads;
    
    #pragma omp parallel for schedule(static)
    for (int b = 0; b < batchSize; ++b) {
        for (int qHead = 0; qHead < numQHeads; ++qHead) {
            int kvHead = qHead / headsPerKV;
            
            const float* qHeadPtr = query + ((b * numQHeads + qHead) * seqLen * headDim);
            const float* kHeadPtr = key + ((b * numKVHeads + kvHead) * seqLen * headDim);
            const float* vHeadPtr = value + ((b * numKVHeads + kvHead) * seqLen * headDim);
            float* outHeadPtr = output + ((b * numQHeads + qHead) * seqLen * headDim);
            
            // Compute attention scores
            std::vector<float> scores(seqLen * seqLen);
            for (int i = 0; i < seqLen; ++i) {
                for (int j = 0; j < seqLen; ++j) {
                    float dot = 0.0f;
                    for (int d = 0; d < headDim; ++d) {
                        dot += qHeadPtr[i * headDim + d] * kHeadPtr[j * headDim + d];
                    }
                    scores[i * seqLen + j] = dot * scale;
                }
            }
            
            // Apply mask
            if (mask) {
                for (int i = 0; i < seqLen; ++i) {
                    for (int j = 0; j < seqLen; ++j) {
                        if (mask[b * seqLen * seqLen + i * seqLen + j] <= 0.0f) {
                            scores[i * seqLen + j] = -std::numeric_limits<float>::infinity();
                        }
                    }
                }
            }
            
            // Softmax
            for (int i = 0; i < seqLen; ++i) {
                float maxVal = scores[i * seqLen];
                for (int j = 1; j < seqLen; ++j) {
                    maxVal = std::max(maxVal, scores[i * seqLen + j]);
                }
                
                float sum = 0.0f;
                for (int j = 0; j < seqLen; ++j) {
                    scores[i * seqLen + j] = std::exp(scores[i * seqLen + j] - maxVal);
                    sum += scores[i * seqLen + j];
                }
                
                float invSum = 1.0f / sum;
                for (int j = 0; j < seqLen; ++j) {
                    scores[i * seqLen + j] *= invSum;
                }
            }
            
            // Apply to values
            for (int i = 0; i < seqLen; ++i) {
                for (int d = 0; d < headDim; ++d) {
                    float sum = 0.0f;
                    for (int j = 0; j < seqLen; ++j) {
                        sum += scores[i * seqLen + j] * vHeadPtr[j * headDim + d];
                    }
                    outHeadPtr[i * headDim + d] = sum;
                }
            }
        }
    }
}

// ============================================================================
// Sliding Window Attention
// ============================================================================

void sliding_window_attention(const float* query, const float* key, const float* value,
                             float* output,
                             int batchSize, int seqLen, int numHeads, int headDim,
                             int windowSize, float scale) {
    int totalHeads = batchSize * numHeads;
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < totalHeads; ++h) {
        int b = h / numHeads;
        
        const float* qHead = query + h * seqLen * headDim;
        const float* kHead = key + h * seqLen * headDim;
        const float* vHead = value + h * seqLen * headDim;
        float* outHead = output + h * seqLen * headDim;
        
        for (int i = 0; i < seqLen; ++i) {
            // Only attend to tokens within window
            int windowStart = std::max(0, i - windowSize);
            int windowEnd = std::min(seqLen, i + windowSize + 1);
            int windowLen = windowEnd - windowStart;
            
            // Compute attention scores within window
            std::vector<float> scores(windowLen);
            float maxVal = -std::numeric_limits<float>::infinity();
            
            for (int j = windowStart; j < windowEnd; ++j) {
                float dot = 0.0f;
                for (int d = 0; d < headDim; ++d) {
                    dot += qHead[i * headDim + d] * kHead[j * headDim + d];
                }
                scores[j - windowStart] = dot * scale;
                maxVal = std::max(maxVal, scores[j - windowStart]);
            }
            
            // Softmax
            float sum = 0.0f;
            for (int j = 0; j < windowLen; ++j) {
                scores[j] = std::exp(scores[j] - maxVal);
                sum += scores[j];
            }
            
            float invSum = 1.0f / sum;
            
            // Apply to values
            for (int d = 0; d < headDim; ++d) {
                float outVal = 0.0f;
                for (int j = windowStart; j < windowEnd; ++j) {
                    outVal += scores[j - windowStart] * invSum * vHead[j * headDim + d];
                }
                outHead[i * headDim + d] = outVal;
            }
        }
    }
}

// ============================================================================
// KV Cache Attention (for autoregressive generation)
// ============================================================================

void kv_cache_attention(const float* query,
                       const float* keyCache, const float* valueCache,
                       float* output,
                       int batchSize, int numHeads, int headDim,
                       int cacheLen, int seqLen,
                       float scale) {
    int totalHeads = batchSize * numHeads;
    
    #pragma omp parallel for schedule(static)
    for (int h = 0; h < totalHeads; ++h) {
        const float* qHead = query + h * seqLen * headDim;
        const float* kHead = keyCache + h * cacheLen * headDim;
        const float* vHead = valueCache + h * cacheLen * headDim;
        float* outHead = output + h * seqLen * headDim;
        
        for (int i = 0; i < seqLen; ++i) {
            // Compute attention scores against cache
            std::vector<float> scores(cacheLen);
            float maxVal = -std::numeric_limits<float>::infinity();
            
            for (int j = 0; j < cacheLen; ++j) {
                float dot = 0.0f;
                for (int d = 0; d < headDim; ++d) {
                    dot += qHead[i * headDim + d] * kHead[j * headDim + d];
                }
                scores[j] = dot * scale;
                maxVal = std::max(maxVal, scores[j]);
            }
            
            // Softmax
            float sum = 0.0f;
            for (int j = 0; j < cacheLen; ++j) {
                scores[j] = std::exp(scores[j] - maxVal);
                sum += scores[j];
            }
            
            float invSum = 1.0f / sum;
            
            // Apply to values
            for (int d = 0; d < headDim; ++d) {
                float outVal = 0.0f;
                for (int j = 0; j < cacheLen; ++j) {
                    outVal += scores[j] * invSum * vHead[j * headDim + d];
                }
                outHead[i * headDim + d] = outVal;
            }
        }
    }
}

} // namespace RawrXD::Inference
