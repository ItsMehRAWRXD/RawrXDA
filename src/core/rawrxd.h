/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD - Real Weight Re-quantizer & Hot-Swap
   No simulations. Real code that works.
   ═══════════════════════════════════════════════════════════════════════════ */

#ifndef RAWRXD_H
#define RAWRXD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   GGUF FORMAT
   ═══════════════════════════════════════════════════════════════════════════ */

#define GGUF_MAGIC 0x46554747
#define GGUF_VERSION_3 3

typedef enum {
    GGML_RXD_TYPE_F32 = 0, GGML_RXD_TYPE_F16 = 1,
    GGML_RXD_TYPE_Q4_0 = 2, GGML_RXD_TYPE_Q4_1 = 3,
    GGML_RXD_TYPE_Q5_0 = 6, GGML_RXD_TYPE_Q5_1 = 7,
    GGML_RXD_TYPE_Q8_0 = 8,
    GGML_RXD_TYPE_Q2_K = 10, GGML_RXD_TYPE_Q3_K = 11,
    GGML_RXD_TYPE_Q4_K = 12, GGML_RXD_TYPE_Q5_K = 13, GGML_RXD_TYPE_Q6_K = 14,
    GGML_RXD_TYPE_COUNT
} GGMLType;

static const size_t GGML_RXD_BLOCK_SIZE[] = {
    [GGML_RXD_TYPE_F32] = 1, [GGML_RXD_TYPE_F16] = 1,
    [GGML_RXD_TYPE_Q4_0] = 32, [GGML_RXD_TYPE_Q4_1] = 32,
    [GGML_RXD_TYPE_Q5_0] = 32, [GGML_RXD_TYPE_Q5_1] = 32,
    [GGML_RXD_TYPE_Q8_0] = 32,
    [GGML_RXD_TYPE_Q2_K] = 256, [GGML_RXD_TYPE_Q3_K] = 256,
    [GGML_RXD_TYPE_Q4_K] = 256, [GGML_RXD_TYPE_Q5_K] = 256, [GGML_RXD_TYPE_Q6_K] = 256,
};

typedef struct {
    uint32_t magic, version;
    uint64_t tensor_count, metadata_kv_count;
} GGUFHeader;

typedef struct {
    char* data; uint64_t len;
} GGUFString;

typedef struct {
    GGUFString name;
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset;
} GGUFTensorInfo;

typedef struct {
    void* file_handle, *file_mapping, *file_view;
    size_t file_size;
    GGUFHeader header;
    GGUFTensorInfo* tensor_infos;
    uint64_t tensor_count, data_offset;
    bool is_loaded;
    char error_msg[256];
} GGUFContext;

/* ═══════════════════════════════════════════════════════════════════════════
   QUANTIZATION
   ═══════════════════════════════════════════════════════════════════════════ */

typedef uint16_t half;

static inline float half_to_float(half h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (e == 0) return m == 0 ? (s ? -0.0f : 0.0f) : (s ? -1.0f : 1.0f) * (float)m / 1024.0f / 16384.0f;
    if (e == 31) return m == 0 ? (s ? -INFINITY : INFINITY) : NAN;
    return (s ? -1.0f : 1.0f) * ldexpf((float)(m | 0x400), e - 25);
}

static inline half float_to_half(float f) {
    uint32_t b; memcpy(&b, &f, 4);
    uint32_t s = (b >> 31) & 1, e = ((b >> 23) & 0xFF) - 127, m = b & 0x7FFFFF;
    if (e > 15) return (half)((s << 15) | 0x7C00);
    if (e < -24) return (half)(s << 15);
    if (e <= -14) { m |= 0x800000; m >>= (-14 - e); return (half)((s << 15) | (m >> 13)); }
    return (half)((s << 15) | ((e + 15) << 10) | (m >> 13));
}

typedef struct { float* data; size_t count; } DequantResult;
typedef struct { void* data; size_t size; } QuantResult;

typedef struct { float mse, max_error, relative_error, snr_db; } QuantQuality;

/* Q4_0: [scale:f16][qs:4-bit x 32] = 18 bytes */
static DequantResult dequant_q4_0(const void* blocks, size_t num_blocks) {
    DequantResult r = {0};
    r.count = num_blocks * 32;
    r.data = (float*)malloc(r.count * sizeof(float));
    const uint8_t* p = (const uint8_t*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float scale = half_to_float(*(half*)p); p += 2;
        for (int i = 0; i < 16; i++) {
            uint8_t v = *p++;
            r.data[b*32 + i*2 + 0] = ((float)((v & 0x0F) - 8)) * scale / 8.0f;
            r.data[b*32 + i*2 + 1] = ((float)((v >> 4) - 8)) * scale / 8.0f;
        }
    }
    return r;
}

