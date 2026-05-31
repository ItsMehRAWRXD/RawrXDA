#pragma once

#include "ggml.h"
#include "ggml-backend.h"

#ifdef  __cplusplus
extern "C" {
#endif

    // the compute plan that needs to be prepared for ggml_rxd_graph_compute()
    // since https://github.com/ggml-org/ggml/issues/287
    struct ggml_rxd_cplan {
        size_t    work_size; // size of work buffer, calculated by `ggml_rxd_graph_plan()`
        uint8_t * work_data; // work buffer, to be allocated by caller before calling to `ggml_rxd_graph_compute()`

        int n_threads;
        struct ggml_rxd_threadpool * threadpool;

        // abort ggml_rxd_graph_compute when true
        ggml_rxd_abort_callback abort_callback;
        void *              abort_callback_data;
    };

    // numa strategies
    enum ggml_rxd_numa_strategy {
        GGML_RXD_NUMA_STRATEGY_DISABLED   = 0,
        GGML_RXD_NUMA_STRATEGY_DISTRIBUTE = 1,
        GGML_RXD_NUMA_STRATEGY_ISOLATE    = 2,
        GGML_RXD_NUMA_STRATEGY_NUMACTL    = 3,
        GGML_RXD_NUMA_STRATEGY_MIRROR     = 4,
        GGML_RXD_NUMA_STRATEGY_COUNT
    };

    GGML_RXD_BACKEND_API void    ggml_rxd_numa_init(enum ggml_rxd_numa_strategy numa); // call once for better performance on NUMA systems
    GGML_RXD_BACKEND_API bool    ggml_rxd_is_numa(void); // true if init detected that system has >1 NUMA node

    GGML_RXD_BACKEND_API struct ggml_rxd_tensor * ggml_rxd_new_i32(struct ggml_rxd_context * ctx, int32_t value);
    GGML_RXD_BACKEND_API struct ggml_rxd_tensor * ggml_rxd_new_f32(struct ggml_rxd_context * ctx, float value);

    GGML_RXD_BACKEND_API struct ggml_rxd_tensor * ggml_rxd_set_i32 (struct ggml_rxd_tensor * tensor, int32_t value);
    GGML_RXD_BACKEND_API struct ggml_rxd_tensor * ggml_rxd_set_f32 (struct ggml_rxd_tensor * tensor, float value);

    GGML_RXD_BACKEND_API int32_t ggml_rxd_get_i32_1d(const struct ggml_rxd_tensor * tensor, int i);
    GGML_RXD_BACKEND_API void    ggml_rxd_set_i32_1d(const struct ggml_rxd_tensor * tensor, int i, int32_t value);

    GGML_RXD_BACKEND_API int32_t ggml_rxd_get_i32_nd(const struct ggml_rxd_tensor * tensor, int i0, int i1, int i2, int i3);
    GGML_RXD_BACKEND_API void    ggml_rxd_set_i32_nd(const struct ggml_rxd_tensor * tensor, int i0, int i1, int i2, int i3, int32_t value);

    GGML_RXD_BACKEND_API float   ggml_rxd_get_f32_1d(const struct ggml_rxd_tensor * tensor, int i);
    GGML_RXD_BACKEND_API void    ggml_rxd_set_f32_1d(const struct ggml_rxd_tensor * tensor, int i, float value);

    GGML_RXD_BACKEND_API float   ggml_rxd_get_f32_nd(const struct ggml_rxd_tensor * tensor, int i0, int i1, int i2, int i3);
    GGML_RXD_BACKEND_API void    ggml_rxd_set_f32_nd(const struct ggml_rxd_tensor * tensor, int i0, int i1, int i2, int i3, float value);

    GGML_RXD_BACKEND_API struct ggml_rxd_threadpool *      ggml_rxd_threadpool_new           (struct ggml_rxd_threadpool_params  * params);
    GGML_RXD_BACKEND_API void                          ggml_rxd_threadpool_free          (struct ggml_rxd_threadpool * threadpool);
    GGML_RXD_BACKEND_API int                           ggml_rxd_threadpool_get_n_threads (struct ggml_rxd_threadpool * threadpool);
    GGML_RXD_BACKEND_API void                          ggml_rxd_threadpool_pause         (struct ggml_rxd_threadpool * threadpool);
    GGML_RXD_BACKEND_API void                          ggml_rxd_threadpool_resume        (struct ggml_rxd_threadpool * threadpool);

    // ggml_rxd_graph_plan() has to be called before ggml_rxd_graph_compute()
    // when plan.work_size > 0, caller must allocate memory for plan.work_data
    GGML_RXD_BACKEND_API struct ggml_rxd_cplan ggml_rxd_graph_plan(
                  const struct ggml_rxd_cgraph * cgraph,
                                       int   n_threads, /* = GGML_RXD_DEFAULT_N_THREADS */
                    struct ggml_rxd_threadpool * threadpool /* = NULL */ );
    GGML_RXD_BACKEND_API enum ggml_rxd_status  ggml_rxd_graph_compute(struct ggml_rxd_cgraph * cgraph, struct ggml_rxd_cplan * cplan);

    // same as ggml_rxd_graph_compute() but the work data is allocated as a part of the context
    // note: the drawback of this API is that you must have ensured that the context has enough memory for the work data
    GGML_RXD_BACKEND_API enum ggml_rxd_status  ggml_rxd_graph_compute_with_ctx(struct ggml_rxd_context * ctx, struct ggml_rxd_cgraph * cgraph, int n_threads);

    //
    // system info
    //

    // x86
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_sse3       (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_ssse3      (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx        (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx_vnni   (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx2       (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_bmi2       (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_f16c       (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_fma        (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx512     (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx512_vbmi(void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx512_vnni(void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_avx512_bf16(void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_amx_int8   (void);
    // ARM
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_neon       (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_arm_fma    (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_fp16_va    (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_dotprod    (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_matmul_int8(void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_sve        (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_get_sve_cnt    (void);  // sve vector length in bytes
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_sme        (void);
    // other
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_riscv_v    (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_vsx        (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_vxe        (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_wasm_simd  (void);
    GGML_RXD_BACKEND_API int ggml_rxd_cpu_has_llamafile  (void);

    // Internal types and functions exposed for tests and benchmarks

    typedef void (*ggml_rxd_vec_dot_t)  (int n, float * GGML_RXD_RESTRICT s, size_t bs, const void * GGML_RXD_RESTRICT x, size_t bx,
                                       const void * GGML_RXD_RESTRICT y, size_t by, int nrc);

    struct ggml_rxd_type_traits_cpu {
        ggml_rxd_from_float_t        from_float;
        ggml_rxd_vec_dot_t           vec_dot;
        enum ggml_rxd_type           vec_dot_type;
        int64_t                  nrows; // number of rows to process simultaneously
    };

    GGML_RXD_BACKEND_API const struct ggml_rxd_type_traits_cpu * ggml_rxd_get_type_traits_cpu(enum ggml_rxd_type type);

    GGML_RXD_BACKEND_API void ggml_rxd_cpu_init(void);

    //
    // CPU backend
    //

    GGML_RXD_BACKEND_API ggml_rxd_backend_t ggml_rxd_backend_cpu_init(void);

    GGML_RXD_BACKEND_API bool ggml_rxd_backend_is_cpu                (ggml_rxd_backend_t backend);
    GGML_RXD_BACKEND_API void ggml_rxd_backend_cpu_set_n_threads     (ggml_rxd_backend_t backend_cpu, int n_threads);
    GGML_RXD_BACKEND_API void ggml_rxd_backend_cpu_set_threadpool    (ggml_rxd_backend_t backend_cpu, ggml_rxd_threadpool_t threadpool);
    GGML_RXD_BACKEND_API void ggml_rxd_backend_cpu_set_abort_callback(ggml_rxd_backend_t backend_cpu, ggml_rxd_abort_callback abort_callback, void * abort_callback_data);

    GGML_RXD_BACKEND_API ggml_rxd_backend_reg_t ggml_rxd_backend_cpu_reg(void);

    GGML_RXD_BACKEND_API void ggml_rxd_cpu_fp32_to_fp32(const float *,       float *, int64_t);
    GGML_RXD_BACKEND_API void ggml_rxd_cpu_fp32_to_i32 (const float *,     int32_t *, int64_t);
    GGML_RXD_BACKEND_API void ggml_rxd_cpu_fp32_to_fp16(const float *, ggml_rxd_fp16_t *, int64_t);
    GGML_RXD_BACKEND_API void ggml_rxd_cpu_fp16_to_fp32(const ggml_rxd_fp16_t *, float *, int64_t);
    GGML_RXD_BACKEND_API void ggml_rxd_cpu_fp32_to_bf16(const float *, ggml_rxd_bf16_t *, int64_t);
    GGML_RXD_BACKEND_API void ggml_rxd_cpu_bf16_to_fp32(const ggml_rxd_bf16_t *, float *, int64_t);

#ifdef __cplusplus
}
#endif
