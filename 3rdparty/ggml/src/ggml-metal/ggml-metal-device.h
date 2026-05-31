#pragma once

#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ggml_rxd_metal_buffer_id {
    void * metal; // id<MTLBuffer>
    size_t offs;
};

typedef struct ggml_rxd_metal_device * ggml_rxd_metal_device_t;

//
// MTLFunctionConstantValues wrapper
//

typedef struct ggml_rxd_metal_cv * ggml_rxd_metal_cv_t;

ggml_rxd_metal_cv_t ggml_rxd_metal_cv_init(void);
void ggml_rxd_metal_cv_free(ggml_rxd_metal_cv_t cv);

void ggml_rxd_metal_cv_set_int16(ggml_rxd_metal_cv_t cv, int16_t value, int32_t idx);
void ggml_rxd_metal_cv_set_int32(ggml_rxd_metal_cv_t cv, int32_t value, int32_t idx);
void ggml_rxd_metal_cv_set_bool (ggml_rxd_metal_cv_t cv, bool    value, int32_t idx);

//
// MTLComputePipelineState wrapper
//

typedef struct ggml_rxd_metal_pipeline * ggml_rxd_metal_pipeline_t;

ggml_rxd_metal_pipeline_t ggml_rxd_metal_pipeline_init(void);
void ggml_rxd_metal_pipeline_free(ggml_rxd_metal_pipeline_t pipeline);

void ggml_rxd_metal_pipeline_set_nsg(ggml_rxd_metal_pipeline_t pipeline, int nsg);
int  ggml_rxd_metal_pipeline_get_nsg(ggml_rxd_metal_pipeline_t pipeline);

void ggml_rxd_metal_pipeline_set_nr0(ggml_rxd_metal_pipeline_t pipeline, int nr0);
int  ggml_rxd_metal_pipeline_get_nr0(ggml_rxd_metal_pipeline_t pipeline);

void ggml_rxd_metal_pipeline_set_nr1(ggml_rxd_metal_pipeline_t pipeline, int nr1);
int  ggml_rxd_metal_pipeline_get_nr1(ggml_rxd_metal_pipeline_t pipeline);

void   ggml_rxd_metal_pipeline_set_smem(ggml_rxd_metal_pipeline_t pipeline, size_t smem);
size_t ggml_rxd_metal_pipeline_get_smem(ggml_rxd_metal_pipeline_t pipeline);

int ggml_rxd_metal_pipeline_max_theads_per_threadgroup(ggml_rxd_metal_pipeline_t pipeline);

// a collection of pipelines
typedef struct ggml_rxd_metal_pipelines * ggml_rxd_metal_pipelines_t;

ggml_rxd_metal_pipelines_t ggml_rxd_metal_pipelines_init(void);
void ggml_rxd_metal_pipelines_free(ggml_rxd_metal_pipelines_t ppls);

void                  ggml_rxd_metal_pipelines_add(ggml_rxd_metal_pipelines_t ppls, const char * name, ggml_rxd_metal_pipeline_t pipeline);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_pipelines_get(ggml_rxd_metal_pipelines_t ppls, const char * name);

//
// MTLCommandBuffer wrapper
//

typedef void * ggml_rxd_metal_cmd_buf_t;

//
// MTLComputeCommandEncoder wrapper
//

typedef struct ggml_rxd_metal_encoder * ggml_rxd_metal_encoder_t;

ggml_rxd_metal_encoder_t ggml_rxd_metal_encoder_init(ggml_rxd_metal_cmd_buf_t cmd_buf_raw, bool concurrent);
void ggml_rxd_metal_encoder_free(ggml_rxd_metal_encoder_t encoder);

void ggml_rxd_metal_encoder_debug_group_push(ggml_rxd_metal_encoder_t encoder, const char * name);
void ggml_rxd_metal_encoder_debug_group_pop (ggml_rxd_metal_encoder_t encoder);

void ggml_rxd_metal_encoder_set_pipeline(ggml_rxd_metal_encoder_t encoder, ggml_rxd_metal_pipeline_t pipeline);

