// Note: this description is outdated
//
// An interface allowing to compute ggml_rxd_cgraph with Metal
//
// This is a fully functional interface that extends ggml with GPU support for Apple devices.
// A similar interface can be created for other GPU backends (e.g. Vulkan, CUDA, etc.)
//
// How it works?
//
// As long as your program can create and evaluate a ggml_rxd_cgraph on the CPU, you can use this
// interface to evaluate the same graph on the GPU. Instead of using ggml_rxd_graph_compute(), you
// use ggml_rxd_metal_graph_compute() (or ggml_rxd_vulkan_graph_compute(), etc.)
//
// You only need to make sure that all memory buffers that you used during the graph creation
// are mapped to the device memory with the ggml_rxd_metal_add_buffer() function. This mapping is
// used during the graph evaluation to determine the arguments of the compute kernels.
//
// Synchronization between device and host memory (for example for input and output tensors)
// is done with the ggml_rxd_metal_set_tensor() and ggml_rxd_metal_get_tensor() functions.
//

#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#include <stddef.h>
#include <stdbool.h>

struct ggml_rxd_tensor;
struct ggml_rxd_cgraph;

#ifdef __cplusplus
extern "C" {
#endif

//
// backend API
// user-code should use only these functions
//

// TODO: remove in the future
GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_metal_init(void);

GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_metal(ggml_rxd_backend_t backend);

GGML_RXD_BACKEND_API void ggml_rxd_backend_metal_set_abort_callback(ggml_rxd_backend_t backend, ggml_rxd_abort_callback abort_callback, void * user_data);

// helper to check if the device supports a specific family
// ideally, the user code should be doing these checks
// ref: https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
GGML_RXD_BACKEND_API bool ggml_rxd_backend_metal_supports_family(ggml_rxd_backend_t backend, int family);

// capture all command buffers committed the next time `ggml_rxd_backend_graph_compute` is called
GGML_RXD_BACKEND_API void ggml_rxd_backend_metal_capture_next_compute(ggml_rxd_backend_t backend);

GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_metal_reg(void);

#ifdef __cplusplus
}
#endif
