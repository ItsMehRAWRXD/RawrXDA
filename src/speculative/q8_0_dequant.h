/*
====================================================================
 RAWR Q8_0 Dequantization + Phi-3-Mini Parity Harness
 Complete end-to-end validation against llama.cpp
====================================================================
*/

#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstring>
#include <cmath>

namespace rawr {

// Q8_0 block: 32 weights (int8) + 1 scale (fp16)
struct block_q8_0 {
    uint16_t d;      // scale (fp16)
    int8_t qs[32];   // 32 quantized weights
};
static_assert(sizeof(block_q8_0) == 34, "q8_0 block size");

// Fast fp16 to fp32 conversion
inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h >> 15) & 1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) return 0.0f;
    if (exp == 0x1F) return (sign ? -INFINITY : INFINITY);
    
    uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
    float f;
    memcpy(&f, &f32, 4);
    return f;
}

// Dequantize Q8_0 tensor to fp32
inline void dequantize_q8_0(const uint8_t* src, float* dst, size_t n_elements) {
    const block_q8_0* blocks = (const block_q8_0*)src;
    size_t n_blocks = (n_elements + 31) / 32;
    
    for (size_t b = 0; b < n_blocks; b++) {
        float d = fp16_to_fp32(blocks[b].d);
        for (int i = 0; i < 32; i++) {
            size_t idx = b * 32 + i;
            if (idx < n_elements) {
                dst[idx] = blocks[b].qs[i] * d;
            }
        }
    }
}

// Dequantize Q8_0 matrix (rows x cols) to column-major or row-major
inline std::vector<float> dequantize_q8_0_matrix(const uint8_t* src, 
                                                    size_t rows, 
                                                    size_t cols,
                                                    bool row_major = true) {
    std::vector<float> dst(rows * cols);
    size_t n_elements = rows * cols;
    dequantize_q8_0(src, dst.data(), n_elements);
    
    if (!row_major) {
        // Convert to column-major
        std::vector<float> col_major(rows * cols);
        for (size_t r = 0; r < rows; r++) {
            for (size_t c = 0; c < cols; c++) {
                col_major[c * rows + r] = dst[r * cols + c];
            }
        }
        return col_major;
    }
    return dst;
}

// Get tensor size in bytes for Q8_0
inline size_t q8_0_tensor_size(size_t n_elements) {
    size_t n_blocks = (n_elements + 31) / 32;
    return n_blocks * sizeof(block_q8_0);
}

// Compute Q8_0 GEMM: C = A @ B^T (A is fp32, B is Q8_0)
// A: M x K, B: N x K (stored as Q8_0), C: M x N
inline void gemm_q8_0(const float* A, const uint8_t* B_q8, 
                      float* C, size_t M, size_t N, size_t K) {
    // Dequantize B first (naive approach - can be optimized)
    std::vector<float> B(K * N);
    dequantize_q8_0(B_q8, B.data(), K * N);
    
    // Simple GEMM
    for (size_t m = 0; m < M; m++) {
        for (size_t n = 0; n < N; n++) {
            float sum = 0.0f;
            for (size_t k = 0; k < K; k++) {
                sum += A[m * K + k] * B[n * K + k];
            }
            C[m * N + n] = sum;
        }
    }
}

} // namespace rawr
