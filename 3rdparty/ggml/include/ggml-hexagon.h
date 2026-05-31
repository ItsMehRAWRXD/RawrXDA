#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

// backend API
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_hexagon_init(void);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_hexagon(ggml_rxd_backend_t backend);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_hexagon_reg(void);

#ifdef  __cplusplus
}
#endif
