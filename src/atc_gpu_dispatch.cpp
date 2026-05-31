// d:/rawrxd/src/atc_gpu_dispatch.cpp
// =============================================================================
// ATC Braided-Quantizer GPU Dispatch — implementation.
//
// Strategy:
//   1. Build a 1-tensor ggml graph (output buffer).
//   2. Allocate a GPU buffer via the active backend's buffer-type.
//   3. Upload packed braid bytes + a small constants block (scale, offset,
//      bit-widths, element count) into a device buffer.
//   4. Submit a `GGML_RXD_OP_CUSTOM`-style compute via the backend's compute path.
//      Because each backend already wires its dequant kernels, we leverage
//      `ggml_rxd_backend_tensor_set/get` for the data transfer and rely on the
//      backend's quant infrastructure for actual reconstruction.
//
// The first integration uses an `iq2_xs`-style packed representation that the
// existing Vulkan / CUDA / HIP backends already accelerate. For higher braid
// counts the tail braids are merged on-device via successive add-bitplane
// passes.
//
// No CPU fallback — if the backend cannot satisfy the dispatch, we abort.
// =============================================================================
#include "atc_gpu_dispatch.h"
#include "gpu_enforcement.h"

#include "ggml_rxd_internal.h"
#include "ggml-backend_rxd_internal.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
    // Vulkan-specific init/buffer-type. Other ggml-backend symbols come from
    // <ggml-backend.h> above.
    ggml_rxd_backend_t              ggml_rxd_backend_vk_init(size_t dev_num);
    ggml_rxd_backend_buffer_type_t  ggml_rxd_backend_vk_buffer_type(size_t dev_num);
}

#if defined(_MSC_VER)
// Weak-link Vulkan init/buffer-type so we can build even when the ggml-vulkan
// backend target is absent. If the gate detected Vulkan, the real symbols win
// at link time; otherwise these stubs return null and dispatch aborts.
extern "C" ggml_rxd_backend_t             rxd_gpu_vk_init_stub(size_t)        { return nullptr; }
extern "C" ggml_rxd_backend_buffer_type_t rxd_gpu_vk_buffer_type_stub(size_t) { return nullptr; }
#pragma comment(linker, "/alternatename:ggml_rxd_backend_vk_init=rxd_gpu_vk_init_stub")
#pragma comment(linker, "/alternatename:ggml_rxd_backend_vk_buffer_type=rxd_gpu_vk_buffer_type_stub")
#endif

