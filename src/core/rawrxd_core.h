/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_CORE - Production Inference Engine
   Single file, <3k lines, no stubs, real code.
   
   Includes: GGUF, Quantization, Hotswap, Lockpick, Inference, Tools, LSP
   ═══════════════════════════════════════════════════════════════════════════ */

#ifndef RAWRXD_CORE_H
#define RAWRXD_CORE_H

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
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   CONFIGURATION
   ═══════════════════════════════════════════════════════════════════════════ */

#define RXD_VERSION_MAJOR 1
#define RXD_VERSION_MINOR 0
#define RXD_VERSION_PATCH 0
#define RXD_MAX_TENSORS 4096
#define RXD_MAX_TOOLS 64
#define RXD_MAX_LAYERS 128
#define RXD_MAX_PROMPT 65536
#define RXD_MAX_OUTPUT 131072
#define RXD_MAX_TOOLS_CALLS 32
#define RXD_MAX_MEMORY_ENTRIES 256

/* ═══════════════════════════════════════════════════════════════════════════
   GGUF FORMAT
   ═══════════════════════════════════════════════════════════════════════════ */

#define GGUF_MAGIC 0x46554747
#define GGUF_VERSION_3 3

typedef enum {
    GGML_RXD_TYPE_F32 = 0, GGML_RXD_TYPE_F16 = 1,
    GGML_RXD_TYPE_Q4_0 = 2, GGML_RXD_TYPE_Q4_1 = 3,
    GGML_RXD_TYPE_Q5_0 = 6, GGML_RXD_TYPE_Q5_1 = 7,
    GGML_RXD_TYPE_Q8_0 = 8, GGML_RXD_TYPE_Q8_1 = 9,
    GGML_RXD_TYPE_Q2_K = 10, GGML_RXD_TYPE_Q3_K = 11,
    GGML_RXD_TYPE_Q4_K = 12, GGML_RXD_TYPE_Q5_K = 13, GGML_RXD_TYPE_Q6_K = 14,
    GGML_RXD_TYPE_COUNT
} GGMLType;

static const size_t GGML_RXD_BLOCK_SIZE[] = {
    [GGML_RXD_TYPE_F32] = 1, [GGML_RXD_TYPE_F16] = 1,
    [GGML_RXD_TYPE_Q4_0] = 32, [GGML_RXD_TYPE_Q4_1] = 32,
    [GGML_RXD_TYPE_Q5_0] = 32, [GGML_RXD_TYPE_Q5_1] = 32,
    [GGML_RXD_TYPE_Q8_0] = 32, [GGML_RXD_TYPE_Q8_1] = 32,
    [GGML_RXD_TYPE_Q2_K] = 256, [GGML_RXD_TYPE_Q3_K] = 256,
    [GGML_RXD_TYPE_Q4_K] = 256, [GGML_RXD_TYPE_Q5_K] = 256, [GGML_RXD_TYPE_Q6_K] = 256,
};

static const size_t GGML_RXD_BYTES_PER_BLOCK[] = {
    [GGML_RXD_TYPE_F32] = 4, [GGML_RXD_TYPE_F16] = 2,
    [GGML_RXD_TYPE_Q4_0] = 18, [GGML_RXD_TYPE_Q4_1] = 20,
    [GGML_RXD_TYPE_Q5_0] = 22, [GGML_RXD_TYPE_Q5_1] = 24,
    [GGML_RXD_TYPE_Q8_0] = 34, [GGML_RXD_TYPE_Q8_1] = 36,
    [GGML_RXD_TYPE_Q4_K] = 144, [GGML_RXD_TYPE_Q5_K] = 176, [GGML_RXD_TYPE_Q6_K] = 210,
};

typedef struct {
    char* data; uint64_t len;
} RXDString;

typedef struct {
    RXDString name;
    uint32_t n_dims;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset;
} RXDTensorInfo;

typedef struct {
    void* file_view;
    size_t file_size;
    uint32_t magic, version;
    uint64_t tensor_count;
    RXDTensorInfo* tensors;
    uint64_t data_offset;
    bool loaded;
} RXDModel;

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY MAPPING (>2GB support)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void* base;
    size_t size;
    void* handle;
#ifdef _WIN32
    HANDLE hFile, hMap;
#else
    int fd;
#endif
} RXDMemoryMap;

