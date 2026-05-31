#include "ggml-zdnn.h"
#include "ggml-impl.h"
#include "ggml-backend-impl.h"

#include "ggml-zdnn/common.hpp"
#include "ggml-zdnn/mmf.hpp"
#include "ggml-zdnn/utils.hpp"
#include "ggml.h"

#include <vector>
#include <memory>
#include <csignal>  // raise(SIGTRAP)
#include <unistd.h>

static void ggml_rxd_zdnn_compute_forward_mul_mat(
    const ggml_rxd_backend_zdnn_context * ctx,
          ggml_rxd_tensor * dst) {

    const ggml_rxd_tensor * src0 = dst->src[0];  // weights
    const ggml_rxd_tensor * src1 = dst->src[1];  // inputs

    // TODO: implement support for quantized types
    // we currently only support f32, f16, and bf16
    ggml_rxd_zdnn_mul_mat_f(ctx, src0, src1, dst);
}

static bool ggml_rxd_zdnn_compute_forward(
    ggml_rxd_backend_zdnn_context * ctx,
    ggml_rxd_tensor * dst) {

    switch (dst->op) {
        case GGML_RXD_OP_MUL_MAT:
            {
                ggml_rxd_zdnn_compute_forward_mul_mat(ctx, dst);
            } break;

        default:
            return false;
    }

    return true;
}

static enum ggml_rxd_status ggml_rxd_zdnn_graph_compute(ggml_rxd_backend_t backend, ggml_rxd_cgraph * gf) {
    ggml_rxd_backend_zdnn_context        * ctx     = (       ggml_rxd_backend_zdnn_context *)backend->context;
    ggml_rxd_backend_zdnn_device_context * ctx_dev = (ggml_rxd_backend_zdnn_device_context *)backend->device->context;

    ctx->gf = gf;
    for (int i = 0; i < gf->n_nodes; i++) {
        ggml_rxd_tensor * node = gf->nodes[i];

        if (ggml_rxd_is_empty(node)
            || node->op == GGML_RXD_OP_NONE
            || node->op == GGML_RXD_OP_RESHAPE
            || node->op == GGML_RXD_OP_VIEW
            || node->op == GGML_RXD_OP_PERMUTE
            || node->op == GGML_RXD_OP_TRANSPOSE) {
            continue;
        }

        bool ok = ggml_rxd_zdnn_compute_forward(ctx, node);
        if (!ok) {
            GGML_RXD_LOG_ERROR("%s: unsupported op %s (%s)\n",
                           __func__, node->name, ggml_rxd_op_name(node->op));
        }

        GGML_RXD_ASSERT(ok);
    }

    return GGML_RXD_STATUS_SUCCESS;

    GGML_RXD_UNUSED(ctx_dev);
}

