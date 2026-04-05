// omega_kernels.hip
// RDNA3 attention kernels for Omega Singularity.
// Build with hipcc and link the resulting object/library into the Titan lane.

#include <hip/hip_runtime.h>
#include <hip/hip_fp16.h>

#include <stdint.h>

namespace {

__device__ __forceinline__ float dot2_accumulate(float acc, half2 qv, half2 kv) {
#if defined(__HIP_PLATFORM_AMD__)
    // On Navi32/RDNA3 this lowers to V_DOT2_F32_F16.
    asm volatile(
        "v_dot2_f32_f16 %0, %1, %2, %0"
        : "+v"(acc)
        : "v"(qv), "v"(kv));
    return acc;
#else
    float2 qf = __half22float2(qv);
    float2 kf = __half22float2(kv);
    return acc + qf.x * kf.x + qf.y * kf.y;
#endif
}

__global__ void omega_attention11x_scores_kernel(
    const half* __restrict__ q,
    const half* __restrict__ k,
    float* __restrict__ scores,
    int seq_len,
    int head_dim) {

    const int q_row = blockIdx.y;    // 0..10
    const int k_row = blockIdx.x;    // 0..seq_len-1
    if (q_row >= 11 || k_row >= seq_len) return;

    float acc = 0.0f;
    const int q_base = q_row * head_dim;
    const int k_base = k_row * head_dim;

    int d = 0;
    for (; d + 1 < head_dim; d += 2) {
        half2 qv = *reinterpret_cast<const half2*>(&q[q_base + d]);
        half2 kv = *reinterpret_cast<const half2*>(&k[k_base + d]);
        acc = dot2_accumulate(acc, qv, kv);
    }

    if (d < head_dim) {
        acc += __half2float(q[q_base + d]) * __half2float(k[k_base + d]);
    }

    scores[q_row * seq_len + k_row] = acc;
}

__global__ void omega_softmax11x_kernel(float* __restrict__ scores, int seq_len) {
    const int row = blockIdx.x;
    if (row >= 11) return;

    float* row_scores = scores + row * seq_len;

    float max_v = -3.402823466e+38F;
    for (int i = threadIdx.x; i < seq_len; i += blockDim.x) {
        max_v = fmaxf(max_v, row_scores[i]);
    }

    __shared__ float smax[256];
    smax[threadIdx.x] = max_v;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            smax[threadIdx.x] = fmaxf(smax[threadIdx.x], smax[threadIdx.x + stride]);
        }
        __syncthreads();
    }

    max_v = smax[0];

    float sum_v = 0.0f;
    for (int i = threadIdx.x; i < seq_len; i += blockDim.x) {
        float e = expf(row_scores[i] - max_v);
        row_scores[i] = e;
        sum_v += e;
    }

    __shared__ float ssum[256];
    ssum[threadIdx.x] = sum_v;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (threadIdx.x < stride) {
            ssum[threadIdx.x] += ssum[threadIdx.x + stride];
        }
        __syncthreads();
    }

    sum_v = fmaxf(ssum[0], 1.0e-12f);
    for (int i = threadIdx.x; i < seq_len; i += blockDim.x) {
        row_scores[i] /= sum_v;
    }
}

__global__ void omega_apply_values11x_kernel(
    const float* __restrict__ probs,
    const float* __restrict__ v,
    float* __restrict__ out,
    int seq_len,
    int head_dim) {

    const int q_row = blockIdx.y;   // 0..10
    const int d = blockIdx.x * blockDim.x + threadIdx.x;
    if (q_row >= 11 || d >= head_dim) return;

    float acc = 0.0f;
    const float* prow = probs + q_row * seq_len;

    for (int t = 0; t < seq_len; ++t) {
        acc += prow[t] * v[t * head_dim + d];
    }

    out[q_row * head_dim + d] = acc;
}

} // namespace

extern "C" int RawrXD_Omega_Attention11x_HIP(
    const void* q_half,
    const void* k_half,
    const float* v_f32,
    float* out_f32,
    int seq_len,
    int head_dim,
    uintptr_t stream_handle) {

    if (!q_half || !k_half || !v_f32 || !out_f32 || seq_len <= 0 || head_dim <= 0) {
        return static_cast<int>(hipErrorInvalidValue);
    }

    hipStream_t stream = reinterpret_cast<hipStream_t>(stream_handle);

    float* d_scores = nullptr;
    const size_t scores_bytes = static_cast<size_t>(11) * static_cast<size_t>(seq_len) * sizeof(float);

#if HIP_VERSION_MAJOR >= 5
    hipError_t st = hipMallocAsync(reinterpret_cast<void**>(&d_scores), scores_bytes, stream);
    if (st != hipSuccess) {
        st = hipMalloc(reinterpret_cast<void**>(&d_scores), scores_bytes);
    }
#else
    hipError_t st = hipMalloc(reinterpret_cast<void**>(&d_scores), scores_bytes);
#endif

    if (st != hipSuccess || !d_scores) {
        return static_cast<int>(st);
    }

    const dim3 score_grid(static_cast<unsigned int>(seq_len), 11u, 1u);
    omega_attention11x_scores_kernel<<<score_grid, 1u, 0u, stream>>>(
        reinterpret_cast<const half*>(q_half),
        reinterpret_cast<const half*>(k_half),
        d_scores,
        seq_len,
        head_dim);

    st = hipGetLastError();
    if (st != hipSuccess) {
        hipFree(d_scores);
        return static_cast<int>(st);
    }

    omega_softmax11x_kernel<<<11u, 256u, 0u, stream>>>(d_scores, seq_len);
    st = hipGetLastError();
    if (st != hipSuccess) {
        hipFree(d_scores);
        return static_cast<int>(st);
    }

    const dim3 out_grid(static_cast<unsigned int>((head_dim + 255) / 256), 11u, 1u);
    omega_apply_values11x_kernel<<<out_grid, 256u, 0u, stream>>>(
        d_scores,
        v_f32,
        out_f32,
        seq_len,
        head_dim);

    st = hipGetLastError();

#if HIP_VERSION_MAJOR >= 5
    hipError_t free_st = hipFreeAsync(d_scores, stream);
    if (free_st != hipSuccess) {
        hipFree(d_scores);
    }
#else
    hipFree(d_scores);
#endif

    return static_cast<int>(st);
}
