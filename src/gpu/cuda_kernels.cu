// ============================================================================
// cuda_kernels.cu — Real CUDA __global__ kernels for inference
// ============================================================================
// Compile with nvcc when RAWR_HAS_CUDA. Linked by cuda_inference_engine.cpp.
// Kernels: MatMul (tiled), Scaled Dot-Product Attention, LayerNorm.
// ============================================================================

#include <cuda_runtime.h>
#include <cstdio>

#define TILE 16

// ----------------------------------------------------------------------------
// Matrix multiply: C = A * B  (A: M×K, B: K×N, C: M×N). Column-major.
// ----------------------------------------------------------------------------
__global__ void kernelMatMul(const float* __restrict__ A, const float* __restrict__ B,
                             float* __restrict__ C, int M, int K, int N) {
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= M || col >= N) return;
    float sum = 0.f;
    for (int i = 0; i < K; i++)
        sum += A[row * K + i] * B[i * N + col];
    C[row * N + col] = sum;
}

// ----------------------------------------------------------------------------
// LayerNorm: output = (input - mean) / sqrt(var + eps) * weight + bias
// ----------------------------------------------------------------------------
__global__ void kernelLayerNorm(const float* __restrict__ input,
                                const float* __restrict__ weight,
                                const float* __restrict__ bias,
                                int size, float epsilon, float* __restrict__ output) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;
    __shared__ float shSum[256];
    __shared__ float shSq[256];
    float v = input[idx];
    shSum[threadIdx.x] = v;
    shSq[threadIdx.x] = v * v;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            shSum[threadIdx.x] += shSum[threadIdx.x + s];
            shSq[threadIdx.x] += shSq[threadIdx.x + s];
        }
        __syncthreads();
    }
    float mean = (blockDim.x < size) ? shSum[0] / (float)blockDim.x : shSum[0] / (float)size;
    float var = (blockDim.x < size) ? (shSq[0] / (float)blockDim.x - mean * mean) : (shSq[0] / (float)size - mean * mean);
    float rstd = 1.f / sqrtf(var + epsilon);
    output[idx] = (v - mean) * rstd * weight[idx] + bias[idx];
}

// Simplified single-block LayerNorm for correctness (full reduction over size)
__global__ void kernelLayerNormFull(const float* __restrict__ input,
                                    const float* __restrict__ weight,
                                    const float* __restrict__ bias,
                                    int size, float epsilon, float* __restrict__ output) {
    __shared__ float mean;
    __shared__ float var;
    float sum = 0.f, sq = 0.f;
    for (int i = threadIdx.x; i < size; i += blockDim.x) {
        float v = input[i];
        sum += v;
        sq += v * v;
    }
    __shared__ float blockSum[256], blockSq[256];
    blockSum[threadIdx.x] = sum;
    blockSq[threadIdx.x] = sq;
    __syncthreads();
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) {
            blockSum[threadIdx.x] += blockSum[threadIdx.x + s];
            blockSq[threadIdx.x] += blockSq[threadIdx.x + s];
        }
        __syncthreads();
    }
    if (threadIdx.x == 0) {
        mean = blockSum[0] / (float)size;
        var = blockSq[0] / (float)size - mean * mean;
    }
    __syncthreads();
    for (int i = threadIdx.x; i < size; i += blockDim.x) {
        float rstd = 1.f / sqrtf(var + epsilon);
        output[i] = (input[i] - mean) * rstd * weight[i] + bias[i];
    }
}

// ----------------------------------------------------------------------------
// Scaled dot-product attention (simplified): one head, output = softmax(Q*K^T/sqrt(d))*V
// Q: seqLen x headDim, K: seqLen x headDim, V: seqLen x headDim, output: seqLen x headDim
// ----------------------------------------------------------------------------
__global__ void kernelAttentionScores(const float* __restrict__ Q, const float* __restrict__ K,
                                      float* __restrict__ scores, int seqLen, int headDim) {
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    int col = blockIdx.y * blockDim.y + threadIdx.y;
    if (row >= seqLen || col >= seqLen) return;
    float scale = 1.f / sqrtf((float)headDim);
    float sum = 0.f;
    for (int d = 0; d < headDim; d++)
        sum += Q[row * headDim + d] * K[col * headDim + d];
    scores[row * seqLen + col] = sum * scale;
}

__global__ void kernelAttentionSoftmax(float* __restrict__ scores, int seqLen) {
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= seqLen) return;
    float* r = scores + row * seqLen;
    float maxV = r[0];
    for (int c = 1; c < seqLen; c++)
        if (r[c] > maxV) maxV = r[c];
    float sum = 0.f;
    for (int c = 0; c < seqLen; c++) {
        r[c] = expf(r[c] - maxV);
        sum += r[c];
    }
    for (int c = 0; c < seqLen; c++)
        r[c] /= sum;
}

__global__ void kernelAttentionOut(const float* __restrict__ scores, const float* __restrict__ V,
                                   float* __restrict__ output, int seqLen, int headDim) {
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    int d = threadIdx.y;
    if (row >= seqLen || d >= headDim) return;
    float sum = 0.f;
    for (int c = 0; c < seqLen; c++)
        sum += scores[row * seqLen + c] * V[c * headDim + d];
    output[row * headDim + d] = sum;
}

// ----------------------------------------------------------------------------
// C API for cuda_inference_engine.cpp (extern "C" when included from C++)
// ----------------------------------------------------------------------------
extern "C" {

int cudaLaunchMatMul(const float* d_A, int M, int K, const float* d_B, int N, float* d_C) {
    dim3 block(TILE, TILE);
    dim3 grid((N + TILE - 1) / TILE, (M + TILE - 1) / TILE);
    kernelMatMul<<<grid, block>>>(d_A, d_B, d_C, M, K, N);
    cudaError_t e = cudaDeviceSynchronize();
    return (e == cudaSuccess) ? 0 : (int)e;
}

int cudaLaunchLayerNorm(const float* d_input, const float* d_weight, const float* d_bias,
                        int size, float epsilon, float* d_output) {
    int blockSize = (size < 256) ? size : 256;
    kernelLayerNormFull<<<1, blockSize>>>(d_input, d_weight, d_bias, size, epsilon, d_output);
    cudaError_t e = cudaDeviceSynchronize();
    return (e == cudaSuccess) ? 0 : (int)e;
}

int cudaLaunchAttention(const float* d_query, const float* d_key, const float* d_value,
                        int seqLen, int headDim, float* d_output, float* d_workspace) {
    // d_workspace must be seqLen*seqLen floats for scores
    kernelAttentionScores<<<dim3((seqLen+15)/16, (seqLen+15)/16), dim3(16,16)>>>(
        d_query, d_key, d_workspace, seqLen, headDim);
    cudaError_t e = cudaDeviceSynchronize();
    if (e != cudaSuccess) return (int)e;
    kernelAttentionSoftmax<<<(seqLen+255)/256, 256>>>(d_workspace, seqLen);
    e = cudaDeviceSynchronize();
    if (e != cudaSuccess) return (int)e;
    kernelAttentionOut<<<dim3((seqLen+15)/16), dim3(16, headDim)>>>(
        d_workspace, d_value, d_output, seqLen, headDim);
    e = cudaDeviceSynchronize();
    return (e == cudaSuccess) ? 0 : (int)e;
}

} // extern "C"
