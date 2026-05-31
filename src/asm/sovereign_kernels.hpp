#pragma once
#include <cstdint>

// ============================================================================
// Sovereign Kernel Bridge — Pure x64 MASM (ml64.exe) ABI
// Zero CRT. AVX2 + F16C.
// ============================================================================

extern "C" {

    // Dequantize Q4_0 blocks to f32 row using AVX2 + F16C
    // src  = const void* (q4_0 blocks, 18 bytes per 32 weights)
    // dst  = float* (output f32 row, must be n elements)
    // n    = int64_t (must be multiple of 32)
    void __stdcall Sovereign_DequantizeRow_Q4_0_AVX2(const void* src, float* dst, int64_t n);

    // In-place RMSNorm on f32 row using AVX2
    // x      = float* (in-place, must be n elements)
    // n      = int64_t (must be multiple of 8)
    // weight = const float* (per-element scale, must be n elements)
    // eps    = float (small constant to prevent division by zero)
    void __stdcall Sovereign_RMSNorm_F32_AVX2(float* x, int64_t n, const float* weight, float eps);

    // Non-temporal copy using vmovntdq + sfence
    // dst   = void* (destination, 32-byte aligned recommended)
    // src   = const void* (source)
    // bytes = int64_t (must be multiple of 32)
    void __stdcall Sovereign_CopyBuffer_NT_AVX2(void* dst, const void* src, int64_t bytes);

    // Matrix-vector multiply: y = A * x
    // A       = const float* (row-major matrix, n_rows x n_cols)
    // x       = const float* (vector, n_cols elements)
    // y       = float* (output vector, n_rows elements)
    // n_rows  = int64_t
    // n_cols  = int64_t (must be multiple of 8)
    void __stdcall Sovereign_MatVec_F32_AVX2(const float* A, const float* x, float* y, int64_t n_rows, int64_t n_cols);

} // extern "C"