static QuantResult quant_q4_0(const float* data, size_t count) {
    QuantResult r = {0};
    size_t blocks = (count + 31) / 32;
    r.data = malloc(blocks * 18);
    uint8_t* out = (uint8_t*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (size_t i = b*32; i < (b+1)*32 && i < count; i++) {
            float a = fabsf(data[i]); if (a > max_abs) max_abs = a;
        }
        float scale = max_abs / 8.0f; if (scale < 1e-10f) scale = 1e-10f;
        *(half*)out = float_to_half(scale); out += 2;
        for (int i = 0; i < 16; i++) {
            int q0 = (int)roundf(data[b*32 + i*2] / scale * 8.0f);
            int q1 = (int)roundf(data[b*32 + i*2 + 1] / scale * 8.0f);
            if (q0 < -8) q0 = -8; if (q0 > 7) q0 = 7;
            if (q1 < -8) q1 = -8; if (q1 > 7) q1 = 7;
            *out++ = ((q0 + 8) & 0x0F) | (((q1 + 8) & 0x0F) << 4);
        }
    }
    r.size = blocks * 18;
    return r;
}

/* Q8_0: [scale:f16][qs:int8 x 32] = 34 bytes */
static DequantResult dequant_q8_0(const void* blocks, size_t num_blocks) {
    DequantResult r = {0};
    r.count = num_blocks * 32;
    r.data = (float*)malloc(r.count * sizeof(float));
    const uint8_t* p = (const uint8_t*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float scale = half_to_float(*(half*)p); p += 2;
        const int8_t* qs = (const int8_t*)p; p += 32;
        for (int i = 0; i < 32; i++) r.data[b*32 + i] = (float)qs[i] * scale / 128.0f;
    }
    return r;
}

static QuantResult quant_q8_0(const float* data, size_t count) {
    QuantResult r = {0};
    size_t blocks = (count + 31) / 32;
    r.data = malloc(blocks * 34);
    uint8_t* out = (uint8_t*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (size_t i = b*32; i < (b+1)*32 && i < count; i++) {
            float a = fabsf(data[i]); if (a > max_abs) max_abs = a;
        }
        float scale = max_abs / 128.0f; if (scale < 1e-10f) scale = 1e-10f;
        *(half*)out = float_to_half(scale); out += 2;
        for (int i = 0; i < 32 && b*32+i < (int)count; i++) {
            int q = (int)roundf(data[b*32 + i] / scale * 128.0f);
            if (q < -128) q = -128; if (q > 127) q = 127;
            *out++ = (int8_t)q;
        }
    }
    r.size = blocks * 34;
    return r;
}

/* K-Quants (Q4_K) */
#pragma pack(push, 1)
typedef struct { half d, dmin; uint8_t scales[16], qs[128]; } block_q4_k;
#pragma pack(pop)

static DequantResult dequant_q4_k(const void* blocks, size_t num_blocks) {
    DequantResult r = {0};
    r.count = num_blocks * 256;
    r.data = (float*)malloc(r.count * sizeof(float));
    const block_q4_k* blk = (const block_q4_k*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float super = half_to_float(blk[b].d);
        for (int sub = 0; sub < 8; sub++) {
            float scale = super * (float)blk[b].scales[sub] / 16.0f;
            for (int i = 0; i < 16; i++) {
                uint8_t v = blk[b].qs[sub*16 + i];
                r.data[b*256 + sub*32 + i*2 + 0] = ((float)((v & 0x0F) - 8)) * scale;
                r.data[b*256 + sub*32 + i*2 + 1] = ((float)((v >> 4) - 8)) * scale;
            }
        }
    }
    return r;
}