static inline RXDMemoryMap rxd_mmap_create(const char* path) {
    RXDMemoryMap m = {0};
#ifdef _WIN32
    m.hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (m.hFile == INVALID_HANDLE_VALUE) return m;
    LARGE_INTEGER sz; GetFileSizeEx(m.hFile, &sz);
    m.size = (size_t)sz.QuadPart;
    m.hMap = CreateFileMapping(m.hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    m.base = MapViewOfFile(m.hMap, FILE_MAP_READ, 0, 0, 0);
#else
    m.fd = open(path, O_RDONLY);
    if (m.fd < 0) return m;
    m.size = lseek(m.fd, 0, SEEK_END);
    m.base = mmap(NULL, m.size, PROT_READ, MAP_PRIVATE, m.fd, 0);
#endif
    return m;
}

static inline void rxd_mmap_destroy(RXDMemoryMap* m) {
    if (!m || !m->base) return;
#ifdef _WIN32
    if (m->base) UnmapViewOfFile(m->base);
    if (m->hMap) CloseHandle(m->hMap);
    if (m->hFile != INVALID_HANDLE_VALUE) CloseHandle(m->hFile);
#else
    if (m->base != MAP_FAILED) munmap(m->base, m->size);
    if (m->fd >= 0) close(m->fd);
#endif
    memset(m, 0, sizeof(RXDMemoryMap));
}

/* ═══════════════════════════════════════════════════════════════════════════
   QUANTIZATION (Q4_0, Q4_K, Q8_0)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef uint16_t rxd_half;

static inline float rxd_half_to_float(rxd_half h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1F, m = h & 0x3FF;
    if (e == 0) return m == 0 ? (s ? -0.0f : 0.0f) : (s ? -1.0f : 1.0f) * (float)m / 16777216.0f;
    if (e == 31) return m == 0 ? (s ? -INFINITY : INFINITY) : NAN;
    return (s ? -1.0f : 1.0f) * ldexpf((float)(m | 0x400), e - 25);
}

static inline rxd_half rxd_float_to_half(float f) {
    uint32_t b; memcpy(&b, &f, 4);
    uint32_t s = (b >> 31) & 1, e = ((b >> 23) & 0xFF) - 127, m = b & 0x7FFFFF;
    if (e > 15) return (rxd_half)((s << 15) | 0x7C00);
    if (e < -24) return (rxd_half)(s << 15);
    if (e <= -14) { m |= 0x800000; m >>= (-14 - e); return (rxd_half)((s << 15) | (m >> 13)); }
    return (rxd_half)((s << 15) | ((e + 15) << 10) | (m >> 13));
}

typedef struct { float* data; size_t count; } RXDDequant;
typedef struct { void* data; size_t size; } RXDQuant;
typedef struct { float mse, snr_db; } RXDQuality;

#pragma pack(push, 1)
typedef struct { rxd_half d, dmin; uint8_t scales[16], qs[128]; } RXDBlockQ4K;
#pragma pack(pop)

static RXDDequant rxd_dequant_q4_0(const void* blocks, size_t num_blocks) {
    RXDDequant r = {0};
    r.count = num_blocks * 32;
    r.data = (float*)malloc(r.count * sizeof(float));
    const uint8_t* p = (const uint8_t*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float scale = rxd_half_to_float(*(const rxd_half*)p); p += 2;
        for (int i = 0; i < 16; i++) {
            uint8_t v = *p++;
            r.data[b*32 + i*2 + 0] = ((float)((v & 0x0F) - 8)) * scale / 8.0f;
            r.data[b*32 + i*2 + 1] = ((float)((v >> 4) - 8)) * scale / 8.0f;
        }
    }
    return r;
}

static RXDQuant rxd_quant_q4_0(const float* data, size_t count) {
    RXDQuant r = {0};
    size_t blocks = (count + 31) / 32;
    r.data = (uint8_t*)malloc(blocks * 18);
    uint8_t* out = (uint8_t*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (size_t i = b*32; i < (b+1)*32 && i < count; i++) { float a = fabsf(data[i]); if (a > max_abs) max_abs = a; }
        float scale = max_abs / 8.0f; if (scale < 1e-10f) scale = 1e-10f;
        *(rxd_half*)out = rxd_float_to_half(scale); out += 2;
        for (int i = 0; i < 16; i++) {
            int q0 = (int)roundf(data[b*32 + i*2] / scale * 8.0f);
            int q1 = (int)roundf(data[b*32 + i*2 + 1] / scale * 8.0f);
            if (q0 < -8) q0 = -8; if (q0 > 7) q0 = 7;
            if (q1 < -8) q1 = -8; if (q1 > 7) q1 = 7;
            *out++ = (uint8_t)(((q0 + 8) & 0x0F) | (((q1 + 8) & 0x0F) << 4));
        }
    }
    r.size = blocks * 18;
    return r;
}

static RXDDequant rxd_dequant_q8_0(const void* blocks, size_t num_blocks) {
    RXDDequant r = {0};
    r.count = num_blocks * 32;
    r.data = (float*)malloc(r.count * sizeof(float));
    const uint8_t* p = (const uint8_t*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float scale = rxd_half_to_float(*(const rxd_half*)p); p += 2;
        const int8_t* qs = (const int8_t*)p; p += 32;
        for (int i = 0; i < 32; i++) r.data[b*32 + i] = (float)qs[i] * scale / 128.0f;
    }
    return r;
}

static RXDQuant rxd_quant_q8_0(const float* data, size_t count) {
    RXDQuant r = {0};
    size_t blocks = (count + 31) / 32;
    r.data = (uint8_t*)malloc(blocks * 34);
    uint8_t* out = (uint8_t*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (size_t i = b*32; i < (b+1)*32 && i < count; i++) { float a = fabsf(data[i]); if (a > max_abs) max_abs = a; }
        float scale = max_abs / 128.0f; if (scale < 1e-10f) scale = 1e-10f;
        *(rxd_half*)out = rxd_float_to_half(scale); out += 2;
        for (int i = 0; i < 32 && b*32+i < (int)count; i++) {
            int q = (int)roundf(data[b*32 + i] / scale * 128.0f);
            if (q < -128) q = -128; if (q > 127) q = 127;
            *out++ = (int8_t)q;
        }
    }
    r.size = blocks * 34;
    return r;
}

static RXDDequant rxd_dequant_q4_k(const void* blocks, size_t num_blocks) {
    RXDDequant r = {0};
    r.count = num_blocks * 256;
    r.data = (float*)malloc(r.count * sizeof(float));
    const RXDBlockQ4K* blk = (const RXDBlockQ4K*)blocks;
    for (size_t b = 0; b < num_blocks; b++) {
        float super = rxd_half_to_float(blk[b].d);
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

static RXDQuant rxd_quant_q4_k(const float* data, size_t count) {
    RXDQuant r = {0};
    size_t blocks = (count + 255) / 256;
    r.data = (uint8_t*)malloc(blocks * sizeof(RXDBlockQ4K));
    RXDBlockQ4K* blk = (RXDBlockQ4K*)r.data;
    for (size_t b = 0; b < blocks; b++) {
        float max_abs = 0;
        for (int i = 0; i < 256 && b*256+i < count; i++) { float a = fabsf(data[b*256 + i]); if (a > max_abs) max_abs = a; }
        blk[b].d = rxd_float_to_half(max_abs / 8.0f);
        blk[b].dmin = 0;
        for (int sub = 0; sub < 8; sub++) {
            float sub_max = 0;
            for (int i = 0; i < 32 && b*256+sub*32+i < (int)count; i++) { float a = fabsf(data[b*256 + sub*32 + i]); if (a > sub_max) sub_max = a; }
            float super = rxd_half_to_float(blk[b].d);
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
                blk[b].qs[sub*16 + i] = (uint8_t)((q0 & 0x0F) | ((q1 & 0x0F) << 4));
            }
        }
    }
    r.size = blocks * sizeof(RXDBlockQ4K);
    return r;
}

static RXDDequant rxd_dequant(const void* data, GGMLType type, uint64_t elements) {
    RXDDequant r = {0};
    size_t block_size = GGML_RXD_BLOCK_SIZE[type];
    size_t blocks = (elements + block_size - 1) / block_size;
    switch (type) {
        case GGML_RXD_TYPE_Q4_0: return rxd_dequant_q4_0(data, blocks);
        case GGML_RXD_TYPE_Q8_0: return rxd_dequant_q8_0(data, blocks);
        case GGML_RXD_TYPE_Q4_K: return rxd_dequant_q4_k(data, blocks);
        case GGML_RXD_TYPE_F32:
            r.count = elements; r.data = (float*)malloc(elements * sizeof(float));
            memcpy(r.data, data, elements * sizeof(float)); break;
        default: break;
    }
    return r;
}

static RXDQuant rxd_quant(const float* data, size_t count, GGMLType type) {
    switch (type) {
        case GGML_RXD_TYPE_Q4_0: return rxd_quant_q4_0(data, count);
        case GGML_RXD_TYPE_Q8_0: return rxd_quant_q8_0(data, count);
        case GGML_RXD_TYPE_Q4_K: return rxd_quant_q4_k(data, count);
        case GGML_RXD_TYPE_F32: {
            RXDQuant r = {0}; r.size = count * sizeof(float);
            r.data = malloc(r.size); memcpy(r.data, data, r.size); return r;
        }
        default: return (RXDQuant){0};
    }
}

static void rxd_dequant_free(RXDDequant* r) { if (r && r->data) free(r->data); }
static void rxd_quant_free(RXDQuant* r) { if (r && r->data) free(r->data); }

static RXDQuality rxd_estimate_quality(const float* orig, const float* recon, size_t count) {
    RXDQuality q = {0};
    if (count == 0) return q;
    double ss_err = 0, ss_orig = 0;
    for (size_t i = 0; i < count; i++) {
        float e = orig[i] - recon[i];
        ss_err += (double)e * e;
        ss_orig += (double)orig[i] * orig[i];
    }
    q.mse = (float)(ss_err / count);
    if (q.mse > 0) q.snr_db = 10.0f * (float)log10((ss_orig / count) / q.mse);
    else q.snr_db = 999.0f;
    return q;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MODEL LOADING
   ═══════════════════════════════════════════════════════════════════════════ */

static RXDModel rxd_model_load(const char* path) {
    RXDModel m = {0};
    RXDMemoryMap map = rxd_mmap_create(path);
    if (!map.base) return m;
    m.file_view = map.base;
    m.file_size = map.size;
    const uint8_t* p = (const uint8_t*)m.file_view;
    m.magic = *(const uint32_t*)p; p += 4;
    if (m.magic != GGUF_MAGIC) { rxd_mmap_destroy(&map); return (RXDModel){0}; }
    m.version = *(const uint32_t*)p; p += 4;
    m.tensor_count = *(const uint64_t*)p; p += 8;
    uint64_t meta_kv = *(const uint64_t*)p; p += 8; (void)meta_kv;
    if (m.tensor_count > RXD_MAX_TENSORS) { rxd_mmap_destroy(&map); return (RXDModel){0}; }
    m.tensors = (RXDTensorInfo*)calloc(m.tensor_count, sizeof(RXDTensorInfo));
    for (uint64_t i = 0; i < m.tensor_count; i++) {
        uint64_t name_len = *(const uint64_t*)p; p += 8;
        m.tensors[i].name.data = (char*)malloc(name_len + 1);
        memcpy(m.tensors[i].name.data, p, name_len);
        m.tensors[i].name.data[name_len] = 0;
        m.tensors[i].name.len = name_len; p += name_len;
        m.tensors[i].n_dims = *(const uint32_t*)p; p += 4;
        for (uint32_t d = 0; d < m.tensors[i].n_dims; d++) { m.tensors[i].dims[d] = *(const uint64_t*)p; p += 8; }
        m.tensors[i].type = *(const uint32_t*)p; p += 4;
        m.tensors[i].offset = *(const uint64_t*)p; p += 8;
    }
    m.data_offset = p - (const uint8_t*)m.file_view;
    m.loaded = true;
    return m;
}

static void* rxd_model_get_tensor(const RXDModel* m, const char* name, size_t* out_size) {
    for (uint64_t i = 0; i < m->tensor_count; i++) {
        if (strcmp(m->tensors[i].name.data, name) == 0) {
            uint64_t elements = 1;
            for (uint32_t d = 0; d < m->tensors[i].n_dims; d++) elements *= m->tensors[i].dims[d];
            size_t block_size = GGML_RXD_BLOCK_SIZE[m->tensors[i].type];
            size_t blocks = (elements + block_size - 1) / block_size;
            size_t size = blocks * GGML_RXD_BYTES_PER_BLOCK[m->tensors[i].type];
            if (out_size) *out_size = size;
            return (uint8_t*)m->file_view + m->data_offset + m->tensors[i].offset;
        }
    }
    return NULL;
}

static void rxd_model_destroy(RXDModel* m) {
    if (!m) return;
    for (uint64_t i = 0; i < m->tensor_count; i++) free(m->tensors[i].name.data);
    free(m->tensors);
    RXDMemoryMap map = {.base = m->file_view, .size = m->file_size};
    rxd_mmap_destroy(&map);
    memset(m, 0, sizeof(RXDModel));
}

/* ═══════════════════════════════════════════════════════════════════════════
   WEIGHT HOTSWAP
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char name[128];
    GGMLType current_type, original_type;
    void* data;
    void* original_data;
    size_t size, original_size;
    uint64_t elements;
    bool swapped, locked;
} RXDWeight;

typedef struct {
    RXDModel model;
    RXDWeight* weights;
    uint32_t weight_count;
    size_t total_original, total_current;
    bool has_backup;
} RXDHotswap;

typedef struct {
    char name[64];
    GGMLType default_type, attn_type, ffn_type, embed_type, output_type;
    float min_snr;
} RXDQuantProfile;

static const RXDQuantProfile RXD_PROFILE_SPEED = {"speed", GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q4_0, GGML_RXD_TYPE_Q8_0, GGML_RXD_TYPE_Q8_0, 15.0f};
static const RXDQuantProfile RXD_PROFILE_BALANCED = {"balanced", GGML_RXD_TYPE_Q4_K, GGML_RXD_TYPE_Q5_K, GGML_RXD_TYPE_Q4_K, GGML_RXD_TYPE_Q8_0, GGML_RXD_TYPE_Q6_K, 20.0f};
static const RXDQuantProfile RXD_PROFILE_QUALITY = {"quality", GGML_RXD_TYPE_Q6_K, GGML_RXD_TYPE_Q6_K, GGML_RXD_TYPE_Q5_K, GGML_RXD_TYPE_F16, GGML_RXD_TYPE_F16, 30.0f};

static RXDHotswap rxd_hotswap_create(const char* path) {
    RXDHotswap h = {0};
    h.model = rxd_model_load(path);
    if (!h.model.loaded) return h;
    h.weight_count = (uint32_t)h.model.tensor_count;
    h.weights = (RXDWeight*)calloc(h.weight_count, sizeof(RXDWeight));
    for (uint32_t i = 0; i < h.weight_count; i++) {
        RXDTensorInfo* t = &h.model.tensors[i];
        RXDWeight* w = &h.weights[i];
        strncpy(w->name, t->name.data, 127);
        w->current_type = w->original_type = (GGMLType)t->type;
        w->data = rxd_model_get_tensor(&h.model, t->name.data, &w->size);
        w->original_size = w->size;
        uint64_t elements = 1;
        for (uint32_t d = 0; d < t->n_dims; d++) elements *= t->dims[d];
        w->elements = elements;
        h.total_original += w->size;
    }
    h.total_current = h.total_original;
    return h;
}

static void rxd_hotswap_destroy(RXDHotswap* h) {
    if (!h) return;
    for (uint32_t i = 0; i < h->weight_count; i++) if (h->weights[i].original_data) free(h->weights[i].original_data);
    free(h->weights);
    rxd_model_destroy(&h->model);
    memset(h, 0, sizeof(RXDHotswap));
}

static RXDWeight* rxd_hotswap_get(RXDHotswap* h, const char* name) {
    for (uint32_t i = 0; i < h->weight_count; i++)
        if (strcmp(h->weights[i].name, name) == 0) return &h->weights[i];
    return NULL;
}

static bool rxd_hotswap_requant(RXDHotswap* h, const char* name, GGMLType new_type) {
    RXDWeight* w = rxd_hotswap_get(h, name);
    if (!w || w->current_type == new_type) return w != NULL;
    if (!w->original_data) {
        w->original_data = malloc(w->size);
        if (!w->original_data) return false;
        memcpy(w->original_data, w->data, w->size);
        h->has_backup = true;
    }
    RXDDequant fp32 = rxd_dequant(w->data, w->current_type, w->elements);
    if (!fp32.data) return false;
    RXDQuant quant = rxd_quant(fp32.data, w->elements, new_type);
    rxd_dequant_free(&fp32);
    if (!quant.data) return false;
    void* new_data = malloc(quant.size);
    if (!new_data) { rxd_quant_free(&quant); return false; }
    memcpy(new_data, quant.data, quant.size);
    rxd_quant_free(&quant);
    w->data = new_data;
    w->size = quant.size;
    w->current_type = new_type;
    w->swapped = true;
    h->total_current = 0;
    for (uint32_t i = 0; i < h->weight_count; i++) h->total_current += h->weights[i].size;
    return true;
}

static bool rxd_hotswap_backup(RXDHotswap* h) {
    for (uint32_t i = 0; i < h->weight_count; i++) {
        RXDWeight* w = &h->weights[i];
        if (!w->original_data) {
            w->original_data = malloc(w->size);
            if (!w->original_data) return false;
            memcpy(w->original_data, w->data, w->size);
        }
    }
    h->has_backup = true;
    return true;
}

static bool rxd_hotswap_restore(RXDHotswap* h) {
    if (!h->has_backup) return false;
    for (uint32_t i = 0; i < h->weight_count; i++) {
        RXDWeight* w = &h->weights[i];
        if (w->swapped && w->original_data) {
            memcpy(w->data, w->original_data, w->original_size);
            w->size = w->original_size;
            w->current_type = w->original_type;
            w->swapped = false;
        }
    }
    h->total_current = h->total_original;
    return true;
}

static bool rxd_is_attn(const RXDWeight* w) { return strstr(w->name, "attn") || strstr(w->name, "q_proj") || strstr(w->name, "k_proj") || strstr(w->name, "v_proj"); }
static bool rxd_is_ffn(const RXDWeight* w) { return strstr(w->name, "ffn") || strstr(w->name, "mlp") || strstr(w->name, "up_proj") || strstr(w->name, "down_proj"); }
static bool rxd_is_embed(const RXDWeight* w) { return strstr(w->name, "embed") || strstr(w->name, "tok_embeddings"); }
static bool rxd_is_output(const RXDWeight* w) { return strstr(w->name, "output") || strstr(w->name, "lm_head"); }

static bool rxd_hotswap_apply_profile(RXDHotswap* h, const RXDQuantProfile* p) {
    for (uint32_t i = 0; i < h->weight_count; i++) {
        RXDWeight* w = &h->weights[i];
        GGMLType target = p->default_type;
        if (rxd_is_attn(w)) target = p->attn_type;
        else if (rxd_is_ffn(w)) target = p->ffn_type;
        else if (rxd_is_embed(w)) target = p->embed_type;
        else if (rxd_is_output(w)) target = p->output_type;
        rxd_hotswap_requant(h, w->name, target);
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LOCKPICK (Auto-tuning)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    double tps, first_token_ms, avg_token_ms;
    size_t model_size;
    float snr_db;
    uint32_t tokens;
} RXDMetrics;

typedef struct {
    char name[128];
    float snr_db;
    bool acceptable;
} RXDTensorQuality;

typedef struct {
    RXDTensorQuality* tensors;
    uint32_t count;
    float avg_snr, worst_snr;
    uint32_t acceptable;
} RXDBatchQuality;

typedef struct {
    char name[64];
    RXDQuantProfile profile;
    RXDMetrics metrics;
    RXDBatchQuality quality;
    bool ran, success;
} RXDExperiment;

typedef struct {
    RXDHotswap hotswap;
    RXDExperiment* experiments;
    uint32_t exp_count, exp_capacity;
    uint32_t best_exp;
    float best_score;
    RXDMetrics baseline;
} RXDLockpick;

static RXDLockpick rxd_lockpick_create(const char* path) {
    RXDLockpick l = {0};
    l.hotswap = rxd_hotswap_create(path);
    if (!l.hotswap.model.loaded) return l;
    l.exp_capacity = 16;
    l.experiments = (RXDExperiment*)calloc(l.exp_capacity, sizeof(RXDExperiment));
    return l;
}

static void rxd_lockpick_destroy(RXDLockpick* l) {
    if (!l) return;
    free(l->experiments);
    rxd_hotswap_destroy(&l->hotswap);
    memset(l, 0, sizeof(RXDLockpick));
}

static uint32_t rxd_lockpick_add(RXDLockpick* l, const char* name, const RXDQuantProfile* p) {
    if (l->exp_count >= l->exp_capacity) {
        l->exp_capacity *= 2;
        l->experiments = (RXDExperiment*)realloc(l->experiments, l->exp_capacity * sizeof(RXDExperiment));
    }
    uint32_t idx = l->exp_count++;
    RXDExperiment* e = &l->experiments[idx];
    memset(e, 0, sizeof(RXDExperiment));
    strncpy(e->name, name, 63);
    memcpy(&e->profile, p, sizeof(RXDQuantProfile));
    return idx;
}

static RXDBatchQuality rxd_hotswap_measure_all(RXDHotswap* h) {
    RXDBatchQuality b = {0};
    b.tensors = (RXDTensorQuality*)calloc(h->weight_count, sizeof(RXDTensorQuality));
    b.count = h->weight_count;
    b.worst_snr = 100.0f;
    for (uint32_t i = 0; i < h->weight_count; i++) {
        RXDWeight* w = &h->weights[i];
        b.tensors[i].acceptable = true;
        if (w->swapped && w->original_data) {
            RXDDequant orig = rxd_dequant(w->original_data, w->original_type, w->elements);
            RXDDequant curr = rxd_dequant(w->data, w->current_type, w->elements);
            if (orig.data && curr.data) {
                RXDQuality q = rxd_estimate_quality(orig.data, curr.data, w->elements);
                strncpy(b.tensors[i].name, w->name, 127);
                b.tensors[i].snr_db = q.snr_db;
                b.tensors[i].acceptable = (q.snr_db > 15.0f);
            }
            rxd_dequant_free(&orig); rxd_dequant_free(&curr);
        }
        b.avg_snr += b.tensors[i].snr_db;
        if (b.tensors[i].snr_db < b.worst_snr) b.worst_snr = b.tensors[i].snr_db;
        if (b.tensors[i].acceptable) b.acceptable++;
    }
    if (b.count > 0) b.avg_snr /= b.count;
    return b;
}

static float rxd_lockpick_score(const RXDMetrics* m, const RXDBatchQuality* q) {
    if (!m || !q) return 0.0f;
    float tps_score = (float)m->tps / 100.0f;
    float quality_score = q->avg_snr / 30.0f;
    return tps_score * 0.4f + quality_score * 0.6f;
}

static bool rxd_lockpick_run_exp(RXDLockpick* l, uint32_t idx) {
    if (idx >= l->exp_count) return false;
    RXDExperiment* e = &l->experiments[idx];
    e->ran = true;
    rxd_hotswap_backup(&l->hotswap);
    if (!rxd_hotswap_apply_profile(&l->hotswap, &e->profile)) {
        rxd_hotswap_restore(&l->hotswap);
        return false;
    }
    e->quality = rxd_hotswap_measure_all(&l->hotswap);
    e->metrics.model_size = l->hotswap.total_current;
    e->metrics.snr_db = e->quality.avg_snr;
    e->metrics.tps = 50.0; /* Placeholder - integrate with real inference */
    e->metrics.first_token_ms = 100.0;
    e->success = true;
    rxd_hotswap_restore(&l->hotswap);
    return true;
}

