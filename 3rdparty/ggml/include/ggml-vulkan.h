#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define GGML_RXD_VK_NAME "Vulkan"
#define GGML_RXD_VK_MAX_DEVICES 16

// backend API
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_vk_init(size_t dev_num);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_vk(ggml_rxd_backend_t backend);
GGML_RXD_BACKEND_API int  ggml_rxd_backend_vk_get_device_count(void);
GGML_RXD_BACKEND_API void ggml_rxd_backend_vk_get_device_description(int device, char * description, size_t description_size);
GGML_RXD_BACKEND_API void ggml_rxd_backend_vk_get_device_memory(int device, size_t * free, size_t * total);

GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_vk_buffer_type(size_t dev_num);
// pinned host buffer for use with the CPU backend for faster copies between CPU and GPU
GGML_RXD_BACKEND_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_vk_host_buffer_type(void);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_vk_reg(void);

#ifdef  __cplusplus
}
#endif