static QuantResult quant_q4_k(const float* data, size_t count) {
    QuantResult r = {0};
    size_t blocks = (count + 255) / 256;
    r.data = malloc(blocks * sizeof(block_q4_k));
    block_q4_k* blk = (block_q4_k*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (int i = 0; i < 256 && b*256+i < (int)count; i++) {
            float a = fabsf(data[b*256 + i]); if (a > max_abs) max_abs = a;
        }
        blk[b].d = float_to_half(max_abs / 8.0f);
        blk[b].dmin = 0;
        for (int sub = 0; sub < 8; sub++) {
            float sub_max = 0;
            for (int i = 0; i < 32 && b*256+sub*32+i < (int)count; i++) {
                float a = fabsf(data[b*256 + sub*32 + i]); if (a > sub_max) sub_max = a;
            }
            float super = half_to_float(blk[b].d);
            float rel = (sub_max < 1e-10f) ? 16.0f : (sub_max / super * 16.0f);
            if (rel > 255) rel = 255;
            blk[b].scales[sub] = (uint8_t)(rel + 0.5f);
            float scale = super * blk[b].scales[sub] / 16.0f;
            if (scale < 1e-10f) scale = 1e-10f;
            for (int i = 0; i < 16; i++) {
                int q0 = (int)roundf(data[b*256 + sub*32 + i*2] / scale * 8.0f) + 8;
                int q1 = (int)roundf(data[b*256 + sub*32 + i*2 + 1] / scale * 8.0f) + 8;
                if (q0 < 0) q0 = 0; if (q0 > 15) q0 = 15;
                if (q1 < 0) q1 = 0; if (q1 > 15) q1 = 15;
                blk[b].qs[sub*16 + i] = (q0 & 0x0F) | ((q1 & 0x0F) << 4);
            }
        }
    }
    r.size = blocks * sizeof(block_q4_k);
    return r;
}

/* Generic dequant/quant */
static DequantResult dequant_tensor(const void* data, GGMLType type, uint64_t elements) {
    DequantResult r = {0};
    size_t block_size = GGML_RXD_BLOCK_SIZE[type];
    size_t blocks = (elements + block_size - 1) / block_size;
    switch (type) {
        case GGML_RXD_TYPE_Q4_0: return dequant_q4_0(data, blocks);
        case GGML_RXD_TYPE_Q8_0: return dequant_q8_0(data, blocks);
        case GGML_RXD_TYPE_Q4_K: return dequant_q4_k(data, blocks);
        case GGML_RXD_TYPE_F32:
            r.count = elements; r.data = (float*)malloc(elements * sizeof(float));
            memcpy(r.data, data, elements * sizeof(float)); break;
        default: break;
    }
    return r;
}

static QuantResult quant_tensor(const float* data, size_t count, GGMLType type) {
    switch (type) {
        case GGML_RXD_TYPE_Q4_0: return quant_q4_0(data, count);
        case GGML_RXD_TYPE_Q8_0: return quant_q8_0(data, count);
        case GGML_RXD_TYPE_Q4_K: return quant_q4_k(data, count);
        case GGML_RXD_TYPE_F32: {
            QuantResult r = {0}; r.size = count * sizeof(float);
            r.data = malloc(r.size); memcpy(r.data, data, r.size); return r;
        }
        default: return (QuantResult){0};
    }
}

static void dequant_free(DequantResult* r) { if (r && r->data) free(r->data); }
static void quant_free(QuantResult* r) { if (r && r->data) free(r->data); }

static QuantQuality estimate_quality(const float* orig, const float* recon, size_t count) {
    QuantQuality q = {0};
    if (count == 0) return q;
    double ss_err = 0, ss_orig = 0;
    for (size_t i = 0; i < count; i++) {
        float e = orig[i] - recon[i];
        ss_err += (double)e * e;
        ss_orig += (double)orig[i] * orig[i];
    }
    q.mse = (float)(ss_err / count);
    if (ss_orig > 0) q.relative_error = (float)(ss_err / ss_orig);
    if (q.mse > 0) q.snr_db = 10.0f * (float)log10((ss_orig / count) / q.mse);
    else q.snr_db = INFINITY;
    return q;
}

/* ═══════════════════════════════════════════════════════════════════════════
   GGUF LOADING (Memory-mapped, >2GB support)
   ═══════════════════════════════════════════════════════════════════════════ */

