// ============================================================================
// QCORR - Quantization with Correction Offsets - Implementation
// ============================================================================

#define QCORR_IMPLEMENTATION
#include "qcorr.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <intrin.h>
#include <malloc.h>
#else
#include <x86intrin.h>
#include <stdlib.h>
#endif

// ============================================================================
// Memory Management
// ============================================================================
void* qcorr_alloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void* qcorr_alloc_aligned(size_t size, size_t alignment) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

void qcorr_free(void* ptr) {
#ifdef _WIN32
    if (ptr) _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// ============================================================================
// Quantization Primitives
// ============================================================================
void quantize_int4(const float* src, uint8_t* dst, uint32_t n, float scale, float min_val) {
    for (uint32_t i = 0; i < n; i += 2) {
        int8_t v0 = (int8_t)fmaxf(-7.0f, fminf(7.0f, roundf((src[i] - min_val) / scale)));
        int8_t v1 = (i + 1 < n) ? (int8_t)fmaxf(-7.0f, fminf(7.0f, roundf((src[i + 1] - min_val) / scale))) : 0;
        dst[i / 2] = (v0 & 0x0F) | ((v1 & 0x0F) << 4);
    }
}

void dequantize_int4(const uint8_t* src, float* dst, uint32_t n, float scale, float min_val) {
    for (uint32_t i = 0; i < n; i += 2) {
        uint8_t packed = src[i / 2];
        int8_t v0 = (packed & 0x0F);
        if (v0 > 7) v0 -= 16;
        dst[i] = v0 * scale + min_val;
        
        if (i + 1 < n) {
            int8_t v1 = (packed >> 4) & 0x0F;
            if (v1 > 7) v1 -= 16;
            dst[i + 1] = v1 * scale + min_val;
        }
    }
}

void quantize_int8(const float* src, int8_t* dst, uint32_t n, float* scale, float* min_val) {
    float max_val = src[0], min_v = src[0];
    for (uint32_t i = 1; i < n; i++) {
        if (src[i] > max_val) max_val = src[i];
        if (src[i] < min_v) min_v = src[i];
    }
    
    float abs_max = fmaxf(fabsf(max_val), fabsf(min_v));
    *scale = abs_max / 127.0f;
    *min_val = 0.0f;
    
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = (int8_t)fmaxf(-127.0f, fminf(127.0f, roundf(src[i] / *scale)));
    }
}

void dequantize_int8(const int8_t* src, float* dst, uint32_t n, float scale, float min_val) {
    (void)min_val;
    for (uint32_t i = 0; i < n; i++) {
        dst[i] = src[i] * scale;
    }
}

// ============================================================================
// SIMD-Optimized Matrix Operations
// ============================================================================
#ifdef __AVX512F__
static float dot_product_int8_avx512(const int8_t* weights, const float* input, uint32_t n, float scale) {
    __m512 sum_vec = _mm512_setzero_ps();
    __m512 scale_vec = _mm512_set1_ps(scale);
    
    uint32_t i = 0;
    for (; i + 15 < n; i += 16) {
        __m128i v8 = _mm_loadu_si128((__m128i*)(weights + i));
        __m512i v32 = _mm512_cvtepi8_epi32(v8);
        __m512 w = _mm512_cvtepi32_ps(v32);
        __m512 x = _mm512_loadu_ps(input + i);
        __m512 prod = _mm512_mul_ps(w, x);
        sum_vec = _mm512_add_ps(sum_vec, prod);
    }
    
    float sum = _mm512_reduce_add_ps(sum_vec);
    for (; i < n; i++) sum += weights[i] * input[i] * scale;
    return sum;
}
#else
static float dot_product_int8_scalar(const int8_t* weights, const float* input, uint32_t n, float scale) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) sum += weights[i] * input[i] * scale;
    return sum;
}
#endif

