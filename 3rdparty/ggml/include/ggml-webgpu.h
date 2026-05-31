#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define GGML_RXD_WEBGPU_NAME "WebGPU"

// Needed for examples in ggml
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_webgpu_init(void);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_webgpu_reg(void);

#ifdef  __cplusplus
}
#endif
