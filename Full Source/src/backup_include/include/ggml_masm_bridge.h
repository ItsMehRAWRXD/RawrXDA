// ggml_masm_bridge.h
// C-callable interface for MASM GGML tensor ops
// Exposes MASM routines for C++ integration

#ifndef GGML_MASM_BRIDGE_H
#define GGML_MASM_BRIDGE_H

#include <cstdint>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========================================
// GGML MASM Quantization Functions
// ========================================

// Quantization type enum (must match GGML types)
enum GgmlQuantType {
    GGML_TYPE_F32     = 0,
    GGML_TYPE_F16     = 1,
    GGML_TYPE_Q4_0    = 2,
    GGML_TYPE_Q4_1    = 3,
    GGML_TYPE_Q5_0    = 6,
    GGML_TYPE_Q5_1    = 7,
    GGML_TYPE_Q8_0    = 8,
    GGML_TYPE_Q8_1    = 9,
    GGML_TYPE_Q2_K    = 10,
    GGML_TYPE_Q3_K    = 11,
    GGML_TYPE_Q4_K    = 12,
    GGML_TYPE_Q5_K    = 13,
    GGML_TYPE_Q6_K    = 14,
    GGML_TYPE_Q8_K    = 15,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS  = 17,
    GGML_TYPE_IQ3_XXS = 18,
    GGML_TYPE_IQ1_S   = 19,
    GGML_TYPE_IQ4_NL  = 20,
    GGML_TYPE_IQ3_S   = 21,
    GGML_TYPE_IQ2_S   = 22,
    GGML_TYPE_IQ4_XS  = 23,
    GGML_TYPE_I8      = 24,
    GGML_TYPE_I16     = 25,
    GGML_TYPE_I32     = 26,
    GGML_TYPE_I64     = 27,
    GGML_TYPE_F64     = 28,
    GGML_TYPE_IQ1_M   = 29,
    GGML_TYPE_BF16    = 30
};

// Quantize float32 to Q4_0
void quantize_q4_0(const float* src, void* dst, int64_t n);
void quantize_q4_1(const float* src, void* dst, int64_t n);
void quantize_q5_0(const float* src, void* dst, int64_t n);
void quantize_q5_1(const float* src, void* dst, int64_t n);
void quantize_q8_0(const float* src, void* dst, int64_t n);
void quantize_q8_1(const float* src, void* dst, int64_t n);

// Dequantize to float32
void dequantize_q4_0(const void* src, float* dst, int64_t n);
void dequantize_q4_1(const void* src, float* dst, int64_t n);
void dequantize_q5_0(const void* src, float* dst, int64_t n);
void dequantize_q5_1(const void* src, float* dst, int64_t n);
void dequantize_q8_0(const void* src, float* dst, int64_t n);
void dequantize_q8_1(const void* src, float* dst, int64_t n);

// ========================================
// GGML MASM Tensor Operations
// ========================================

// Matrix multiplication: C = A * B
void ggml_masm_mul_mat(const float* A, const float* B, float* C, 
                       int64_t M, int64_t N, int64_t K);

// Matrix multiplication with quantized weights
void ggml_masm_mul_mat_q4_0(const float* A, const void* B_q4, float* C,
                             int64_t M, int64_t N, int64_t K);
void ggml_masm_mul_mat_q8_0(const float* A, const void* B_q8, float* C,
                             int64_t M, int64_t N, int64_t K);

// Element-wise operations
void ggml_masm_add(const float* a, const float* b, float* result, int64_t n);
void ggml_masm_sub(const float* a, const float* b, float* result, int64_t n);
void ggml_masm_mul(const float* a, const float* b, float* result, int64_t n);
void ggml_masm_div(const float* a, const float* b, float* result, int64_t n);

// Activation functions
void ggml_masm_relu(const float* src, float* dst, int64_t n);
void ggml_masm_gelu(const float* src, float* dst, int64_t n);
void ggml_masm_silu(const float* src, float* dst, int64_t n);
void ggml_masm_tanh(const float* src, float* dst, int64_t n);