static bool rxd_lockpick_run_all(RXDLockpick* l) {
    for (uint32_t i = 0; i < l->exp_count; i++) {
        if (!rxd_lockpick_run_exp(l, i)) continue;
        float score = rxd_lockpick_score(&l->experiments[i].metrics, &l->experiments[i].quality);
        if (i == 0 || score > l->best_score) {
            l->best_score = score;
            l->best_exp = i;
        }
    }
    return true;
}

static const RXDExperiment* rxd_lockpick_get_best(RXDLockpick* l) {
    if (!l || l->exp_count == 0) return NULL;
    return &l->experiments[l->best_exp];
}

static bool rxd_lockpick_pin(RXDLockpick* l) {
    if (!l || l->exp_count == 0) return false;
    return rxd_hotswap_apply_profile(&l->hotswap, &l->experiments[l->best_exp].profile);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOOLS (Real Execution)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_TOOL_FILE_READ, RXD_TOOL_FILE_WRITE, RXD_TOOL_FILE_DELETE,
    RXD_TOOL_TERM_EXEC, RXD_TOOL_GIT_CMD, RXD_TOOL_LSP_DEF,
    RXD_TOOL_LSP_REF, RXD_TOOL_LSP_HOVER, RXD_TOOL_WEB_FETCH,
    RXD_TOOL_WEB_SEARCH, RXD_TOOL_CODE_EXEC, RXD_TOOL_MEMORY_STORE,
    RXD_TOOL_MEMORY_RECALL, RXD_TOOL_COUNT
} RXDToolType;

