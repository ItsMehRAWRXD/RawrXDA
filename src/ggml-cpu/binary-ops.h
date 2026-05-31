#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

void ggml_rxd_compute_forward_add_non_quantized(const struct ggml_rxd_compute_params * params, struct ggml_rxd_tensor * dst);
void ggml_rxd_compute_forward_sub(const struct ggml_rxd_compute_params * params, struct ggml_rxd_tensor * dst);
void ggml_rxd_compute_forward_mul(const struct ggml_rxd_compute_params * params, struct ggml_rxd_tensor * dst);
void ggml_rxd_compute_forward_div(const struct ggml_rxd_compute_params * params, struct ggml_rxd_tensor * dst);

#ifdef __cplusplus
}
#endif