// ============================================================================
// Outlier Detection
// ============================================================================
void find_outliers(const float* weights, const int8_t* quant_weights, uint32_t n, float scale, float percentile, SparseCorrection* correction) {
    float* errors = qcorr_alloc(n * sizeof(float));
    
    for (uint32_t i = 0; i < n; i++) {
        float quant_val = quant_weights[i] * scale;
        errors[i] = fabsf(weights[i] - quant_val);
    }
    
    // Find k-th largest error
    uint32_t k = (uint32_t)(n * (1.0f - percentile / 100.0f));
    float threshold = 0.0f;
    
    // Simple selection for threshold
    for (uint32_t i = 0; i < k + 1 && i < n; i++) {
        uint32_t max_idx = i;
        for (uint32_t j = i + 1; j < n; j++) {
            if (errors[j] > errors[max_idx]) max_idx = j;
        }
        float tmp = errors[i];
        errors[i] = errors[max_idx];
        errors[max_idx] = tmp;
        if (i == k) threshold = errors[i];
    }
    
    // Count outliers
    uint32_t outlier_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (errors[i] >= threshold) outlier_count++;
    }
    
    // Allocate correction
    correction->capacity = outlier_count;
    correction->indices = qcorr_alloc(outlier_count * sizeof(uint32_t));
    correction->values = qcorr_alloc(outlier_count * sizeof(float));
    correction->num_outliers = outlier_count;
    
    // Store outliers
    uint32_t idx = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (errors[i] >= threshold) {
            correction->indices[idx] = i;
            correction->values[idx] = weights[i] - quant_weights[i] * scale;
            idx++;
        }
    }
    
    qcorr_free(errors);
}

// ============================================================================
// SVD Compression
// ============================================================================
void svd_compress(const float* residual, uint32_t n, uint32_t m, uint32_t rank, SVDCorrection* svd_corr) {
    svd_corr->U = qcorr_alloc(n * rank * sizeof(float));
    svd_corr->V = qcorr_alloc(rank * m * sizeof(float));
    svd_corr->rank = rank;
    svd_corr->n = n;
    svd_corr->m = m;
    
    // Initialize with small random values
    for (uint32_t i = 0; i < n * rank; i++) svd_corr->U[i] = residual[i % (n * m)] * 0.01f;
    for (uint32_t i = 0; i < rank * m; i++) svd_corr->V[i] = residual[i % (n * m)] * 0.01f;
}

void apply_svd_correction(const SVDCorrection* svd_corr, const float* input, float* output, uint32_t batch_size) {
    uint32_t n = svd_corr->n;
    uint32_t m = svd_corr->m;
    uint32_t k = svd_corr->rank;
    
    float* temp = qcorr_alloc(k * batch_size * sizeof(float));
    
    // temp = V^T * input
    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t i = 0; i < k; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < m; j++) {
                sum += svd_corr->V[i * m + j] * input[b * m + j];
            }
            temp[b * k + i] = sum;
        }
    }
    
    // output += U * temp
    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t i = 0; i < n; i++) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < k; j++) {
                sum += svd_corr->U[i * k + j] * temp[b * k + j];
            }
            output[b * n + i] += sum;
        }
    }
    
    qcorr_free(temp);
}

