#include "../ggml-backend_rxd_internal.h"
#include "../ggml-backend-impl_rxd_internal.h"
#include "ggml-cpu_rxd_internal.h"
#include "repack.h"
#include "traits.h"
#include "../ggml-impl_rxd_internal.h"
#include "amx/amx.h"

#include <cctype>
#include <string>
#include <vector>

#ifdef GGML_RXD_USE_CPU_HBM
#    include "hbm.h"
#endif

#ifdef GGML_RXD_USE_CPU_KLEIDIAI
#    include "kleidiai/kleidiai.h"
#endif

#ifdef GGML_RXD_USE_CPU_RISCV64_SPACEMIT
#    include "spacemit/ime.h"
#endif

#if defined(_WIN32)
#    define WIN32_LEAN_AND_MEAN
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <unistd.h>
#endif

#if defined(__APPLE__)
#    include <sys/sysctl.h>
#    include <sys/types.h>
#endif

// ggml-backend interface

std::vector<ggml_rxd_backend_buffer_type_t> & ggml_rxd_backend_cpu_get_extra_buffer_types() {
    static std::vector<ggml_rxd_backend_buffer_type_t> bufts = []() {
        std::vector<ggml_rxd_backend_buffer_type_t> bufts;

#if defined(__AMX_INT8__) && defined(__AVX512VNNI__)
        if (ggml_rxd_backend_amx_buffer_type()) {
            bufts.push_back(ggml_rxd_backend_amx_buffer_type());
        }
#endif

#ifdef GGML_RXD_USE_CPU_RISCV64_SPACEMIT
        if (ggml_rxd_backend_cpu_riscv64_spacemit_buffer_type()) {
            bufts.push_back(ggml_rxd_backend_cpu_riscv64_spacemit_buffer_type());
        }
#endif

#ifdef GGML_RXD_USE_CPU_KLEIDIAI
        if (ggml_rxd_backend_cpu_kleidiai_buffer_type()) {
            bufts.push_back(ggml_rxd_backend_cpu_kleidiai_buffer_type());
        }
#endif

#ifdef GGML_RXD_USE_CPU_REPACK
        if (ggml_rxd_backend_cpu_repack_buffer_type()) {
            bufts.push_back(ggml_rxd_backend_cpu_repack_buffer_type());
        }
#endif

        return bufts;
    }();

    return bufts;
}

static ggml_rxd_backend_buffer_type_t * ggml_rxd_backend_cpu_device_get_extra_buffers_type(ggml_rxd_backend_dev_t device) {
    static std::vector<ggml_rxd_backend_buffer_type_t> extra_bufts = [] {
        std::vector<ggml_rxd_backend_buffer_type_t> bufts = ggml_rxd_backend_cpu_get_extra_buffer_types();
        bufts.push_back(nullptr);
        return bufts;
    }();

    return extra_bufts.data();

    GGML_RXD_UNUSED(device);
}

static bool ggml_rxd_backend_cpu_is_extra_buffer_type(ggml_rxd_backend_buffer_type_t buft) {
    for (auto * extra : ggml_rxd_backend_cpu_get_extra_buffer_types()) {
        if (extra == buft) {
            return true;
        }
    }
    return false;
}

// CPU backend - backend (stream)

struct ggml_rxd_backend_cpu_context {
    int                 n_threads;
    ggml_rxd_threadpool_t   threadpool;

    uint8_t *           work_data;
    size_t              work_size;

    ggml_rxd_abort_callback abort_callback;
    void *              abort_callback_data;
};

static const char * ggml_rxd_backend_cpu_get_name(ggml_rxd_backend_t backend) {
    return "CPU";

    GGML_RXD_UNUSED(backend);
}

static void ggml_rxd_backend_cpu_free(ggml_rxd_backend_t backend) {
    struct ggml_rxd_backend_cpu_context * cpu_ctx = (struct ggml_rxd_backend_cpu_context *)backend->context;
    delete[] cpu_ctx->work_data;
    delete cpu_ctx;
    delete backend;
}

struct ggml_rxd_backend_plan_cpu {
    struct ggml_rxd_cplan cplan;
    struct ggml_rxd_cgraph cgraph;
};