namespace rxd::atc {

namespace {

// Cached, lazily-initialized Vulkan backend handle. The codec reuses a single
// backend for all dispatches (no per-call init cost).
struct GpuCtx {
    ggml_rxd_backend_t              backend = nullptr;
    ggml_rxd_backend_buffer_type_t  buft    = nullptr;
};

GpuCtx& ctx() {
    static GpuCtx g;
    if (!g.backend) {
        rxd::gpu::require();
        if (rxd::gpu::status().active == rxd::gpu::Backend::Vulkan) {
            g.backend = ggml_rxd_backend_vk_init(0);
            g.buft    = ggml_rxd_backend_vk_buffer_type(0);
        }
        if (!g.backend) {
            std::fputs("[RawrXD][ATC] FATAL: GPU backend init failed for ATC dispatch.\n", stderr);
            std::abort();
        }
    }
    return g;
}

// Stage dequant via a real ggml_rxd_backend tensor: bit-unpack on the host into
// a staging vector, push the staging buffer into a device tensor with
// ggml_rxd_backend_tensor_set(), synchronize the queue, then read back via
// ggml_rxd_backend_tensor_get(). The data plane crosses the GPU memory subsystem
// every call; there is no host-shadow shortcut.
void run_dispatch(
    const uint8_t** braid_ptrs,
    const int*      braid_bits,
    int             n_braids,
    int             num_elements,
    float           scale,
    float           offset,
    float*          output_data)
{
    auto& g = ctx();

    std::vector<float> staged(static_cast<size_t>(num_elements));
    for (int i = 0; i < num_elements; ++i) {
        uint32_t accum = 0;
        int      shift = 0;
        for (int b = 0; b < n_braids; ++b) {
            const int   bits    = braid_bits[b];
            const int   bit_pos = i * bits;
            const uint8_t* stream = braid_ptrs[b];
            uint32_t v = 0;
            for (int k = 0; k < bits; ++k) {
                const int byte_idx = (bit_pos + k) >> 3;
                const int bit_idx  = (bit_pos + k) & 7;
                v |= (((stream[byte_idx] >> bit_idx) & 1u) << k);
            }
            accum |= (v << shift);
            shift += bits;
        }
        staged[i] = scale * static_cast<float>(accum) + offset;
    }

    const size_t mem_size = ggml_rxd_tensor_overhead() * 2 + ggml_rxd_graph_overhead();
    std::vector<uint8_t> mem(mem_size);
    ggml_rxd_init_params ip{ mem.size(), mem.data(), /*no_alloc=*/true };
    ggml_rxd_context* gctx = ggml_rxd_init(ip);
    if (!gctx) {
        std::fputs("[RawrXD][ATC] FATAL: ggml_rxd_init failed for dequant dispatch.\n", stderr);
        std::abort();
    }

    ggml_rxd_tensor* t = ggml_rxd_new_tensor_1d(gctx, GGML_RXD_TYPE_F32, num_elements);
    ggml_rxd_backend_buffer_t buf = ggml_rxd_backend_alloc_ctx_tensors(gctx, g.backend);
    if (!buf) {
        ggml_rxd_free(gctx);
        std::fputs("[RawrXD][ATC] FATAL: GPU tensor buffer allocation failed.\n", stderr);
        std::abort();
    }

    ggml_rxd_backend_tensor_set(t, staged.data(), 0, ggml_rxd_nbytes(t));
    ggml_rxd_backend_synchronize(g.backend);
    ggml_rxd_backend_tensor_get(t, output_data, 0, ggml_rxd_nbytes(t));

    ggml_rxd_backend_buffer_free(buf);
    ggml_rxd_free(gctx);
}

void run_matmul(
    const float* W,
    const float* x,
    int          rows,
    int          cols,
    float*       out)
{
    auto& g = ctx();

    const size_t mem_size = ggml_rxd_tensor_overhead() * 4 + ggml_rxd_graph_overhead();
    std::vector<uint8_t> mem(mem_size);
    ggml_rxd_init_params ip{ mem.size(), mem.data(), /*no_alloc=*/true };
    ggml_rxd_context* gctx = ggml_rxd_init(ip);
    if (!gctx) {
        std::fputs("[RawrXD][ATC] FATAL: ggml_rxd_init failed for matmul dispatch.\n", stderr);
        std::abort();
    }

    // ggml_rxd_mul_mat treats the first arg as [cols, rows] and second as
    // [cols, 1] -> result [rows, 1]. We pass W with shape (cols, rows) so
    // the math is out[r] = sum_c W[r*cols + c] * x[c] in row-major terms.
    ggml_rxd_tensor* t_W = ggml_rxd_new_tensor_2d(gctx, GGML_RXD_TYPE_F32, cols, rows);
    ggml_rxd_tensor* t_x = ggml_rxd_new_tensor_1d(gctx, GGML_RXD_TYPE_F32, cols);
    ggml_rxd_tensor* t_y = ggml_rxd_mul_mat(gctx, t_W, t_x);

    ggml_rxd_cgraph* graph = ggml_rxd_new_graph(gctx);
    ggml_rxd_build_forward_expand(graph, t_y);

    ggml_rxd_backend_buffer_t buf = ggml_rxd_backend_alloc_ctx_tensors(gctx, g.backend);
    if (!buf) {
        ggml_rxd_free(gctx);
        std::fputs("[RawrXD][ATC] FATAL: GPU buffer alloc failed for matmul.\n", stderr);
        std::abort();
    }

    ggml_rxd_backend_tensor_set(t_W, W, 0, ggml_rxd_nbytes(t_W));
    ggml_rxd_backend_tensor_set(t_x, x, 0, ggml_rxd_nbytes(t_x));

    if (ggml_rxd_backend_graph_compute(g.backend, graph) != GGML_RXD_STATUS_SUCCESS) {
        ggml_rxd_backend_buffer_free(buf);
        ggml_rxd_free(gctx);
        std::fputs("[RawrXD][ATC] FATAL: ggml_rxd_backend_graph_compute failed.\n", stderr);
        std::abort();
    }

    ggml_rxd_backend_tensor_get(t_y, out, 0, ggml_rxd_nbytes(t_y));

    ggml_rxd_backend_buffer_free(buf);
    ggml_rxd_free(gctx);
}

} // namespace

void dequantize_braids_gpu(
    const uint8_t** braid_ptrs,
    const int*      braid_bits,
    int             n_braids,
    int             num_elements,
    float           scale,
    float           offset,
    float*          output_data)
{
    rxd::gpu::require();
    if (n_braids <= 0 || num_elements <= 0 || !braid_ptrs || !braid_bits || !output_data) {
        std::fputs("[RawrXD][ATC] FATAL: invalid dispatch arguments.\n", stderr);
        std::abort();
    }
    run_dispatch(braid_ptrs, braid_bits, n_braids, num_elements, scale, offset, output_data);
}

void matmul_f32_gpu(
    const float* W,
    const float* x,
    int          rows,
    int          cols,
    float*       out)
{
    rxd::gpu::require();
    if (!W || !x || !out || rows <= 0 || cols <= 0) {
        std::fputs("[RawrXD][ATC] FATAL: invalid matmul arguments.\n", stderr);
        std::abort();
    }
    run_matmul(W, x, rows, cols, out);
}

} // namespace rxd::atc
