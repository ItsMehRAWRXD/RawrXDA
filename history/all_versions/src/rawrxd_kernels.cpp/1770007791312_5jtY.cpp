#include <cmath>
#include <vector>
#include <immintrin.h> // For AVX intrinsics if available, else standard

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

} // extern "C"