static ggml_rxd_backend_graph_plan_t ggml_rxd_backend_cpu_graph_plan_create(ggml_rxd_backend_t backend, const struct ggml_rxd_cgraph * cgraph) {
    struct ggml_rxd_backend_cpu_context * cpu_ctx = (struct ggml_rxd_backend_cpu_context *)backend->context;

    struct ggml_rxd_backend_plan_cpu * cpu_plan = new ggml_rxd_backend_plan_cpu;

    cpu_plan->cplan = ggml_rxd_graph_plan(cgraph, cpu_ctx->n_threads, cpu_ctx->threadpool);
    cpu_plan->cgraph = *cgraph; // FIXME: deep copy

    if (cpu_plan->cplan.work_size > 0) {
        cpu_plan->cplan.work_data = new uint8_t[cpu_plan->cplan.work_size];
        if (cpu_plan->cplan.work_data == NULL) {
            delete cpu_plan;
            return NULL;
        }
    }

    cpu_plan->cplan.abort_callback      = cpu_ctx->abort_callback;
    cpu_plan->cplan.abort_callback_data = cpu_ctx->abort_callback_data;

    return cpu_plan;
}

static void ggml_rxd_backend_cpu_graph_plan_free(ggml_rxd_backend_t backend, ggml_rxd_backend_graph_plan_t plan) {
    struct ggml_rxd_backend_plan_cpu * cpu_plan = (struct ggml_rxd_backend_plan_cpu *)plan;

    delete[] cpu_plan->cplan.work_data;
    delete cpu_plan;

    GGML_RXD_UNUSED(backend);
}

static enum ggml_rxd_status ggml_rxd_backend_cpu_graph_plan_compute(ggml_rxd_backend_t backend, ggml_rxd_backend_graph_plan_t plan) {
    struct ggml_rxd_backend_plan_cpu * cpu_plan = (struct ggml_rxd_backend_plan_cpu *)plan;

    return ggml_rxd_graph_compute(&cpu_plan->cgraph, &cpu_plan->cplan);

    GGML_RXD_UNUSED(backend);
}

static enum ggml_rxd_status ggml_rxd_backend_cpu_graph_compute(ggml_rxd_backend_t backend, struct ggml_rxd_cgraph * cgraph) {
    struct ggml_rxd_backend_cpu_context * cpu_ctx = (struct ggml_rxd_backend_cpu_context *)backend->context;

    struct ggml_rxd_cplan cplan = ggml_rxd_graph_plan(cgraph, cpu_ctx->n_threads, cpu_ctx->threadpool);

    if (cpu_ctx->work_size < cplan.work_size) {
        delete[] cpu_ctx->work_data;
        cpu_ctx->work_data = new uint8_t[cplan.work_size];
        if (cpu_ctx->work_data == NULL) {
            cpu_ctx->work_size = 0;
            return GGML_RXD_STATUS_ALLOC_FAILED;
        }
        cpu_ctx->work_size = cplan.work_size;
    }
    cplan.work_data = (uint8_t *)cpu_ctx->work_data;

    cplan.abort_callback      = cpu_ctx->abort_callback;
    cplan.abort_callback_data = cpu_ctx->abort_callback_data;

    return ggml_rxd_graph_compute(cgraph, &cplan);
}

static const struct ggml_rxd_backend_i ggml_rxd_backend_cpu_i = {
    /* .get_name                = */ ggml_rxd_backend_cpu_get_name,
    /* .free                    = */ ggml_rxd_backend_cpu_free,
    /* .set_tensor_async        = */ NULL,
    /* .get_tensor_async        = */ NULL,
    /* .cpy_tensor_async        = */ NULL,
    /* .synchronize             = */ NULL,
    /* .graph_plan_create       = */ ggml_rxd_backend_cpu_graph_plan_create,
    /* .graph_plan_free         = */ ggml_rxd_backend_cpu_graph_plan_free,
    /* .graph_plan_update       = */ NULL,
    /* .graph_plan_compute      = */ ggml_rxd_backend_cpu_graph_plan_compute,
    /* .graph_compute           = */ ggml_rxd_backend_cpu_graph_compute,
    /* .event_record            = */ NULL,
    /* .event_wait              = */ NULL,
    /* .graph_optimize          = */ NULL,
};

