#include "cuda_kernels.cuh"
#include <cuda_runtime.h>
#include <cmath>

/**
 * Q2_K Dequantization Kernel
 * 
 * Q2_K: 2-bit quantization with block-wise scaling
 * Each block has 32 values quantized to 2 bits
 * Block size: 13 bytes (4 scales + 32 2-bit values + delta + dmin)
 * Output: 32 float32 values per block
 */
__global__ void dequantize_q2k_cuda(
    const BlockQ2K* quantized,
    float* output,
    uint32_t numBlocks
) {
    uint32_t blockIdx_x = blockIdx.x;
    uint32_t threadIdx_x = threadIdx.x;
    
    if (blockIdx_x >= numBlocks) return;
    
    const BlockQ2K& block = quantized[blockIdx_x];
    float* blockOutput = output + blockIdx_x * 32;
    
    // Each thread processes 4 output values
    #pragma unroll
    for (int i = 0; i < 4; ++i) {
        int idx = threadIdx_x * 4 + i;
        if (idx >= 32) break;
        
        // Extract scale for this value
        uint8_t scaleIdx = idx / 2;
        uint8_t scale = (block.scales[scaleIdx] >> ((idx % 2) * 4)) & 0x0F;
        
        // Extract quantized value (2 bits)
        uint8_t byteIdx = idx / 4;
        uint8_t bitOffset = (idx % 4) * 2;
        uint8_t quantized_val = (block.qs[byteIdx] >> bitOffset) & 0x03;
        
        // Dequantize
        float scaled = (quantized_val - 2.0f) * (float)scale * 1.0f / 15.0f;
        blockOutput[idx] = scaled + block.d;
    }
}

/**
 * Q3_K Dequantization Kernel
 * 
 * Q3_K: 3-bit quantization with improved scaling
 * Each block has 32 values quantized to 3 bits
 * Block size: 22 bytes (32 3-bit values + scales + deltas)
 * Output: 32 float32 values per block
 */
__global__ void dequantize_q3k_cuda(
    const BlockQ3K* quantized,
    float* output,
    uint32_t numBlocks
) {
    uint32_t blockIdx_x = blockIdx.x;
    uint32_t threadIdx_x = threadIdx.x;
    
    if (blockIdx_x >= numBlocks) return;
    
    const BlockQ3K& block = quantized[blockIdx_x];
    float* blockOutput = output + blockIdx_x * 32;
    
    // Each thread processes 2 output values
    #pragma unroll
    for (int i = 0; i < 2; ++i) {
        int idx = threadIdx_x * 2 + i;
        if (idx >= 32) break;
        
        // Extract high bit from hmask
        uint8_t byteIdx = idx / 8;
        uint8_t bitOffset = idx % 8;
        uint8_t highBit = (block.hmask[byteIdx] >> bitOffset) & 0x01;
        
        // Extract lower 2 bits from qs
        uint8_t qsIdx = idx / 4;
        uint8_t qsBitOffset = (idx % 4) * 2;
        uint8_t lowBits = (block.qs[qsIdx] >> qsBitOffset) & 0x03;
        
        // Combine to get 3-bit value
        uint8_t quantized_val = (highBit << 2) | lowBits;
        
        // Get scale for this value
        uint8_t scaleIdx = idx / 4;
        int8_t scale = block.scales[scaleIdx];
        
        // Dequantize
        float scaled = (quantized_val - 4.0f) * (float)scale * 1.0f / 31.0f;
        blockOutput[idx] = scaled + block.d;
    }
}

/**
 * Q5_K Dequantization Kernel
 * 
 * Q5_K: 5-bit quantization with higher precision
 * Each block has 32 values quantized to 5 bits
 * Block size: 40 bytes
 * Output: 32 float32 values per block
 */
__global__ void dequantize_q5k_cuda(
    const BlockQ5K* quantized,
    float* output,
    uint32_t numBlocks
) {
    uint32_t blockIdx_x = blockIdx.x;
    uint32_t threadIdx_x = threadIdx.x;
    
    if (blockIdx_x >= numBlocks) return;
    
    const BlockQ5K& block = quantized[blockIdx_x];
    float* blockOutput = output + blockIdx_x * 32;
    
    #pragma unroll
    for (int i = 0; i < 1; ++i) {
        int idx = threadIdx_x;
        if (idx >= 32) return;
        
        // Extract high 1 bit from qh
        uint8_t qhIdx = idx / 8;
        uint8_t qhBit = (block.qh[qhIdx] >> (idx % 8)) & 0x01;
        
        // Extract lower 4 bits from qs
        uint8_t qsIdx = idx / 2;
        uint8_t qsBits = (block.qs[qsIdx] >> ((idx % 2) * 4)) & 0x0F;
        
        // Combine to get 5-bit value
        uint8_t quantized_val = (qhBit << 4) | qsBits;
        
        // Get scale
        uint8_t scaleIdx = idx / 4;
        int8_t scale = block.scales[scaleIdx];
        
        // Dequantize
        float scaled = (quantized_val - 16.0f) * (float)scale * 1.0f / 63.0f;
        blockOutput[idx] = scaled + block.d;
    }
}

/**
 * Matrix Multiplication Kernel (Standard)
 * 
 * Computes C = A * B^T where:
 * - A is M x K
 * - B is N x K (stored transposed)
 * - C is M x N
 * 
 * Uses 32x32 blocks with shared memory
 */
