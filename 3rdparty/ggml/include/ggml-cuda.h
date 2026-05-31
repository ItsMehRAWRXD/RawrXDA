#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef GGML_RXD_USE_HIP
#define GGML_RXD_CUDA_NAME "ROCm"
#define GGML_RXD_CUBLAS_NAME "hipBLAS"
#elif defined(GGML_RXD_USE_MUSA)
#define GGML_RXD_CUDA_NAME "MUSA"
#define GGML_RXD_CUBLAS_NAME "muBLAS"
#else
#define GGML_RXD_CUDA_NAME "CUDA"
#define GGML_RXD_CUBLAS_NAME "cuBLAS"
#endif
#define GGML_RXD_CUDA_MAX_DEVICES       16

// backend API
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_cuda_init(int device);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_cuda(ggml_rxd_backend_t backend);

// device buffer
GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_cuda_buffer_type(int device);

// split tensor buffer that splits matrices by rows across multiple devices
GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_cuda_split_buffer_type(int main_device, const float * tensor_split);

// pinned host buffer for use with the CPU backend for faster copies between CPU and GPU
GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_cuda_host_buffer_type(void);

GGML_RXD_BACKEND_API int  ggml_rxd_backend_cuda_get_device_count(void);
GGML_RXD_BACKEND_API void ggml_rxd_backend_cuda_get_device_description(int device, char * description, size_t description_size);
GGML_RXD_BACKEND_API void ggml_rxd_backend_cuda_get_device_memory(int device, size_t * free, size_t * total);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_cuda_register_host_buffer(void * buffer, size_t size);
GGML_RXD_BACKEND_API void ggml_rxd_backend_cuda_unregister_host_buffer(void * buffer);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_cuda_reg(void);

#ifdef  __cplusplus
}
#endif
