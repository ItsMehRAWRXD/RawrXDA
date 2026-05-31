#pragma once

#include "ggml-metal-device.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// backend context
//

typedef struct ggml_rxd_metal * ggml_rxd_metal_t;

ggml_rxd_metal_t ggml_rxd_metal_init(ggml_rxd_metal_device_t dev);
void ggml_rxd_metal_free(ggml_rxd_metal_t ctx);

void ggml_rxd_metal_synchronize(ggml_rxd_metal_t ctx);

void ggml_rxd_metal_set_tensor_async(ggml_rxd_metal_t ctx, struct ggml_rxd_tensor * tensor, const void * data, size_t offset, size_t size);
void ggml_rxd_metal_get_tensor_async(ggml_rxd_metal_t ctx, const struct ggml_rxd_tensor * tensor, void * data, size_t offset, size_t size);

enum ggml_rxd_status ggml_rxd_metal_graph_compute (ggml_rxd_metal_t ctx, struct ggml_rxd_cgraph * gf);
void             ggml_rxd_metal_graph_optimize(ggml_rxd_metal_t ctx, struct ggml_rxd_cgraph * gf);

void ggml_rxd_metal_set_n_cb            (ggml_rxd_metal_t ctx, int n_cb);
void ggml_rxd_metal_set_abort_callback  (ggml_rxd_metal_t ctx, ggml_rxd_abort_callback abort_callback, void * user_data);
bool ggml_rxd_metal_supports_family     (ggml_rxd_metal_t ctx, int family);
void ggml_rxd_metal_capture_next_compute(ggml_rxd_metal_t ctx);

#ifdef __cplusplus
}
#endif
