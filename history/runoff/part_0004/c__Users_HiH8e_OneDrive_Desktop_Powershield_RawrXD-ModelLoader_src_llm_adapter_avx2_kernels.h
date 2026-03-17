#pragma once
// AVX2 SIMD kernels for Q4_0 quantization and RoPE
// Only compiled when GGUF_ENABLE_AVX2=ON

#include <cstdint>

#ifdef GGUF_ENABLE_AVX2
#include <immintrin.h>

namespace RawrXD {

// Q4_0 block format (32 weights per block)
struct BlockQ4_0 {
    uint16_t d;      // FP16 scale factor
    uint8_t qs[16];  // 32 x 4-bit weights (2 per byte)
};

/**
 * AVX2-optimized Q4_0 matrix multiplication (GEMM)
 * C = A * B where A is Q4_0 quantized, B and C are FP32
 * 
 * @param A: Q4_0 quantized matrix [n x k] (in blocks)
 * @param B: FP32 matrix [k x m]
 * @param C: FP32 output matrix [n x m]
 * @param n: Rows of A
 * @param k: Cols of A / Rows of B
 * @param m: Cols of B
 */
void gemm_q4_0_avx2(const BlockQ4_0* A, const float* B, float* C,
                    int n, int k, int m);

/**
 * AVX2-optimized RoPE rotation (processes 8 floats at once)
 * 
 * @param x: Input tensor to rotate [head_dim]
 * @param head_dim: Dimension per head (must be multiple of 8)
 * @param pos: Position index
 * @param inv_freq: Precomputed inverse frequencies [head_dim/2]
 */
void rope_rotate_8(float* x, int head_dim, int pos, const float* inv_freq);

/**
 * Check CPU for AVX2 support at runtime
 */
bool cpu_has_avx2();

/**
 * Convert FP16 to FP32 (for Q4_0 scale factors)
 */
inline float half_to_float(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    // Handle special cases
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        // Denormal
        exp = 1;
        while (!(mant & 0x400)) { mant <<= 1; exp--; }
        mant &= 0x3FF;
    } else if (exp == 31) {
        // Inf or NaN
        return sign ? -INFINITY : INFINITY;
    }
    
    // Convert to FP32
    uint32_t f32_exp = exp - 15 + 127;
    uint32_t f32_mant = mant << 13;
    uint32_t bits = (sign << 31) | (f32_exp << 23) | f32_mant;
    
    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

} // namespace RawrXD

#endif // GGUF_ENABLE_AVX2
