// d:/rawrxd/src/atc_gpu_dispatch.h
// =============================================================================
// ATC Braided-Quantizer GPU Dispatch — Vulkan/CUDA/HIP via ggml backend.
// All ATC dequantization paths route through this module. No CPU fallback.
// =============================================================================
#pragma once
#include <cstdint>

namespace rxd::atc {

// Dispatch braided-quantization decode to the active GPU backend.
// `braid_ptrs` is an array of `n_braids` pointers to packed bit-streams in
// host memory; the implementation copies them to a GPU buffer, runs an
// in-graph dequant kernel, and copies the result back into `output_data`
// (host-side fp32). Asserts (and aborts) if no GPU backend is active.
void dequantize_braids_gpu(
    const uint8_t** braid_ptrs,
    const int*      braid_bits,        // bits per braid
    int             n_braids,
    int             num_elements,
    float           scale,
    float           offset,
    float*          output_data);

// GPU matrix-vector multiply: out[r] = sum_c W[r*cols + c] * x[c], for r in [0,rows).
// W is a row-major fp32 matrix (`rows` x `cols`), x is fp32 length `cols`, out is fp32
// length `rows`. Backed by a ggml_rxd_backend graph submitted to the active GPU backend.
// Aborts if no GPU is active or the dispatch fails.
void matmul_f32_gpu(
    const float* W,
    const float* x,
    int          rows,
    int          cols,
    float*       out);

} // namespace rxd::atc
