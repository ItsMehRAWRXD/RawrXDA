#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>

struct ggml_tensor {
    void* data;
    size_t size;
    int n_dims;
    int64_t ne[4];
    size_t nb[4];
    int type;
    int backend;
};

struct ggml_context {
    std::vector<uint8_t> buffer;
    size_t offset;
    std::mutex mutex;
};

struct ggml_cgraph {
    std::vector<ggml_tensor*> nodes;
    std::vector<ggml_tensor*> leafs;
};

#define GGML_TYPE_F32 0
#define GGML_TYPE_F16 1
#define GGML_TYPE_Q4_0 2
#define GGML_TYPE_Q8_0 3
#define GGML_TYPE_Q2_K 4

#define GGML_BACKEND_CPU 0
#define GGML_BACKEND_GPU 1

#ifdef __cplusplus
extern "C" {
#endif

size_t ggml_type_size(int type);

ggml_context* ggml_init(size_t mem_size);
void ggml_free(ggml_context* ctx);

ggml_tensor* ggml_new_tensor(ggml_context* ctx, int type, int n_dims, const int64_t* ne);
ggml_tensor* ggml_new_tensor_1d(ggml_context* ctx, int type, int64_t ne0);
ggml_tensor* ggml_new_tensor_2d(ggml_context* ctx, int type, int64_t ne0, int64_t ne1);
ggml_tensor* ggml_new_tensor_3d(ggml_context* ctx, int type, int64_t ne0, int64_t ne1, int64_t ne2);


ggml_tensor* ggml_add(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b);
ggml_tensor* ggml_mul(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b);
ggml_tensor* ggml_mul_mat(ggml_context* ctx, ggml_tensor* a, ggml_tensor* b);

ggml_tensor* ggml_relu(ggml_context* ctx, ggml_tensor* a);
ggml_tensor* ggml_gelu(ggml_context* ctx, ggml_tensor* a);
ggml_tensor* ggml_norm(ggml_context* ctx, ggml_tensor* a, float eps);
ggml_tensor* ggml_rms_norm(ggml_context* ctx, ggml_tensor* a, float eps);
ggml_tensor* ggml_soft_max(ggml_context* ctx, ggml_tensor* a);
ggml_tensor* ggml_rope(ggml_context* ctx, ggml_tensor* a, int n_past, int n_dims, int mode);

void ggml_build_forward_expand(ggml_cgraph* cgraph, ggml_tensor* tensor);
void ggml_graph_compute(ggml_context* ctx, ggml_cgraph* cgraph);

void* ggml_aligned_malloc(size_t size);
void ggml_aligned_free(void* ptr);

void ggml_backend_cpu_set_n_threads(int n_threads);

void ggml_quantize_q4_0(const float* src, void* dst, int n, int k, int64_t* hist);
void ggml_quantize_q8_0(const float* src, void* dst, int n, int k, int64_t* hist);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace RawrXD {
namespace GGML {

class Context {
public:
    Context(size_t size) : ctx_(ggml_init(size)) {}
    ~Context() { ggml_free(ctx_); }

    ggml_context* get() { return ctx_; }

    ggml_tensor* new_tensor_1d(int type, int64_t ne0) {
        return ggml_new_tensor_1d(ctx_, type, ne0);
    }

    ggml_tensor* new_tensor_2d(int type, int64_t ne0, int64_t ne1) {
        return ggml_new_tensor_2d(ctx_, type, ne0, ne1);
    }

    ggml_tensor* new_tensor_3d(int type, int64_t ne0, int64_t ne1, int64_t ne2) {
        return ggml_new_tensor_3d(ctx_, type, ne0, ne1, ne2);
    }

    ggml_tensor* add(ggml_tensor* a, ggml_tensor* b) {
        return ggml_add(ctx_, a, b);
    }

    ggml_tensor* mul_mat(ggml_tensor* a, ggml_tensor* b) {
        return ggml_mul_mat(ctx_, a, b);
    }

    ggml_tensor* relu(ggml_tensor* a) {
        return ggml_relu(ctx_, a);
    }

    ggml_tensor* gelu(ggml_tensor* a) {
        return ggml_gelu(ctx_, a);
    }

    ggml_tensor* norm(ggml_tensor* a, float eps = 1e-5f) {
        return ggml_norm(ctx_, a, eps);
    }

    ggml_tensor* rms_norm(ggml_tensor* a, float eps = 1e-5f) {
        return ggml_rms_norm(ctx_, a, eps);
    }

    ggml_tensor* softmax(ggml_tensor* a) {
        return ggml_soft_max(ctx_, a);
    }

    ggml_tensor* rope(ggml_tensor* a, int n_past, int n_dims, int mode) {
        return ggml_rope(ctx_, a, n_past, n_dims, mode);
    }

private:
    ggml_context* ctx_;
};

} // namespace GGML
} // namespace RawrXD
#endif