// ============================================================================
// Tensor Quantization
// ============================================================================
void quantize_tensor(const float* weights, QTensor* tensor, uint32_t rows, uint32_t cols, const QuantParams* params) {
    tensor->rows = rows;
    tensor->cols = cols;
    tensor->quant_type = params->target_type;
    tensor->corr_mode = params->corr_mode;
    tensor->block_rows = params->block_size;
    tensor->block_cols = params->block_size;
    
    uint32_t num_blocks = ((rows + params->block_size - 1) / params->block_size) *
                          ((cols + params->block_size - 1) / params->block_size);
    tensor->scales = qcorr_alloc(num_blocks * sizeof(float));
    tensor->mins = qcorr_alloc(num_blocks * sizeof(float));
    
    size_t quant_size = (params->target_type == QINT4) ? (rows * cols + 1) / 2 : rows * cols;
    tensor->quant_data = qcorr_alloc(quant_size);
    
    // Quantize block by block
    for (uint32_t br = 0; br < rows; br += params->block_size) {
        for (uint32_t bc = 0; bc < cols; bc += params->block_size) {
            uint32_t block_idx = (br / params->block_size) * ((cols + params->block_size - 1) / params->block_size) + (bc / params->block_size);
            
            // Find block bounds
            float max_val = -1e30f, min_val = 1e30f;
            for (uint32_t r = br; r < br + params->block_size && r < rows; r++) {
                for (uint32_t c = bc; c < bc + params->block_size && c < cols; c++) {
                    float val = weights[r * cols + c];
                    if (val > max_val) max_val = val;
                    if (val < min_val) min_val = val;
                }
            }
            
            float scale = (max_val - min_val) / 255.0f;
            tensor->scales[block_idx] = scale;
            tensor->mins[block_idx] = min_val;
            
            // Quantize
            if (params->target_type == QINT4) {
                for (uint32_t r = br; r < br + params->block_size && r < rows; r++) {
                    for (uint32_t c = bc; c < bc + params->block_size && c < cols; c++) {
                        uint32_t idx = r * cols + c;
                        int8_t quant = (int8_t)fmaxf(-7.0f, fminf(7.0f, roundf((weights[idx] - min_val) / scale)));
                        uint8_t* packed = (uint8_t*)tensor->quant_data;
                        if (idx % 2 == 0) {
                            packed[idx / 2] = (packed[idx / 2] & 0xF0) | (quant & 0x0F);
                        } else {
                            packed[idx / 2] = (packed[idx / 2] & 0x0F) | ((quant & 0x0F) << 4);
                        }
                    }
                }
            } else {
                int8_t* quant_data = (int8_t*)tensor->quant_data;
                for (uint32_t r = br; r < br + params->block_size && r < rows; r++) {
                    for (uint32_t c = bc; c < bc + params->block_size && c < cols; c++) {
                        uint32_t idx = r * cols + c;
                        quant_data[idx] = (int8_t)fmaxf(-127.0f, fminf(127.0f, roundf(weights[idx] / scale)));
                    }
                }
            }
        }
    }
    
    // Calculate MSE
    tensor->mse = 0.0f;
    uint32_t n = rows * cols;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t block_idx = (i / cols / params->block_size) * ((cols + params->block_size - 1) / params->block_size) + (i % cols / params->block_size);
        float quant_val;
        
        if (params->target_type == QINT4) {
            uint8_t packed = ((uint8_t*)tensor->quant_data)[i / 2];
            int8_t v = (i % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
            if (v > 7) v -= 16;
            quant_val = v * tensor->scales[block_idx] + tensor->mins[block_idx];
        } else {
            quant_val = ((int8_t*)tensor->quant_data)[i] * tensor->scales[block_idx];
        }
        
        float error = weights[i] - quant_val;
        tensor->mse += error * error;
    }
    tensor->mse /= n;
    
    // Compression ratio
    size_t orig_size = n * sizeof(float);
    size_t quant_size_bits = (params->target_type == QINT4) ? (n + 1) / 2 : n;
    tensor->compression_ratio = (float)orig_size / quant_size_bits;
}

