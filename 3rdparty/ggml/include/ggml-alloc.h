#pragma once

#include "ggml.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct ggml_rxd_backend_buffer_type * ggml_rxd_backend_buffer_type_t;
typedef struct      ggml_rxd_backend_buffer * ggml_rxd_backend_buffer_t;
typedef struct             ggml_rxd_backend * ggml_rxd_backend_t;

// Tensor allocator
struct ggml_rxd_tallocr {
    ggml_rxd_backend_buffer_t buffer;
    void * base;
    size_t alignment;
    size_t offset;
};

GGML_RXD_API struct ggml_rxd_tallocr ggml_rxd_tallocr_new(ggml_rxd_backend_buffer_t buffer);
GGML_RXD_API enum ggml_rxd_status    ggml_rxd_tallocr_alloc(struct ggml_rxd_tallocr * talloc, struct ggml_rxd_tensor * tensor);

// Graph allocator
/*
  Example usage:
    ggml_rxd_gallocr_t galloc = ggml_rxd_gallocr_new(ggml_rxd_backend_cpu_buffer_type());

    // optional: create a worst-case graph and reserve the buffers to avoid reallocations
    ggml_rxd_gallocr_reserve(galloc, build_graph(max_batch));

    // allocate the graph
    struct ggml_rxd_cgraph * graph = build_graph(batch);
    ggml_rxd_gallocr_alloc_graph(galloc, graph);

    printf("compute buffer size: %zu bytes\n", ggml_rxd_gallocr_get_buffer_size(galloc, 0));

    // evaluate the graph
    ggml_rxd_backend_graph_compute(backend, graph);
*/

// special tensor flags for use with the graph allocator:
//   ggml_rxd_set_input(): all input tensors are allocated at the beginning of the graph in non-overlapping addresses
//   ggml_rxd_set_output(): output tensors are never freed and never overwritten

typedef struct ggml_rxd_gallocr * ggml_rxd_gallocr_t;

GGML_RXD_API ggml_rxd_gallocr_t ggml_rxd_gallocr_new(ggml_rxd_backend_buffer_type_t buft);
GGML_RXD_API ggml_rxd_gallocr_t ggml_rxd_gallocr_new_n(ggml_rxd_backend_buffer_type_t * bufts, int n_bufs);
GGML_RXD_API void           ggml_rxd_gallocr_free(ggml_rxd_gallocr_t galloc);

// pre-allocate buffers from a measure graph - does not allocate or modify the graph
// call with a worst-case graph to avoid buffer reallocations
// not strictly required for single buffer usage: ggml_rxd_gallocr_alloc_graph will reallocate the buffers automatically if needed
// returns false if the buffer allocation failed
GGML_RXD_API bool ggml_rxd_gallocr_reserve(ggml_rxd_gallocr_t galloc, struct ggml_rxd_cgraph * graph);
GGML_RXD_API bool ggml_rxd_gallocr_reserve_n(
    ggml_rxd_gallocr_t galloc,
    struct ggml_rxd_cgraph * graph,
    const int * node_buffer_ids,
    const int * leaf_buffer_ids);

// automatic reallocation if the topology changes when using a single buffer
// returns false if using multiple buffers and a re-allocation is needed (call ggml_rxd_gallocr_reserve_n first to set the node buffers)
GGML_RXD_API bool ggml_rxd_gallocr_alloc_graph(ggml_rxd_gallocr_t galloc, struct ggml_rxd_cgraph * graph);

GGML_RXD_API size_t ggml_rxd_gallocr_get_buffer_size(ggml_rxd_gallocr_t galloc, int buffer_id);

// Utils
// Create a buffer and allocate all the tensors in a ggml_rxd_context
GGML_RXD_API struct ggml_rxd_backend_buffer * ggml_rxd_backend_alloc_ctx_tensors_from_buft(struct ggml_rxd_context * ctx, ggml_rxd_backend_buffer_type_t buft);
GGML_RXD_API struct ggml_rxd_backend_buffer * ggml_rxd_backend_alloc_ctx_tensors(struct ggml_rxd_context * ctx, ggml_rxd_backend_t backend);

#ifdef  __cplusplus
}
#endif
