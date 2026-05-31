#pragma once

#include "ggml.h"
#include "ggml-backend.h"


#ifdef  __cplusplus
extern "C" {
#endif

// backend API
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_blas_init(void);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_blas(ggml_rxd_backend_t backend);

// number of threads used for conversion to float
// for openblas and blis, this will also set the number of threads used for blas operations
GGML_RXD_BACKEND_API void ggml_rxd_backend_blas_set_n_threads(ggml_rxd_backend_t backend_blas, int n_threads);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_blas_reg(void);


#ifdef  __cplusplus
}
#endif
