#ifndef GGML_RXD_SYCL_WKV_HPP
#define GGML_RXD_SYCL_WKV_HPP

#include "common.hpp"

void ggml_rxd_sycl_op_rwkv_wkv6(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

void ggml_rxd_sycl_op_rwkv_wkv7(ggml_rxd_backend_sycl_context & ctx, ggml_rxd_tensor * dst);

#endif // GGML_RXD_SYCL_WKV_HPP
