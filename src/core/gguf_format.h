#ifndef RAWRXD_GGUF_FORMAT_H
#define RAWRXD_GGUF_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
GGUF FILE FORMAT - ACTUAL BINARY LAYOUT

This is the real format used by llama.cpp/ggml. No simulations.
═══════════════════════════════════════════════════════════════════════════ */

/* GGUF Magic Number */
#define GGUF_MAGIC 0x46554747 /* "GGUF" in little-endian */

/* GGUF Version */
#define GGUF_VERSION_3 3

/* GGUF Value Types */
typedef enum {
    GGUF_TYPE_UINT8 = 0,
    GGUF_TYPE_INT8 = 1,
    GGUF_TYPE_UINT16 = 2,
    GGUF_TYPE_INT16 = 3,
    GGUF_TYPE_UINT32 = 4,
    GGUF_TYPE_INT32 = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL = 7,
    GGUF_TYPE_STRING = 8,
    GGUF_TYPE_ARRAY = 9,
    GGUF_TYPE_UINT64 = 10,
    GGUF_TYPE_INT64 = 11,
    GGUF_TYPE_FLOAT64 = 12
} GGUFValueType;

/* GGML Tensor Types (Quantization Formats) */
typedef enum {
    GGML_RXD_TYPE_F32 = 0,
    GGML_RXD_TYPE_F16 = 1,
    GGML_RXD_TYPE_Q4_0 = 2, /* 4-bit, block 32, scale */
    GGML_RXD_TYPE_Q4_1 = 3, /* 4-bit, block 32, scale + min */
    GGML_RXD_TYPE_Q4_2 = 4, /* (unused) */
    GGML_RXD_TYPE_Q4_3 = 5, /* (unused) */
    GGML_RXD_TYPE_Q5_0 = 6, /* 5-bit, block 32, scale */
    GGML_RXD_TYPE_Q5_1 = 7, /* 5-bit, block 32, scale + min */
    GGML_RXD_TYPE_Q8_0 = 8, /* 8-bit, block 32, scale */
    GGML_RXD_TYPE_Q8_1 = 9, /* 8-bit, block 32, scale + min */
    GGML_RXD_TYPE_Q2_K = 10, /* 2-bit K-quant */
    GGML_RXD_TYPE_Q3_K = 11, /* 3-bit K-quant */
    GGML_RXD_TYPE_Q4_K = 12, /* 4-bit K-quant */
    GGML_RXD_TYPE_Q5_K = 13, /* 5-bit K-quant */
    GGML_RXD_TYPE_Q6_K = 14, /* 6-bit K-quant */
    GGML_RXD_TYPE_Q8_K = 15, /* 8-bit K-quant */
    GGML_RXD_TYPE_IQ2_XXS = 16, /* 2-bit IQ */
    GGML_RXD_TYPE_IQ2_XS = 17,
    GGML_RXD_TYPE_IQ3_XXS = 18,
    GGML_RXD_TYPE_IQ1_S = 19,
    GGML_RXD_TYPE_IQ1_M = 20,
    GGML_RXD_TYPE_IQ4_NL = 21,
    GGML_RXD_TYPE_IQ3_S = 22,
    GGML_RXD_TYPE_IQ2_S = 23,
    GGML_RXD_TYPE_IQ4_XS = 24,
    GGML_RXD_TYPE_I8 = 25,
    GGML_RXD_TYPE_I16 = 26,
    GGML_RXD_TYPE_I32 = 27,
    GGML_RXD_TYPE_I64 = 28,
    GGML_RXD_TYPE_F64 = 29,
    GGML_RXD_TYPE_BF16 = 30,
    GGML_RXD_TYPE_COUNT
} GGMLType;

