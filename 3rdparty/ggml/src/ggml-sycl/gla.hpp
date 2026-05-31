#ifndef GGML_RXD_SYCL_GLA_HPP
#define GGML_RXD_SYCL_GLA_HPP

#include "common.hpp"

void ggml_rxd_sycl_op_gated_linear_attn(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

#endif  // GGML_RXD_SYCL_GLA_HPP
