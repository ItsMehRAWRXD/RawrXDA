/* weight_hotswap.c - Weight Hotswap Implementation */
#include "weight_hotswap.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Profiles are defined in header as static const */

/* ═══════════════════════════════════════════════════════════════════════════
GGUF STUB IMPLEMENTATIONS
These are simplified implementations for testing. Full implementation uses MASM.
═══════════════════════════════════════════════════════════════════════════ */

/* Stub GGUF context for testing */
typedef struct {
    void* file_handle;
    void* file_mapping;
    void* file_view;
    size_t file_size;
    uint64_t tensor_count;
    void* tensor_infos;
    uint64_t data_offset;
    char error_msg[256];
} StubGGUFContext;

GGUFContext* gguf_load(const char* path) {
    /* For now, return a stub context */
    StubGGUFContext* ctx = calloc(1, sizeof(StubGGUFContext));
    return (GGUFContext*)ctx;
}

void* gguf_get_tensor_data(const GGUFContext* ctx, const char* name, size_t* out_size) {
    if (!ctx || !name) return NULL;
    if (out_size) *out_size = 0;
    return NULL;
}

uint64_t gguf_get_tensor_count(const GGUFContext* ctx) {
    if (!ctx) return 0;
    return ((const StubGGUFContext*)ctx)->tensor_count;
}

void gguf_close(GGUFContext* ctx) {
    if (ctx) free(ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
SESSION MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

HotswapSession* hotswap_create(const char* gguf_path) {
    if (!gguf_path) return NULL;
    
    HotswapSession* session = calloc(1, sizeof(HotswapSession));
    if (!session) return NULL;
    
    /* Load GGUF context */
    session->gguf = gguf_load(gguf_path);
    if (!session->gguf) {
        free(session);
        return NULL;
    }
    
    /* Get tensor count */
    session->tensor_count = (uint32_t)gguf_get_tensor_count(session->gguf);
    if (session->tensor_count == 0) {
        gguf_close(session->gguf);
        free(session);
        return NULL;
    }
    
    /* Allocate tensor array */
    session->tensors = calloc(session->tensor_count, sizeof(WeightTensor));
    if (!session->tensors) {
        gguf_close(session->gguf);
        free(session);
        return NULL;
    }
    
    /* Initialize tensors */
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        /* Get tensor info by index - need to iterate through tensor_infos */
        const GGUFTensorInfo* info = &session->gguf->tensor_infos[i];
        
        if (info) {
            strncpy(tensor->name, info->name.data, sizeof(tensor->name) - 1);
            tensor->current_type = (GGMLType)info->type;
            tensor->original_type = tensor->current_type;
            
            size_t tensor_size = 0;
            tensor->data = gguf_get_tensor_data(session->gguf, info->name.data, &tensor_size);
            tensor->original_data = tensor->data;
            tensor->size = tensor_size;
            tensor->original_size = tensor_size;
            tensor->elements = gguf_tensor_elements(info);
            tensor->buffer_type = WEIGHT_BUFFER_MAPPED;
            tensor->is_swapped = false;
            tensor->is_locked = false;
            
            session->total_original_size += tensor->size;
        }
    }
    
    session->total_current_size = session->total_original_size;
    session->compression_ratio = 1.0f;
    session->has_backup = false;
    session->is_suspended = false;
    
    return session;
}

HotswapSession* hotswap_from_context(GGUFContext* ctx) {
    if (!ctx) return NULL;
    
    HotswapSession* session = calloc(1, sizeof(HotswapSession));
    if (!session) return NULL;
    
    session->gguf = ctx;
    session->tensor_count = (uint32_t)gguf_get_tensor_count(ctx);
    
    if (session->tensor_count == 0) {
        free(session);
        return NULL;
    }
    
    session->tensors = calloc(session->tensor_count, sizeof(WeightTensor));
    if (!session->tensors) {
        free(session);
        return NULL;
    }
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        const GGUFTensorInfo* info = &ctx->tensor_infos[i];
        
        if (info) {
            strncpy(tensor->name, info->name.data, sizeof(tensor->name) - 1);
            tensor->current_type = (GGMLType)info->type;
            tensor->original_type = tensor->current_type;
            
            size_t tensor_size = 0;
            tensor->data = gguf_get_tensor_data(ctx, info->name.data, &tensor_size);
            tensor->original_data = tensor->data;
            tensor->size = tensor_size;
            tensor->original_size = tensor_size;
            tensor->elements = gguf_tensor_elements(info);
            tensor->buffer_type = WEIGHT_BUFFER_MAPPED;
            tensor->is_swapped = false;
            tensor->is_locked = false;
            
            session->total_original_size += tensor->size;
        }
    }
    
    session->total_current_size = session->total_original_size;
    session->compression_ratio = 1.0f;
    session->has_backup = false;
    session->is_suspended = false;
    
    return session;
}