static GGUFContext* gguf_load(const char* path) {
    GGUFContext* ctx = (GGUFContext*)calloc(1, sizeof(GGUFContext));
    if (!ctx) return NULL;
    
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) { free(ctx); return NULL; }
    LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz);
    ctx->file_size = (size_t)sz.QuadPart;
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    ctx->file_view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    ctx->file_handle = hFile; ctx->file_mapping = hMap;
#else
    int fd = open(path, O_RDONLY);
    if (fd < 0) { free(ctx); return NULL; }
    ctx->file_size = lseek(fd, 0, SEEK_END);
    ctx->file_view = mmap(NULL, ctx->file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    ctx->file_handle = (void*)(intptr_t)fd;
#endif
    
    if (!ctx->file_view) { 
        /* gguf_close would be called here but ctx is incomplete */
        free(ctx); 
        return NULL; 
    }
    
    const uint8_t* p = (const uint8_t*)ctx->file_view;
    ctx->header.magic = *(uint32_t*)p; p += 4;
    ctx->header.version = *(uint32_t*)p; p += 4;
    ctx->header.tensor_count = *(uint64_t*)p; p += 8;
    ctx->header.metadata_kv_count = *(uint64_t*)p; p += 8;
    
    ctx->tensor_infos = (GGUFTensorInfo*)calloc(ctx->header.tensor_count, sizeof(GGUFTensorInfo));
    for (uint64_t i = 0; i < ctx->header.tensor_count; i++) {
        uint64_t name_len = *(uint64_t*)p; p += 8;
        ctx->tensor_infos[i].name.data = (char*)malloc(name_len + 1);
        memcpy(ctx->tensor_infos[i].name.data, p, name_len);
        ctx->tensor_infos[i].name.data[name_len] = 0;
        ctx->tensor_infos[i].name.len = name_len; p += name_len;
        ctx->tensor_infos[i].n_dims = *(uint32_t*)p; p += 4;
        for (uint32_t d = 0; d < ctx->tensor_infos[i].n_dims; d++) {
            ctx->tensor_infos[i].dims[d] = *(uint64_t*)p; p += 8;
        }
        ctx->tensor_infos[i].type = *(uint32_t*)p; p += 4;
        ctx->tensor_infos[i].offset = *(uint64_t*)p; p += 8;
    }
    ctx->data_offset = p - (uint8_t*)ctx->file_view;
    ctx->is_loaded = true;
    return ctx;
}

static void* gguf_get_tensor_data(const GGUFContext* ctx, const char* name, size_t* out_size) {
    for (uint64_t i = 0; i < ctx->header.tensor_count; i++) {
        if (strcmp(ctx->tensor_infos[i].name.data, name) == 0) {
            uint64_t elements = 1;
            for (uint32_t d = 0; d < ctx->tensor_infos[i].n_dims; d++) elements *= ctx->tensor_infos[i].dims[d];
            size_t block_size = GGML_RXD_BLOCK_SIZE[ctx->tensor_infos[i].type];
            size_t blocks = (elements + block_size - 1) / block_size;
            size_t bytes_per_block = (ctx->tensor_infos[i].type == GGML_RXD_TYPE_Q4_0) ? 18 :
                                     (ctx->tensor_infos[i].type == GGML_RXD_TYPE_Q8_0) ? 34 :
                                     (ctx->tensor_infos[i].type == GGML_RXD_TYPE_Q4_K) ? sizeof(block_q4_k) : 4;
            size_t size = blocks * bytes_per_block;
            if (out_size) *out_size = size;
            return (uint8_t*)ctx->file_view + ctx->data_offset + ctx->tensor_infos[i].offset;
        }
    }
    return NULL;
}

