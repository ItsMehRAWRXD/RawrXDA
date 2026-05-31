// gguf_loader.h
// Real GGUF binary parser for RawrXD
// Parses GGUF v3 format: magic, version, tensor info, metadata

#ifndef GGUF_LOADER_H
#define GGUF_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mmap_file.h"

#ifdef __cplusplus
extern "C" {
#endif

// GGUF v3 constants
#define GGUF_MAGIC   0x46554747  // "GGUF" in little-endian
#define GGUF_VERSION 3

// GGML types (subset)
typedef enum {
    GGML_TYPE_F32  = 0,
    GGML_TYPE_F16  = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q8_1 = 9,
    GGML_TYPE_Q2_K = 10,
    GGML_TYPE_Q3_K = 11,
    GGML_TYPE_Q4_K = 12,
    GGML_TYPE_Q5_K = 13,
    GGML_TYPE_Q6_K = 14,
    GGML_TYPE_Q8_K = 15,
    GGML_TYPE_IQ2_XXS = 16,
    GGML_TYPE_IQ2_XS  = 17,
    GGML_TYPE_IQ3_XXS = 18,
    GGML_TYPE_IQ1_S   = 19,
    GGML_TYPE_IQ4_NL  = 20,
    GGML_TYPE_IQ3_S   = 21,
    GGML_TYPE_IQ4_XS  = 22,
    GGML_TYPE_I8      = 23,
    GGML_TYPE_I16     = 24,
    GGML_TYPE_I32     = 25,
    GGML_TYPE_I64     = 26,
    GGML_TYPE_F64     = 27,
    GGML_TYPE_IQ1_M   = 28,
    GGML_TYPE_COUNT
} ggml_type_t;

static const char* ggml_type_name(int type) {
    switch (type) {
        case GGML_TYPE_F32:  return "f32";
        case GGML_TYPE_F16:  return "f16";
        case GGML_TYPE_Q4_0: return "q4_0";
        case GGML_TYPE_Q4_1: return "q4_1";
        case GGML_TYPE_Q5_0: return "q5_0";
        case GGML_TYPE_Q5_1: return "q5_1";
        case GGML_TYPE_Q8_0: return "q8_0";
        case GGML_TYPE_Q8_1: return "q8_1";
        case GGML_TYPE_Q2_K: return "q2_K";
        case GGML_TYPE_Q3_K: return "q3_K";
        case GGML_TYPE_Q4_K: return "q4_K";
        case GGML_TYPE_Q5_K: return "q5_K";
        case GGML_TYPE_Q6_K: return "q6_K";
        case GGML_TYPE_Q8_K: return "q8_K";
        default: return "unknown";
    }
}

static size_t ggml_type_size(int type) {
    switch (type) {
        case GGML_TYPE_F32:  return 4;
        case GGML_TYPE_F16:  return 2;
        case GGML_TYPE_Q4_0: return 18;  // 32 weights: 16 bytes + 2 bytes (scale/min)
        case GGML_TYPE_Q4_1: return 20;  // 32 weights: 16 bytes + 4 bytes (scale/min as fp16)
        case GGML_TYPE_Q8_0: return 34;  // 32 weights: 32 bytes + 2 bytes (scale)
        default: return 1;
    }
}

// GGUF header
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
} GGUFHeader;

// Tensor info
typedef struct {
    char name[256];
    int n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset;
    void *data;
    size_t size;
} GGUFTensor;

// GGUF model
typedef struct {
    MMapFile *mmap;
    GGUFHeader header;
    GGUFTensor *tensors;
    uint64_t tensor_data_offset;
    
    // Metadata cache
    int n_vocab;
    int n_ctx;
    int n_embd;
    int n_head;
    int n_layer;
    int n_ff;
} GGUFModel;

// Read a little-endian value
static inline uint32_t read_u32(const uint8_t **ptr) {
    uint32_t val = *(uint32_t*)(*ptr);
    *ptr += 4;
    return val;
}

static inline uint64_t read_u64(const uint8_t **ptr) {
    uint64_t val = *(uint64_t*)(*ptr);
    *ptr += 8;
    return val;
}

static inline int32_t read_i32(const uint8_t **ptr) {
    int32_t val = *(int32_t*)(*ptr);
    *ptr += 4;
    return val;
}

static inline float read_f32(const uint8_t **ptr) {
    float val = *(float*)(*ptr);
    *ptr += 4;
    return val;
}

// Read a GGUF string
static void read_string(const uint8_t **ptr, char *out, size_t max_len) {
    uint64_t len = read_u64(ptr);
    size_t to_copy = (len < max_len - 1) ? len : max_len - 1;
    memcpy(out, *ptr, to_copy);
    out[to_copy] = '\0';
    *ptr += len;
}