void ggml_rxd_metal_encoder_set_bytes (ggml_rxd_metal_encoder_t encoder, void * data, size_t size, int idx);
void ggml_rxd_metal_encoder_set_buffer(ggml_rxd_metal_encoder_t encoder, struct ggml_rxd_metal_buffer_id buffer, int idx);

void ggml_rxd_metal_encoder_set_threadgroup_memory_size(ggml_rxd_metal_encoder_t encoder, size_t size, int idx);

void ggml_rxd_metal_encoder_dispatch_threadgroups(ggml_rxd_metal_encoder_t encoder, int tg0, int tg1, int tg2, int tptg0, int tptg1, int tptg2);

void ggml_rxd_metal_encoder_memory_barrier(ggml_rxd_metal_encoder_t encoder);

void ggml_rxd_metal_encoder_end_encoding(ggml_rxd_metal_encoder_t encoder);

//
// MTLLibrary wrapper
//

typedef struct ggml_rxd_metal_library * ggml_rxd_metal_library_t;

ggml_rxd_metal_library_t ggml_rxd_metal_library_init            (ggml_rxd_metal_device_t dev);
ggml_rxd_metal_library_t ggml_rxd_metal_library_init_from_source(ggml_rxd_metal_device_t dev, const char * source, bool verbose);