// ============================================================================
// Matrix Multiplication with Corrections
// ============================================================================
void qmatmul(const QTensor* weights, const float* input, float* output, uint32_t batch_size, uint32_t in_features, uint32_t out_features) {
    memset(output, 0, batch_size * out_features * sizeof(float));
    
    for (uint32_t b = 0; b < batch_size; b++) {
        for (uint32_t o = 0; o < out_features; o++) {
            float sum = 0.0f;
            
            for (uint32_t i = 0; i < in_features; i++) {
                uint32_t block_idx = (o / weights->block_rows) * ((in_features + weights->block_cols - 1) / weights->block_cols) + (i / weights->block_cols);
                float scale = weights->scales[block_idx];
                float min_val = weights->mins ? weights->mins[block_idx] : 0.0f;
                
                float w;
                if (weights->quant_type == QINT4) {
                    uint8_t packed = ((uint8_t*)weights->quant_data)[(o * in_features + i) / 2];
                    int8_t v = ((o * in_features + i) % 2 == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
                    if (v > 7) v -= 16;
                    w = v * scale + min_val;
                } else {
                    w = ((int8_t*)weights->quant_data)[o * in_features + i] * scale;
                }
                
                sum += w * input[b * in_features + i];
            }
            
            output[b * out_features + o] = sum;
        }
    }
    
    // Apply corrections
    if (weights->corr_mode == CORR_SPARSE) {
        const SparseCorrection* corr = &weights->correction.sparse;
        for (uint32_t b = 0; b < batch_size; b++) {
            for (uint32_t i = 0; i < corr->num_outliers; i++) {
                uint32_t idx = corr->indices[i];
                uint32_t row = idx / in_features;
                uint32_t col = idx % in_features;
                output[b * out_features + row] += corr->values[i] * input[b * in_features + col];
            }
        }
    } else if (weights->corr_mode == CORR_SVD) {
        apply_svd_correction(&weights->correction.svd, input, output, batch_size);
    }
}

// ============================================================================
// Model Creation/Destruction
// ============================================================================
QModel* qmodel_create(uint32_t num_layers, uint32_t hidden_size, uint32_t vocab_size, uint32_t max_seq_len) {
    QModel* model = qcorr_alloc(sizeof(QModel));
    model->num_layers = num_layers;
    model->hidden_size = hidden_size;
    model->vocab_size = vocab_size;
    model->max_seq_len = max_seq_len;
    
    model->layers = qcorr_alloc(num_layers * sizeof(QLayer));
    model->kv_cache_k = qcorr_alloc(num_layers * sizeof(float*));
    model->kv_cache_v = qcorr_alloc(num_layers * sizeof(float*));
    
    for (uint32_t l = 0; l < num_layers; l++) {
        model->kv_cache_k[l] = qcorr_alloc(max_seq_len * hidden_size * sizeof(float));
        model->kv_cache_v[l] = qcorr_alloc(max_seq_len * hidden_size * sizeof(float));
    }
    
    return model;
}

void qmodel_destroy(QModel* model) {
    if (!model) return;
    
    for (uint32_t l = 0; l < model->num_layers; l++) {
        qcorr_free(model->kv_cache_k[l]);
        qcorr_free(model->kv_cache_v[l]);
    }
    
    qcorr_free(model->layers);
    qcorr_free(model->kv_cache_k);
    qcorr_free(model->kv_cache_v);
    qcorr_free(model);
}

// ============================================================================
// Model Save/Load
// ============================================================================
int qmodel_save(const QModel* model, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    fwrite("QMOD", 1, 4, f);
    uint32_t version = 1;
    fwrite(&version, 4, 1, f);
    fwrite(&model->num_layers, 4, 1, f);
    fwrite(&model->hidden_size, 4, 1, f);
    fwrite(&model->vocab_size, 4, 1, f);
    fwrite(&model->max_seq_len, 4, 1, f);
    
    fclose(f);
    return 0;
}

QModel* qmodel_load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    char magic[4];
    fread(magic, 1, 4, f);
    if (memcmp(magic, "QMOD", 4) != 0) { fclose(f); return NULL; }
    
    uint32_t version;
    fread(&version, 4, 1, f);
    
    QModel* model = qcorr_alloc(sizeof(QModel));
    fread(&model->num_layers, 4, 1, f);
    fread(&model->hidden_size, 4, 1, f);
    fread(&model->vocab_size, 4, 1, f);
    fread(&model->max_seq_len, 4, 1, f);
    
    fclose(f);
    return model;
}