/* Block sizes for each quantization type */
static const size_t GGML_RXD_BLOCK_SIZE[GGML_RXD_TYPE_COUNT] = {
    [GGML_RXD_TYPE_F32] = 1,
    [GGML_RXD_TYPE_F16] = 1,
    [GGML_RXD_TYPE_Q4_0] = 32,
    [GGML_RXD_TYPE_Q4_1] = 32,
    [GGML_RXD_TYPE_Q5_0] = 32,
    [GGML_RXD_TYPE_Q5_1] = 32,
    [GGML_RXD_TYPE_Q8_0] = 32,
    [GGML_RXD_TYPE_Q8_1] = 32,
    [GGML_RXD_TYPE_Q2_K] = 256,
    [GGML_RXD_TYPE_Q3_K] = 256,
    [GGML_RXD_TYPE_Q4_K] = 256,
    [GGML_RXD_TYPE_Q5_K] = 256,
    [GGML_RXD_TYPE_Q6_K] = 256,
    [GGML_RXD_TYPE_Q8_K] = 256,
};

/* GGUF Header */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} GGUFHeader;

/* GGUF String */
typedef struct {
    uint64_t len;
    char* data;
} GGUFString;

/* GGUF Metadata Value */
typedef struct {
    uint32_t type;
    union {
        uint8_t uint8;
        int8_t int8;
        uint16_t uint16;
        int16_t int16;
        uint32_t uint32;
        int32_t int32;
        float float32;
        bool bool_val;
        GGUFString string;
        struct {
            uint32_t type;
            uint64_t count;
            void* data;
        } array;
        uint64_t uint64;
        int64_t int64;
        double float64;
    } value;
} GGUFValue;

/* GGUF Metadata KV */
typedef struct {
    GGUFString key;
    GGUFValue value;
} GGUFMetadataKV;

/* GGUF Tensor Info */
typedef struct {
    GGUFString name;
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset; /* Offset into data section */
} GGUFTensorInfo;

/* GGUF File Context */
typedef struct {
    /* File handle */
    void* file_handle; /* HANDLE on Windows, int fd on Linux */
    void* file_mapping; /* Memory mapping handle */
    void* file_view; /* Base pointer */
    size_t file_size; /* Total file size */

    /* Header */
    GGUFHeader header;
    GGUFMetadataKV* metadata;
    uint64_t metadata_count;

    /* Tensors */
    GGUFTensorInfo* tensor_infos;
    uint64_t tensor_count;
    uint64_t data_offset; /* Offset to tensor data */

    /* Alignment */
    uint64_t alignment; /* Usually 32 or 64 */

    /* State */
    bool is_loaded;
    char error_msg[256];
} GGUFContext;

/* ═══════════════════════════════════════════════════════════════════════════
GGUF LOADING FUNCTIONS
═══════════════════════════════════════════════════════════════════════════ */

/* Load GGUF file using memory mapping (no 2GB limit) */
GGUFContext* gguf_load(const char* path);

/* Load from memory (for embedded models) */
GGUFContext* gguf_load_from_memory(const void* data, size_t size);

/* Get tensor data pointer (zero-copy) */
void* gguf_get_tensor_data(const GGUFContext* ctx, const char* name, size_t* out_size);

/* Get tensor info */
const GGUFTensorInfo* gguf_get_tensor_info(const GGUFContext* ctx, const char* name);

/* Get metadata value */
const GGUFValue* gguf_get_metadata(const GGUFContext* ctx, const char* key);

/* Get tensor count */
uint64_t gguf_get_tensor_count(const GGUFContext* ctx);

/* Close and free */
void gguf_close(GGUFContext* ctx);

/* ═══════════════════════════════════════════════════════════════════════════
TENSOR SHAPE UTILITIES
═══════════════════════════════════════════════════════════════════════════ */

/* Calculate number of elements in tensor */
static inline uint64_t gguf_tensor_elements(const GGUFTensorInfo* info) {
    uint64_t count = 1;
    for (uint32_t i = 0; i < info->n_dims; i++) {
        count *= info->dims[i];
    }
    return count;
}

/* Get tensor size in bytes */
size_t gguf_tensor_size_bytes(const GGUFTensorInfo* info);

/* Get type name */
const char* gguf_type_name(GGMLType type);

/* Get type size per element (including quantization overhead) */
size_t gguf_type_size(GGMLType type);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_GGUF_FORMAT_H */