void ggml_rxd_metal_library_free(ggml_rxd_metal_library_t lib);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline    (ggml_rxd_metal_library_t lib, const char * name);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_compile_pipeline(ggml_rxd_metal_library_t lib, const char * base, const char * name, ggml_rxd_metal_cv_t cv);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_base              (ggml_rxd_metal_library_t lib, enum ggml_rxd_op op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_cpy               (ggml_rxd_metal_library_t lib, enum ggml_rxd_type tsrc, enum ggml_rxd_type tdst);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_pool_2d           (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op, enum ggml_rxd_op_pool op_pool);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_get_rows          (ggml_rxd_metal_library_t lib, enum ggml_rxd_type tsrc);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_set_rows          (ggml_rxd_metal_library_t lib, enum ggml_rxd_type tidx, enum ggml_rxd_type tdst);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_repeat            (ggml_rxd_metal_library_t lib, enum ggml_rxd_type tsrc);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_unary             (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_glu               (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_sum               (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_sum_rows          (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_cumsum_blk        (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_cumsum_add        (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_soft_max          (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_ssm_conv          (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_ssm_scan          (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_rwkv              (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mv_ext        (ggml_rxd_metal_library_t lib, enum ggml_rxd_type tsrc0, enum ggml_rxd_type tsrc1, int nsg, int nxpsg, int r1ptg);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mm            (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mv            (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mm_id_map0    (ggml_rxd_metal_library_t lib, int ne02, int ne20);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mm_id         (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_mul_mv_id         (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_argmax            (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_argsort           (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_argsort_merge     (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_bin               (ggml_rxd_metal_library_t lib, enum ggml_rxd_op op, int32_t n_fuse, bool row);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_l2_norm           (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_group_norm        (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_norm              (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op, int32_t n_fuse);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_rope              (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_im2col            (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_conv_transpose_1d (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_conv_transpose_2d (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_conv_2d           (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_upscale           (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_pad               (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_pad_reflect_1d    (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_arange            (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_timestep_embedding(ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_opt_step_adamw    (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);
ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_opt_step_sgd      (ggml_rxd_metal_library_t lib, const struct ggml_rxd_tensor * op);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_flash_attn_ext_pad(
        ggml_rxd_metal_library_t lib,
        const struct ggml_rxd_tensor * op,
        bool    has_mask,
        int32_t ncpsg);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_flash_attn_ext_blk(
        ggml_rxd_metal_library_t lib,
        const struct ggml_rxd_tensor * op,
        int32_t nqptg,
        int32_t ncpsg);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_flash_attn_ext(
        ggml_rxd_metal_library_t lib,
        const struct ggml_rxd_tensor * op,
        bool    has_mask,
        bool    has_sinks,
        bool    has_bias,
        bool    has_scap,
        bool    has_kvpad,
        int32_t nsg);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_flash_attn_ext_vec(
        ggml_rxd_metal_library_t lib,
        const struct ggml_rxd_tensor * op,
        bool    has_mask,
        bool    has_sinks,
        bool    has_bias,
        bool    has_scap,
        bool    has_kvpad,
        int32_t nsg,
        int32_t nwg);

ggml_rxd_metal_pipeline_t ggml_rxd_metal_library_get_pipeline_flash_attn_ext_vec_reduce(
        ggml_rxd_metal_library_t lib,
        const struct ggml_rxd_tensor * op,
        int32_t dv,
        int32_t nwg);

//
// device
//

struct ggml_rxd_metal_device_props {
    char name[128];

    size_t max_buffer_size;
    size_t max_working_set_size;
    size_t max_theadgroup_memory_size;

    bool has_simdgroup_reduction;
    bool has_simdgroup_mm;
    bool has_unified_memory;
    bool has_bfloat;
    bool has_tensor;
    bool use_residency_sets;
    bool use_shared_buffers;

    bool supports_gpu_family_apple7;
};

ggml_rxd_metal_device_t ggml_rxd_metal_device_init(void);
void ggml_rxd_metal_device_free(ggml_rxd_metal_device_t dev);

// return a singleton that is automatically destroyed when the program exits
ggml_rxd_metal_device_t ggml_rxd_metal_device_get(void);

void * ggml_rxd_metal_device_get_obj  (ggml_rxd_metal_device_t dev); // id<MTLDevice>
void * ggml_rxd_metal_device_get_queue(ggml_rxd_metal_device_t dev); // id<MTLCommandQueue>

ggml_rxd_metal_library_t ggml_rxd_metal_device_get_library(ggml_rxd_metal_device_t dev);

void ggml_rxd_metal_device_get_memory(ggml_rxd_metal_device_t dev, size_t * free, size_t * total);
bool ggml_rxd_metal_device_supports_op(ggml_rxd_metal_device_t dev, const struct ggml_rxd_tensor * op);

const struct ggml_rxd_metal_device_props * ggml_rxd_metal_device_get_props(ggml_rxd_metal_device_t dev);

//
// device buffers
//

typedef struct ggml_rxd_metal_buffer * ggml_rxd_metal_buffer_t;

ggml_rxd_metal_buffer_t ggml_rxd_metal_buffer_init(ggml_rxd_metal_device_t dev, size_t size, bool shared);
ggml_rxd_metal_buffer_t ggml_rxd_metal_buffer_map (ggml_rxd_metal_device_t dev, void * ptr, size_t size, size_t max_tensor_size);

void   ggml_rxd_metal_buffer_free     (ggml_rxd_metal_buffer_t buf);
void * ggml_rxd_metal_buffer_get_base (ggml_rxd_metal_buffer_t buf);
bool   ggml_rxd_metal_buffer_is_shared(ggml_rxd_metal_buffer_t buf);

void   ggml_rxd_metal_buffer_memset_tensor(ggml_rxd_metal_buffer_t buf, struct ggml_rxd_tensor * tensor, uint8_t value, size_t offset, size_t size);
void   ggml_rxd_metal_buffer_set_tensor   (ggml_rxd_metal_buffer_t buf, struct ggml_rxd_tensor * tensor, const void * data, size_t offset, size_t size);
void   ggml_rxd_metal_buffer_get_tensor   (ggml_rxd_metal_buffer_t buf, const struct ggml_rxd_tensor * tensor, void * data, size_t offset, size_t size);
void   ggml_rxd_metal_buffer_clear        (ggml_rxd_metal_buffer_t buf, uint8_t value);

// finds the Metal buffer that contains the tensor data on the GPU device
// the assumption is that there is 1-to-1 mapping between the host and device memory buffers, so we can find the
// Metal buffer based on the host memory pointer
//
struct ggml_rxd_metal_buffer_id ggml_rxd_metal_buffer_get_id(ggml_rxd_metal_buffer_t buf, const struct ggml_rxd_tensor * t);

#ifdef __cplusplus
}
#endif