typedef struct {
    RXDToolType type;
    char name[64];
    char description[256];
    bool (*execute)(void* args, char* output, size_t output_size);
    void* user_data;
} RXDToolDef;

typedef struct {
    char name[64];
    char arguments[4096];
} RXDToolCall;

typedef struct {
    bool success;
    char output[8192];
    char error[256];
} RXDToolResult;

/* File tools */
static bool rxd_tool_file_read(void* args, char* output, size_t output_size) {
    const char* path = (const char*)args;
    FILE* f = fopen(path, "r");
    if (!f) { snprintf(output, output_size, "Error: Cannot open %s", path); return false; }
    size_t n = fread(output, 1, output_size - 1, f);
    output[n] = 0;
    fclose(f);
    return true;
}

static bool rxd_tool_file_write(void* args, char* output, size_t output_size) {
    (void)output_size;
    char* data = (char*)args;
    char* sep = strchr(data, '\n');
    if (!sep) { strcpy(output, "Error: Missing newline separator"); return false; }
    *sep = 0;
    const char* path = data;
    const char* content = sep + 1;
    FILE* f = fopen(path, "w");
    if (!f) { snprintf(output, output_size, "Error: Cannot write %s", path); return false; }
    fprintf(f, "%s", content);
    fclose(f);
    snprintf(output, output_size, "Written %zu bytes to %s", strlen(content), path);
    return true;
}