void hotswap_destroy(HotswapSession* session) {
    if (!session) return;
    
    /* Restore original weights if swapped */
    if (session->has_backup) {
        hotswap_restore(session);
    }
    
    /* Free pool */
    if (session->weight_pool) {
        free(session->weight_pool);
    }
    
    /* Free tensors */
    free(session->tensors);
    
    /* Free GGUF context */
    if (session->gguf) {
        gguf_close(session->gguf);
    }
    
    free(session);
}

bool hotswap_suspend(HotswapSession* session, InferenceEngine* engine) {
    if (!session || !engine) return false;
    
    session->is_suspended = true;
    return true;
}

bool hotswap_resume(HotswapSession* session, InferenceEngine* engine) {
    if (!session || !engine) return false;
    
    session->is_suspended = false;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
WEIGHT MANIPULATION
═══════════════════════════════════════════════════════════════════════════ */

WeightTensor* hotswap_get_tensor(HotswapSession* session, const char* name) {
    if (!session || !name) return NULL;
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        if (strcmp(session->tensors[i].name, name) == 0) {
            return &session->tensors[i];
        }
    }
    
    return NULL;
}

WeightTensor* hotswap_get_tensor_idx(HotswapSession* session, uint32_t index) {
    if (!session || index >= session->tensor_count) return NULL;
    return &session->tensors[index];
}

bool hotswap_requant_tensor(
    HotswapSession* session,
    const char* name,
    GGMLType new_type
) {
    if (!session || !name) return false;
    
    WeightTensor* tensor = hotswap_get_tensor(session, name);
    if (!tensor) return false;
    
    if (tensor->is_locked) {
        snprintf(session->error_msg, sizeof(session->error_msg),
                 "Tensor '%s' is locked", name);
        return false;
    }
    
    /* Create backup if needed */
    if (!session->has_backup) {
        hotswap_backup(session);
    }
    
    /* Dequantize to FP32 */
    DequantResult fp32 = dequant_tensor(tensor->data, tensor->current_type, tensor->elements);
    if (!fp32.data) {
        snprintf(session->error_msg, sizeof(session->error_msg),
                 "Dequantization failed for '%s'", name);
        return false;
    }
    
    /* Re-quantize to new type */
    QuantResult quant = quant_tensor(fp32.data, tensor->elements, new_type);
    dequant_result_free(&fp32);
    
    if (!quant.data) {
        snprintf(session->error_msg, sizeof(session->error_msg),
                 "Quantization failed for '%s'", name);
        return false;
    }
    
    /* Update tensor */
    tensor->data = quant.data;
    tensor->size = quant.size;
    tensor->current_type = new_type;
    tensor->is_swapped = true;
    
    /* Update session stats */
    session->total_current_size -= tensor->original_size;
    session->total_current_size += tensor->size;
    session->compression_ratio = (float)session->total_original_size / session->total_current_size;
    
    return true;
}