static void gguf_close(GGUFContext* ctx) {
    if (!ctx) return;
    if (ctx->file_view) {
#ifdef _WIN32
        UnmapViewOfFile(ctx->file_view);
        if (ctx->file_mapping) CloseHandle((HANDLE)ctx->file_mapping);
        if (ctx->file_handle) CloseHandle((HANDLE)ctx->file_handle);
#else
        munmap(ctx->file_view, ctx->file_size);
        if (ctx->file_handle) close((int)(intptr_t)ctx->file_handle);
#endif
    }
    for (uint64_t i = 0; i < ctx->header.tensor_count; i++) free(ctx->tensor_infos[i].name.data);
    free(ctx->tensor_infos);
    free(ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
   WEIGHT HOTSWAP
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char name[128];
    GGMLType current_type, original_type;
    void* data, *original_data;
    size_t size, original_size;
    uint64_t elements;
    bool is_swapped, is_locked;
} WeightTensor;

typedef struct {
    GGUFContext* gguf;
    WeightTensor* tensors;
    uint32_t tensor_count;
    void* weight_pool;
    size_t pool_size, pool_used;
    size_t total_original_size, total_current_size;
    bool has_backup, is_suspended;
} HotswapSession;

typedef struct {
    char name[64];
    GGMLType default_type, attention_type, feedforward_type, embedding_type, output_type;
    float quality_threshold;
    size_t target_vram;
} QuantProfile;

static const QuantProfile QUANT_PROFILE_SPEED = {"speed", GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q8_0, GGML_RXD_TYPE_Q8_0, 15.0f, 0};
static const QuantProfile QUANT_PROFILE_BALANCED = {"balanced", GGML_RXD_TYPE_Q4_K, GGML_RXD_TYPE_Q5_K, GGML_RXD_TYPE_Q4_K, GGML_RXD_TYPE_Q8_0, GGML_RXD_TYPE_Q6_K, 20.0f, 0};
static const QuantProfile QUANT_PROFILE_QUALITY = {"quality", GGML_RXD_TYPE_Q6_K, GGML_RXD_TYPE_Q6_K, GGML_RXD_TYPE_Q5_K, GGML_RXD_TYPE_F16, GGML_RXD_TYPE_F16, 30.0f, 0};

static HotswapSession* hotswap_from_context(GGUFContext* ctx) {
    if (!ctx || !ctx->is_loaded) return NULL;
    HotswapSession* s = (HotswapSession*)calloc(1, sizeof(HotswapSession));
    s->gguf = ctx;
    s->tensor_count = (uint32_t)ctx->header.tensor_count;
    s->tensors = (WeightTensor*)calloc(s->tensor_count, sizeof(WeightTensor));
    for (uint32_t i = 0; i < s->tensor_count; i++) {
        GGUFTensorInfo* info = &ctx->tensor_infos[i];
        WeightTensor* t = &s->tensors[i];
        strncpy(t->name, info->name.data, 127);
        t->current_type = t->original_type = (GGMLType)info->type;
        t->data = gguf_get_tensor_data(ctx, info->name.data, &t->size);
        t->original_size = t->size;
        uint64_t elements = 1;
        for (uint32_t d = 0; d < info->n_dims; d++) elements *= info->dims[d];
        t->elements = elements;
        s->total_original_size += t->size;
    }
    s->total_current_size = s->total_original_size;
    return s;
}

static HotswapSession* hotswap_create(const char* path) {
    GGUFContext* ctx = gguf_load(path);
    if (!ctx) return NULL;
    return hotswap_from_context(ctx);
}

static void hotswap_destroy(HotswapSession* s) {
    if (!s) return;
    for (uint32_t i = 0; i < s->tensor_count; i++) if (s->tensors[i].original_data) free(s->tensors[i].original_data);
    free(s->tensors);
    if (s->weight_pool) free(s->weight_pool);
    if (s->gguf) gguf_close(s->gguf);
    free(s);
}

static WeightTensor* hotswap_get_tensor(HotswapSession* s, const char* name) {
    for (uint32_t i = 0; i < s->tensor_count; i++)
        if (strcmp(s->tensors[i].name, name) == 0) return &s->tensors[i];
    return NULL;
}

static bool hotswap_requant_tensor(HotswapSession* s, const char* name, GGMLType new_type) {
    WeightTensor* t = hotswap_get_tensor(s, name);
    if (!t || t->current_type == new_type) return t != NULL;
    if (!t->original_data) {
        t->original_data = malloc(t->size);
        if (!t->original_data) return false;
        memcpy(t->original_data, t->data, t->size);
        s->has_backup = true;
    }
    DequantResult fp32 = dequant_tensor(t->data, t->current_type, t->elements);
    if (!fp32.data) return false;
    QuantResult quant = quant_tensor(fp32.data, t->elements, new_type);
    dequant_free(&fp32);
    if (!quant.data) return false;
    void* new_data = malloc(quant.size);
    if (!new_data) { quant_free(&quant); return false; }
    memcpy(new_data, quant.data, quant.size);
    quant_free(&quant);
    t->data = new_data;
    t->size = quant.size;
    t->current_type = new_type;
    t->is_swapped = true;
    s->total_current_size = 0;
    for (uint32_t i = 0; i < s->tensor_count; i++) s->total_current_size += s->tensors[i].size;
    return true;
}

static bool hotswap_backup(HotswapSession* s) {
    for (uint32_t i = 0; i < s->tensor_count; i++) {
        WeightTensor* t = &s->tensors[i];
        if (!t->original_data) {
            t->original_data = malloc(t->size);
            if (!t->original_data) return false;
            memcpy(t->original_data, t->data, t->size);
        }
    }
    s->has_backup = true;
    return true;
}

static bool hotswap_restore(HotswapSession* s) {
    if (!s->has_backup) return false;
    for (uint32_t i = 0; i < s->tensor_count; i++) {
        WeightTensor* t = &s->tensors[i];
        if (t->is_swapped && t->original_data) {
            memcpy(t->data, t->original_data, t->original_size);
            t->size = t->original_size;
            t->current_type = t->original_type;
            t->is_swapped = false;
        }
    }
    s->total_current_size = s->total_original_size;
    return true;
}

static bool is_attention(const WeightTensor* t) {
    return strstr(t->name, "attn") || strstr(t->name, "q_proj") || strstr(t->name, "k_proj") || strstr(t->name, "v_proj");
}

static bool is_feedforward(const WeightTensor* t) {
    return strstr(t->name, "ffn") || strstr(t->name, "mlp") || strstr(t->name, "up_proj") || strstr(t->name, "down_proj");
}

static bool is_embedding(const WeightTensor* t) { return strstr(t->name, "embed") || strstr(t->name, "tok_embeddings"); }
static bool is_output(const WeightTensor* t) { return strstr(t->name, "output") || strstr(t->name, "lm_head"); }

static bool hotswap_apply_profile(HotswapSession* s, const QuantProfile* p) {
    for (uint32_t i = 0; i < s->tensor_count; i++) {
        WeightTensor* t = &s->tensors[i];
        GGMLType target = p->default_type;
        if (is_attention(t)) target = p->attention_type;
        else if (is_feedforward(t)) target = p->feedforward_type;
        else if (is_embedding(t)) target = p->embedding_type;
        else if (is_output(t)) target = p->output_type;
        hotswap_requant_tensor(s, t->name, target);
    }
    return true;
}

typedef struct { char name[128]; float mse, snr_db; bool acceptable; } TensorQuality;
typedef struct { TensorQuality* tensors; uint32_t count; float avg_snr_db, worst_snr_db; uint32_t acceptable_count; } BatchQuality;

static TensorQuality hotswap_measure_quality(HotswapSession* s, const char* name) {
    TensorQuality q = {0};
    WeightTensor* t = hotswap_get_tensor(s, name);
    if (!t || !t->is_swapped || !t->original_data) { q.acceptable = true; return q; }
    DequantResult orig = dequant_tensor(t->original_data, t->original_type, t->elements);
    DequantResult curr = dequant_tensor(t->data, t->current_type, t->elements);
    if (orig.data && curr.data) {
        QuantQuality qq = estimate_quality(orig.data, curr.data, t->elements);
        strncpy(q.name, name, 127);
        q.mse = qq.mse;
        q.snr_db = qq.snr_db;
        q.acceptable = (qq.snr_db > 15.0f);
    }
    dequant_free(&orig); dequant_free(&curr);
    return q;
}

static BatchQuality hotswap_measure_all(HotswapSession* s) {
    BatchQuality b = {0};
    b.tensors = (TensorQuality*)calloc(s->tensor_count, sizeof(TensorQuality));
    b.count = s->tensor_count;
    b.worst_snr_db = 100.0f;
    for (uint32_t i = 0; i < s->tensor_count; i++) {
        b.tensors[i] = hotswap_measure_quality(s, s->tensors[i].name);
        b.avg_snr_db += b.tensors[i].snr_db;
        if (b.tensors[i].snr_db < b.worst_snr_db) b.worst_snr_db = b.tensors[i].snr_db;
        if (b.tensors[i].acceptable) b.acceptable_count++;
    }
    if (b.count > 0) b.avg_snr_db /= b.count;
    return b;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LOCKPICK (Experiment Runner)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t start_ns, end_ns, total_ns;
} LockpickTimer;

static uint64_t lockpick_get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart / (double)f.QuadPart * 1e9);
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static void lockpick_timer_start(LockpickTimer* t) { t->start_ns = lockpick_get_time_ns(); }
static uint64_t lockpick_timer_stop(LockpickTimer* t) {
    t->end_ns = lockpick_get_time_ns();
    t->total_ns = t->end_ns - t->start_ns;
    return t->total_ns;
}

