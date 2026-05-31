#pragma once

#include "ggml.h"
#include "ggml-alloc.h"

#ifdef GGML_RXD_BACKEND_SHARED
#    if defined(_WIN32) && !defined(__MINGW32__)
#        ifdef GGML_RXD_BACKEND_BUILD
#            define GGML_RXD_BACKEND_API __declspec(dllexport) extern
#        else
#            define GGML_RXD_BACKEND_API __declspec(dllimport) extern
#        endif
#    else
#        define GGML_RXD_BACKEND_API __attribute__ ((visibility ("default"))) extern
#    endif
#else
#    define GGML_RXD_BACKEND_API extern
#endif

#ifdef  __cplusplus
extern "C" {
#endif

    typedef struct ggml_rxd_backend_buffer_type * ggml_rxd_backend_buffer_type_t;
    typedef struct ggml_rxd_backend_buffer * ggml_rxd_backend_buffer_t;
    typedef struct ggml_rxd_backend_event * ggml_rxd_backend_event_t;
    typedef struct ggml_rxd_backend * ggml_rxd_backend_t;
    typedef void * ggml_rxd_backend_graph_plan_t;
    typedef struct ggml_rxd_backend_reg * ggml_rxd_backend_reg_t;
    typedef struct ggml_rxd_backend_device * ggml_rxd_backend_dev_t;


    //
    // Backend buffer type
    //

    GGML_RXD_API const char *          ggml_rxd_backend_buft_name          (ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API ggml_rxd_backend_buffer_t ggml_rxd_backend_buft_alloc_buffer  (ggml_rxd_backend_buffer_type_t buft, size_t size);
    GGML_RXD_API size_t                ggml_rxd_backend_buft_get_alignment (ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API size_t                ggml_rxd_backend_buft_get_max_size  (ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API size_t                ggml_rxd_backend_buft_get_alloc_size(ggml_rxd_backend_buffer_type_t buft, const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool                  ggml_rxd_backend_buft_is_host       (ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API ggml_rxd_backend_dev_t    ggml_rxd_backend_buft_get_device    (ggml_rxd_backend_buffer_type_t buft);

    //
    // Backend buffer
    //

    enum ggml_rxd_backend_buffer_usage {
        GGML_RXD_BACKEND_BUFFER_USAGE_ANY = 0,
        GGML_RXD_BACKEND_BUFFER_USAGE_WEIGHTS = 1,
        GGML_RXD_BACKEND_BUFFER_USAGE_COMPUTE = 2,
    };

    GGML_RXD_API const char *                   ggml_rxd_backend_buffer_name          (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API void                           ggml_rxd_backend_buffer_free          (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API void *                         ggml_rxd_backend_buffer_get_base      (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API size_t                         ggml_rxd_backend_buffer_get_size      (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API enum ggml_rxd_status               ggml_rxd_backend_buffer_init_tensor   (ggml_rxd_backend_buffer_t buffer, struct ggml_rxd_tensor * tensor);
    GGML_RXD_API size_t                         ggml_rxd_backend_buffer_get_alignment (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API size_t                         ggml_rxd_backend_buffer_get_max_size  (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API size_t                         ggml_rxd_backend_buffer_get_alloc_size(ggml_rxd_backend_buffer_t buffer, const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API void                           ggml_rxd_backend_buffer_clear         (ggml_rxd_backend_buffer_t buffer, uint8_t value);
    GGML_RXD_API bool                           ggml_rxd_backend_buffer_is_host       (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API void                           ggml_rxd_backend_buffer_set_usage     (ggml_rxd_backend_buffer_t buffer, enum ggml_rxd_backend_buffer_usage usage);
    GGML_RXD_API enum ggml_rxd_backend_buffer_usage ggml_rxd_backend_buffer_get_usage     (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API ggml_rxd_backend_buffer_type_t     ggml_rxd_backend_buffer_get_type      (ggml_rxd_backend_buffer_t buffer);
    GGML_RXD_API void                           ggml_rxd_backend_buffer_reset         (ggml_rxd_backend_buffer_t buffer);

    // tensor copy between different backends
    GGML_RXD_API void ggml_rxd_backend_tensor_copy(struct ggml_rxd_tensor * src, struct ggml_rxd_tensor * dst);

    //
    // Backend (stream)
    //

    GGML_RXD_API ggml_rxd_guid_t  ggml_rxd_backend_guid(ggml_rxd_backend_t backend);
    GGML_RXD_API const char * ggml_rxd_backend_name(ggml_rxd_backend_t backend);
    GGML_RXD_API void         ggml_rxd_backend_free(ggml_rxd_backend_t backend);

    GGML_RXD_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_get_default_buffer_type(ggml_rxd_backend_t backend);
    GGML_RXD_API ggml_rxd_backend_buffer_t      ggml_rxd_backend_alloc_buffer(ggml_rxd_backend_t backend, size_t size);
    GGML_RXD_API size_t                     ggml_rxd_backend_get_alignment(ggml_rxd_backend_t backend);
    GGML_RXD_API size_t                     ggml_rxd_backend_get_max_size(ggml_rxd_backend_t backend);

    GGML_RXD_API void ggml_rxd_backend_tensor_set_async(ggml_rxd_backend_t backend,       struct ggml_rxd_tensor * tensor, const void * data, size_t offset, size_t size);
    GGML_RXD_API void ggml_rxd_backend_tensor_get_async(ggml_rxd_backend_t backend, const struct ggml_rxd_tensor * tensor,       void * data, size_t offset, size_t size);

    // "offset" refers to the offset in tensor->data for setting/getting data
    GGML_RXD_API void ggml_rxd_backend_tensor_set(      struct ggml_rxd_tensor * tensor, const void * data, size_t offset, size_t size);
    GGML_RXD_API void ggml_rxd_backend_tensor_get(const struct ggml_rxd_tensor * tensor,       void * data, size_t offset, size_t size);
    GGML_RXD_API void ggml_rxd_backend_tensor_memset(   struct ggml_rxd_tensor * tensor,     uint8_t value, size_t offset, size_t size);

    GGML_RXD_API void ggml_rxd_backend_synchronize(ggml_rxd_backend_t backend);

    GGML_RXD_API ggml_rxd_backend_graph_plan_t ggml_rxd_backend_graph_plan_create(ggml_rxd_backend_t backend, struct ggml_rxd_cgraph * cgraph);
    GGML_RXD_API void                      ggml_rxd_backend_graph_plan_free  (ggml_rxd_backend_t backend, ggml_rxd_backend_graph_plan_t plan);

    GGML_RXD_API enum ggml_rxd_status ggml_rxd_backend_graph_plan_compute (ggml_rxd_backend_t backend, ggml_rxd_backend_graph_plan_t plan);
    GGML_RXD_API enum ggml_rxd_status ggml_rxd_backend_graph_compute      (ggml_rxd_backend_t backend, struct ggml_rxd_cgraph * cgraph);
    GGML_RXD_API enum ggml_rxd_status ggml_rxd_backend_graph_compute_async(ggml_rxd_backend_t backend, struct ggml_rxd_cgraph * cgraph);

    // NOTE: will be removed, use device version instead
    GGML_RXD_API bool ggml_rxd_backend_supports_op(ggml_rxd_backend_t backend, const struct ggml_rxd_tensor * op);
    GGML_RXD_API bool ggml_rxd_backend_supports_buft(ggml_rxd_backend_t backend, ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API bool ggml_rxd_backend_offload_op(ggml_rxd_backend_t backend, const struct ggml_rxd_tensor * op);

    // asynchronous copy
    // the copy is performed after all the currently queued operations in backend_src
    // backend_dst will wait for the copy to complete before performing other operations
    // automatic fallback to sync copy if async is not supported
    GGML_RXD_API void ggml_rxd_backend_tensor_copy_async(ggml_rxd_backend_t backend_src, ggml_rxd_backend_t backend_dst, struct ggml_rxd_tensor * src, struct ggml_rxd_tensor * dst);

    GGML_RXD_API ggml_rxd_backend_dev_t ggml_rxd_backend_get_device(ggml_rxd_backend_t backend);

    //
    // Events
    //

    GGML_RXD_API ggml_rxd_backend_event_t ggml_rxd_backend_event_new(ggml_rxd_backend_dev_t device);
    GGML_RXD_API void                 ggml_rxd_backend_event_free(ggml_rxd_backend_event_t event);
    GGML_RXD_API void                 ggml_rxd_backend_event_record(ggml_rxd_backend_event_t event, ggml_rxd_backend_t backend);
    GGML_RXD_API void                 ggml_rxd_backend_event_synchronize(ggml_rxd_backend_event_t event);
    GGML_RXD_API void                 ggml_rxd_backend_event_wait(ggml_rxd_backend_t backend, ggml_rxd_backend_event_t event);

    //
    // Backend device
    //

    enum ggml_rxd_backend_dev_type {
        // CPU device using system memory
        GGML_RXD_BACKEND_DEVICE_TYPE_CPU,
        // GPU device using dedicated memory
        GGML_RXD_BACKEND_DEVICE_TYPE_GPU,
        // integrated GPU device using host memory
        GGML_RXD_BACKEND_DEVICE_TYPE_IGPU,
        // accelerator devices intended to be used together with the CPU backend (e.g. BLAS or AMX)
        GGML_RXD_BACKEND_DEVICE_TYPE_ACCEL
    };

    // functionality supported by the device
    struct ggml_rxd_backend_dev_caps {
        // asynchronous operations
        bool async;
        // pinned host buffer
        bool host_buffer;
        // creating buffers from host ptr
        bool buffer_from_host_ptr;
        // event synchronization
        bool events;
    };

    // all the device properties
    struct ggml_rxd_backend_dev_props {
        // device name
        const char * name;
        // device description
        const char * description;
        // device free memory in bytes
        size_t memory_free;
        // device total memory in bytes
        size_t memory_total;
        // device type
        enum ggml_rxd_backend_dev_type type;
        // device id
        //   for PCI devices, this should be the PCI bus id formatted as "domain:bus:device.function" (e.g. "0000:01:00.0")
        //   if the id is unknown, this should be NULL
        const char * device_id;
        // device capabilities
        struct ggml_rxd_backend_dev_caps caps;
    };

    GGML_RXD_API const char *                  ggml_rxd_backend_dev_name(ggml_rxd_backend_dev_t device);
    GGML_RXD_API const char *                  ggml_rxd_backend_dev_description(ggml_rxd_backend_dev_t device);
    GGML_RXD_API void                          ggml_rxd_backend_dev_memory(ggml_rxd_backend_dev_t device, size_t * free, size_t * total);
    GGML_RXD_API enum ggml_rxd_backend_dev_type    ggml_rxd_backend_dev_type(ggml_rxd_backend_dev_t device);
    GGML_RXD_API void                          ggml_rxd_backend_dev_get_props(ggml_rxd_backend_dev_t device, struct ggml_rxd_backend_dev_props * props);
    GGML_RXD_API ggml_rxd_backend_reg_t            ggml_rxd_backend_dev_backend_reg(ggml_rxd_backend_dev_t device);
    GGML_RXD_API ggml_rxd_backend_t                ggml_rxd_backend_dev_init(ggml_rxd_backend_dev_t device, const char * params);
    GGML_RXD_API ggml_rxd_backend_buffer_type_t    ggml_rxd_backend_dev_buffer_type(ggml_rxd_backend_dev_t device);
    GGML_RXD_API ggml_rxd_backend_buffer_type_t    ggml_rxd_backend_dev_host_buffer_type(ggml_rxd_backend_dev_t device);
    GGML_RXD_API ggml_rxd_backend_buffer_t         ggml_rxd_backend_dev_buffer_from_host_ptr(ggml_rxd_backend_dev_t device, void * ptr, size_t size, size_t max_tensor_size);

    GGML_RXD_API bool                          ggml_rxd_backend_dev_supports_op(ggml_rxd_backend_dev_t device, const struct ggml_rxd_tensor * op);
    GGML_RXD_API bool                          ggml_rxd_backend_dev_supports_buft(ggml_rxd_backend_dev_t device, ggml_rxd_backend_buffer_type_t buft);
    GGML_RXD_API bool                          ggml_rxd_backend_dev_offload_op(ggml_rxd_backend_dev_t device, const struct ggml_rxd_tensor * op);

    //
    // Backend (reg)
    //

    GGML_RXD_API const char *       ggml_rxd_backend_reg_name(ggml_rxd_backend_reg_t reg);
    GGML_RXD_API size_t             ggml_rxd_backend_reg_dev_count(ggml_rxd_backend_reg_t reg);
    GGML_RXD_API ggml_rxd_backend_dev_t ggml_rxd_backend_reg_dev_get(ggml_rxd_backend_reg_t reg, size_t index);
    GGML_RXD_API void *             ggml_rxd_backend_reg_get_proc_address(ggml_rxd_backend_reg_t reg, const char * name);

    // Common functions that may be obtained using ggml_rxd_backend_reg_get_proc_address

    // Split buffer type for tensor parallelism
    typedef ggml_rxd_backend_buffer_type_t   (*ggml_rxd_backend_split_buffer_type_t)(int main_device, const float * tensor_split);
    // Set the number of threads for the backend
    typedef void                         (*ggml_rxd_backend_set_n_threads_t)(ggml_rxd_backend_t backend, int n_threads);
    // Get additional buffer types provided by the device (returns a NULL-terminated array)
    typedef ggml_rxd_backend_buffer_type_t * (*ggml_rxd_backend_dev_get_extra_bufts_t)(ggml_rxd_backend_dev_t device);
    // Set the abort callback for the backend
    typedef void                         (*ggml_rxd_backend_set_abort_callback_t)(ggml_rxd_backend_t backend, ggml_rxd_abort_callback abort_callback, void * abort_callback_data);
    // Get a list of feature flags supported by the backend (returns a NULL-terminated array)
    struct ggml_rxd_backend_feature {
        const char * name;
        const char * value;
    };
    typedef struct ggml_rxd_backend_feature * (*ggml_rxd_backend_get_features_t)(ggml_rxd_backend_reg_t reg);

    //
    // Backend registry
    //

    GGML_RXD_API void ggml_rxd_backend_register(ggml_rxd_backend_reg_t reg);

    GGML_RXD_API void ggml_rxd_backend_device_register(ggml_rxd_backend_dev_t device);

    // Backend (reg) enumeration
    GGML_RXD_API size_t             ggml_rxd_backend_reg_count(void);
    GGML_RXD_API ggml_rxd_backend_reg_t ggml_rxd_backend_reg_get(size_t index);
    GGML_RXD_API ggml_rxd_backend_reg_t ggml_rxd_backend_reg_by_name(const char * name);

    // Device enumeration
    GGML_RXD_API size_t             ggml_rxd_backend_dev_count(void);
    GGML_RXD_API ggml_rxd_backend_dev_t ggml_rxd_backend_dev_get(size_t index);
    GGML_RXD_API ggml_rxd_backend_dev_t ggml_rxd_backend_dev_by_name(const char * name);
    GGML_RXD_API ggml_rxd_backend_dev_t ggml_rxd_backend_dev_by_type(enum ggml_rxd_backend_dev_type type);

    // Direct backend (stream) initialization
    // = ggml_rxd_backend_dev_init(ggml_rxd_backend_dev_by_name(name), params)
    GGML_RXD_API ggml_rxd_backend_t ggml_rxd_backend_init_by_name(const char * name, const char * params);
    // = ggml_rxd_backend_dev_init(ggml_rxd_backend_dev_by_type(type), params)
    GGML_RXD_API ggml_rxd_backend_t ggml_rxd_backend_init_by_type(enum ggml_rxd_backend_dev_type type, const char * params);
    // = ggml_rxd_backend_dev_init(ggml_rxd_backend_dev_by_type(GPU) OR ggml_rxd_backend_dev_by_type(CPU), NULL)
    GGML_RXD_API ggml_rxd_backend_t ggml_rxd_backend_init_best(void);

    // Load a backend from a dynamic library and register it
    GGML_RXD_API ggml_rxd_backend_reg_t ggml_rxd_backend_load(const char * path);
    // Unload a backend if loaded dynamically and unregister it
    GGML_RXD_API void               ggml_rxd_backend_unload(ggml_rxd_backend_reg_t reg);
    // Load all known backends from dynamic libraries
    GGML_RXD_API void               ggml_rxd_backend_load_all(void);
    GGML_RXD_API void               ggml_rxd_backend_load_all_from_path(const char * dir_path);

    //
    // Backend scheduler
    //

    // The backend scheduler allows for multiple backend devices to be used together
    // Handles compute buffer allocation, assignment of tensors to backends, and copying of tensors between backends
    // The backends are selected based on:
    // - the backend that supports the operation
    // - the location of the pre-allocated tensors (e.g. the weights)
    /*
      Example usage:

        // operations that use tensors allocated in a buffer with USAGE_WEIGHTS will be assigned
        // preferrably to run on the same backend as the buffer
        ggml_rxd_backend_buffer_set_usage(buf_weights, GGML_RXD_BACKEND_BUFFER_USAGE_WEIGHTS);

        sched = ggml_rxd_backend_sched_new({backend_gpu, backend_gpu2, backend_cpu}, NULL, num_backends, GGML_RXD_DEFAULT_GRAPH_SIZE, false, true);

        // initialize buffers from a max size graph (optional)
        reserve_graph = build_graph(sched, max_batch_size);

        // manually assign nodes to a backend (optional, should not be needed in most cases)
        struct ggml_rxd_tensor * node = ggml_rxd_mul_mat(ctx, ...);
        ggml_rxd_backend_sched_set_tensor_backend(sched, node, backend_gpu);

        ggml_rxd_backend_sched_reserve(sched, reserve_graph);

        // compute
        graph = build_graph(sched); // the graph and its tensors are single-use in terms of allocation, multi-use in terms of computation
        for (int i = 0; i < 10; ++i) {
            ggml_rxd_backend_sched_graph_compute(sched, graph); // on the first iteration the graph is allocated automatically
        }

        // if there are graph inputs:
        graph = build_graph(sched); // get a new graph that is not allocated (the metadata for the old graph is freed once ggml_rxd_free is called)
        ggml_rxd_backend_sched_reset(sched); // clear the allocation of the previous graph
        ggml_rxd_backend_sched_alloc_graph(sched, graph); // explicitly allocate the new graph but do not execute it
        ggml_rxd_backend_tensor_set(input_tensor, ...); // copy data to the newly allocated graph tensors
        ggml_rxd_backend_sched_graph_compute(sched, graph); // execute the graph

        // as an alternative to the above it is also possible to assign the inputs to a dedicated context and
        // allocate them statically via ggml_rxd_backend_alloc_ctx_tensors
    }
    */

    typedef struct ggml_rxd_backend_sched * ggml_rxd_backend_sched_t;

    // Evaluation callback for each node in the graph (set with ggml_rxd_backend_sched_set_eval_callback)
    // when ask == true, the scheduler wants to know if the user wants to observe this node
    // this allows the scheduler to batch nodes together in order to evaluate them in a single call
    //
    // when ask == false, the scheduler is passing the node tensor to the user for observation
    // if the user returns false, the scheduler will cancel the graph compute
    //
    typedef bool (*ggml_rxd_backend_sched_eval_callback)(struct ggml_rxd_tensor * t, bool ask, void * user_data);

    // Initialize a backend scheduler, backends with low index are given priority over backends with high index
    GGML_RXD_API ggml_rxd_backend_sched_t ggml_rxd_backend_sched_new(ggml_rxd_backend_t * backends, ggml_rxd_backend_buffer_type_t * bufts, int n_backends, size_t graph_size, bool parallel, bool op_offload);
    GGML_RXD_API void                 ggml_rxd_backend_sched_free(ggml_rxd_backend_sched_t sched);

    // Initialize backend buffers from a measure graph
    GGML_RXD_API bool                 ggml_rxd_backend_sched_reserve(ggml_rxd_backend_sched_t sched, struct ggml_rxd_cgraph * measure_graph); // returns success

    GGML_RXD_API int                  ggml_rxd_backend_sched_get_n_backends(ggml_rxd_backend_sched_t sched);
    GGML_RXD_API ggml_rxd_backend_t       ggml_rxd_backend_sched_get_backend(ggml_rxd_backend_sched_t sched, int i);

    // Get the number of splits of the last graph
    GGML_RXD_API int                  ggml_rxd_backend_sched_get_n_splits(ggml_rxd_backend_sched_t sched);
    GGML_RXD_API int                  ggml_rxd_backend_sched_get_n_copies(ggml_rxd_backend_sched_t sched);

    GGML_RXD_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_sched_get_buffer_type(ggml_rxd_backend_sched_t sched, ggml_rxd_backend_t backend);
    GGML_RXD_API size_t                     ggml_rxd_backend_sched_get_buffer_size(ggml_rxd_backend_sched_t sched, ggml_rxd_backend_t backend);

    GGML_RXD_API void                 ggml_rxd_backend_sched_set_tensor_backend(ggml_rxd_backend_sched_t sched, struct ggml_rxd_tensor * node, ggml_rxd_backend_t backend);
    GGML_RXD_API ggml_rxd_backend_t       ggml_rxd_backend_sched_get_tensor_backend(ggml_rxd_backend_sched_t sched, struct ggml_rxd_tensor * node);

    // Split graph without allocating it
    GGML_RXD_API void                 ggml_rxd_backend_sched_split_graph(ggml_rxd_backend_sched_t sched, struct ggml_rxd_cgraph * graph);

    // Allocate and compute graph on the backend scheduler
    GGML_RXD_API bool                 ggml_rxd_backend_sched_alloc_graph(ggml_rxd_backend_sched_t sched, struct ggml_rxd_cgraph * graph); // returns success
    GGML_RXD_API enum ggml_rxd_status     ggml_rxd_backend_sched_graph_compute(ggml_rxd_backend_sched_t sched, struct ggml_rxd_cgraph * graph);
    GGML_RXD_API enum ggml_rxd_status     ggml_rxd_backend_sched_graph_compute_async(ggml_rxd_backend_sched_t sched, struct ggml_rxd_cgraph * graph);
    GGML_RXD_API void                 ggml_rxd_backend_sched_synchronize(ggml_rxd_backend_sched_t sched);

    // Reset all assignments and allocators - must be called before changing the node backends or allocating a new graph.
    // This in effect deallocates all tensors that were previously allocated and leaves them with dangling pointers.
    // The correct way to use this API is to discard the deallocated tensors and create new ones.
    GGML_RXD_API void                 ggml_rxd_backend_sched_reset(ggml_rxd_backend_sched_t sched);

    // Set a callback to be called for each resulting node during graph compute
    GGML_RXD_API void                 ggml_rxd_backend_sched_set_eval_callback(ggml_rxd_backend_sched_t sched, ggml_rxd_backend_sched_eval_callback callback, void * user_data);

    //
    // Utils
    //

    struct ggml_rxd_backend_graph_copy {
        ggml_rxd_backend_buffer_t buffer;
        struct ggml_rxd_context * ctx_allocated;
        struct ggml_rxd_context * ctx_unallocated;
        struct ggml_rxd_cgraph * graph;
    };

    // Copy a graph to a different backend
    GGML_RXD_API struct ggml_rxd_backend_graph_copy ggml_rxd_backend_graph_copy(ggml_rxd_backend_t backend, struct ggml_rxd_cgraph * graph);
    GGML_RXD_API void                           ggml_rxd_backend_graph_copy_free(struct ggml_rxd_backend_graph_copy copy);

    typedef bool (*ggml_rxd_backend_eval_callback)(int node_index, struct ggml_rxd_tensor * t1, struct ggml_rxd_tensor * t2, void * user_data);

    // Compare the output of two backends
    GGML_RXD_API bool ggml_rxd_backend_compare_graph_backend(ggml_rxd_backend_t backend1, ggml_rxd_backend_t backend2, struct ggml_rxd_cgraph * graph, ggml_rxd_backend_eval_callback callback, void * user_data, struct ggml_rxd_tensor * test_node);

    // Tensor initialization
    GGML_RXD_API enum ggml_rxd_status ggml_rxd_backend_tensor_alloc(ggml_rxd_backend_buffer_t buffer, struct ggml_rxd_tensor * tensor, void * addr);
    GGML_RXD_API enum ggml_rxd_status ggml_rxd_backend_view_init(struct ggml_rxd_tensor * tensor);

    // CPU buffer types are always available
    GGML_RXD_API ggml_rxd_backend_buffer_t      ggml_rxd_backend_cpu_buffer_from_ptr(void * ptr, size_t size);
    GGML_RXD_API ggml_rxd_backend_buffer_type_t ggml_rxd_backend_cpu_buffer_type(void);

#ifdef  __cplusplus
}
#endif
