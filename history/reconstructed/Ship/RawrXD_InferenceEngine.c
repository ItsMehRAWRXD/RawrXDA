// RawrXD Inference Engine - Pure C/ASM Bridge
// This module provides the core inference functionality 
// Called by both IDE and CLI

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <intrin.h>

#pragma comment(lib, "kernel32.lib")

// ============================================================================
// GGUF V3 STRUCTURES
// ============================================================================
#define GGUF_MAGIC 0x46554747  // "GGUF"

typedef enum {
    GGUF_TYPE_UINT8   = 0,
    GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,
    GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10,
    GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12
} GGUFType;

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
    GGML_TYPE_IQ3_XXS = 18
} GGMLType;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
} GGUFHeader;

typedef struct {
    uint64_t len;
    char* data;
} GGUFString;

// Model context - loaded model state
typedef struct {
    HANDLE hFile;
    HANDLE hMapping;
    void* pData;
    uint64_t fileSize;
    
    GGUFHeader header;
    
    // Architecture info from metadata
    uint32_t n_vocab;
    uint32_t n_embd;
    uint32_t n_head;
    uint32_t n_head_kv;
    uint32_t n_layer;
    uint32_t n_ctx;
    uint32_t n_ff;
    float rope_freq_base;
    float rope_freq_scale;
    
    // Tensor data pointers
    void* tok_embeddings;
    void* output_weight;
    void* output_norm;
    
    // Layer weights
    struct {
        void* attn_q;
        void* attn_k;
        void* attn_v;
        void* attn_o;
        void* attn_norm;
        void* ffn_gate;
        void* ffn_up;
        void* ffn_down;
        void* ffn_norm;
    }* layers;
    
    // Quantization type
    GGMLType weight_type;
    
    // KV Cache
    float* kv_cache;
    int kv_cache_pos;
    int kv_cache_size;
    
    BOOL loaded;
} ModelContext;

// Inference state
typedef struct {
    float* logits;
    float* hidden_state;
    int32_t* tokens;
    int n_tokens;
    int n_past;
    float temperature;
    float top_p;
    int top_k;
    int max_new_tokens;
} InferenceState;

// ============================================================================
// MODEL LOADING
// ============================================================================

static uint64_t read_u64(const uint8_t** p) {
    uint64_t v;
    memcpy(&v, *p, 8);
    *p += 8;
    return v;
}

static uint32_t read_u32(const uint8_t** p) {
    uint32_t v;
    memcpy(&v, *p, 4);
    *p += 4;
    return v;
}

static float read_f32(const uint8_t** p) {
    float v;
    memcpy(&v, *p, 4);
    *p += 4;
    return v;
}

static GGUFString read_string(const uint8_t** p) {
    GGUFString s;
    s.len = read_u64(p);
    s.data = (char*)malloc(s.len + 1);
    memcpy(s.data, *p, s.len);
    s.data[s.len] = 0;
    *p += s.len;
    return s;
}

static void skip_value(const uint8_t** p, GGUFType type) {
    switch (type) {
        case GGUF_TYPE_UINT8:
        case GGUF_TYPE_INT8:
        case GGUF_TYPE_BOOL:
            *p += 1;
            break;
        case GGUF_TYPE_UINT16:
        case GGUF_TYPE_INT16:
            *p += 2;
            break;
        case GGUF_TYPE_UINT32:
        case GGUF_TYPE_INT32:
        case GGUF_TYPE_FLOAT32:
            *p += 4;
            break;
        case GGUF_TYPE_UINT64:
        case GGUF_TYPE_INT64:
        case GGUF_TYPE_FLOAT64:
            *p += 8;
            break;
        case GGUF_TYPE_STRING: {
            uint64_t len = read_u64(p);
            *p += len;
            break;
        }
        case GGUF_TYPE_ARRAY: {
            GGUFType elem_type = (GGUFType)read_u32(p);
            uint64_t count = read_u64(p);
            for (uint64_t i = 0; i < count; i++) {
                skip_value(p, elem_type);
            }
            break;
        }
    }
}