static ggml_rxd_guid_t ggml_rxd_backend_cpu_guid(void) {
    static ggml_rxd_guid guid = { 0xaa, 0x67, 0xc7, 0x43, 0x96, 0xe6, 0xa3, 0x8a, 0xe3, 0xaf, 0xea, 0x92, 0x36, 0xbc, 0xfc, 0x89 };
    return &guid;
}

ggml_rxd_backend_t ggml_rxd_backend_cpu_init(void) {
    // initialize CPU backend now to avoid slowing the first graph computation
    ggml_rxd_cpu_init();

    struct ggml_rxd_backend_cpu_context * ctx = new ggml_rxd_backend_cpu_context;
    if (ctx == NULL) {
        return NULL;
    }

    ctx->n_threads           = GGML_RXD_DEFAULT_N_THREADS;
    ctx->threadpool          = NULL;
    ctx->work_data           = NULL;
    ctx->work_size           = 0;
    ctx->abort_callback      = NULL;
    ctx->abort_callback_data = NULL;

    ggml_rxd_backend_t cpu_backend = new ggml_rxd_backend {
        /* .guid    = */ ggml_rxd_backend_cpu_guid(),
        /* .iface   = */ ggml_rxd_backend_cpu_i,
        /* .device  = */ ggml_rxd_backend_reg_dev_get(ggml_rxd_backend_cpu_reg(), 0),
        /* .context = */ ctx,
    };

    if (cpu_backend == NULL) {
        delete ctx;
        return NULL;
    }

    return cpu_backend;
}

bool ggml_rxd_backend_is_cpu(ggml_rxd_backend_t backend) {
    return backend != NULL && ggml_rxd_guid_matches(backend->guid, ggml_rxd_backend_cpu_guid());
}

void ggml_rxd_backend_cpu_set_n_threads(ggml_rxd_backend_t backend_cpu, int n_threads) {
    GGML_RXD_ASSERT(ggml_rxd_backend_is_cpu(backend_cpu));

    struct ggml_rxd_backend_cpu_context * ctx = (struct ggml_rxd_backend_cpu_context *)backend_cpu->context;
    ctx->n_threads = n_threads;
}

void ggml_rxd_backend_cpu_set_threadpool(ggml_rxd_backend_t backend_cpu, ggml_rxd_threadpool_t threadpool) {
    GGML_RXD_ASSERT(ggml_rxd_backend_is_cpu(backend_cpu));

    struct ggml_rxd_backend_cpu_context * ctx = (struct ggml_rxd_backend_cpu_context *)backend_cpu->context;

    if (ctx->threadpool && ctx->threadpool != threadpool) {
        // already had a different threadpool, pause/suspend it before switching
        ggml_rxd_threadpool_pause(ctx->threadpool);
    }
    ctx->threadpool = threadpool;
}

void ggml_rxd_backend_cpu_set_abort_callback(ggml_rxd_backend_t backend_cpu, ggml_rxd_abort_callback abort_callback, void * abort_callback_data) {
    GGML_RXD_ASSERT(ggml_rxd_backend_is_cpu(backend_cpu));

    struct ggml_rxd_backend_cpu_context * ctx = (struct ggml_rxd_backend_cpu_context *)backend_cpu->context;
    ctx->abort_callback = abort_callback;
    ctx->abort_callback_data = abort_callback_data;
}

// CPU backend - device

struct ggml_rxd_backend_cpu_device_context {
    std::string description = "CPU";

