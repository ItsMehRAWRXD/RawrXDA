// GGML compatibility stub - implements the ggml API using our custom backend
// This allows us to remove the external ggml dependency while maintaining API compatibility

#include "ggml_stub.hpp"
#include "gpu_backend.h"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

extern "C" {

// Minimal backend types for compatibility
struct ggml_backend {};
typedef ggml_backend* ggml_backend_t;

enum ggml_status {
    GGML_STATUS_SUCCESS = 0,
    GGML_STATUS_ABORTED = 1,
};

// Context management
ggml_context* ggml_init(size_t mem_size) {
    auto* ctx = new ggml_context;
    ctx->buffer.resize(mem_size);
    ctx->offset = 0;
    return ctx;
}

void ggml_free(ggml_context* ctx) {
    delete ctx;
}

// Tensor creation
ggml_tensor* ggml_new_tensor(ggml_context* ctx, int type, int n_dims, const int64_t* ne) {
    std::lock_guard<std::mutex> lock(ctx->mutex);
    
    auto* tensor = new ggml_tensor;
    tensor->type = type;
    tensor->n_dims = n_dims;
    tensor->backend = GGML_BACKEND_CPU;
    
    size_t size = 1;
    for (int i = 0; i < n_dims; ++i) {
        tensor->ne[i] = ne[i];
        size *= ne[i];
    }
    
    // Calculate strides
    tensor->nb[0] = ggml_type_size(type);
    for (int i = 1; i < n_dims; ++i) {
        tensor->nb[i] = tensor->nb[i-1] * tensor->ne[i-1];
    }
    
    tensor->size = size * ggml_type_size(type);
    
    // Allocate from context buffer
    if (ctx->offset + tensor->size > ctx->buffer.size()) {
        return nullptr; // Out of memory
    }
    
    tensor->data = ctx->buffer.data() + ctx->offset;
    ctx->offset += tensor->size;
    
    return tensor;
}

ggml_tensor* ggml_new_tensor_1d(ggml_context* ctx, int type, int64_t ne0) {
    return ggml_new_tensor(ctx, type, 1, &ne0);
}

ggml_tensor* ggml_new_tensor_2d(ggml_context* ctx, int type, int64_t ne0, int64_t ne1) {
    int64_t ne[2] = {ne0, ne1};
    return ggml_new_tensor(ctx, type, 2, ne);
}

ggml_tensor* ggml_new_tensor_3d(ggml_context* ctx, int type, int64_t ne0, int64_t ne1, int64_t ne2) {
    int64_t ne[3] = {ne0, ne1, ne2};
    return ggml_new_tensor(ctx, type, 3, ne);
}

// Arithmetic operations
ggml_tensor* ggml_add(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    // Simple element-wise addition
    size_t n = a->size / ggml_type_size(a->type);
    float* dst = static_cast<float*>(result->data);
    float* src1 = static_cast<float*>(a->data);
    float* src2 = static_cast<float*>(b->data);
    
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src1[i] + src2[i];
    }
    
    return result;
}

ggml_tensor* ggml_mul(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    // Simple element-wise multiplication
    size_t n = a->size / ggml_type_size(a->type);
    float* dst = static_cast<float*>(result->data);
    float* src1 = static_cast<float*>(a->data);
    float* src2 = static_cast<float*>(b->data);
    
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src1[i] * src2[i];
    }
    
    return result;
}

ggml_tensor* ggml_mul_mat(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b) {
    // Matrix multiplication
    int64_t ne[2] = {a->ne[0], b->ne[1]};
    auto* result = ggml_new_tensor(ctx, a->type, 2, ne);
    if (!result) return nullptr;
    
    // Use our GPU backend for matrix multiplication
    auto* backend = RawrXD::GPU::get_global_backend();
    
    // Create buffers
    auto buf_a = backend->allocate(a->size, RawrXD::GPU::DataType::F32);
    auto buf_b = backend->allocate(b->size, RawrXD::GPU::DataType::F32);
    auto buf_c = backend->allocate(result->size, RawrXD::GPU::DataType::F32);
    
    // Copy data
    backend->copy_to_device(buf_a.get(), a->data, a->size);
    backend->copy_to_device(buf_b.get(), b->data, b->size);
    
    // Perform matrix multiplication
    backend->mat_mul(buf_a.get(), buf_b.get(), buf_c.get(), 
                    a->ne[0], b->ne[1], a->ne[1], false, false);
    
    // Copy result back
    backend->copy_to_host(result->data, buf_c.get(), result->size);
    
    return result;
}

// Activation functions
ggml_tensor* ggml_relu(ggml_context* ctx, ggml_tensor* a) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t n = a->size / ggml_type_size(a->type);
    float* dst = static_cast<float*>(result->data);
    float* src = static_cast<float*>(a->data);
    
    for (size_t i = 0; i < n; ++i) {
        dst[i] = std::max(0.0f, src[i]);
    }
    
    return result;
}