__declspec(dllexport)
ModelContext* LoadModel(const wchar_t* path) {
    ModelContext* ctx = (ModelContext*)calloc(1, sizeof(ModelContext));
    if (!ctx) return NULL;
    
    // Open file
    ctx->hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (ctx->hFile == INVALID_HANDLE_VALUE) {
        free(ctx);
        return NULL;
    }
    
    // Get file size
    LARGE_INTEGER fileSize;
    GetFileSizeEx(ctx->hFile, &fileSize);
    ctx->fileSize = fileSize.QuadPart;
    
    // Memory map
    ctx->hMapping = CreateFileMappingW(ctx->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!ctx->hMapping) {
        CloseHandle(ctx->hFile);
        free(ctx);
        return NULL;
    }
    
    ctx->pData = MapViewOfFile(ctx->hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!ctx->pData) {
        CloseHandle(ctx->hMapping);
        CloseHandle(ctx->hFile);
        free(ctx);
        return NULL;
    }
    
    // Parse GGUF header
    const uint8_t* p = (const uint8_t*)ctx->pData;
    
    ctx->header.magic = read_u32(&p);
    if (ctx->header.magic != GGUF_MAGIC) {
        UnmapViewOfFile(ctx->pData);
        CloseHandle(ctx->hMapping);
        CloseHandle(ctx->hFile);
        free(ctx);
        return NULL;
    }
    
    ctx->header.version = read_u32(&p);
    ctx->header.n_tensors = read_u64(&p);
    ctx->header.n_kv = read_u64(&p);
    
    // Parse metadata KV pairs
    for (uint64_t i = 0; i < ctx->header.n_kv; i++) {
        GGUFString key = read_string(&p);
        GGUFType type = (GGUFType)read_u32(&p);
        
        // Check for architecture parameters
        if (strcmp(key.data, "llama.embedding_length") == 0 || 
            strcmp(key.data, "general.embedding_length") == 0) {
            ctx->n_embd = read_u32(&p);
        } else if (strcmp(key.data, "llama.attention.head_count") == 0 ||
                   strcmp(key.data, "general.attention.head_count") == 0) {
            ctx->n_head = read_u32(&p);
        } else if (strcmp(key.data, "llama.attention.head_count_kv") == 0) {
            ctx->n_head_kv = read_u32(&p);
        } else if (strcmp(key.data, "llama.block_count") == 0 ||
                   strcmp(key.data, "general.block_count") == 0) {
            ctx->n_layer = read_u32(&p);
        } else if (strcmp(key.data, "llama.context_length") == 0 ||
                   strcmp(key.data, "general.context_length") == 0) {
            ctx->n_ctx = read_u32(&p);
        } else if (strcmp(key.data, "llama.feed_forward_length") == 0 ||
                   strcmp(key.data, "general.feed_forward_length") == 0) {
            ctx->n_ff = read_u32(&p);
        } else if (strcmp(key.data, "llama.vocab_size") == 0 ||
                   strcmp(key.data, "general.vocab_size") == 0) {
            ctx->n_vocab = read_u32(&p);
        } else if (strcmp(key.data, "llama.rope.freq_base") == 0) {
            ctx->rope_freq_base = read_f32(&p);
        } else {
            skip_value(&p, type);
        }
        
        free(key.data);
    }
    
    // Set defaults
    if (ctx->n_head_kv == 0) ctx->n_head_kv = ctx->n_head;
    if (ctx->rope_freq_base == 0) ctx->rope_freq_base = 10000.0f;
    
    // Allocate layer weight pointers
    if (ctx->n_layer > 0) {
        ctx->layers = (void*)calloc(ctx->n_layer, sizeof(*ctx->layers));
    }
    
    // Parse tensor info and find data offsets
    const uint8_t* tensor_info_start = p;
    uint64_t total_tensor_size = 0;
    
    for (uint64_t i = 0; i < ctx->header.n_tensors; i++) {
        GGUFString name = read_string(&p);
        uint32_t n_dims = read_u32(&p);
        
        uint64_t tensor_size = 1;
        for (uint32_t d = 0; d < n_dims; d++) {
            uint64_t dim = read_u64(&p);
            tensor_size *= dim;
        }
        
        GGMLType dtype = (GGMLType)read_u32(&p);
        uint64_t offset = read_u64(&p);
        
        // Calculate actual size based on type
        int bits_per_elem = 32;
        switch (dtype) {
            case GGML_TYPE_F16: bits_per_elem = 16; break;
            case GGML_TYPE_Q4_0:
            case GGML_TYPE_Q4_1:
            case GGML_TYPE_Q4_K: bits_per_elem = 4; break;
            case GGML_TYPE_Q5_0:
            case GGML_TYPE_Q5_1:
            case GGML_TYPE_Q5_K: bits_per_elem = 5; break;
            case GGML_TYPE_Q8_0:
            case GGML_TYPE_Q8_1:
            case GGML_TYPE_Q8_K: bits_per_elem = 8; break;
            case GGML_TYPE_Q2_K: bits_per_elem = 2; break;
            case GGML_TYPE_Q3_K: bits_per_elem = 3; break;
            case GGML_TYPE_Q6_K: bits_per_elem = 6; break;
            default: bits_per_elem = 32; break;
        }
        
        total_tensor_size += (tensor_size * bits_per_elem + 7) / 8;
        
        free(name.data);
    }
    
    // Allocate KV cache
    ctx->kv_cache_size = ctx->n_ctx * ctx->n_embd * 2;  // K and V
    ctx->kv_cache = (float*)_aligned_malloc(ctx->kv_cache_size * sizeof(float), 64);
    if (ctx->kv_cache) {
        memset(ctx->kv_cache, 0, ctx->kv_cache_size * sizeof(float));
    }
    
    ctx->loaded = TRUE;
    return ctx;
}