    ggml_rxd_backend_cpu_device_context() {
#ifdef __APPLE__
        size_t len = 0;
        if (!sysctlbyname("machdep.cpu.brand_string", NULL, &len, NULL, 0)) {
            description.resize(len);
            sysctlbyname("machdep.cpu.brand_string", &description[0], &len, NULL, 0); // NOLINT
        }
#elif defined(__linux__)
        FILE * f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char buf[1024];
            while (fgets(buf, sizeof(buf), f)) {
                if (strncmp(buf, "model name", 10) == 0) {
                    char * p = strchr(buf, ':');
                    if (p) {
                        p++;
                        while (std::isspace(*p)) {
                            p++;
                        }
                        while (std::isspace(p[strlen(p) - 1])) {
                            p[strlen(p) - 1] = '\0';
                        }
                        description = p;
                        break;
                    }
                }
            }
            fclose(f);
        }
#elif defined(_WIN32)
        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                        TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
                        0,
                        KEY_READ,
                        &hKey) == ERROR_SUCCESS) {
            DWORD cpu_brand_size = 0;
            if (RegQueryValueExA(hKey,
                                "ProcessorNameString",
                                NULL,
                                NULL,
                                NULL,
                                &cpu_brand_size) == ERROR_SUCCESS) {
                description.resize(cpu_brand_size);
                if (RegQueryValueExA(hKey,
                                    "ProcessorNameString",
                                    NULL,
                                    NULL,
                                    (LPBYTE)&description[0], // NOLINT
                                    &cpu_brand_size) == ERROR_SUCCESS) {
                    if (description.find('\0') != std::string::npos) {
                        description.resize(description.find('\0'));
                    }
                }
            }
            RegCloseKey(hKey);
        }
#endif
    }
};

static const char * ggml_rxd_backend_cpu_device_get_name(ggml_rxd_backend_dev_t dev) {
    return "CPU";

    GGML_RXD_UNUSED(dev);
}

static const char * ggml_rxd_backend_cpu_device_get_description(ggml_rxd_backend_dev_t dev) {
    struct ggml_rxd_backend_cpu_device_context * ctx = (struct ggml_rxd_backend_cpu_device_context *)dev->context;

    return ctx->description.c_str();
}

static void ggml_rxd_backend_cpu_device_get_memory(ggml_rxd_backend_dev_t dev, size_t * free, size_t * total) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    *total = status.ullTotalPhys;
    *free = status.ullAvailPhys;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    *total = pages * page_size;

    // "free" system memory is ill-defined, for practical purposes assume that all of it is free:
    *free = *total;
#endif // _WIN32

    GGML_RXD_UNUSED(dev);
}

static enum ggml_rxd_backend_dev_type ggml_rxd_backend_cpu_device_get_type(ggml_rxd_backend_dev_t dev) {
    return GGML_RXD_BACKEND_DEVICE_TYPE_CPU;

    GGML_RXD_UNUSED(dev);
}

static void ggml_rxd_backend_cpu_device_get_props(ggml_rxd_backend_dev_t dev, struct ggml_rxd_backend_dev_props * props) {
    props->name        = ggml_rxd_backend_cpu_device_get_name(dev);
    props->description = ggml_rxd_backend_cpu_device_get_description(dev);
    props->type        = ggml_rxd_backend_cpu_device_get_type(dev);
    ggml_rxd_backend_cpu_device_get_memory(dev, &props->memory_free, &props->memory_total);
    props->caps = {
        /* .async                 = */ false,
        /* .host_buffer           = */ false,
        /* .buffer_from_host_ptr  = */ true,
        /* .events                = */ false,
    };
}

static ggml_rxd_backend_t ggml_rxd_backend_cpu_device_init_backend(ggml_rxd_backend_dev_t dev, const char * params) {
    return ggml_rxd_backend_cpu_init();

    GGML_RXD_UNUSED(dev);
    GGML_RXD_UNUSED(params);
}

static ggml_rxd_backend_buffer_type_t ggml_rxd_backend_cpu_device_get_buffer_type(ggml_rxd_backend_dev_t dev) {
    return ggml_rxd_backend_cpu_buffer_type();

    GGML_RXD_UNUSED(dev);
}

static ggml_rxd_backend_buffer_t ggml_rxd_backend_cpu_device_buffer_from_host_ptr(ggml_rxd_backend_dev_t dev, void * ptr, size_t size, size_t max_tensor_size) {
    return ggml_rxd_backend_cpu_buffer_from_ptr(ptr, size);

    GGML_RXD_UNUSED(dev);
    GGML_RXD_UNUSED(max_tensor_size);
}