// ============================================================================
// Inference Context
// ============================================================================
QContext* qcontext_create(QModel* model) {
    QContext* ctx = qcorr_alloc(sizeof(QContext));
    ctx->model = model;
    ctx->hidden_states = qcorr_alloc(model->hidden_size * sizeof(float));
    ctx->attention_scores = qcorr_alloc(model->hidden_size * sizeof(float));
    ctx->mlp_hidden = qcorr_alloc(model->hidden_size * 4 * sizeof(float));
    ctx->workspace_size = model->hidden_size * 16 * sizeof(float);
    ctx->workspace = qcorr_alloc(ctx->workspace_size);
    return ctx;
}

void qcontext_destroy(QContext* ctx) {
    if (!ctx) return;
    qcorr_free(ctx->hidden_states);
    qcorr_free(ctx->attention_scores);
    qcorr_free(ctx->mlp_hidden);
    qcorr_free(ctx->workspace);
    qcorr_free(ctx);
}

// ============================================================================
// Forward Pass
// ============================================================================
void qmodel_forward(QContext* ctx, uint32_t token) {
    QModel* model = ctx->model;
    
    // Token embedding (simplified)
    for (uint32_t i = 0; i < model->hidden_size; i++) {
        ctx->hidden_states[i] = 0.01f * (token % 100);
    }
    
    // Process layers (simplified)
    for (uint32_t l = 0; l < model->num_layers; l++) {
        // Self-attention (placeholder)
        for (uint32_t i = 0; i < model->hidden_size; i++) {
            ctx->attention_scores[i] = ctx->hidden_states[i] * 0.5f;
        }
        
        // MLP (placeholder)
        for (uint32_t i = 0; i < model->hidden_size * 4; i++) {
            float x = ctx->mlp_hidden[i];
            ctx->mlp_hidden[i] = 0.5f * x * (1.0f + tanhf(0.7978845608f * (x + 0.044715f * x * x * x)));
        }
        
        // Residual
        for (uint32_t i = 0; i < model->hidden_size; i++) {
            ctx->hidden_states[i] += ctx->attention_scores[i];
        }
    }
    
    ctx->total_tokens++;
}

// ============================================================================
// Token Generation
// ============================================================================
void qmodel_generate(QContext* ctx, const uint32_t* prompt_tokens, uint32_t prompt_len, uint32_t* output_tokens, uint32_t max_tokens) {
    // Process prompt
    for (uint32_t i = 0; i < prompt_len; i++) {
        qmodel_forward(ctx, prompt_tokens[i]);
    }
    
    // Generate tokens
    for (uint32_t i = 0; i < max_tokens; i++) {
        // Find max logit (simplified)
        float max_val = ctx->hidden_states[0];
        uint32_t max_idx = 0;
        for (uint32_t j = 1; j < ctx->model->vocab_size && j < ctx->model->hidden_size; j++) {
            if (ctx->hidden_states[j] > max_val) {
                max_val = ctx->hidden_states[j];
                max_idx = j;
            }
        }
        
        output_tokens[i] = max_idx;
        
        if (i < max_tokens - 1) {
            qmodel_forward(ctx, max_idx);
        }
    }
}

// ============================================================================
// Utility Functions
// ============================================================================
void qmodel_print_stats(const QModel* model) {
    printf("=== QModel Statistics ===\n");
    printf("Layers: %u\n", model->num_layers);
    printf("Hidden size: %u\n", model->hidden_size);
    printf("Vocab size: %u\n", model->vocab_size);
    printf("Max sequence length: %u\n", model->max_seq_len);
}

QModel* convert_fp32_to_quantized(const char* fp32_path, const char* output_path, QuantType target_type, CorrectionMode corr_mode, uint32_t block_size) {
    (void)fp32_path;
    (void)output_path;
    (void)target_type;
    (void)corr_mode;
    (void)block_size;
    // Placeholder - would load FP32 model and quantize
    return qmodel_create(32, 4096, 32000, 2048);
}