__declspec(dllexport)
void UnloadModel(ModelContext* ctx) {
    if (!ctx) return;
    
    if (ctx->kv_cache) _aligned_free(ctx->kv_cache);
    if (ctx->layers) free(ctx->layers);
    if (ctx->pData) UnmapViewOfFile(ctx->pData);
    if (ctx->hMapping) CloseHandle(ctx->hMapping);
    if (ctx->hFile && ctx->hFile != INVALID_HANDLE_VALUE) CloseHandle(ctx->hFile);
    
    free(ctx);
}

// ============================================================================
// QUANTIZATION - Q4_0 DEQUANTIZATION
// ============================================================================

// Q4_0 block: 2 bytes scale + 16 nibbles = 18 bytes for 32 weights
typedef struct {
    uint16_t d;        // FP16 scale
    uint8_t qs[16];    // 32 4-bit quantized values
} block_q4_0;

#define QK4_0 32

// FP16 to FP32 conversion
static float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        // Denormalized
        if (mant == 0) {
            uint32_t bits = sign;
            float f;
            memcpy(&f, &bits, 4);
            return f;
        }
        // Normalize
        exp = 1;
        while ((mant & 0x400) == 0) {
            mant <<= 1;
            exp--;
        }
        mant &= 0x3FF;
    } else if (exp == 31) {
        // Inf/NaN
        uint32_t bits = sign | 0x7F800000 | (mant << 13);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }
    
    uint32_t bits = sign | ((exp + 112) << 23) | (mant << 13);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

__declspec(dllexport)
void DequantizeQ4_0(const void* src, float* dst, int n) {
    const block_q4_0* blocks = (const block_q4_0*)src;
    int nb = n / QK4_0;
    
    for (int i = 0; i < nb; i++) {
        float d = fp16_to_fp32(blocks[i].d);
        
        for (int j = 0; j < 16; j++) {
            uint8_t byte = blocks[i].qs[j];
            int x0 = (byte & 0xF) - 8;
            int x1 = (byte >> 4) - 8;
            
            dst[i * QK4_0 + j * 2 + 0] = x0 * d;
            dst[i * QK4_0 + j * 2 + 1] = x1 * d;
        }
    }
}

// AVX2 optimized version
#ifdef __AVX2__
__declspec(dllexport)
void DequantizeQ4_0_AVX2(const void* src, float* dst, int n) {
    const block_q4_0* blocks = (const block_q4_0*)src;
    int nb = n / QK4_0;
    
    __m256i mask_low = _mm256_set1_epi8(0x0F);
    __m256i offset = _mm256_set1_epi8(8);
    
    for (int i = 0; i < nb; i++) {
        float d = fp16_to_fp32(blocks[i].d);
        __m256 vd = _mm256_set1_ps(d);
        
        // Load 16 bytes of quantized data
        __m128i q = _mm_loadu_si128((const __m128i*)blocks[i].qs);
        
        // Extract low and high nibbles
        __m128i q_lo = _mm_and_si128(q, _mm_set1_epi8(0x0F));
        __m128i q_hi = _mm_and_si128(_mm_srli_epi16(q, 4), _mm_set1_epi8(0x0F));
        
        // Interleave to get proper order
        __m128i q0 = _mm_unpacklo_epi8(q_lo, q_hi);
        __m128i q1 = _mm_unpackhi_epi8(q_lo, q_hi);
        
        // Subtract 8 and convert to float
        __m256i q_int = _mm256_cvtepi8_epi32(_mm_sub_epi8(q0, _mm_set1_epi8(8)));
        __m256 vf = _mm256_mul_ps(_mm256_cvtepi32_ps(q_int), vd);
        _mm256_storeu_ps(&dst[i * QK4_0 + 0], vf);
        
        q_int = _mm256_cvtepi8_epi32(_mm_sub_epi8(_mm_srli_si128(q0, 8), _mm_set1_epi8(8)));
        vf = _mm256_mul_ps(_mm256_cvtepi32_ps(q_int), vd);
        _mm256_storeu_ps(&dst[i * QK4_0 + 8], vf);
        
        q_int = _mm256_cvtepi8_epi32(_mm_sub_epi8(q1, _mm_set1_epi8(8)));
        vf = _mm256_mul_ps(_mm256_cvtepi32_ps(q_int), vd);
        _mm256_storeu_ps(&dst[i * QK4_0 + 16], vf);
        
        q_int = _mm256_cvtepi8_epi32(_mm_sub_epi8(_mm_srli_si128(q1, 8), _mm_set1_epi8(8)));
        vf = _mm256_mul_ps(_mm256_cvtepi32_ps(q_int), vd);
        _mm256_storeu_ps(&dst[i * QK4_0 + 24], vf);
    }
}
#endif

