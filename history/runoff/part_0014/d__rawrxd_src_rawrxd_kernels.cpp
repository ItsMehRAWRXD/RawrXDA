#include "rawrxd_kernels.h"
#include <cmath>
#include <immintrin.h> // For AVX intrinsics if available, else standard
#include <cstdint>

// C++ Implementation of RawrXD Math Kernels
// Used when MASM is not available (e.g. MinGW)

extern "C" {

void MatMul_F16_AVX512(float* y, const float* x, const float* w, int n, int d) {
    // Naive implementation for compatibility
    // y[d] = x[n] * w[d*n]
    for (int i = 0; i < d; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < n; ++j) {
            sum += x[j] * w[i * n + j];
        }
        y[i] = sum;
    }
}

void RMSNorm_AVX512(float* x, float* weight, int size, float eps) {
    float ss = 0.0f;
    for (int i = 0; i < size; ++i) {
        ss += x[i] * x[i];
    }
    ss /= size;
    ss += eps;
    float inv_ss = 1.0f / sqrt(ss);
    
    for (int i = 0; i < size; ++i) {
        x[i] = x[i] * inv_ss * weight[i];
    }
}

void SoftMax_AVX512(float* x, int size) {
    float max_val = -1e9f;
    for (int i = 0; i < size; ++i) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        x[i] = exp(x[i] - max_val);
        sum += x[i];
    }
    
    float inv_sum = 1.0f / sum;
    for (int i = 0; i < size; ++i) {
        x[i] *= inv_sum;
    }
}

void KVCache_Update_AVX512(float* cache, const float* src, int pos, int head_dim) {
    // cache[pos * head_dim] ... = src
    float* dest = cache + (pos * head_dim);
    for (int i = 0; i < head_dim; ++i) {
        dest[i] = src[i];
    }
}

void KVCache_Retrieve_AVX512(float* out, const float* cache, int pos, int head_dim) {
    const float* src = cache + (pos * head_dim);
    for (int i = 0; i < head_dim; ++i) {
        out[i] = src[i];
    }
}

void Dequantize_AVX512(float* out, const void* in, int n) {
    // Stub for dequantization - assuming input is actually float for this stub
    // In real world, 'in' is block_q4_0 etc.
    // For this "Real" logic on MinGW without full ggml, we assume F32 weights for simplicity
    // or zero it out to prevent crash.
    const float* f_in = (const float*)in;
    for(int i=0; i<n; ++i) out[i] = f_in[i];
}

struct Q4_0_Block {
    uint16_t d;
    uint8_t qs[16]; 
};

extern "C" void DequantQ4_0_AVX512(void* src, uint16_t* dst, size_t blocks) {
    Q4_0_Block* blk = (Q4_0_Block*)src;
    for (size_t b = 0; b < blocks; b++) {
        float d;
        { uint16_t h = blk[b].d; uint32_t s = (h & 0x8000u) << 16; uint32_t e = (h >> 10) & 0x1F;
          uint32_t m = h & 0x3FF; if (e == 0) { d = 0.0f; } else { uint32_t f = (s | ((e + 112) << 23) | (m << 13));
          memcpy(&d, &f, 4); } }
        for (int j = 0; j < 32; j++) {
            int nibble = (blk[b].qs[j / 2] >> ((j % 2) * 4)) & 0xF;
            float val = (float)(nibble - 8) * d;
            // Float to F16
            uint32_t bits; memcpy(&bits, &val, 4);
            uint16_t sign = (bits >> 16) & 0x8000;
            int32_t exp = ((bits >> 23) & 0xFF) - 127 + 15;
            uint16_t frac = (bits >> 13) & 0x3FF;
            if (exp <= 0) dst[b*32+j] = sign;
            else if (exp >= 31) dst[b*32+j] = sign | 0x7C00;
            else dst[b*32+j] = sign | (exp << 10) | frac;
        }
    }
}

extern "C" void DequantQ4_0_AVX2(void* src, uint16_t* dst, size_t blocks) {
    DequantQ4_0_AVX512(src, dst, blocks); // Same logic, AVX2 path identical for scalar fallback
}

} // extern "C"