ggml_tensor* ggml_gelu(ggml_context* ctx, ggml_tensor* a) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t n = a->size / ggml_type_size(a->type);
    float* dst = static_cast<float*>(result->data);
    float* src = static_cast<float*>(a->data);
    
    for (size_t i = 0; i < n; ++i) {
        // GELU approximation
        float x = src[i];
        float x3 = x * x * x;
        dst[i] = 0.5f * x * (1.0f + std::tanh(0.7978845608f * (x + 0.044715f * x3)));
    }
    
    return result;
}

// Normalization
ggml_tensor* ggml_norm(ggml_context* ctx, ggml_tensor* a, float eps) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t rows = a->ne[1];
    size_t cols = a->ne[0];
    
    auto* backend = RawrXD::GPU::get_global_backend();
    auto buf_x = backend->allocate(a->size, RawrXD::GPU::DataType::F32);
    auto buf_y = backend->allocate(result->size, RawrXD::GPU::DataType::F32);
    
    backend->copy_to_device(buf_x.get(), a->data, a->size);
    backend->layer_norm(buf_y.get(), nullptr, nullptr, rows, cols, eps);
    backend->copy_to_host(result->data, buf_y.get(), result->size);
    
    return result;
}

ggml_tensor* ggml_rms_norm(ggml_context* ctx, ggml_tensor* a, float eps) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t rows = a->ne[1];
    size_t cols = a->ne[0];
    
    auto* backend = RawrXD::GPU::get_global_backend();
    auto buf_x = backend->allocate(a->size, RawrXD::GPU::DataType::F32);
    auto buf_y = backend->allocate(result->size, RawrXD::GPU::DataType::F32);
    
    backend->copy_to_device(buf_x.get(), a->data, a->size);
    backend->rms_norm(buf_y.get(), nullptr, rows, cols, eps);
    backend->copy_to_host(result->data, buf_y.get(), result->size);
    
    return result;
}

// Softmax
ggml_tensor* ggml_soft_max(ggml_context* ctx, ggml_tensor* a) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t rows = a->ne[1];
    size_t cols = a->ne[0];
    
    auto* backend = RawrXD::GPU::get_global_backend();
    auto buf_x = backend->allocate(a->size, RawrXD::GPU::DataType::F32);
    auto buf_y = backend->allocate(result->size, RawrXD::GPU::DataType::F32);
    
    backend->copy_to_device(buf_x.get(), a->data, a->size);
    backend->softmax(buf_y.get(), rows, cols);
    backend->copy_to_host(result->data, buf_y.get(), result->size);
    
    return result;
}

// RoPE (Rotary Position Embedding)
ggml_tensor* ggml_rope(ggml_context* ctx, ggml_tensor* a, int n_past, int n_dims, int mode) {
    auto* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) return nullptr;
    
    size_t rows = a->ne[1];
    size_t cols = a->ne[0];
    
    auto* backend = RawrXD::GPU::get_global_backend();
    auto buf_x = backend->allocate(a->size, RawrXD::GPU::DataType::F32);
    auto buf_y = backend->allocate(result->size, RawrXD::GPU::DataType::F32);
    
    backend->copy_to_device(buf_x.get(), a->data, a->size);
    backend->rope(buf_y.get(), rows, cols, n_past, 10000.0f); // theta = 10000
    backend->copy_to_host(result->data, buf_y.get(), result->size);
    
    return result;
}

// Graph computation
void ggml_build_forward_expand(ggml_cgraph* cgraph, ggml_tensor* tensor) {
    cgraph->nodes.push_back(tensor);
}

void ggml_graph_compute(ggml_context* ctx, ggml_cgraph* cgraph) {
    // In our implementation, computation happens during op creation
    // This is a no-op for compatibility
    std::cout << "[GGML Stub] Computing graph with " << cgraph->nodes.size() << " nodes" << std::endl;
}

// Memory management
void* ggml_aligned_malloc(size_t size) {
    #if defined(_MSC_VER) || defined(__MINGW32__)
    return _aligned_malloc(size, 64);
    #elif defined(__APPLE__)
    void* ptr = nullptr;
    posix_memalign(&ptr, 64, size);
    return ptr;
    #else
    return std::aligned_alloc(64, size); // 64-byte alignment for SIMD
    #endif
}

void ggml_aligned_free(void* ptr) {
    #if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
    #else
    free(ptr);
    #endif
}

// Backend management
void ggml_backend_cpu_set_n_threads(int n_threads) {
    // Threading handled by our backend
}

// Quantization helpers
void ggml_quantize_q4_0(const float* src, void* dst, int n, int k, int64_t* hist) {
    // Simple quantization to Q4_0 format
    uint8_t* dst_u8 = static_cast<uint8_t*>(dst);
    for (int i = 0; i < n * k; i += 2) {
        int q1 = std::max(0, std::min(15, int(src[i] * 7.5f + 8.0f)));
        int q2 = std::max(0, std::min(15, int(src[i+1] * 7.5f + 8.0f)));
        dst_u8[i/2] = (q1 << 4) | q2;
    }
}