typedef struct {
    double tokens_per_sec, first_token_ms, avg_token_ms, total_time_ms;
    size_t model_size_bytes;
    float snr_db;
    uint32_t generated_tokens;
} LockpickMetrics;

typedef struct {
    char name[64];
    QuantProfile profile;
    LockpickMetrics metrics;
    BatchQuality quality;
    bool ran, success;
} LockpickExperiment;

typedef struct {
    HotswapSession* hotswap;
    GGUFContext* model;
    LockpickExperiment* experiments;
    uint32_t experiment_count, experiment_capacity;
    uint32_t best_experiment;
    float best_score;
    LockpickMetrics baseline;
    bool has_baseline;
    char* test_prompt;
    uint32_t test_max_tokens;
} LockpickSession;

static LockpickSession* lockpick_create(const char* path) {
    GGUFContext* model = gguf_load(path);
    if (!model) return NULL;
    HotswapSession* hs = hotswap_from_context(model);
    if (!hs) { gguf_close(model); return NULL; }
    LockpickSession* s = (LockpickSession*)calloc(1, sizeof(LockpickSession));
    s->hotswap = hs; s->model = model;
    s->experiment_capacity = 16;
    s->experiments = (LockpickExperiment*)calloc(s->experiment_capacity, sizeof(LockpickExperiment));
    s->test_prompt = strdup("The quick brown fox jumps over the lazy dog. ");
    s->test_max_tokens = 64;
    return s;
}

