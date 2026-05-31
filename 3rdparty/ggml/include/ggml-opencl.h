#ifndef GGML_RXD_OPENCL_H
#define GGML_RXD_OPENCL_H

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

//
// backend API
//
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_opencl_init(void);
GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_opencl(ggml_rxd_backend_t backend);

GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_opencl_buffer_type(void);
GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_opencl_host_buffer_type(void);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_opencl_reg(void);

#ifdef  __cplusplus
}
#endif

#endif // GGML_RXD_OPENCL_H
