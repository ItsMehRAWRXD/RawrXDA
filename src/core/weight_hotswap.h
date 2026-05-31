#ifndef RAWRXD_WEIGHT_HOTSWAP_H
#define RAWRXD_WEIGHT_HOTSWAP_H

#include "gguf_format.h"
#include "quant_ops.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
WEIGHT HOTSWAP SYSTEM

Enables on-the-fly re-quantization without restart.
NO SIMULATIONS. Real memory-mapped weight manipulation.
═══════════════════════════════════════════════════════════════════════════ */

/* Forward declaration */
typedef struct InferenceEngine InferenceEngine;

/* Weight buffer types */
typedef enum {
    WEIGHT_BUFFER_CPU, /* System RAM */
    WEIGHT_BUFFER_GPU, /* VRAM (Vulkan/CUDA) */
    WEIGHT_BUFFER_MAPPED /* Memory-mapped file */
} WeightBufferType;

/* Weight tensor handle */
typedef struct {
    char name[128];
    GGMLType current_type;
    GGMLType original_type;
    void* data; /* Current data pointer */
    void* original_data; /* Original data (backup) */
    size_t size; /* Current size in bytes */
    size_t original_size; /* Original size */
    uint64_t elements; /* Number of elements */
    WeightBufferType buffer_type;
    bool is_swapped; /* Has been modified */
    bool is_locked; /* In use by inference */
} WeightTensor;

/* Hotswap session */
typedef struct {
    /* GGUF context */
    GGUFContext* gguf;

    /* Weight tensors */
    WeightTensor* tensors;
    uint32_t tensor_count;

    /* Memory management */
    void* weight_pool; /* Pre-allocated memory for swaps */
    size_t pool_size;
    size_t pool_used;

    /* Statistics */
    size_t total_original_size;
    size_t total_current_size;
    float compression_ratio;

    /* State */
    bool has_backup;
    bool is_suspended;
    char error_msg[256];
} HotswapSession;

/* ═══════════════════════════════════════════════════════════════════════════
SESSION MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

/* Create hotswap session from GGUF file */
HotswapSession* hotswap_create(const char* gguf_path);

/* Create from existing GGUF context (takes ownership) */
HotswapSession* hotswap_from_context(GGUFContext* ctx);

/* Destroy session */
void hotswap_destroy(HotswapSession* session);

/* Suspend inference engine for weight modification */
bool hotswap_suspend(HotswapSession* session, InferenceEngine* engine);

/* Resume inference engine after modification */
bool hotswap_resume(HotswapSession* session, InferenceEngine* engine);

/* ═══════════════════════════════════════════════════════════════════════════
WEIGHT MANIPULATION
═══════════════════════════════════════════════════════════════════════════ */

/* Get tensor by name */
WeightTensor* hotswap_get_tensor(HotswapSession* session, const char* name);

/* Get tensor by index */
WeightTensor* hotswap_get_tensor_idx(HotswapSession* session, uint32_t index);

/* Re-quantize tensor to new type (in-place) */
bool hotswap_requant_tensor(
    HotswapSession* session,
    const char* name,
    GGMLType new_type
);

/* Re-quantize all tensors matching pattern */
typedef bool (*TensorFilter)(const WeightTensor* tensor, void* user_data);

uint32_t hotswap_requant_filtered(
    HotswapSession* session,
    GGMLType new_type,
    TensorFilter filter,
    void* user_data
);

/* Re-quantize specific tensor types only */
uint32_t hotswap_requant_type(
    HotswapSession* session,
    GGMLType src_type,
    GGMLType dst_type
);

/* ═══════════════════════════════════════════════════════════════════════════
MEMORY MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

/* Allocate memory pool for swaps */
bool hotswap_allocate_pool(HotswapSession* session, size_t size);

/* Get current memory usage */
typedef struct {
    size_t original_bytes; /* Original model size */
    size_t current_bytes; /* Current size after quantization */
    size_t pool_allocated; /* Pool size */
    size_t pool_used; /* Pool used */
    float savings_percent; /* Memory saved */
} HotswapMemory;

HotswapMemory hotswap_get_memory(HotswapSession* session);