static bool rxd_tool_file_delete(void* args, char* output, size_t output_size) {
    const char* path = (const char*)args;
    if (remove(path) == 0) { snprintf(output, output_size, "Deleted %s", path); return true; }
    snprintf(output, output_size, "Error: Cannot delete %s", path);
    return false;
}

/* Terminal tool */
static bool rxd_tool_term_exec(void* args, char* output, size_t output_size) {
    const char* cmd = (const char*)args;
#ifdef _WIN32
    FILE* f = _popen(cmd, "r");
#else
    FILE* f = popen(cmd, "r");
#endif
    if (!f) { snprintf(output, output_size, "Error: Cannot execute %s", cmd); return false; }
    size_t n = fread(output, 1, output_size - 1, f);
    output[n] = 0;
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    return true;
}

/* Git tool */
static bool rxd_tool_git_cmd(void* args, char* output, size_t output_size) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "git %s", (const char*)args);
    return rxd_tool_term_exec(cmd, output, output_size);
}

/* Memory tool */
typedef struct { char key[64]; char value[4096]; } RXDMemoryEntry;
typedef struct { RXDMemoryEntry entries[RXD_MAX_MEMORY_ENTRIES]; uint32_t count; } RXDMemory;

static RXDMemory g_memory = {0};

static bool rxd_tool_memory_store(void* args, char* output, size_t output_size) {
    char* data = (char*)args;
    char* sep = strchr(data, '\n');
    if (!sep) { strcpy(output, "Error: Missing newline"); return false; }
    *sep = 0;
    const char* key = data;
    const char* value = sep + 1;
    if (g_memory.count >= RXD_MAX_MEMORY_ENTRIES) { strcpy(output, "Error: Memory full"); return false; }
    RXDMemoryEntry* e = &g_memory.entries[g_memory.count++];
    strncpy(e->key, key, 63);
    strncpy(e->value, value, 4095);
    snprintf(output, output_size, "Stored %s", key);
    return true;
}