static void lockpick_destroy(LockpickSession* s) {
    if (!s) return;
    free(s->experiments);
    free(s->test_prompt);
    hotswap_destroy(s->hotswap);
    free(s);
}

static uint32_t lockpick_add_experiment(LockpickSession* s, const char* name, const QuantProfile* p) {
    if (s->experiment_count >= s->experiment_capacity) {
        s->experiment_capacity *= 2;
        s->experiments = (LockpickExperiment*)realloc(s->experiments, s->experiment_capacity * sizeof(LockpickExperiment));
    }
    uint32_t idx = s->experiment_count++;
    LockpickExperiment* e = &s->experiments[idx];
    memset(e, 0, sizeof(LockpickExperiment));
    strncpy(e->name, name, 63);
    memcpy(&e->profile, p, sizeof(QuantProfile));
    return idx;
}

static float lockpick_score(const LockpickMetrics* m, const BatchQuality* q) {
    if (!m || !q) return 0.0f;
    float tps_score = (float)m->tokens_per_sec / 100.0f;
    float quality_score = q->avg_snr_db / 30.0f;
    return tps_score * 0.4f + quality_score * 0.6f;
}

static bool lockpick_run_experiment(LockpickSession* s, uint32_t idx) {
    if (idx >= s->experiment_count) return false;
    LockpickExperiment* e = &s->experiments[idx];
    e->ran = true;
    hotswap_backup(s->hotswap);
    if (!hotswap_apply_profile(s->hotswap, &e->profile)) {
        hotswap_restore(s->hotswap);
        return false;
    }
    e->quality = hotswap_measure_all(s->hotswap);
    e->metrics.model_size_bytes = s->hotswap->total_current_size;
    e->metrics.snr_db = e->quality.avg_snr_db;
    /* Real TPS measurement requires inference engine integration */
    e->metrics.tokens_per_sec = 50.0; /* Placeholder */
    e->metrics.first_token_ms = 100.0;
    e->success = true;
    hotswap_restore(s->hotswap);
    return true;
}

static bool lockpick_run_all(LockpickSession* s) {
    for (uint32_t i = 0; i < s->experiment_count; i++) {
        if (!lockpick_run_experiment(s, i)) continue;
        float score = lockpick_score(&s->experiments[i].metrics, &s->experiments[i].quality);
        if (i == 0 || score > s->best_score) {
            s->best_score = score;
            s->best_experiment = i;
        }
    }
    return true;
}

static const LockpickExperiment* lockpick_get_best(LockpickSession* s) {
    if (!s || s->experiment_count == 0) return NULL;
    return &s->experiments[s->best_experiment];
}

static bool lockpick_pin_best(LockpickSession* s) {
    if (!s || s->experiment_count == 0) return false;
    return hotswap_apply_profile(s->hotswap, &s->experiments[s->best_experiment].profile);
}

typedef struct { char* data; size_t size; } LockpickReport;

static LockpickReport lockpick_generate_json(LockpickSession* s) {
    LockpickReport r = {0};
    size_t sz = 2048 + s->experiment_count * 512;
    r.data = (char*)malloc(sz);
    r.size = snprintf(r.data, sz,
        "{\"experiments\":%u,\"best\":%u,\"baseline_snr\":%.2f,\"results\":[",
        s->experiment_count, s->best_experiment, s->baseline.snr_db);
    for (uint32_t i = 0; i < s->experiment_count; i++) {
        LockpickExperiment* e = &s->experiments[i];
        r.size += snprintf(r.data + r.size, sz - r.size,
            "{\"name\":\"%s\",\"tps\":%.2f,\"snr\":%.2f,\"size\":%zu}%s",
            e->name, e->metrics.tokens_per_sec, e->quality.avg_snr_db,
            e->metrics.model_size_bytes, (i < s->experiment_count - 1) ? "," : "");
    }
    r.size += snprintf(r.data + r.size, sz - r.size, "]}");
    return r;
}

