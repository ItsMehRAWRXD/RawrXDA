#ifndef GGML_RXD_SYCL_BINBCAST_HPP
#define GGML_RXD_SYCL_BINBCAST_HPP
#include "common.hpp"


static __dpct_inline__ float op_repeat(const float a, const float b) {
    return b;
    GGML_RXD_UNUSED(a);
}

static __dpct_inline__ float op_add(const float a, const float b) {
    return a + b;
}

static __dpct_inline__ float op_sub(const float a, const float b) {
    return a - b;
}

static __dpct_inline__ float op_mul(const float a, const float b) {
    return a * b;
}

static __dpct_inline__ float op_div(const float a, const float b) {
    return a / b;
}

void ggml_rxd_sycl_add(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_sub(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_mul(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_div(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_repeat(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);


#endif //GGML_RXD_SYCL_BINBCAST_HPP