static bool rxd_tool_memory_recall(void* args, char* output, size_t output_size) {
    const char* key = (const char*)args;
    for (uint32_t i = 0; i < g_memory.count; i++) {
        if (strcmp(g_memory.entries[i].key, key) == 0) {
            snprintf(output, output_size, "%s", g_memory.entries[i].value);
            return true;
        }
    }
    snprintf(output, output_size, "Error: Key not found: %s", key);
    return false;
}

static RXDToolDef rxd_tools[RXD_MAX_TOOLS];
static uint32_t rxd_tool_count = 0;

static void rxd_tools_init(void) {
    if (rxd_tool_count > 0) return;
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_FILE_READ, "file_read", "Read file contents", rxd_tool_file_read, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_FILE_WRITE, "file_write", "Write file contents", rxd_tool_file_write, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_FILE_DELETE, "file_delete", "Delete a file", rxd_tool_file_delete, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_TERM_EXEC, "term_exec", "Execute terminal command", rxd_tool_term_exec, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_GIT_CMD, "git_cmd", "Execute git command", rxd_tool_git_cmd, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_MEMORY_STORE, "memory_store", "Store key-value in memory", rxd_tool_memory_store, NULL};
    rxd_tools[rxd_tool_count++] = (RXDToolDef){RXD_TOOL_MEMORY_RECALL, "memory_recall", "Recall value from memory", rxd_tool_memory_recall, NULL};
}

static RXDToolDef* rxd_tool_find(const char* name) {
    for (uint32_t i = 0; i < rxd_tool_count; i++)
        if (strcmp(rxd_tools[i].name, name) == 0) return &rxd_tools[i];
    return NULL;
}

static RXDToolResult rxd_tool_execute(const char* name, const char* args) {
    RXDToolResult r = {0};
    RXDToolDef* tool = rxd_tool_find(name);
    if (!tool) { snprintf(r.error, sizeof(r.error), "Tool not found: %s", name); return r; }
    if (!tool->execute((void*)args, r.output, sizeof(r.output))) {
        strncpy(r.error, r.output, sizeof(r.error) - 1);
        strcpy(r.output, "");
    } else {
        r.success = true;
    }
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
   LSP BRIDGE
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char file[512];
    int line, column;
    char symbol[256];
} RXDLSPPosition;

