#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define RPC_PROTO_MAJOR_VERSION    3
#define RPC_PROTO_MINOR_VERSION    0
#define RPC_PROTO_PATCH_VERSION    0
#define GGML_RXD_RPC_MAX_SERVERS       16

// backend API
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_rpc_init(const char * endpoint, uint32_t device);
GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_rpc(ggml_rxd_backend_t backend);

GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_rpc_buffer_type(const char * endpoint, uint32_t device);

GGML_RXD_BACKEND_API void ggml_rxd_backend_rpc_get_device_memory(const char * endpoint, uint32_t device, size_t * free, size_t * total);

GGML_RXD_BACKEND_API void ggml_rxd_backend_rpc_start_server(const char * endpoint, const char * cache_dir,
                                                    size_t n_threads, size_t n_devices, ggml_rxd_backend_dev_t * devices);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_rpc_reg(void);
GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_rpc_add_server(const char * endpoint);

#ifdef  __cplusplus
}
#endif
