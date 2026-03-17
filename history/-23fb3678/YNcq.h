#pragma once
#include <cstddef>

// Pure scalar transformer primitives - every op is visible
extern "C" {
    // Hand-written assembly kernel (4x4 integer GEMM)
    void gemm_4x4(const int* A, const int* B, int* C);
    
    // Scalar transformer functions (no SIMD, no intrinsics)
    void transformer_attention_scalar(const float* Q, const float* K, const float* V,
                                     float* out, size_t seq_len, size_t head_dim);
    
    void transformer_ffn_scalar(const float* x, const float* W1, const float* W2,
                               const float* b1, const float* b2,
                               float* out, size_t seq_len, size_t d_model, size_t d_ff);
    
    void transformer_layer_norm(float* x, size_t n);
    void transformer_rms_norm(float* x, size_t n);
}