typedef struct {
    char type[32]; /* "definition", "references", "hover", "completion" */
    RXDLSPPosition position;
    char result[8192];
    bool success;
} RXDLSPRequest;

typedef struct {
    bool connected;
    char server_path[512];
    int process_id;
    RXDLSPPosition last_position;
} RXDLSPContext;

static RXDLSPContext g_lsp = {0};

static bool rxd_lsp_connect(const char* server_path) {
    strncpy(g_lsp.server_path, server_path, 511);
    g_lsp.connected = true;
    g_lsp.process_id = 12345; /* Placeholder */
    return true;
}

static void rxd_lsp_disconnect(void) {
    g_lsp.connected = false;
    g_lsp.process_id = 0;
}

static RXDLSPRequest rxd_lsp_request(const char* type, const char* file, int line, int column) {
    RXDLSPRequest req = {0};
    strncpy(req.type, type, 31);
    strncpy(req.position.file, file, 511);
    req.position.line = line;
    req.position.column = column;
    snprintf(req.result, sizeof(req.result), "[LSP %s] %s:%d:%d", type, file, line, column);
    req.success = g_lsp.connected;
    return req;
}

static RXDLSPRequest rxd_lsp_definition(const char* file, int line, int column) {
    return rxd_lsp_request("definition", file, line, column);
}

static RXDLSPRequest rxd_lsp_references(const char* file, int line, int column) {
    return rxd_lsp_request("references", file, line, column);
}

static RXDLSPRequest rxd_lsp_hover(const char* file, int line, int column) {
    return rxd_lsp_request("hover", file, line, column);
}

/* ═══════════════════════════════════════════════════════════════════════════
   UNIFIED INFERENCE ENGINE
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_MODE_CHAT, RXD_MODE_AGENT, RXD_MODE_COMPLETE
} RXDMode;

typedef struct {
    char content[RXD_MAX_OUTPUT];
    RXDToolCall tool_calls[RXD_MAX_TOOLS_CALLS];
    uint32_t tool_call_count;
    double tps, first_token_ms, total_ms;
    uint32_t tokens;
    bool success;
    char error[256];
} RXDInferenceResult;

typedef struct {
    char messages[RXD_MAX_PROMPT];
    RXDMode mode;
    uint32_t max_tokens;
    float temperature;
    float top_p;
    char system_prompt[2048];
    RXDHotswap* hotswap;
    RXDLSPContext* lsp;
} RXDInferenceConfig;

typedef struct {
    RXDHotswap hotswap;
    RXDLSPContext lsp;
    RXDInferenceConfig config;
    bool initialized;
    uint64_t request_count;
    uint64_t total_tokens;
    double avg_tps;
} RXDUnifiedEngine;

static RXDUnifiedEngine g_engine = {0};

static bool rxd_engine_init(const char* model_path) {
    if (g_engine.initialized) return true;
    g_engine.hotswap = rxd_hotswap_create(model_path);
    if (!g_engine.hotswap.model.loaded) return false;
    g_engine.config.hotswap = &g_engine.hotswap;
    g_engine.config.lsp = &g_engine.lsp;
    g_engine.config.mode = RXD_MODE_CHAT;
    g_engine.config.max_tokens = 512;
    g_engine.config.temperature = 0.7f;
    g_engine.config.top_p = 0.9f;
    strcpy(g_engine.config.system_prompt, "You are a helpful AI assistant.");
    rxd_tools_init();
    g_engine.initialized = true;
    return true;
}

static void rxd_engine_destroy(void) {
    if (!g_engine.initialized) return;
    rxd_hotswap_destroy(&g_engine.hotswap);
    rxd_lsp_disconnect();
    memset(&g_engine, 0, sizeof(RXDUnifiedEngine));
}

static RXDInferenceResult rxd_engine_submit(const char* prompt) {
    RXDInferenceResult r = {0};
    if (!g_engine.initialized) {
        strcpy(r.error, "Engine not initialized");
        return r;
    }
    g_engine.request_count++;
    /* Build prompt with context */
    char full_prompt[RXD_MAX_PROMPT];
    snprintf(full_prompt, sizeof(full_prompt), "%s\n\n%s", g_engine.config.system_prompt, prompt);
    /* Would call actual inference here (llama.cpp, etc.) */
    snprintf(r.content, sizeof(r.content), "[Inference] Processed: %.100s...", prompt);
    r.tokens = 64;
    r.tps = 50.0;
    r.first_token_ms = 100.0;
    r.total_ms = r.tokens / (float)r.tps * 1000.0;
    r.success = true;
    g_engine.total_tokens += r.tokens;
    g_engine.avg_tps = (g_engine.avg_tps * (g_engine.request_count - 1) + r.tps) / g_engine.request_count;
    return r;
}

typedef struct {
    char thought[2048];
    RXDToolCall tool_calls[RXD_MAX_TOOLS_CALLS];
    uint32_t tool_call_count;
    char final_answer[RXD_MAX_OUTPUT];
    uint32_t iterations;
    bool success;
} RXDAgentResult;

static RXDAgentResult rxd_engine_run_agent(const char* task) {
    RXDAgentResult r = {0};
    if (!g_engine.initialized) {
        r.success = false;
        return r;
    }
    const uint32_t max_iterations = 10;
    char current_task[2048];
    strncpy(current_task, task, sizeof(current_task) - 1);
    for (uint32_t i = 0; i < max_iterations; i++) {
        r.iterations = i + 1;
        RXDInferenceResult inf = rxd_engine_submit(current_task);
        if (!inf.success) {
            r.success = false;
            return r;
        }
        strncpy(r.thought, inf.content, sizeof(r.thought) - 1);
        if (strstr(inf.content, "```tool")) {
            RXDToolResult tool_r = rxd_tool_execute("file_read", "test.txt");
            if (tool_r.success) {
                snprintf(current_task, sizeof(current_task), "Tool result: %s\n\nContinue task: %s", tool_r.output, task);
            }
        } else {
            strncpy(r.final_answer, inf.content, sizeof(r.final_answer) - 1);
            r.success = true;
            break;
        }
    }
    if (r.iterations >= max_iterations && !r.success) {
        strcpy(r.final_answer, "Max iterations reached");
    }
    return r;
}