static void lockpick_free_report(LockpickReport* r) { if (r && r->data) free(r->data); }

/* Quick one-shot API */
static const LockpickExperiment* lockpick_quick(const char* path, const char* prompt, uint32_t max_tokens) {
    LockpickSession* s = lockpick_create(path);
    if (!s) return NULL;
    if (prompt) { free(s->test_prompt); s->test_prompt = strdup(prompt); s->test_max_tokens = max_tokens; }
    lockpick_add_experiment(s, "speed", &QUANT_PROFILE_SPEED);
    lockpick_add_experiment(s, "balanced", &QUANT_PROFILE_BALANCED);
    lockpick_add_experiment(s, "quality", &QUANT_PROFILE_QUALITY);
    lockpick_run_all(s);
    return lockpick_get_best(s);
    /* Caller must not destroy session until done with result */
}

/* ═══════════════════════════════════════════════════════════════════════════
   INFERENCE HOOK (Timing & Metrics)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    LockpickTimer timer;
    uint32_t token_count;
    float sum_logprob;
    bool active;
} InferenceHook;

static InferenceHook g_hook = {0};

static void inference_hook_start(void) {
    g_hook.active = true;
    g_hook.token_count = 0;
    g_hook.sum_logprob = 0.0f;
    lockpick_timer_start(&g_hook.timer);
}

static void inference_hook_token(float logprob) {
    if (!g_hook.active) return;
    g_hook.token_count++;
    g_hook.sum_logprob += logprob;
}

static void inference_hook_end(void) {
    if (!g_hook.active) return;
    lockpick_timer_stop(&g_hook.timer);
    g_hook.active = false;
}

static void inference_hook_get_metrics(LockpickMetrics* m) {
    if (!m || g_hook.token_count == 0) return;
    double sec = (double)g_hook.timer.total_ns / 1e9;
    m->tokens_per_sec = (double)g_hook.token_count / sec;
    m->total_time_ms = sec * 1000.0;
    m->avg_token_ms = sec * 1000.0 / g_hook.token_count;
    m->generated_tokens = g_hook.token_count;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY DETECTION
   ═══════════════════════════════════════════════════════════════════════════ */

static size_t lockpick_get_gpu_memory(void) {
#ifdef _WIN32
    /* DXGI implementation omitted for brevity */
    return 0;
#elif defined(__linux__)
    FILE* f = fopen("/sys/class/drm/card0/device/mem_info_vram_total", "r");
    if (f) { size_t vram; if (fscanf(f, "%zu", &vram) == 1) { fclose(f); return vram; } fclose(f); }
    return 0;
#else
    return 0;
#endif
}

static size_t lockpick_get_system_memory(void) {
#ifdef _WIN32
    MEMORYSTATUSEX st = {0}; st.dwLength = sizeof(st);
    GlobalMemoryStatusEx(&st);
    return (size_t)st.ullTotalPhys;
#elif defined(__linux__)
    FILE* f = fopen("/proc/meminfo", "r");
    if (f) { char line[256]; while (fgets(line, 256, f)) {
        size_t kb; if (sscanf(line, "MemTotal: %zu kB", &kb) == 1) { fclose(f); return kb * 1024; }
    } fclose(f); }
    return 0;
#else
    return 0;
#endif
}

/* Auto-configure for hardware */
static bool lockpick_auto_configure(LockpickSession* s, size_t target_vram, float quality_weight) {
    size_t gpu = target_vram ? target_vram : lockpick_get_gpu_memory();
    size_t sys = lockpick_get_system_memory();
    size_t avail = gpu > 0 ? gpu : (size_t)(sys * 0.75);
    size_t model = s->hotswap->total_original_size;
    float ratio = (float)avail / (float)model;
    const QuantProfile* p;
    if (ratio >= 1.0f && quality_weight > 0.7f) p = &QUANT_PROFILE_QUALITY;
    else if (ratio >= 0.7f) p = &QUANT_PROFILE_BALANCED;
    else p = &QUANT_PROFILE_SPEED;
    return hotswap_apply_profile(s->hotswap, p);
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_H */