uint32_t hotswap_requant_filtered(
    HotswapSession* session,
    GGMLType new_type,
    TensorFilter filter,
    void* user_data
) {
    if (!session) return 0;
    
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        if (filter && !filter(tensor, user_data)) {
            continue;
        }
        
        if (hotswap_requant_tensor(session, tensor->name, new_type)) {
            count++;
        }
    }
    
    return count;
}

uint32_t hotswap_requant_type(
    HotswapSession* session,
    GGMLType src_type,
    GGMLType dst_type
) {
    if (!session) return 0;
    
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        if (tensor->current_type == src_type) {
            if (hotswap_requant_tensor(session, tensor->name, dst_type)) {
                count++;
            }
        }
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
MEMORY MANAGEMENT
═══════════════════════════════════════════════════════════════════════════ */

bool hotswap_allocate_pool(HotswapSession* session, size_t size) {
    if (!session) return false;
    
    session->weight_pool = malloc(size);
    if (!session->weight_pool) return false;
    
    session->pool_size = size;
    session->pool_used = 0;
    
    return true;
}

HotswapMemory hotswap_get_memory(HotswapSession* session) {
    HotswapMemory mem = {0};
    
    if (!session) return mem;
    
    mem.original_bytes = session->total_original_size;
    mem.current_bytes = session->total_current_size;
    mem.pool_allocated = session->pool_size;
    mem.pool_used = session->pool_used;
    
    if (mem.original_bytes > 0) {
        mem.savings_percent = 100.0f * (1.0f - (float)mem.current_bytes / mem.original_bytes);
    }
    
    return mem;
}

/* ═══════════════════════════════════════════════════════════════════════════
BACKUP / RESTORE
═══════════════════════════════════════════════════════════════════════════ */

bool hotswap_backup(HotswapSession* session) {
    if (!session) return false;
    
    /* Already backed up */
    if (session->has_backup) return true;
    
    /* Allocate pool for backups */
    size_t backup_size = session->total_original_size;
    if (!hotswap_allocate_pool(session, backup_size)) {
        return false;
    }
    
    /* Copy original data to pool */
    size_t offset = 0;
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        if (tensor->original_data && tensor->original_size > 0) {
            memcpy((char*)session->weight_pool + offset,
                   tensor->original_data,
                   tensor->original_size);
            tensor->original_data = (char*)session->weight_pool + offset;
            offset += tensor->original_size;
        }
    }
    
    session->has_backup = true;
    return true;
}

bool hotswap_restore(HotswapSession* session) {
    if (!session) return false;
    
    /* No backup to restore */
    if (!session->has_backup) return true;
    
    /* Restore all tensors */
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        
        if (tensor->is_swapped) {
            /* Free swapped data */
            if (tensor->data != tensor->original_data) {
                free(tensor->data);
            }
            
            /* Restore original */
            tensor->data = tensor->original_data;
            tensor->size = tensor->original_size;
            tensor->current_type = tensor->original_type;
            tensor->is_swapped = false;
        }
    }
    
    /* Reset stats */
    session->total_current_size = session->total_original_size;
    session->compression_ratio = 1.0f;
    
    return true;
}

