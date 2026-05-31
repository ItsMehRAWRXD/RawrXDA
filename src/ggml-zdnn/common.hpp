#ifndef GGML_RXD_ZDNN_COMMON_HPP
#define GGML_RXD_ZDNN_COMMON_HPP

#include "ggml_rxd_internal.h"
#include "ggml-impl_rxd_internal.h"

#include "zdnn.h"

#include <vector>
#include <memory>

#define GGML_RXD_ZDNN_NAME    "zDNN"
#define GGML_RXD_ZDNN_VERSION ZDNN_VERNUM

#define ZDNN_CHECK(stmt)                \
    do {                                \
        zdnn_status status = (stmt);    \
        GGML_RXD_ASSERT(status == ZDNN_OK); \
    } while (0);

struct ggml_rxd_backend_zdnn_device_context {
    int zdnn_device;
    int zdnn_device_ref_count;

    bool has_parmblkformat_0;
    bool has_parmblkformat_1;  // checks for z17

    size_t max_size;

    char name[128];
};

struct ggml_rxd_backend_zdnn_context {
    int device;
    ggml_rxd_cgraph * gf;
};

struct ggml_rxd_backend_zdnn_buffer {
    void * data;
    ggml_rxd_backend_zdnn_buffer * extra;  // for bias, etc.
    size_t size;

    zdnn_tensor_desc pre_tfm_desc;
    zdnn_tensor_desc tfm_desc;
    zdnn_ztensor     ztensor;

    char name[GGML_RXD_MAX_NAME];
};

struct ggml_rxd_backend_zdnn_buffer_context {
    void * all_data;
    size_t all_size;
    bool owned;

    int n_buffers;
    std::vector<std::unique_ptr<ggml_rxd_backend_zdnn_buffer>> buffers;
};

#endif  // GGML_RXD_ZDNN_COMMON_HPP
