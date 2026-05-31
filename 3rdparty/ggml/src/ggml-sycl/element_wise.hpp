#ifndef GGML_RXD_SYCL_ELEMENTWISE_HPP
#define GGML_RXD_SYCL_ELEMENTWISE_HPP

#include "common.hpp"
#include "ggml.h"
#include <limits> // For std::numeric_limits

template <typename T>
T neg_infinity() {
    return -std::numeric_limits<T>::infinity();
}

template<typename T_Dst, typename T_Src = T_Dst>
struct typed_data {
    const T_Src * src;
    T_Dst * dst;
};

template<typename T_Dst, typename T_Src = T_Dst>
typed_data<T_Dst, T_Src> cast_data(ggml_rxd_tensor * dst) {
    return {
        /* .src = */ static_cast<const T_Src *>(dst->src[0]->data),
        /* .dst = */ static_cast<T_Dst *>(dst->data)
    };
}

const float GELU_QUICK_COEF = -1.702f;


void ggml_rxd_sycl_sqrt(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_sin(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_cos(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_acc(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_gelu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_silu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_gelu_quick(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_gelu_erf(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_tanh(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_relu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_sigmoid(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_hardsigmoid(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_hardswish(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_exp(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_log(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_neg(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_step(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_leaky_relu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_sqr(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_upscale(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_clamp(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_sgn(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_abs(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_elu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_geglu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_reglu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_swiglu(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_geglu_erf(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_geglu_quick(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_floor(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_ceil(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_round(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);
void ggml_rxd_sycl_trunc(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_arange(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

#endif // GGML_RXD_SYCL_ELEMENTWISE_HPP