void hotswap_clear_backup(HotswapSession* session) {
    if (!session) return;
    
    if (session->weight_pool) {
        free(session->weight_pool);
        session->weight_pool = NULL;
        session->pool_size = 0;
        session->pool_used = 0;
    }
    
    session->has_backup = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
QUALITY MEASUREMENT
═══════════════════════════════════════════════════════════════════════════ */

TensorQuality hotswap_measure_quality(
    HotswapSession* session,
    const char* name
) {
    TensorQuality quality = {0};
    
    if (!session || !name) {
        strcpy(quality.tensor_name, "ERROR");
        return quality;
    }
    
    WeightTensor* tensor = hotswap_get_tensor(session, name);
    if (!tensor) {
        snprintf(quality.tensor_name, sizeof(quality.tensor_name), "NOT_FOUND:%s", name);
        return quality;
    }
    
    /* Need original data */
    if (!tensor->original_data) {
        strcpy(quality.tensor_name, "NO_ORIGINAL");
        return quality;
    }
    
    /* Dequantize original */
    DequantResult original_fp32 = dequant_tensor(
        tensor->original_data,
        tensor->original_type,
        tensor->elements
    );
    
    /* Dequantize current */
    DequantResult current_fp32 = dequant_tensor(
        tensor->data,
        tensor->current_type,
        tensor->elements
    );
    
    if (!original_fp32.data || !current_fp32.data) {
        dequant_result_free(&original_fp32);
        dequant_result_free(&current_fp32);
        strcpy(quality.tensor_name, "DEQUANT_FAILED");
        return quality;
    }
    
    /* Measure quality */
    QuantQuality q = estimate_quant_quality(
        original_fp32.data,
        current_fp32.data,
        tensor->elements
    );
    
    strncpy(quality.tensor_name, tensor->name, sizeof(quality.tensor_name) - 1);
    quality.mse = q.mse;
    quality.max_error = q.max_error;
    quality.relative_error = q.relative_error;
    quality.snr_db = q.snr_db;
    quality.is_acceptable = q.snr_db >= 15.0f; /* Default threshold */
    
    dequant_result_free(&original_fp32);
    dequant_result_free(&current_fp32);
    
    return quality;
}

BatchQuality hotswap_measure_all(HotswapSession* session) {
    BatchQuality batch = {0};
    
    if (!session) return batch;
    
    batch.tensors = calloc(session->tensor_count, sizeof(TensorQuality));
    if (!batch.tensors) return batch;
    
    batch.count = session->tensor_count;
    batch.avg_mse = 0.0f;
    batch.avg_snr_db = 0.0f;
    batch.worst_snr_db = 100.0f;
    batch.acceptable_count = 0;
    batch.unacceptable_count = 0;
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        TensorQuality q = hotswap_measure_quality(session, session->tensors[i].name);
        batch.tensors[i] = q;
        
        batch.avg_mse += q.mse;
        batch.avg_snr_db += q.snr_db;
        
        if (q.snr_db < batch.worst_snr_db) {
            batch.worst_snr_db = q.snr_db;
        }
        
        if (q.is_acceptable) {
            batch.acceptable_count++;
        } else {
            batch.unacceptable_count++;
        }
    }
    
    if (batch.count > 0) {
        batch.avg_mse /= batch.count;
        batch.avg_snr_db /= batch.count;
    }
    
    return batch;
}

/* ═══════════════════════════════════════════════════════════════════════════
PROFILE APPLICATION
═══════════════════════════════════════════════════════════════════════════ */

uint32_t hotswap_apply_profile(
    HotswapSession* session,
    const QuantProfile* profile
) {
    if (!session || !profile) return 0;
    
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < session->tensor_count; i++) {
        WeightTensor* tensor = &session->tensors[i];
        GGMLType target_type = profile->default_type;
        
        /* Determine target type based on tensor name */
        const char* name = tensor->name;
        
        if (strstr(name, "attn") || strstr(name, "attention")) {
            target_type = profile->attention_type;
        } else if (strstr(name, "ffn") || strstr(name, "feedforward")) {
            target_type = profile->feedforward_type;
        } else if (strstr(name, "embed") || strstr(name, "token")) {
            target_type = profile->embedding_type;
        } else if (strstr(name, "output") || strstr(name, "head")) {
            target_type = profile->output_type;
        }
        
        /* Skip if already target type */
        if (tensor->current_type == target_type) {
            continue;
        }
        
        /* Re-quantize */
        if (hotswap_requant_tensor(session, tensor->name, target_type)) {
            count++;
        }
    }
    
    return count;
}