// ============================================================================
// BASIC MATH OPERATIONS
// ============================================================================

__declspec(dllexport)
void MatMul(float* C, const float* A, const float* B, int M, int N, int K) {
    // C[M,N] = A[M,K] * B[K,N]
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}

__declspec(dllexport)
void RMSNorm(float* out, const float* x, const float* weight, int n, float eps) {
    // Compute RMS
    float ss = 0.0f;
    for (int i = 0; i < n; i++) {
        ss += x[i] * x[i];
    }
    ss = 1.0f / sqrtf(ss / n + eps);
    
    // Normalize and scale
    for (int i = 0; i < n; i++) {
        out[i] = x[i] * ss * weight[i];
    }
}

__declspec(dllexport)
void Softmax(float* x, int n) {
    float max_val = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > max_val) max_val = x[i];
    }
    
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    
    for (int i = 0; i < n; i++) {
        x[i] /= sum;
    }
}

__declspec(dllexport)
void SiLU(float* x, int n) {
    for (int i = 0; i < n; i++) {
        x[i] = x[i] / (1.0f + expf(-x[i]));
    }
}

// ============================================================================
// SAMPLING
// ============================================================================

static int sample_argmax(const float* logits, int n) {
    int max_idx = 0;
    float max_val = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
            max_idx = i;
        }
    }
    return max_idx;
}

static int sample_top_p(float* logits, int n, float top_p, float temperature) {
    // Apply temperature
    for (int i = 0; i < n; i++) {
        logits[i] /= temperature;
    }
    
    // Softmax
    Softmax(logits, n);
    
    // Sort indices by probability (descending)
    int* indices = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) indices[i] = i;
    
    // Simple bubble sort (replace with proper sort for production)
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (logits[indices[j]] < logits[indices[j + 1]]) {
                int tmp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = tmp;
            }
        }
    }
    
    // Compute cumulative probability and find cutoff
    float cumsum = 0.0f;
    int cutoff = n;
    for (int i = 0; i < n; i++) {
        cumsum += logits[indices[i]];
        if (cumsum > top_p) {
            cutoff = i + 1;
            break;
        }
    }
    
    // Renormalize
    float sum = 0.0f;
    for (int i = 0; i < cutoff; i++) {
        sum += logits[indices[i]];
    }
    
    // Sample
    float r = (float)rand() / RAND_MAX * sum;
    float cdf = 0.0f;
    int selected = indices[0];
    for (int i = 0; i < cutoff; i++) {
        cdf += logits[indices[i]];
        if (r < cdf) {
            selected = indices[i];
            break;
        }
    }
    
    free(indices);
    return selected;
}

__declspec(dllexport)
int SampleNext(float* logits, int n_vocab, float temperature, float top_p, int top_k) {
    if (temperature <= 0.0f) {
        return sample_argmax(logits, n_vocab);
    }
    
    // Top-k truncation
    if (top_k > 0 && top_k < n_vocab) {
        // Find k-th largest value
        float* sorted = (float*)malloc(n_vocab * sizeof(float));
        memcpy(sorted, logits, n_vocab * sizeof(float));
        
        // Partial sort to find threshold
        for (int i = 0; i < top_k; i++) {
            for (int j = i + 1; j < n_vocab; j++) {
                if (sorted[j] > sorted[i]) {
                    float tmp = sorted[i];
                    sorted[i] = sorted[j];
                    sorted[j] = tmp;
                }
            }
        }
        
        float threshold = sorted[top_k - 1];
        free(sorted);
        
        // Zero out values below threshold
        for (int i = 0; i < n_vocab; i++) {
            if (logits[i] < threshold) {
                logits[i] = -INFINITY;
            }
        }
    }
    
    return sample_top_p(logits, n_vocab, top_p, temperature);
}

// ============================================================================
// FORWARD PASS (STUB - WIRE TO ASM KERNELS)
// ============================================================================

__declspec(dllexport)
int ForwardPass(ModelContext* ctx, int token, int pos) {
    if (!ctx || !ctx->loaded) return -1;
    
    // TODO: Implement full transformer forward pass
    // 1. Token embedding lookup
    // 2. For each layer:
    //    a. RMS norm
    //    b. Self-attention (Q, K, V projections, attention, output projection)
    //    c. Residual add
    //    d. RMS norm
    //    e. FFN (gate, up, down)
    //    f. Residual add
    // 3. Final RMS norm
    // 4. Output projection to logits
    
    // For now, return placeholder
    return 0;
}

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