// Normalization
void ggml_masm_norm(const float* src, float* dst, int64_t n);
void ggml_masm_rms_norm(const float* src, float* dst, int64_t n, float eps);
void ggml_masm_group_norm(const float* src, float* dst, int64_t n, int64_t groups);

// Rotary Position Embedding (RoPE)
void ggml_masm_rope_f32(float* dst, const float* src, int64_t n_dims, 
                        int64_t mode, int64_t n_ctx, float freq_base, 
                        float freq_scale, int64_t n_orig_ctx);

// Attention
void ggml_masm_flash_attn_f32(float* q, float* k, float* v, float* dst,
                              int64_t n_tokens, int64_t n_heads, 
                              int64_t d_head, bool masked);

// Softmax
void ggml_masm_soft_max(const float* src, float* dst, int64_t n);
void ggml_masm_soft_max_rows(const float* src, float* dst, 
                             int64_t rows, int64_t cols);

// Copy operations
void ggml_masm_cpy_f32(const float* src, float* dst, int64_t n);
void ggml_masm_dup(const float* src, float* dst, int64_t n);

// ========================================
// GGML MASM BLAS Replacement Functions
// ========================================

// BLAS Level 1: Vector operations
float ggml_masm_vec_dot_f32(const float* x, const float* y, int64_t n);
void ggml_masm_vec_scale_f32(float* x, float scale, int64_t n);
void ggml_masm_vec_add_f32(const float* x, const float* y, float* result, int64_t n);

// BLAS Level 2: Matrix-vector operations
void ggml_masm_gemv_f32(const float* A, const float* x, float* y,
                        int64_t M, int64_t N, float alpha, float beta);

// BLAS Level 3: Matrix-matrix operations (GEMM)
void ggml_masm_gemm_f32(const float* A, const float* B, float* C,
                        int64_t M, int64_t N, int64_t K,
                        float alpha, float beta);

// ========================================
// GGML MASM Threading Functions
// ========================================

// Initialize threading system
int64_t ggml_masm_threading_init(int32_t num_threads);
void ggml_masm_threading_shutdown();
int32_t ggml_masm_get_optimal_threads();

// Parallel matrix multiplication
void ggml_masm_mul_mat_parallel(const float* A, const float* B, float* C,
                                int64_t M, int64_t N, int64_t K,
                                int32_t num_threads);

// ========================================
// GGML MASM Utility Functions
// ========================================

int32_t ggml_masm_get_type_block_size(int32_t type);
int32_t ggml_masm_get_type_size(int32_t type);
bool ggml_masm_is_quantized(int32_t type);
const char* ggml_masm_get_type_name(int32_t type);

// ========================================
// Legacy interface (for backwards compatibility)
// ========================================

enum GGML_OP {
    GGML_OP_MATMUL = 0,
    GGML_OP_ADD = 1,
    GGML_OP_MUL = 2,
    GGML_OP_QUANTIZE_Q8_0 = 3,
    GGML_OP_QUANTIZE_Q2_K = 4,
    GGML_OP_DEQUANTIZE_Q8_0 = 5,
    GGML_OP_DEQUANTIZE_Q2_K = 6
};

int GGML_BackendDispatch(int op, void* A, void* B, void* C, int sizeA, int sizeB, int sizeC);
int QuantizeQ8_0(void* tensor, int size);
int QuantizeQ2_K(void* tensor, int size);
int DequantizeQ8_0(void* tensor, int size);
int DequantizeQ2_K(void* tensor, int size);

// KV cache routines
int KVCacheInit(int maxTokens);
int KVCacheAddTokens(void* tokens, int count);
int KVCacheEvict(int count);
int KVCacheGetTokens(void* outTokens, int maxCount);

// Compression routines
int AdaptiveQuantization(void* model, int layers);
int SVDCompress(void* tensor, int size);
int SparsePrune(void* tensor, int size);
int IntegerMatMul(void* A, void* B, void* C, int sizeA, int sizeB, int sizeC);

#ifdef __cplusplus
}
#endif

#endif // GGML_MASM_BRIDGE_H
