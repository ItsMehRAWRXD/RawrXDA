/**
 * @file llama_kernel_ops.h
 * @brief Core tensor operations optimized for llama.cpp integration
 * 
 * Provides optimized CPU/GPU tensor operations for transformer inference.
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD::Inference {

// ============================================================================
// CPU Feature Detection
// ============================================================================

struct CPUFeatures {
    bool hasSSE2 = false;
    bool hasAVX = false;
    bool hasAVX2 = false;
    bool hasAVX512F = false;
    bool hasAVX512VL = false;
    bool hasFMA = false;
};

// ============================================================================
// Quantized Matrix Multiplication
// ============================================================================

/**
 * @brief Q4_0 quantized GEMM
 * @param A Quantized weights (M x K, Q4_0 format)
 * @param B Activations (K x N, float32)
 * @param C Output (M x N, float32)
 * @param M Rows of A
 * @param N Cols of B
 * @param K Inner dimension
 * @param lda Leading dimension of A
 * @param ldb Leading dimension of B
 * @param ldc Leading dimension of C
 */
void gemm_q4_0(const void* A, const void* B, float* C,
               int M, int N, int K,
               int lda, int ldb, int ldc);

// ============================================================================
// Float32 Matrix Multiplication
// ============================================================================

/**
 * @brief Float32 GEMM with automatic SIMD dispatch
 */
void gemm_f32(const float* A, const float* B, float* C,
              int M, int N, int K,
              int lda, int ldb, int ldc);

/**
 * @brief AVX-512 float32 GEMM
 */
void gemm_f32_avx512(const float* A, const float* B, float* C,
                     int M, int N, int K,
                     int lda, int ldb, int ldc);

/**
 * @brief AVX2 float32 GEMM
 */
void gemm_f32_avx2(const float* A, const float* B, float* C,
                   int M, int N, int K,
                   int lda, int ldb, int ldc);

/**
 * @brief Scalar float32 GEMM (fallback)
 */
void gemm_f32_scalar(const float* A, const float* B, float* C,
                     int M, int N, int K,
                     int lda, int ldb, int ldc);

// ============================================================================
// Layer Normalization
// ============================================================================

/**
 * @brief Layer normalization
 * @param input Input tensor (rows x cols)
 * @param output Output tensor (rows x cols)
 * @param gamma Scale parameters (cols)
 * @param beta Shift parameters (cols)
 * @param rows Number of rows
 * @param cols Number of columns
 * @param epsilon Small constant for numerical stability
 */
void layer_norm(const float* input, float* output,
                const float* gamma, const float* beta,
                int rows, int cols, float epsilon = 1e-5f);

/**
 * @brief Fused layer normalization with residual connection
 * @param input Input tensor
 * @param residual Residual tensor to add before normalization
 * @param output Output tensor
 * @param gamma Scale parameters
 * @param beta Shift parameters
 * @param rows Number of rows
 * @param cols Number of columns
 * @param epsilon Small constant for numerical stability
 */
void fused_layer_norm_residual(const float* input, const float* residual,
                               float* output,
                               const float* gamma, const float* beta,
                               int rows, int cols, float epsilon = 1e-5f);

// ============================================================================
// RoPE (Rotary Position Embedding)
// ============================================================================

/**
 * @brief Apply rotary position embeddings to Q and K tensors
 * @param q Query tensor (numHeads x seqLen x headDim)
 * @param k Key tensor (numHeads x seqLen x headDim)
 * @param headDim Dimension per head
 * @param numHeads Number of attention heads
 * @param seqLen Sequence length
 * @param posOffset Position offset for this sequence
 * @param theta Base frequency for RoPE (default: 10000.0)
 */
void apply_rope(float* q, float* k,
                int headDim, int numHeads, int seqLen,
                int posOffset, float theta = 10000.0f);

// ============================================================================
// Softmax
// ============================================================================

/**
 * @brief Softmax over rows
 * @param data Input/output data (rows x cols)
 * @param rows Number of rows
 * @param cols Number of columns
 */
void softmax(float* data, int rows, int cols);

/**
 * @brief Masked softmax over rows
 * @param data Input/output data (rows x cols)
 * @param mask Mask tensor (rows x cols), values > 0 are valid
 * @param rows Number of rows
 * @param cols Number of columns
 * @param maskValue Value to use for masked positions
 */
void softmax_masked(float* data, const float* mask,
                    int rows, int cols, float maskValue = -1e9f);

// ============================================================================
// Activation Functions
// ============================================================================

/**
 * @brief SiLU (Swish) activation: x * sigmoid(x)
 */
void silu(float* data, int size);

/**
 * @brief GELU activation
 */
void gelu(float* data, int size);

/**
 * @brief ReLU activation
 */
void relu(float* data, int size);

// ============================================================================
// Element-wise Operations
// ============================================================================

/**
 * @brief Element-wise addition: out = a + b
 */
void add(const float* a, const float* b, float* out, int size);

/**
 * @brief Element-wise multiplication: out = a * b
 */
void mul(const float* a, const float* b, float* out, int size);

/**
 * @brief Scale all elements: data *= scalar
 */
void scale(float* data, float scalar, int size);

// ============================================================================
// Memory Operations
// ============================================================================

/**
 * @brief Copy data from src to dst
 */
void copy(const float* src, float* dst, int size);

/**
 * @brief Zero out memory
 */
void zero(float* data, int size);

} // namespace RawrXD::Inference
