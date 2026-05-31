#pragma once
#include <cstdint>
#include <cstddef>

extern "C" {
    void MatMul_F16_AVX512(float* y, const float* x, const float* w, int n, int d);
    void RMSNorm_AVX512(float* x, float* weight, int size, float eps);
    void SoftMax_AVX512(float* x, int size);
    void KVCache_Update_AVX512(float* cache, const float* src, int pos, int head_dim);
    void KVCache_Retrieve_AVX512(float* out, const float* cache, int pos, int head_dim);
    void Dequantize_AVX512(float* out, const void* in, int n);
    void DequantQ4_0_AVX512(void* src, uint16_t* dst, size_t blocks);
    void DequantQ4_0_AVX2(void* src, uint16_t* dst, size_t blocks);
}