void ggml_quantize_q8_0(const float* src, void* dst, int n, int k, int64_t* hist) {
    // Simple quantization to Q8_0 format
    int8_t* dst_s8 = static_cast<int8_t*>(dst);
    for (int i = 0; i < n * k; ++i) {
        dst_s8[i] = std::max(-127, std::min(127, int(src[i] * 127.0f)));
    }
}

// ===== Missing ggml compatibility helpers =====

void ggml_abort(const char* file, int line, const char* fmt, ...) {
    std::fprintf(stderr, "[GGML Stub] abort at %s:%d: ", file ? file : "?", line);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
    std::abort();
}

FILE* ggml_fopen(const char* fname, const char* mode) {
    return std::fopen(fname, mode);
}

size_t ggml_nbytes(const ggml_tensor* tensor) {
    return tensor ? tensor->size : 0;
}

size_t ggml_nbytes_pad(const ggml_tensor* tensor) {
    return ggml_nbytes(tensor);
}

int64_t ggml_blck_size(int type) {
    switch (type) {
        case GGML_TYPE_Q4_0:
        case GGML_TYPE_Q8_0:
            return 32;
        case GGML_TYPE_Q2_K:
            return 256;
        default:
            return 1;
    }
}

size_t ggml_type_size(int type) {
    switch (type) {
        case GGML_TYPE_F32: return 4;
        case GGML_TYPE_F16: return 2;
        case GGML_TYPE_Q4_0: return 1;
        case GGML_TYPE_Q8_0: return 1;
        case GGML_TYPE_Q2_K: return 1;
        default: return 4;
    }
}

const char* ggml_type_name(int type) {
    switch (type) {
        case GGML_TYPE_F32: return "f32";
        case GGML_TYPE_F16: return "f16";
        case GGML_TYPE_Q4_0: return "q4_0";
        case GGML_TYPE_Q8_0: return "q8_0";
        case GGML_TYPE_Q2_K: return "q2_k";
        default: return "unknown";
    }
}

int ggml_n_dims(const ggml_tensor* tensor) {
    return tensor ? tensor->n_dims : 0;
}

bool ggml_is_contiguous(const ggml_tensor* tensor) {
    (void)tensor;
    return true;
}

size_t ggml_tensor_overhead(void) {
    return sizeof(ggml_tensor);
}

void ggml_set_no_alloc(ggml_context* ctx, bool no_alloc) {
    (void)ctx;
    (void)no_alloc;
}

ggml_tensor* ggml_set_name(ggml_tensor* tensor, const char* name) {
    (void)name;
    return tensor;
}

void ggml_backend_tensor_get(const ggml_tensor* tensor, void* data, size_t offset, size_t size) {
    if (!tensor || !data || !tensor->data) {
        return;
    }
    const uint8_t* src = static_cast<const uint8_t*>(tensor->data);
    std::memcpy(data, src + offset, size);
}

void ggml_log_internal(int level, const char* fmt, ...) {
    (void)level;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fprintf(stderr, "\n");
}

bool ggml_is_quantized(int type) {
    return type == GGML_TYPE_Q4_0 || type == GGML_TYPE_Q8_0 || type == GGML_TYPE_Q2_K;
}

ggml_tensor* ggml_scale(ggml_context* ctx, ggml_tensor* a, float s) {
    if (!a) {
        return nullptr;
    }
    ggml_tensor* result = ggml_new_tensor(ctx, a->type, a->n_dims, a->ne);
    if (!result) {
        return nullptr;
    }
    size_t n = a->size / ggml_type_size(a->type);
    float* dst = static_cast<float*>(result->data);
    float* src = static_cast<float*>(a->data);
    for (size_t i = 0; i < n; ++i) {
        dst[i] = src[i] * s;
    }
    return result;
}

ggml_tensor* ggml_view_1d(ggml_context* ctx, ggml_tensor* a, int64_t ne0, size_t offset) {
    if (!a) {
        return nullptr;
    }
    ggml_tensor* view = ggml_new_tensor_1d(ctx, a->type, ne0);
    if (!view) {
        return nullptr;
    }
    view->data = static_cast<uint8_t*>(a->data) + offset;
    view->size = ne0 * ggml_type_size(a->type);
    return view;
}

ggml_tensor* ggml_get_rows(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b) {
    (void)ctx;
    (void)b;
    return a;
}

ggml_cgraph* ggml_new_graph(ggml_context* ctx) {
    (void)ctx;
    return new ggml_cgraph();
}

void ggml_backend_free(ggml_backend_t backend) {
    delete backend;
}

int ggml_backend_graph_compute(ggml_backend_t backend, ggml_cgraph* cgraph) {
    (void)backend;
    (void)cgraph;
    return GGML_STATUS_SUCCESS;
}

ggml_backend_t ggml_backend_cpu_init(void) {
    return new ggml_backend();
}

bool ggml_cpu_has_avx(void) {
    return false;
}

} // extern "C"