__global__ void matmul_cuda(
    const float* A,
    const float* B,
    float* C,
    uint32_t M,
    uint32_t N,
    uint32_t K
) {
    // Shared memory for block multiplication
    __shared__ float sA[32][32];
    __shared__ float sB[32][32];
    
    uint32_t row = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t col = blockIdx.x * blockDim.x + threadIdx.x;
    
    float sum = 0.0f;
    
    // Iterate over K in chunks
    for (uint32_t k = 0; k < K; k += 32) {
        // Load A into shared memory
        if (row < M && (k + threadIdx.x) < K) {
            sA[threadIdx.y][threadIdx.x] = A[row * K + k + threadIdx.x];
        } else {
            sA[threadIdx.y][threadIdx.x] = 0.0f;
        }
        
        // Load B into shared memory
        if (col < N && (k + threadIdx.y) < K) {
            sB[threadIdx.y][threadIdx.x] = B[col * K + k + threadIdx.y];
        } else {
            sB[threadIdx.y][threadIdx.x] = 0.0f;
        }
        
        __syncthreads();
        
        // Compute partial sum
        #pragma unroll
        for (uint32_t i = 0; i < 32; ++i) {
            sum += sA[threadIdx.y][i] * sB[threadIdx.x][i];
        }
        
        __syncthreads();
    }
    
    // Write result
    if (row < M && col < N) {
        C[row * N + col] = sum;
    }
}

/**
 * Optimized 4x4 Block Matrix Multiplication
 * 
 * Better for smaller matrices and memory efficiency
 */
__global__ void matmul_4x4_cuda(
    const float* A,
    const float* B,
    float* C,
    uint32_t M,
    uint32_t N,
    uint32_t K
) {
    uint32_t row = blockIdx.y * 4 + threadIdx.y;
    uint32_t col = blockIdx.x * 4 + threadIdx.x;
    
    if (row >= M || col >= N) return;
    
    float sum = 0.0f;
    for (uint32_t k = 0; k < K; ++k) {
        sum += A[row * K + k] * B[col * K + k];
    }
    
    C[row * N + col] = sum;
}

/**
 * Vector Addition Kernel: Z = X + Y
 */
__global__ void add_cuda(
    const float* X,
    const float* Y,
    float* Z,
    uint32_t numElements
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < numElements) {
        Z[idx] = X[idx] + Y[idx];
    }
}

/**
 * Element-wise Multiplication Kernel: Z = X * Y
 */
__global__ void elementwise_mul_cuda(
    const float* X,
    const float* Y,
    float* Z,
    uint32_t numElements
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx < numElements) {
        Z[idx] = X[idx] * Y[idx];
    }
}

/**
 * Softmax Kernel
 * 
 * Computes softmax along last dimension
 */
__global__ void softmax_cuda(
    float* X,
    uint32_t rows,
    uint32_t cols
) {
    uint32_t row = blockIdx.x;
    if (row >= rows) return;
    
    float* rowPtr = X + row * cols;
    
    // Find max for numerical stability
    float maxVal = rowPtr[threadIdx.x];
    for (uint32_t i = threadIdx.x + blockDim.x; i < cols; i += blockDim.x) {
        maxVal = fmaxf(maxVal, rowPtr[i]);
    }
    
    // Compute exponentials
    float sum = 0.0f;
    for (uint32_t i = threadIdx.x; i < cols; i += blockDim.x) {
        float val = expf(rowPtr[i] - maxVal);
        rowPtr[i] = val;
        sum += val;
    }
    
    // Normalize
    for (uint32_t i = threadIdx.x; i < cols; i += blockDim.x) {
        rowPtr[i] /= sum;
    }
}

/**
 * Layer Normalization Kernel
 * 
 * Normalizes to mean=0, std=1, then applies weight and bias
 */
__global__ void layer_norm_cuda(
    const float* input,
    float* output,
    const float* weight,
    const float* bias,
    uint32_t numElements,
    float epsilon
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (idx >= numElements) return;
    
    // Compute mean
    float mean = 0.0f;
    for (uint32_t i = 0; i < numElements; ++i) {
        mean += input[i];
    }
    mean /= numElements;
    
    // Compute variance
    float variance = 0.0f;
    for (uint32_t i = 0; i < numElements; ++i) {
        float diff = input[i] - mean;
        variance += diff * diff;
    }
    variance /= numElements;
    
    // Normalize
    float normalized = (input[idx] - mean) / sqrtf(variance + epsilon);
    output[idx] = normalized * weight[idx] + bias[idx];
}

/**
 * Token Sampling Kernel
 * 
 * Samples next token from logits using temperature
 */
__global__ void sample_token_cuda(
    const float* logits,
    uint32_t vocabSize,
    float temperature,
    uint32_t seed,
    uint32_t* sampledToken
) {
    // Apply temperature to logits
    float maxLogit = logits[0];
    for (uint32_t i = 1; i < vocabSize; ++i) {
        maxLogit = fmaxf(maxLogit, logits[i]);
    }
    
    // Compute probabilities
    float sum = 0.0f;
    for (uint32_t i = threadIdx.x; i < vocabSize; i += blockDim.x) {
        float prob = expf((logits[i] - maxLogit) / temperature);
        sum += prob;
    }
    
    // Sample based on probabilities
    float threshold = 0.5f;  // Simple threshold for now
    for (uint32_t i = 0; i < vocabSize; ++i) {
        float prob = expf((logits[i] - maxLogit) / temperature) / sum;
        if (prob > threshold) {
            *sampledToken = i;
            return;
        }
        threshold -= prob;
    }
}