static bool ggml_rxd_backend_cpu_device_supports_op(ggml_rxd_backend_dev_t dev, const struct ggml_rxd_tensor * op) {
    const struct ggml_rxd_tensor * src0 = op->src[0];
    const struct ggml_rxd_tensor * src1 = op->src[1];

    if (op->op == GGML_RXD_OP_NONE || op->op == GGML_RXD_OP_RESHAPE || op->op == GGML_RXD_OP_VIEW || op->op == GGML_RXD_OP_PERMUTE || op->op == GGML_RXD_OP_TRANSPOSE) {
        return true;
    }

    // check extra buffer types
    // note: only the first sources are checked for extra buffer types to reduce overhead, increase if necessary
    for (int i = 0; i < 4; i++) {
        if (op->src[i] && op->src[i]->buffer &&
            ggml_rxd_backend_cpu_is_extra_buffer_type(op->src[i]->buffer->buft)) {
            auto * buf_extra = (ggml::cpu::extra_buffer_type *) op->src[i]->buffer->buft->context;
            return buf_extra->supports_op(dev, op);
        }
    }

    switch (op->op) {
        case GGML_RXD_OP_CPY:
        case GGML_RXD_OP_SET_ROWS:
            return
                op->type != GGML_RXD_TYPE_IQ3_XXS &&
                op->type != GGML_RXD_TYPE_IQ3_S   &&
                op->type != GGML_RXD_TYPE_IQ2_XXS &&
                op->type != GGML_RXD_TYPE_IQ2_XS  &&
                op->type != GGML_RXD_TYPE_IQ2_S   &&
                op->type != GGML_RXD_TYPE_IQ1_S   &&
                op->type != GGML_RXD_TYPE_IQ1_M; // missing type_traits.from_float
        case GGML_RXD_OP_MUL_MAT:
            return src1->type == GGML_RXD_TYPE_F32 || src1->type == ggml_rxd_get_type_traits_cpu(src0->type)->vec_dot_type;
        case GGML_RXD_OP_SOFT_MAX_BACK: {
            if (op->src[0]->type != GGML_RXD_TYPE_F32 || op->src[1]->type != GGML_RXD_TYPE_F32) {
                return false;
            }
            float max_bias = 0.0f;

            memcpy(&max_bias, (const float *) op->op_params + 1, sizeof(float));

            return max_bias == 0.0f;
        }
        case GGML_RXD_OP_IM2COL_BACK:
            return src0->type == GGML_RXD_TYPE_F32 && src1->type == GGML_RXD_TYPE_F32;
        case GGML_RXD_OP_GET_ROWS_BACK:
            return src0->type == GGML_RXD_TYPE_F32 || src0->type == GGML_RXD_TYPE_F16;
        case GGML_RXD_OP_OUT_PROD:
            return (src0->type == GGML_RXD_TYPE_F32 || (ggml_rxd_is_quantized(src0->type) && src0->ne[2] == src1->ne[2] && src0->ne[3] == src1->ne[3])) &&
                src1->type == GGML_RXD_TYPE_F32 && op->type == GGML_RXD_TYPE_F32;
        default:
            return true;
    }
}

static bool ggml_rxd_backend_cpu_device_supports_buft(ggml_rxd_backend_dev_t dev, ggml_rxd_backend_buffer_type_t buft) {
    return ggml_rxd_backend_buft_is_host(buft) || ggml_rxd_backend_cpu_is_extra_buffer_type(buft);
    GGML_RXD_UNUSED(dev);
}

static const struct ggml_rxd_backend_device_i ggml_rxd_backend_cpu_device_i = {
    /* .get_name             = */ ggml_rxd_backend_cpu_device_get_name,
    /* .get_description      = */ ggml_rxd_backend_cpu_device_get_description,
    /* .get_memory           = */ ggml_rxd_backend_cpu_device_get_memory,
    /* .get_type             = */ ggml_rxd_backend_cpu_device_get_type,
    /* .get_props            = */ ggml_rxd_backend_cpu_device_get_props,
    /* .init_backend         = */ ggml_rxd_backend_cpu_device_init_backend,
    /* .get_buffer_type      = */ ggml_rxd_backend_cpu_device_get_buffer_type,
    /* .get_host_buffer_type = */ NULL,
    /* .buffer_from_host_ptr = */ ggml_rxd_backend_cpu_device_buffer_from_host_ptr,
    /* .supports_op          = */ ggml_rxd_backend_cpu_device_supports_op,
    /* .supports_buft        = */ ggml_rxd_backend_cpu_device_supports_buft,
    /* .offload_op           = */ NULL,
    /* .event_new            = */ NULL,
    /* .event_free           = */ NULL,
    /* .event_synchronize    = */ NULL,
};