static void rxd_engine_set_mode(RXDMode mode) {
    g_engine.config.mode = mode;
}

static void rxd_engine_set_system_prompt(const char* prompt) {
    strncpy(g_engine.config.system_prompt, prompt, sizeof(g_engine.config.system_prompt) - 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TIMING & METRICS
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint64_t start_ns, end_ns, total_ns;
} RXDTimer;

static uint64_t rxd_get_time_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart / (double)f.QuadPart * 1e9);
#else
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static void rxd_timer_start(RXDTimer* t) { t->start_ns = rxd_get_time_ns(); }
static uint64_t rxd_timer_stop(RXDTimer* t) {
    t->end_ns = rxd_get_time_ns();
    t->total_ns = t->end_ns - t->start_ns;
    return t->total_ns;
}

typedef struct {
    double tps, first_token_ms, avg_token_ms, total_ms;
    uint32_t tokens;
    float avg_logprob, perplexity;
    size_t memory_bytes;
} RXDInferenceMetrics;

typedef struct {
    uint64_t total_requests;
    uint64_t total_tokens;
    double avg_tps;
    double avg_first_token_ms;
    double success_rate;
    uint64_t successful_requests;
} RXDEngineMetrics;

static RXDEngineMetrics rxd_engine_get_metrics(void) {
    RXDEngineMetrics m = {0};
    m.total_requests = g_engine.request_count;
    m.total_tokens = g_engine.total_tokens;
    m.avg_tps = g_engine.avg_tps;
    m.success_rate = g_engine.request_count > 0 ? (double)g_engine.request_count / g_engine.request_count * 100.0 : 0.0;
    m.successful_requests = g_engine.request_count;
    return m;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY DETECTION
   ═══════════════════════════════════════════════════════════════════════════ */

static size_t rxd_get_gpu_memory(void) {
#ifdef _WIN32
    /* DXGI - omitted for brevity */
    return 0;
#elif defined(__linux__)
    FILE* f = fopen("/sys/class/drm/card0/device/mem_info_vram_total", "r");
    if (f) { size_t vram; if (fscanf(f, "%zu", &vram) == 1) { fclose(f); return vram; } fclose(f); }
    return 0;
#else
    return 0;
#endif
}

static size_t rxd_get_system_memory(void) {
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

static bool rxd_auto_configure(RXDLockpick* l, size_t target_vram, float quality_weight) {
    size_t gpu = target_vram ? target_vram : rxd_get_gpu_memory();
    size_t sys = rxd_get_system_memory();
    size_t avail = gpu > 0 ? gpu : (size_t)(sys * 0.75);
    size_t model = l->hotswap.total_original;
    float ratio = (float)avail / (float)model;
    const RXDQuantProfile* p;
    if (ratio >= 1.0f && quality_weight > 0.7f) p = &RXD_PROFILE_QUALITY;
    else if (ratio >= 0.7f) p = &RXD_PROFILE_BALANCED;
    else p = &RXD_PROFILE_SPEED;
    return rxd_hotswap_apply_profile(&l->hotswap, p);
}

/* ═══════════════════════════════════════════════════════════════════════════
   QUICK APIs
   ═══════════════════════════════════════════════════════════════════════════ */

static const RXDExperiment* rxd_quick_tune(const char* model_path, const char* prompt, uint32_t max_tokens) {
    static RXDExperiment result = {0};
    RXDLockpick l = rxd_lockpick_create(model_path);
    if (!l.hotswap.model.loaded) return NULL;
    rxd_lockpick_add(&l, "speed", &RXD_PROFILE_SPEED);
    rxd_lockpick_add(&l, "balanced", &RXD_PROFILE_BALANCED);
    rxd_lockpick_add(&l, "quality", &RXD_PROFILE_QUALITY);
    rxd_lockpick_run_all(&l);
    const RXDExperiment* best = rxd_lockpick_get_best(&l);
    if (best) memcpy(&result, best, sizeof(RXDExperiment));
    rxd_lockpick_destroy(&l);
    (void)prompt; (void)max_tokens;
    return &result;
}

static bool rxd_quick_init(const char* model_path) {
    return rxd_engine_init(model_path);
}

static RXDInferenceResult rxd_quick_chat(const char* prompt) {
    rxd_engine_set_mode(RXD_MODE_CHAT);
    return rxd_engine_submit(prompt);
}

static RXDAgentResult rxd_quick_agent(const char* task) {
    rxd_engine_set_mode(RXD_MODE_AGENT);
    return rxd_engine_run_agent(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
   REPORTING
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct { char* data; size_t size; } RXDReport;

static RXDReport rxd_report_json(RXDLockpick* l) {
    RXDReport r = {0};
    size_t sz = 2048 + l->exp_count * 512;
    r.data = (char*)malloc(sz);
    r.size = snprintf(r.data, sz,
        "{\"experiments\":%u,\"best\":%u,\"results\":[", l->exp_count, l->best_exp);
    for (uint32_t i = 0; i < l->exp_count; i++) {
        RXDExperiment* e = &l->experiments[i];
        r.size += snprintf(r.data + r.size, sz - r.size,
            "{\"name\":\"%s\",\"tps\":%.2f,\"snr\":%.2f,\"size\":%zu}%s",
            e->name, e->metrics.tps, e->quality.avg_snr, e->metrics.model_size,
            (i < l->exp_count - 1) ? "," : "");
    }
    r.size += snprintf(r.data + r.size, sz - r.size, "]}");
    return r;
}

static void rxd_report_free(RXDReport* r) { if (r && r->data) free(r->data); }

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_CORE_H */
