// ============================================================================
// QCORR - Quantization with Correction Offsets for 800B+ Models
// Enables running GLM-5 on 16GB VRAM with FP16+ quality from INT4
// ============================================================================

#ifndef QCORR_H
#define QCORR_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Quantization Types
// ============================================================================
typedef enum {
    QINT8 = 0,      // 8-bit integer
    QINT4 = 1,      // 4-bit integer (packed)
    QINT2 = 2,      // 2-bit integer (packed)
    QFP8 = 3,       // 8-bit floating point (E4M3/E5M2)
    QFP16 = 4,      // 16-bit floating point
    QFP32 = 5       // 32-bit floating point (reference)
} QuantType;

// ============================================================================
// Correction Modes
// ============================================================================
typedef enum {
    CORR_NONE = 0,       // No correction
    CORR_SPARSE = 1,     // Sparse correction (top outliers)
    CORR_SVD = 2,        // Low-rank SVD correction
    CORR_ADAPTIVE = 3    // Adaptive per-layer correction
} CorrectionMode;

// ============================================================================
// Constants
// ============================================================================
#define QCORR_BLOCK_SIZE 128
#define QCORR_ALIGNMENT 64
#define QCORR_MAX_LAYERS 256
#define QCORR_MAX_OUTLIERS 65536

// ============================================================================
// Sparse Correction (for outliers)
// ============================================================================
typedef struct {
    uint32_t* indices;       // Indices of outlier weights
    float* values;           // Correction values (FP16/FP32)
    uint32_t num_outliers;
    uint32_t capacity;
} SparseCorrection;

// ============================================================================
// SVD Correction (low-rank)
// ============================================================================
typedef struct {
    float* U;               // Left singular vectors (n x k)
    float* V;                // Right singular vectors (k x m)
    uint32_t rank;
    uint32_t n, m;
} SVDCorrection;

// ============================================================================
// Quantized Tensor
// ============================================================================
typedef struct {
    // Primary quantized weights
    QuantType quant_type;
    void* quant_data;
    uint32_t rows, cols;
    
    // Scale factors (per-block)
    float* scales;
    float* mins;
    uint32_t block_rows, block_cols;
    
    // Correction mechanisms
    CorrectionMode corr_mode;
    union {
        SparseCorrection sparse;
        SVDCorrection svd;
    } correction;
    
    // Statistics
    float mse;
    float compression_ratio;
} QTensor;

// ============================================================================
// Model Layer
// ============================================================================
typedef struct {
    QTensor* weights;        // Weight matrices (Q, K, V, O, MLP)
    QTensor* bias;           // Bias vectors
    float* layernorm_gamma;
    float* layernorm_beta;
    
    // Attention specific
    uint32_t num_heads;
    uint32_t head_dim;
    uint32_t num_kv_heads;
    
    // MoE specific
    uint32_t num_experts;
    uint32_t active_experts;
} QLayer;

// ============================================================================
// Full Model
// ============================================================================
typedef struct {
    QLayer* layers;
    uint32_t num_layers;
    uint32_t hidden_size;
    uint32_t vocab_size;
    uint32_t max_seq_len;
    
    // Embeddings
    QTensor* token_embeddings;
    QTensor* position_embeddings;
    
    // KV Cache
    float** kv_cache_k;
    float** kv_cache_v;
    uint32_t kv_cache_size;
    uint32_t kv_cache_pos;
} QModel;

// ============================================================================
// Quantization Parameters
// ============================================================================
typedef struct {
    QuantType target_type;
    CorrectionMode corr_mode;
    uint32_t block_size;
    uint32_t outlier_percentile;  // Top X% to treat as outliers
    uint32_t svd_rank;             // For SVD correction
    float threshold;               // Error threshold for corrections
} QuantParams;

// ============================================================================
// Inference Context
// ============================================================================
typedef struct {
    QModel* model;
    
    // Activation buffers
    float* hidden_states;
    float* attention_scores;
    float* mlp_hidden;
    
    // Workspace
    float* workspace;
    size_t workspace_size;
    
    // Performance tracking
    uint64_t total_tokens;
    uint64_t total_time_us;
    uint32_t tokens_per_second;
} QContext;

// ============================================================================
// Memory Management
// ============================================================================
void* qcorr_alloc(size_t size);
void* qcorr_alloc_aligned(size_t size, size_t alignment);
void qcorr_free(void* ptr);

// ============================================================================
// Quantization Functions
// ============================================================================
void quantize_int4(const float* src, uint8_t* dst, uint32_t n, float scale, float min_val);
void dequantize_int4(const uint8_t* src, float* dst, uint32_t n, float scale, float min_val);
void quantize_int8(const float* src, int8_t* dst, uint32_t n, float* scale, float* min_val);
void dequantize_int8(const int8_t* src, float* dst, uint32_t n, float scale, float min_val);

// ============================================================================
// Tensor Operations
// ============================================================================
void quantize_tensor(const float* weights, QTensor* tensor, uint32_t rows, uint32_t cols, const QuantParams* params);
void dequantize_tensor(const QTensor* tensor, float* output);
void qmatmul(const QTensor* weights, const float* input, float* output, uint32_t batch_size, uint32_t in_features, uint32_t out_features);

// ============================================================================
// Correction Functions
// ============================================================================
void find_outliers(const float* weights, const int8_t* quant_weights, uint32_t n, float scale, float percentile, SparseCorrection* correction);
void apply_sparse_correction(const SparseCorrection* corr, const float* input, float* output, uint32_t batch_size);
void svd_compress(const float* residual, uint32_t n, uint32_t m, uint32_t rank, SVDCorrection* svd_corr);
void apply_svd_correction(const SVDCorrection* svd_corr, const float* input, float* output, uint32_t batch_size);

// ============================================================================
// Model Functions
// ============================================================================
QModel* qmodel_create(uint32_t num_layers, uint32_t hidden_size, uint32_t vocab_size, uint32_t max_seq_len);
void qmodel_destroy(QModel* model);
int qmodel_save(const QModel* model, const char* path);
QModel* qmodel_load(const char* path);

// ============================================================================
// Inference Functions
// ============================================================================
QContext* qcontext_create(QModel* model);
void qcontext_destroy(QContext* ctx);
void qmodel_forward(QContext* ctx, uint32_t token);
void qmodel_generate(QContext* ctx, const uint32_t* prompt_tokens, uint32_t prompt_len, uint32_t* output_tokens, uint32_t max_tokens);

// ============================================================================
// Utility Functions
// ============================================================================
QModel* convert_fp32_to_quantized(const char* fp32_path, const char* output_path, QuantType target_type, CorrectionMode corr_mode, uint32_t block_size);
void qmodel_print_stats(const QModel* model);

#ifdef __cplusplus
}
#endif

#endif // QCORR_H