/* ═══════════════════════════════════════════════════════════════════════════
BACKUP / RESTORE
═══════════════════════════════════════════════════════════════════════════ */

/* Create backup of current weights */
bool hotswap_backup(HotswapSession* session);

/* Restore from backup */
bool hotswap_restore(HotswapSession* session);

/* Clear backup */
void hotswap_clear_backup(HotswapSession* session);

/* ═══════════════════════════════════════════════════════════════════════════
QUALITY MEASUREMENT
═══════════════════════════════════════════════════════════════════════════ */

/* Measure quality loss from re-quantization */
typedef struct {
    char tensor_name[128];
    float mse;
    float max_error;
    float relative_error;
    float snr_db;
    bool is_acceptable;
} TensorQuality;

TensorQuality hotswap_measure_quality(
    HotswapSession* session,
    const char* name
);

/* Batch quality measurement */
typedef struct {
    TensorQuality* tensors;
    uint32_t count;
    float avg_mse;
    float avg_snr_db;
    float worst_snr_db;
    uint32_t acceptable_count;
    uint32_t unacceptable_count;
} BatchQuality;

BatchQuality hotswap_measure_all(HotswapSession* session);

/* ═══════════════════════════════════════════════════════════════════════════
CONFIGURATION PROFILES
═══════════════════════════════════════════════════════════════════════════ */

/* Quantization profile */
typedef struct {
    char name[64];
    GGMLType default_type;
    GGMLType attention_type; /* Attention weights */
    GGMLType feedforward_type; /* FFN weights */
    GGMLType embedding_type; /* Embeddings */
    GGMLType output_type; /* Output layer */
    float quality_threshold; /* Min SNR in dB */
    size_t target_vram; /* Target VRAM usage */
} QuantProfile;

/* Built-in profiles */
static const QuantProfile QUANT_PROFILE_SPEED = {
    .name = "speed",
    .default_type = GGML_RXD_TYPE_Q4_0,
    .attention_type = GGML_RXD_TYPE_Q4_0,
    .feedforward_type = GGML_RXD_TYPE_Q4_0,
    .embedding_type = GGML_RXD_TYPE_Q8_0,
    .output_type = GGML_RXD_TYPE_Q8_0,
    .quality_threshold = 15.0f, /* 15 dB minimum SNR */
    .target_vram = 0 /* No limit */
};

static const QuantProfile QUANT_PROFILE_BALANCED = {
    .name = "balanced",
    .default_type = GGML_RXD_TYPE_Q4_K,
    .attention_type = GGML_RXD_TYPE_Q5_K,
    .feedforward_type = GGML_RXD_TYPE_Q4_K,
    .embedding_type = GGML_RXD_TYPE_Q8_0,
    .output_type = GGML_RXD_TYPE_Q6_K,
    .quality_threshold = 20.0f,
    .target_vram = 0
};

static const QuantProfile QUANT_PROFILE_QUALITY = {
    .name = "quality",
    .default_type = GGML_RXD_TYPE_Q6_K,
    .attention_type = GGML_RXD_TYPE_Q6_K,
    .feedforward_type = GGML_RXD_TYPE_Q5_K,
    .embedding_type = GGML_RXD_TYPE_F16,
    .output_type = GGML_RXD_TYPE_F16,
    .quality_threshold = 30.0f,
    .target_vram = 0
};

/* Apply profile - returns number of tensors modified */
uint32_t hotswap_apply_profile(
    HotswapSession* session,
    const QuantProfile* profile
);

/* Auto-select profile for VRAM constraint */
const QuantProfile* hotswap_select_profile(
    HotswapSession* session,
    size_t target_vram_bytes
);

/* ═══════════════════════════════════════════════════════════════════════════
INTEGRATION WITH INFERENCE ENGINE
═══════════════════════════════════════════════════════════════════════════ */

/* Get tensor pointers for inference engine */
void** hotswap_get_tensor_ptrs(HotswapSession* session, uint32_t* count);

/* Validate all tensors are ready for inference */
bool hotswap_validate(HotswapSession* session);

/* Export modified weights to new GGUF file */
bool hotswap_export(
    HotswapSession* session,
    const char* output_path
);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_WEIGHT_HOTSWAP_H */