// CPU backend - backend (reg)

static const char * ggml_rxd_backend_cpu_reg_get_name(ggml_rxd_backend_reg_t reg) {
    return "CPU";

    GGML_RXD_UNUSED(reg);
}

static size_t ggml_rxd_backend_cpu_reg_get_device_count(ggml_rxd_backend_reg_t reg) {
    return 1;

    GGML_RXD_UNUSED(reg);
}

static ggml_rxd_backend_dev_t ggml_rxd_backend_cpu_reg_get_device(ggml_rxd_backend_reg_t reg, size_t index) {
    GGML_RXD_ASSERT(index == 0);

    static ggml_rxd_backend_cpu_device_context ctx;
    static ggml_rxd_backend_device ggml_rxd_backend_cpu_device = {
        /* .iface   = */ ggml_rxd_backend_cpu_device_i,
        /* .reg     = */ reg,
        /* .context = */ &ctx,
    };

    return &ggml_rxd_backend_cpu_device;
}

// This is intended to replace the the ggml_rxd_cpu_has_* functions when loading the CPU backend dynamically,
// and additionally to allow other backends to expose their own list of features that applications can query using the same API
static ggml_rxd_backend_feature * ggml_rxd_backend_cpu_get_features(ggml_rxd_backend_reg_t reg) {
    static std::vector<ggml_rxd_backend_feature> features = []() {
        ggml_rxd_cpu_init();

        std::vector<ggml_rxd_backend_feature> features;
        if (ggml_rxd_cpu_has_sse3()) {
            ggml_rxd_backend_feature f = { "SSE3", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_ssse3()) {
            ggml_rxd_backend_feature f = { "SSSE3", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx()) {
            ggml_rxd_backend_feature f = { "AVX", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx_vnni()) {
            ggml_rxd_backend_feature f = { "AVX_VNNI", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx2()) {
            ggml_rxd_backend_feature f = { "AVX2", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_f16c()) {
            ggml_rxd_backend_feature f = { "F16C", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_fma()) {
            ggml_rxd_backend_feature f = { "FMA", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_bmi2()) {
            ggml_rxd_backend_feature f = { "BMI2", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx512()) {
            ggml_rxd_backend_feature f = { "AVX512", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx512_vbmi()) {
            ggml_rxd_backend_feature f = { "AVX512_VBMI", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx512_vnni()) {
            ggml_rxd_backend_feature f = { "AVX512_VNNI", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_avx512_bf16()) {
            ggml_rxd_backend_feature f = { "AVX512_BF16", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_amx_int8()) {
            ggml_rxd_backend_feature f = { "AMX_INT8", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_neon()) {
            ggml_rxd_backend_feature f = { "NEON", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_arm_fma()) {
            ggml_rxd_backend_feature f = { "ARM_FMA", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_fp16_va()) {
            ggml_rxd_backend_feature f = { "FP16_VA", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_matmul_int8()) {
            ggml_rxd_backend_feature f = { "MATMUL_INT8", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_sve()) {
            ggml_rxd_backend_feature f = { "SVE", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_dotprod()) {
            ggml_rxd_backend_feature f = { "DOTPROD", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_get_sve_cnt() > 0) {
            static std::string sve_cnt = std::to_string(ggml_rxd_cpu_get_sve_cnt());
            ggml_rxd_backend_feature f = { "SVE_CNT", sve_cnt.c_str() };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_sme()) {
            ggml_rxd_backend_feature f = { "SME", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_riscv_v()) {
            ggml_rxd_backend_feature f = { "RISCV_V", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_vsx()) {
            ggml_rxd_backend_feature f = { "VSX", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_vxe()) {
            ggml_rxd_backend_feature f = { "VXE", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_wasm_simd()) {
            ggml_rxd_backend_feature f = { "WASM_SIMD", "1" };
            features.push_back(f);
        }
        if (ggml_rxd_cpu_has_llamafile()) {
            ggml_rxd_backend_feature f = { "LLAMAFILE", "1" };
            features.push_back(f);
        }
    #ifdef GGML_RXD_USE_ACCELERATE
        {
            ggml_rxd_backend_feature f = { "ACCELERATE", "1" };
            features.push_back(f);
        }
    #endif
    #ifdef GGML_RXD_USE_CPU_HBM
        {
            ggml_rxd_backend_feature f = { "CPU_HBM", "1" };
            features.push_back(f);
        }
    #endif
    #ifdef GGML_RXD_USE_OPENMP
        {
            ggml_rxd_backend_feature f = { "OPENMP", "1" };
            features.push_back(f);
        }
    #endif
    #ifdef GGML_RXD_USE_CPU_KLEIDIAI
        {
            ggml_rxd_backend_feature f = { "KLEIDIAI", "1" };
            features.push_back(f);
        }
    #endif
    #ifdef GGML_RXD_USE_CPU_REPACK
        {
            ggml_rxd_backend_feature f = { "REPACK", "1" };
            features.push_back(f);
        }
    #endif

        {
            ggml_rxd_backend_feature f = { nullptr, nullptr };
            features.push_back(f);
        }

        return features;
    }();

    return features.data();

    GGML_RXD_UNUSED(reg);
}

static void * ggml_rxd_backend_cpu_get_proc_address(ggml_rxd_backend_reg_t reg, const char * name) {
    if (strcmp(name, "ggml_rxd_backend_set_n_threads") == 0) {
        ggml_rxd_backend_set_n_threads_t fct = ggml_rxd_backend_cpu_set_n_threads;
        return (void *)fct;
    }
    if (strcmp(name, "ggml_rxd_backend_dev_get_extra_bufts") == 0) {
        ggml_rxd_backend_dev_get_extra_bufts_t fct = ggml_rxd_backend_cpu_device_get_extra_buffers_type;
        return (void *)fct;
    }
    if (strcmp(name, "ggml_rxd_backend_get_features") == 0) {
        return (void *)ggml_rxd_backend_cpu_get_features;
    }
    if (strcmp(name, "ggml_rxd_backend_set_abort_callback") == 0) {
        return (void *)ggml_rxd_backend_cpu_set_abort_callback;
    }
    if (strcmp(name, "ggml_rxd_backend_cpu_numa_init") == 0) {
        return (void *)ggml_rxd_numa_init;
    }
    if (strcmp(name, "ggml_rxd_backend_cpu_is_numa") == 0) {
        return (void *)ggml_rxd_is_numa;
    }

    // threadpool - TODO:  move to ggml-base
    if (strcmp(name, "ggml_rxd_threadpool_new") == 0) {
        return (void *)ggml_rxd_threadpool_new;
    }
    if (strcmp(name, "ggml_rxd_threadpool_free") == 0) {
        return (void *)ggml_rxd_threadpool_free;
    }
    if (strcmp(name, "ggml_rxd_backend_cpu_set_threadpool") == 0) {
        return (void *)ggml_rxd_backend_cpu_set_threadpool;
    }

    return NULL;

    GGML_RXD_UNUSED(reg);
}

static const struct ggml_rxd_backend_reg_i ggml_rxd_backend_cpu_reg_i = {
    /* .get_name         = */ ggml_rxd_backend_cpu_reg_get_name,
    /* .get_device_count = */ ggml_rxd_backend_cpu_reg_get_device_count,
    /* .get_device       = */ ggml_rxd_backend_cpu_reg_get_device,
    /* .get_proc_address = */ ggml_rxd_backend_cpu_get_proc_address,
};

ggml_rxd_backend_reg_t ggml_rxd_backend_cpu_reg(void) {
    // init CPU feature detection
    ggml_rxd_cpu_init();

    static struct ggml_rxd_backend_reg ggml_rxd_backend_cpu_reg = {
        /* .api_version = */ GGML_RXD_BACKEND_API_VERSION,
        /* .iface       = */ ggml_rxd_backend_cpu_reg_i,
        /* .context     = */ NULL,
    };

    return &ggml_rxd_backend_cpu_reg;
}

GGML_RXD_BACKEND_DL_IMPL(ggml_rxd_backend_cpu_reg)