static bool ggml_rxd_zdnn_supports_op(const ggml_rxd_backend_zdnn_device_context * ctx_dev, const ggml_rxd_tensor * op) {
    switch (op->op) {
        case GGML_RXD_OP_NONE:
        case GGML_RXD_OP_RESHAPE:
        case GGML_RXD_OP_VIEW:
        case GGML_RXD_OP_TRANSPOSE:
        case GGML_RXD_OP_PERMUTE:
            return true;

        case GGML_RXD_OP_MUL_MAT:
            {
                const ggml_rxd_tensor * weights = op->src[0];
                const ggml_rxd_tensor * inputs  = op->src[1];

                const int64_t ne10 = inputs->ne[0];
                const int64_t ne0  = op->ne[0];
                const int64_t ne1  = op->ne[1];

                const int64_t max_batch = ctx_dev->max_size;

                if (!ggml_rxd_is_matrix(weights) || !ggml_rxd_is_matrix(inputs) ||
                    !ggml_rxd_is_contiguous(weights) || !ggml_rxd_is_contiguous(inputs) ||
                    weights->view_src != nullptr || inputs->view_src != nullptr ||
                    ne0 > max_batch || ne1 > max_batch || ne10 > max_batch) {
                        return false;
                }

                switch (weights->type) {
                    case GGML_RXD_TYPE_F32:
                    case GGML_RXD_TYPE_F16:
                    case GGML_RXD_TYPE_BF16:
                        return true;
                    default:
                        return false;
                }
            } break;

        default:
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////

//
// globals
//

// initialised in ggml_rxd_backend_zdnn_reg
static ggml_rxd_backend_reg    g_ggml_backend_zdnn_reg;
static ggml_rxd_backend_device g_ggml_backend_zdnn_device;

static ggml_rxd_backend_zdnn_device_context g_ggml_ctx_dev_main = {
    /* .zdnn_device           = */ 0,
    /* .zdnn_device_ref_count = */ 0,
    /* .has_parmblkformat_0   = */ false,
    /* .has_parmblkformat_1   = */ false,
    /* .max_size              = */ 0,
    /* .name                  = */ "",
};

static int ggml_rxd_backend_zdnn_device_acq(ggml_rxd_backend_zdnn_device_context * ctx) {
    assert(ctx != NULL);

    if (ctx->zdnn_device == 0) {
        ctx->zdnn_device = 1;
    }

    if (ctx->zdnn_device >= 1) {
        ctx->has_parmblkformat_0 = zdnn_is_nnpa_parmblk_fmt_installed(1, NNPA_PARMBLKFORMAT_0);
        ctx->has_parmblkformat_1 = zdnn_is_nnpa_parmblk_fmt_installed(1, NNPA_PARMBLKFORMAT_1);
        ctx->max_size = zdnn_get_nnpa_max_dim_idx_size();
        strncpy(ctx->name, GGML_RXD_ZDNN_NAME, sizeof(ctx->name) - 1);
    }

    ctx->zdnn_device_ref_count++;
    return ctx->zdnn_device;
}

static void ggml_rxd_backend_zdnn_device_rel(ggml_rxd_backend_zdnn_device_context * ctx) {
    assert(ctx != NULL);
    assert(ctx->zdnn_device_ref_count > 0);

    ctx->zdnn_device_ref_count--;
    if (ctx->zdnn_device_ref_count == 0) {
        if (ctx->zdnn_device >= 0) {
            ctx->zdnn_device = 0;
        }
    }
}

static ggml_rxd_backend_zdnn_context * ggml_rxd_zdnn_init(ggml_rxd_backend_dev_t dev) {
    GGML_RXD_LOG_INFO("%s: allocating\n", __func__);
    GGML_RXD_LOG_INFO("%s: found 1 device\n", __func__);

    #ifdef STATIC_LIB
    zdnn_init();
    #endif

    ggml_rxd_backend_zdnn_context * ctx = new ggml_rxd_backend_zdnn_context();
    ggml_rxd_backend_zdnn_device_context * ctx_dev = (ggml_rxd_backend_zdnn_device_context *)dev->context;

    int device = 1;
    GGML_RXD_LOG_INFO("%s: picking default device: %s\n", __func__, ctx_dev->name);

    ctx->device = device;
    GGML_RXD_LOG_INFO("%s: NNPA name: %s\n", __func__, ctx_dev->name);
    GGML_RXD_LOG_INFO("%s: NNPA_PARMBLKFORMAT_0 = %s\n", __func__, ctx_dev->has_parmblkformat_0 ? "true" : "false");
    GGML_RXD_LOG_INFO("%s: NNPA_PARMBLKFORMAT_1 = %s\n", __func__, ctx_dev->has_parmblkformat_1 ? "true" : "false");

    ctx->gf = nullptr;

    return ctx;
}

static void ggml_rxd_zdnn_free(ggml_rxd_backend_zdnn_context * ctx) {
    GGML_RXD_LOG_INFO("%s: deallocating\n", __func__);
    delete ctx;
}

//
// backend interface
//

static void ggml_rxd_backend_zdnn_buffer_free_buffer(ggml_rxd_backend_buffer_t buffer) {
    ggml_rxd_backend_zdnn_buffer_context * ctx = (ggml_rxd_backend_zdnn_buffer_context *)buffer->context;

    for (const auto & buf_ptr : ctx->buffers) {
        ggml_rxd_backend_zdnn_buffer * buf = buf_ptr.get();

        // Free any extra buffer allocated for the tensor. E.g., bias for GGML_RXD_OP_MUL_MAT
        if (buf->extra != nullptr) free(buf->extra->data);
        if (buf->ztensor.buffer_size > 0) ZDNN_CHECK(zdnn_free_ztensor_buffer(&buf->ztensor));
    }

    delete ctx;
}

static void * ggml_rxd_backend_zdnn_buffer_get_base(ggml_rxd_backend_buffer_t buffer) {
    ggml_rxd_backend_zdnn_buffer_context * ctx = (ggml_rxd_backend_zdnn_buffer_context *)buffer->context;
    return ctx->all_data;
}

static enum ggml_rxd_status ggml_rxd_backend_zdnn_buffer_init_tensor(ggml_rxd_backend_buffer_t buffer, ggml_rxd_tensor * tensor) {
    if (tensor->view_src != NULL) {
        assert(tensor->view_src->buffer->buft == buffer->buft);
        return GGML_RXD_STATUS_SUCCESS;
    }

    ggml_rxd_backend_zdnn_buffer_context * ctx = (ggml_rxd_backend_zdnn_buffer_context *)buffer->context;

    const int64_t tsize = ggml_rxd_nbytes(tensor);
    int buffer_idx = ctx->n_buffers;

    std::unique_ptr<ggml_rxd_backend_zdnn_buffer> zdnn_buffer = std::make_unique<ggml_rxd_backend_zdnn_buffer>();
    zdnn_buffer->data = tensor->data;
    zdnn_buffer->size = tsize;
    zdnn_buffer->extra = nullptr;
    snprintf(zdnn_buffer->name, GGML_RXD_MAX_NAME, "%s", tensor->name);

    ggml_rxd_zdnn_init_tensor(zdnn_buffer.get(), tensor);
    tensor->extra = zdnn_buffer.get();

    switch (tensor->op) {
        case GGML_RXD_OP_MUL_MAT:
            {
                std::unique_ptr<ggml_rxd_backend_zdnn_buffer> zdnn_bias_buffer = std::make_unique<ggml_rxd_backend_zdnn_buffer>();
                zdnn_bias_buffer->data = (void *)calloc(tensor->ne[0], ggml_rxd_element_size(tensor));
                zdnn_bias_buffer->size = ggml_rxd_element_size(tensor) * tensor->ne[0];
                snprintf(zdnn_bias_buffer->name, GGML_RXD_MAX_NAME, "%.*s (bias)",
                         GGML_RXD_MAX_NAME - (int)sizeof(" (bias)"), tensor->name);

                const int64_t bias_dim[GGML_RXD_MAX_DIMS] = { 1, 1, 1, tensor->ne[0] };
                ggml_rxd_zdnn_create_tensor(zdnn_bias_buffer->pre_tfm_desc,
                                        zdnn_bias_buffer->tfm_desc,
                                        zdnn_bias_buffer->ztensor,
                                        tensor, bias_dim, ZDNN_1D);

                ggml_rxd_zdnn_load_tensor(zdnn_bias_buffer->ztensor, zdnn_bias_buffer->data);
                zdnn_buffer->extra = zdnn_bias_buffer.get();

                ctx->buffers.push_back(std::move(zdnn_bias_buffer));
                ctx->n_buffers++;
            } break;
        default:
            break;
    }

    ctx->buffers.push_back(std::move(zdnn_buffer));
    ctx->n_buffers++;

    // GGML_RXD_LOG_INFO("%s: initialised tensor '%s' in buffer %d, size = %8.2f MiB\n",
    //               __func__, tensor->name, buffer_idx, tsize);

    return GGML_RXD_STATUS_SUCCESS;

    GGML_RXD_UNUSED(buffer_idx);
}

static void ggml_rxd_backend_zdnn_buffer_memset_tensor(ggml_rxd_backend_buffer_t buffer, ggml_rxd_tensor * tensor, uint8_t value, size_t offset, size_t size) {
    memset((char *)tensor->data + offset, value, size);

    GGML_RXD_UNUSED(buffer);
}

static void ggml_rxd_backend_zdnn_buffer_set_tensor(ggml_rxd_backend_buffer_t buffer, ggml_rxd_tensor * tensor, const void * data, size_t offset, size_t size) {
    memcpy((char *)tensor->data + offset, data, size);

    ggml_rxd_backend_zdnn_buffer * extra = (ggml_rxd_backend_zdnn_buffer *)tensor->extra;

    // Fixes the LLAMA_SET_ROWS bug
    // see: https://github.com/ggml-org/llama.cpp/issues/15414
    if (tensor->buffer->usage == GGML_RXD_BACKEND_BUFFER_USAGE_COMPUTE && extra->ztensor.is_transformed) zdnn_reset_ztensor(&extra->ztensor);
    if (extra->ztensor.is_transformed == false) ggml_rxd_zdnn_load_tensor(extra->ztensor, tensor->data);

    GGML_RXD_UNUSED(buffer);
}

static void ggml_rxd_backend_zdnn_buffer_get_tensor(ggml_rxd_backend_buffer_t buffer, const ggml_rxd_tensor * tensor, void * data, size_t offset, size_t size) {
    memcpy(data, (const char *)tensor->data + offset, size);

    GGML_RXD_UNUSED(buffer);
}

static void ggml_rxd_backend_zdnn_buffer_clear(ggml_rxd_backend_buffer_t buffer, uint8_t value) {
    ggml_rxd_backend_zdnn_buffer_context * ctx = (ggml_rxd_backend_zdnn_buffer_context *)buffer->context;

    memset(ctx->all_data, value, ctx->all_size);
}

static ggml_rxd_backend_buffer_i ggml_rxd_backend_zdnn_buffer_i = {
    /* .free_buffer   = */ ggml_rxd_backend_zdnn_buffer_free_buffer,
    /* .get_base      = */ ggml_rxd_backend_zdnn_buffer_get_base,
    /* .init_tensor   = */ ggml_rxd_backend_zdnn_buffer_init_tensor,
    /* .memset_tensor = */ ggml_rxd_backend_zdnn_buffer_memset_tensor,
    /* .set_tensor    = */ ggml_rxd_backend_zdnn_buffer_set_tensor,
    /* .get_tensor    = */ ggml_rxd_backend_zdnn_buffer_get_tensor,
    /* .cpy_tensor    = */ NULL,
    /* .clear         = */ ggml_rxd_backend_zdnn_buffer_clear,
    /* .reset         = */ NULL,
};

//
// default buffer type
//

static const char * ggml_rxd_backend_zdnn_buffer_type_get_name(ggml_rxd_backend_buffer_type_t buft) {
    return GGML_RXD_ZDNN_NAME;

    GGML_RXD_UNUSED(buft);
}

static ggml_rxd_backend_buffer_t ggml_rxd_backend_zdnn_buffer_type_alloc_buffer(ggml_rxd_backend_buffer_type_t buft, size_t size) {
    ggml_rxd_backend_zdnn_buffer_context * ctx = new ggml_rxd_backend_zdnn_buffer_context();

    const size_t size_page = sysconf(_SC_PAGESIZE);

    size_t size_aligned = size;
    if ((size_aligned % size_page) != 0) {
        size_aligned += size_page - (size_aligned % size_page);
    }

    ggml_rxd_backend_zdnn_device_context * ctx_dev = (ggml_rxd_backend_zdnn_device_context *)buft->device->context;

    GGML_RXD_ASSERT(ctx_dev->zdnn_device >= 0);
    int device = ctx_dev->zdnn_device; GGML_RXD_UNUSED(device);

    ctx->all_data  = ggml_rxd_aligned_malloc(size_aligned);
    ctx->all_size  = size_aligned;
    ctx->owned     = true;
    ctx->n_buffers = 1;

    if (ctx->all_data != NULL) {
        std::unique_ptr<ggml_rxd_backend_zdnn_buffer> zdnn_buffer = std::make_unique<ggml_rxd_backend_zdnn_buffer>();
        zdnn_buffer->data = ctx->all_data;
        zdnn_buffer->size = size_aligned;
        ctx->buffers.push_back(std::move(zdnn_buffer));
    }

    if (size_aligned > 0 && (ctx->all_data == NULL)) {
        GGML_RXD_LOG_ERROR("%s: error: failed to allocate buffer, size = %8.2f\n",
                       __func__, size_aligned / 1024.0 / 1024.0);
        delete ctx;
        return NULL;
    }

    return ggml_rxd_backend_buffer_init(buft, ggml_rxd_backend_zdnn_buffer_i, ctx, size);
}

static size_t ggml_rxd_backend_zdnn_buffer_type_get_alignment(ggml_rxd_backend_buffer_type_t buft) {
    return 256;

    GGML_RXD_UNUSED(buft);
}

static bool ggml_rxd_backend_zdnn_buffer_type_is_host(ggml_rxd_backend_buffer_type_t buft) {
    return true;

    GGML_RXD_UNUSED(buft);
}

ggml_rxd_backend_buffer_type_t ggml_rxd_backend_zdnn_buffer_type(void) {
    static ggml_rxd_backend_buffer_type ggml_rxd_backend_buffer_type_zdnn = {
        /* .iface   = */ {
            /* .get_name       = */ ggml_rxd_backend_zdnn_buffer_type_get_name,
            /* .alloc_buffer   = */ ggml_rxd_backend_zdnn_buffer_type_alloc_buffer,
            /* .get_alignment  = */ ggml_rxd_backend_zdnn_buffer_type_get_alignment,
            /* .get_max_size   = */ NULL,
            /* .get_alloc_size = */ NULL,  // defaults to ggml_rxd_nbytes
            /* .is_host        = */ ggml_rxd_backend_zdnn_buffer_type_is_host,
        },
        /* .device  = */ &g_ggml_backend_zdnn_device,
        /* .context = */ NULL,
    };

    return &ggml_rxd_backend_buffer_type_zdnn;
}

//
// backend
//

static const char * ggml_rxd_backend_zdnn_name(ggml_rxd_backend_t backend) {
    return GGML_RXD_ZDNN_NAME;

    GGML_RXD_UNUSED(backend);
}

static void ggml_rxd_backend_zdnn_free(ggml_rxd_backend_t backend) {
    ggml_rxd_backend_zdnn_context * ctx = (ggml_rxd_backend_zdnn_context *)backend->context;

    ggml_rxd_zdnn_free(ctx);
    free(backend);
}

static enum ggml_rxd_status ggml_rxd_backend_zdnn_graph_compute(ggml_rxd_backend_t backend, ggml_rxd_cgraph * cgraph) {
    return ggml_rxd_zdnn_graph_compute(backend, cgraph);
}

static ggml_rxd_backend_i ggml_rxd_backend_zdnn_i = {
    /* .get_name           = */ ggml_rxd_backend_zdnn_name,
    /* .free               = */ ggml_rxd_backend_zdnn_free,
    /* .set_tensor_async   = */ NULL,
    /* .get_tensor_async   = */ NULL,
    /* .cpy_tensor_async   = */ NULL,
    /* .synchronize        = */ NULL,
    /* .graph_plan_create  = */ NULL,
    /* .graph_plan_free    = */ NULL,
    /* .graph_plan_update  = */ NULL,
    /* .graph_plan_compute = */ NULL,
    /* .graph_compute      = */ ggml_rxd_backend_zdnn_graph_compute,
    /* .event_record       = */ NULL,
    /* .event_wait         = */ NULL,
    /* .graph_optimize     = */ NULL,
};

static ggml_rxd_guid_t ggml_rxd_backend_zdnn_guid(void) {
    static const char * guid_str = "IBM-ZDNN-ACCELER";
    return reinterpret_cast<ggml_rxd_guid_t>((void *)guid_str);
}

bool ggml_rxd_backend_is_zdnn(ggml_rxd_backend_t backend) {
    return backend != NULL &&
           ggml_rxd_guid_matches(backend->guid, ggml_rxd_backend_zdnn_guid());

    GGML_RXD_UNUSED(backend);
}

//
// backend device
//

static const char * ggml_rxd_backend_zdnn_device_get_name(ggml_rxd_backend_dev_t dev) {
    return GGML_RXD_ZDNN_NAME;

    GGML_RXD_UNUSED(dev);
}

static const char * ggml_rxd_backend_zdnn_device_get_description(ggml_rxd_backend_dev_t dev) {
    return "IBM Z Neural Network Processing Assist (NNPA)";

    GGML_RXD_UNUSED(dev);
}

static void ggml_rxd_backend_zdnn_device_get_memory(ggml_rxd_backend_dev_t dev, size_t * free, size_t * total) {
    *free  = 0;
    *total = 0;

    GGML_RXD_UNUSED(dev);
}

static enum ggml_rxd_backend_dev_type ggml_rxd_backend_zdnn_device_get_type(ggml_rxd_backend_dev_t dev) {
    return GGML_RXD_BACKEND_DEVICE_TYPE_ACCEL;

    GGML_RXD_UNUSED(dev);
}

static void ggml_rxd_backend_zdnn_device_get_props(ggml_rxd_backend_dev_t dev, ggml_rxd_backend_dev_props * props) {
    props->name        = ggml_rxd_backend_zdnn_device_get_name(dev);
    props->description = ggml_rxd_backend_zdnn_device_get_description(dev);
    props->type        = ggml_rxd_backend_zdnn_device_get_type(dev);
    ggml_rxd_backend_zdnn_device_get_memory(dev, &props->memory_free, &props->memory_total);
    props->caps = (ggml_rxd_backend_dev_caps) {
        /* .async                = */ false,
        /* .host_buffer          = */ false,
        /* .buffer_from_host_ptr = */ false,
        /* .events               = */ false
    };
}

static ggml_rxd_backend_t ggml_rxd_backend_zdnn_device_init(ggml_rxd_backend_dev_t dev, const char * params) {
    ggml_rxd_backend_zdnn_context * ctx = ggml_rxd_zdnn_init(dev);
    if (ctx == NULL) {
        GGML_RXD_LOG_ERROR("%s: error: failed to allocate context\n", __func__);
        return NULL;
    }

    ggml_rxd_backend_t backend = (ggml_rxd_backend *)malloc(sizeof(ggml_rxd_backend));
    *backend = (ggml_rxd_backend) {
        /* .guid       = */ ggml_rxd_backend_zdnn_guid(),
        /* .iface      = */ ggml_rxd_backend_zdnn_i,
        /* .device     = */ dev,
        /* .context    = */ ctx
    };

    return backend;

    GGML_RXD_UNUSED(params);
}

static ggml_rxd_backend_buffer_type_t ggml_rxd_backend_zdnn_device_get_buffer_type(ggml_rxd_backend_dev_t dev) {
    return ggml_rxd_backend_zdnn_buffer_type();

    GGML_RXD_UNUSED(dev);
}

static bool ggml_rxd_backend_zdnn_device_supports_op(ggml_rxd_backend_dev_t dev, const ggml_rxd_tensor * op) {
    ggml_rxd_backend_zdnn_device_context * ctx_dev = (ggml_rxd_backend_zdnn_device_context *) dev->context;

    return ggml_rxd_zdnn_supports_op(ctx_dev, op);
}

static bool ggml_rxd_backend_zdnn_device_supports_buft(ggml_rxd_backend_dev_t dev, ggml_rxd_backend_buffer_type_t buft) {
    return
        buft->iface.get_name == ggml_rxd_backend_zdnn_buffer_type_get_name;

    GGML_RXD_UNUSED(dev);
}

static ggml_rxd_backend_device_i ggml_rxd_backend_zdnn_device_i = {
    /* .get_name             = */ ggml_rxd_backend_zdnn_device_get_name,
    /* .get_description      = */ ggml_rxd_backend_zdnn_device_get_description,
    /* .get_memory           = */ ggml_rxd_backend_zdnn_device_get_memory,
    /* .get_type             = */ ggml_rxd_backend_zdnn_device_get_type,
    /* .get_props            = */ ggml_rxd_backend_zdnn_device_get_props,
    /* .init_backend         = */ ggml_rxd_backend_zdnn_device_init,
    /* .get_buffer_type      = */ ggml_rxd_backend_zdnn_device_get_buffer_type,
    /* .get_host_buffer_type = */ NULL,
    /* .buffer_from_host_ptr = */ NULL,
    /* .supports_op          = */ ggml_rxd_backend_zdnn_device_supports_op,
    /* .supports_buft        = */ ggml_rxd_backend_zdnn_device_supports_buft,
    /* .offload_op           = */ NULL,
    /* .event_new            = */ NULL,
    /* .event_free           = */ NULL,
    /* .event_synchronize    = */ NULL,
};

//
// backend registry
//

static const char * ggml_rxd_backend_zdnn_reg_get_name(ggml_rxd_backend_reg_t reg) {
    return GGML_RXD_ZDNN_NAME;

    GGML_RXD_UNUSED(reg);
}

static size_t ggml_rxd_backend_zdnn_reg_device_count(ggml_rxd_backend_reg_t reg) {
    if (!zdnn_is_nnpa_installed()) {
        return 0;
    }
    return 1;

    GGML_RXD_UNUSED(reg);
}

static ggml_rxd_backend_dev_t ggml_rxd_backend_zdnn_reg_device_get(ggml_rxd_backend_reg_t reg, size_t index) {
    GGML_RXD_ASSERT(index == 0);

    return &g_ggml_backend_zdnn_device;

    GGML_RXD_UNUSED(reg);
    GGML_RXD_UNUSED(index);
}

static ggml_rxd_backend_feature g_ggml_backend_zdnn_features[] = {
    { "NNPA", zdnn_is_nnpa_installed() ? "1" : "0" },
    { "NNPA_PARMBLKFORMAT_0", zdnn_is_nnpa_parmblk_fmt_installed(1, NNPA_PARMBLKFORMAT_0) ? "1" : "0" },
    { "NNPA_PARMBLKFORMAT_1", zdnn_is_nnpa_parmblk_fmt_installed(1, NNPA_PARMBLKFORMAT_1) ? "1" : "0" },
    { NULL, NULL },
};

static ggml_rxd_backend_feature * ggml_rxd_backend_zdnn_get_features(ggml_rxd_backend_reg_t reg) {
    return g_ggml_backend_zdnn_features;

    GGML_RXD_UNUSED(reg);
}

static void * ggml_rxd_backend_zdnn_get_proc_address(ggml_rxd_backend_reg_t reg, const char * name) {
    if (strcmp(name, "ggml_rxd_backend_get_features") == 0) {
        return (void *) ggml_rxd_backend_zdnn_get_features;
    }

    return NULL;

    GGML_RXD_UNUSED(reg);
}

static ggml_rxd_backend_reg_i ggml_rxd_backend_zdnn_reg_i = {
    /* .get_name         = */ ggml_rxd_backend_zdnn_reg_get_name,
    /* .get_device_count = */ ggml_rxd_backend_zdnn_reg_device_count,
    /* .get_device       = */ ggml_rxd_backend_zdnn_reg_device_get,
    /* .get_proc_address = */ ggml_rxd_backend_zdnn_get_proc_address
};

static void ggml_rxd_zdnn_cleanup(void) {
    ggml_rxd_backend_zdnn_device_rel(&g_ggml_ctx_dev_main);
}

// TODO: make thread-safe
ggml_rxd_backend_reg_t ggml_rxd_backend_zdnn_reg(void) {
    ggml_rxd_backend_zdnn_device_acq(&g_ggml_ctx_dev_main);

    // register cleanup callback
    atexit(ggml_rxd_zdnn_cleanup);

    {
        g_ggml_backend_zdnn_reg = (ggml_rxd_backend_reg) {
            /* .api_version = */ GGML_RXD_ZDNN_VERSION,
            /* .iface       = */ ggml_rxd_backend_zdnn_reg_i,
            /* .context     = */ NULL
        };

        g_ggml_backend_zdnn_device = (ggml_rxd_backend_device) {
            /* .iface       = */ ggml_rxd_backend_zdnn_device_i,
            /* .reg         = */ &g_ggml_backend_zdnn_reg,
            /* .context     = */ &g_ggml_ctx_dev_main
        };

        return &g_ggml_backend_zdnn_reg;
    }
}

GGML_RXD_BACKEND_DL_IMPL(ggml_rxd_backend_zdnn_reg)