// Load GGUF model from file
static inline GGUFModel* gguf_load(const char *path) {
    GGUFModel *model = (GGUFModel*)calloc(1, sizeof(GGUFModel));
    if (!model) return NULL;

    model->mmap = mmap_file_open(path);
    if (!model->mmap) {
        fprintf(stderr, "Failed to mmap: %s\n", path);
        free(model);
        return NULL;
    }

    const uint8_t *ptr = (const uint8_t*)mmap_file_data(model->mmap);
    const uint8_t *end = ptr + mmap_file_size(model->mmap);

    // Parse header
    model->header.magic = read_u32(&ptr);
    model->header.version = read_u32(&ptr);
    model->header.n_tensors = read_u64(&ptr);
    model->header.n_kv = read_u64(&ptr);

    if (model->header.magic != GGUF_MAGIC) {
        fprintf(stderr, "Invalid GGUF magic: 0x%08X (expected 0x%08X)\n",
                model->header.magic, GGUF_MAGIC);
        mmap_file_close(model->mmap);
        free(model);
        return NULL;
    }

    if (model->header.version != GGUF_VERSION) {
        fprintf(stderr, "Warning: GGUF version %d (expected %d)\n",
                model->header.version, GGUF_VERSION);
    }

    printf("GGUF loaded: version=%d, tensors=%llu, kv=%llu\n",
           model->header.version,
           (unsigned long long)model->header.n_tensors,
           (unsigned long long)model->header.n_kv);

    // Skip KV pairs (metadata)
    for (uint64_t i = 0; i < model->header.n_kv; i++) {
        char key[256];
        read_string(&ptr, key, sizeof(key));
        
        uint32_t type = read_u32(&ptr);
        
        // Skip value based on type
        switch (type) {
            case 0: // uint8
            case 1: // int8
            case 2: // uint16
            case 3: // int16
            case 4: // uint32
            case 5: // int32
            case 6: // float32
            case 10: // bool
                ptr += 4;
                break;
            case 7: // uint64
            case 8: // int64
            case 9: // float64
                ptr += 8;
                break;
            case 11: { // string
                uint64_t len = read_u64(&ptr);
                ptr += len;
                break;
            }
            case 12: { // array
                uint32_t arr_type = read_u32(&ptr);
                uint64_t arr_len = read_u64(&ptr);
                // Skip array data
                for (uint64_t j = 0; j < arr_len; j++) {
                    switch (arr_type) {
                        case 0: case 1: case 2: case 3:
                        case 4: case 5: case 6: case 10:
                            ptr += 4; break;
                        case 7: case 8: case 9:
                            ptr += 8; break;
                        case 11: {
                            uint64_t len = read_u64(&ptr);
                            ptr += len;
                            break;
                        }
                        default: ptr += 4;
                    }
                }
                break;
            }
            default:
                ptr += 4;
        }
    }

    // Parse tensor info
    model->tensors = (GGUFTensor*)calloc(model->header.n_tensors, sizeof(GGUFTensor));
    if (!model->tensors) {
        mmap_file_close(model->mmap);
        free(model);
        return NULL;
    }

    for (uint64_t i = 0; i < model->header.n_tensors; i++) {
        GGUFTensor *t = &model->tensors[i];
        read_string(&ptr, t->name, sizeof(t->name));
        t->n_dims = read_u32(&ptr);
        for (int d = 0; d < t->n_dims && d < 4; d++) {
            t->dims[d] = read_u64(&ptr);
        }
        t->type = read_u32(&ptr);
        t->offset = read_u64(&ptr);
        
        // Calculate tensor size
        size_t n_elts = 1;
        for (int d = 0; d < t->n_dims; d++) {
            n_elts *= t->dims[d];
        }
        
        // For quantized types, calculate blocks
        size_t block_size = 32;  // Most GGML types use 32-element blocks
        size_t n_blocks = (n_elts + block_size - 1) / block_size;
        size_t type_sz = ggml_type_size(t->type);
        t->size = n_blocks * type_sz;
    }

    // Tensor data starts here (aligned to 32 bytes)
    model->tensor_data_offset = (uint64_t)(ptr - (const uint8_t*)mmap_file_data(model->mmap));
    model->tensor_data_offset = (model->tensor_data_offset + 31) & ~31;  // Align to 32

    // Set up tensor data pointers
    const uint8_t *tensor_base = (const uint8_t*)mmap_file_data(model->mmap) + model->tensor_data_offset;
    
    for (uint64_t i = 0; i < model->header.n_tensors; i++) {
        GGUFTensor *t = &model->tensors[i];
        t->data = (void*)(tensor_base + t->offset);
    }

    return model;
}

// Get tensor by name
static inline GGUFTensor* gguf_get_tensor(GGUFModel *model, const char *name) {
    if (!model || !model->tensors) return NULL;
    
    for (uint64_t i = 0; i < model->header.n_tensors; i++) {
        if (strcmp(model->tensors[i].name, name) == 0) {
            return &model->tensors[i];
        }
    }
    return NULL;
}

// Print model info
static inline void gguf_print_info(GGUFModel *model) {
    if (!model) return;
    
    printf("\n=== GGUF Model Info ===\n");
    printf("Tensors: %llu\n", (unsigned long long)model->header.n_tensors);
    
    size_t total_size = 0;
    for (uint64_t i = 0; i < model->header.n_tensors; i++) {
        GGUFTensor *t = &model->tensors[i];
        printf("  [%3llu] %-32s type=%-6s dims=",
               (unsigned long long)i, t->name, ggml_type_name(t->type));
        for (int d = 0; d < t->n_dims; d++) {
            printf("%llu%s", (unsigned long long)t->dims[d], d < t->n_dims-1 ? "x" : "");
        }
        printf(" size=%zu\n", t->size);
        total_size += t->size;
    }
    printf("Total tensor data: %.2f MB\n", total_size / (1024.0 * 1024.0));
}

// Unload model
static inline void gguf_free(GGUFModel *model) {
    if (!model) return;
    if (model->tensors) free(model->tensors);
    if (model->mmap) mmap_file_close(model->mmap);
    free(model);
}

#ifdef __cplusplus
}
#endif

#endif // GGUF_LOADER_H
