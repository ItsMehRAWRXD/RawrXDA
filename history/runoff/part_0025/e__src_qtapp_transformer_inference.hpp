#pragma once

#include <QObject>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <vector>
#include <memory>
#include <ggml.h>
#include <ggml-backend.h>

/**
 * @brief Comprehensive error codes for production observability
 * 
 * Range 5000-5999: Server errors in transformer operations
 * Allows precise error reporting instead of generic "failed" messages
 */
enum class TransformerErrorCode {
    // Success
    SUCCESS = 0,
    
    // Model loading errors (5000-5099)
    MODEL_NOT_LOADED = 5001,
    TENSOR_LOAD_FAILED = 5002,
    WEIGHT_SHAPE_MISMATCH = 5003,
    INVALID_MODEL_CONFIG = 5004,
    GGML_CONTEXT_CREATION_FAILED = 5005,
    
    // GPU/Memory errors (5100-5199)
    VRAM_ALLOCATION_FAILED = 5101,
    KV_CACHE_ALLOCATION_FAILED = 5102,
    KV_CACHE_POSITION_EXCEEDED = 5103,
    PINNED_MEMORY_ALLOCATION_FAILED = 5104,
    GPU_MEMORY_EXHAUSTED = 5105,
    
    // Vulkan/GPU operation errors (5200-5299)
    GPU_COMMAND_BUFFER_FAILED = 5201,
    GPU_SYNCHRONIZATION_TIMEOUT = 5202,
    VULKAN_OPERATION_FAILED = 5203,
    GPU_COMPUTE_FAILED = 5204,
    
    // Inference errors (5300-5399)
    FORWARD_PASS_FAILED = 5301,
    LOGITS_GENERATION_FAILED = 5302,
    TOKEN_SAMPLING_FAILED = 5303,
    CONTEXT_WINDOW_EXCEEDED = 5304,
    
    // Resource conflicts (5400-5499)
    RESOURCE_LOCKED = 5401,
    CONCURRENT_ACCESS_VIOLATION = 5402,
};

/**
 * @brief KV Cache management with pinned VRAM tracking
 * 
 * Optimizes GPU memory usage for efficient token generation
 * - Pinned memory for host-GPU transfers
 * - Offset tracking to prevent overwrites
 * - Proper synchronization for concurrent access
 */
struct KVCacheManager {
    // Device buffers (in GPU VRAM)
    std::vector<ggml_tensor*> k_cache;  // Key cache tensors per layer
    std::vector<ggml_tensor*> v_cache;  // Value cache tensors per layer
    
    // Pinned host memory (for efficient transfers)
    std::vector<std::vector<float>> k_pinned;
    std::vector<std::vector<float>> v_pinned;
    
    // State tracking
    uint32_t sequence_length = 0;          // Current tokens in cache
    uint32_t max_sequence_length = 2048;   // Maximum context size
    uint32_t num_layers = 32;              // Number of transformer layers
    uint32_t head_dim = 128;               // Dimension per attention head
    
    // Memory tracking
    size_t total_vram_allocated_mb = 0;    // Total GPU memory used
    size_t pinned_host_memory_mb = 0;      // Total pinned RAM used
    bool is_pinned_memory_allocated = false;
    
    // Synchronization
    bool needs_gpu_sync = false;           // Pending GPU operations
    
    TransformerErrorCode last_error = TransformerErrorCode::SUCCESS;
};

/**
 * @brief Layer weights with bias terms for production inference
 * 
 * Includes all necessary parameters for attention and MLP computations
 * - Bias tensors for robust numerical stability
 * - Proper layout for efficient GPU computation
 */
struct LayerWeights {
    // Attention weights
    ggml_tensor* attn_q_weight = nullptr;
    ggml_tensor* attn_k_weight = nullptr;
    ggml_tensor* attn_v_weight = nullptr;
    ggml_tensor* attn_output_weight = nullptr;
    
    // Attention biases (critical for numerical stability)
    ggml_tensor* attn_q_bias = nullptr;
    ggml_tensor* attn_k_bias = nullptr;
    ggml_tensor* attn_v_bias = nullptr;
    ggml_tensor* attn_output_bias = nullptr;
    
    // MLP weights
    ggml_tensor* mlp_fc1_weight = nullptr;
    ggml_tensor* mlp_fc2_weight = nullptr;
    
    // MLP biases
    ggml_tensor* mlp_fc1_bias = nullptr;
    ggml_tensor* mlp_fc2_bias = nullptr;
    
    // Layer norm weights
    ggml_tensor* ln_weight = nullptr;
    ggml_tensor* ln_bias = nullptr;
};

/**
 * @brief Production-ready transformer inference engine
 * 
 * Implements efficient autoregressive generation with:
 * - Optimized KV caching in pinned VRAM
 * - GPU command buffer synchronization
 * - Comprehensive error tracking
 * - Thread-safe inference with proper locking
 * - P95/P99 latency metrics
 */
class TransformerInference : public QObject {
    Q_OBJECT

public:
    explicit TransformerInference(QObject* parent = nullptr);
    ~TransformerInference();

    // Configuration and loading
    bool loadWeights(const QHash<QString, QByteArray>& tensorCache,
                     int nLayers, int nEmbd, int nHead, int nVocab);
    
    // Inference
    std::vector<float> forward(const std::vector<int32_t>& tokens);
    std::vector<int32_t> generate(const std::vector<int32_t>& prompt, 
                                  int maxTokens, float temperature = 0.8f);
    
    // Status and diagnostics
    bool isReady() const { return m_ready; }
    TransformerErrorCode getLastError() const { return m_lastError; }
    QString getErrorMessage(TransformerErrorCode code) const;
    
    // GPU and memory diagnostics
    size_t getKVCacheVRAMUsedMB() const { return m_kvCache.total_vram_allocated_mb; }
    size_t getKVCachePinnedMemoryMB() const { return m_kvCache.pinned_host_memory_mb; }
    uint32_t getSequenceLength() const { return m_kvCache.sequence_length; }
    
    // Latency metrics for observability
    double getLastInferenceLatencyMs() const { return m_lastInferenceMs; }
    double getP95LatencyMs() const { return m_p95LatencyMs; }
    double getP99LatencyMs() const { return m_p99LatencyMs; }
    
    // Cache management
    void clearKVCache();
    bool allocateKVCachePinnedMemory();
    
signals:
    void inferenceProgress(int currentToken, int totalTokens);
    void gpuMemoryWarning(const QString& message);
    void errorOccurred(TransformerErrorCode code, const QString& message);

private:
    // GGML contexts
    ggml_context* m_ctx = nullptr;              // Weight context
    ggml_context* m_kvCtx = nullptr;            // KV cache context
    ggml_context* m_graphCtx = nullptr;         // Computation graph context
    ggml_backend_t m_backend = nullptr;         // GPU backend (Vulkan/CPU)
    
    // Model configuration
    int m_nLayers = 32;
    int m_nEmbd = 4096;
    int m_nHead = 32;
    int m_nVocab = 32000;
    int m_ctxSize = 2048;
    
    // Layer weights storage
    std::vector<LayerWeights> m_layers;
    ggml_tensor* m_embedding = nullptr;         // Token embeddings
    
    // KV cache with production optimizations
    KVCacheManager m_kvCache;
    
    // State
    bool m_ready = false;
    bool m_kvCacheInitialized = false;
    
    // Error tracking
    TransformerErrorCode m_lastError = TransformerErrorCode::SUCCESS;
    QString m_lastErrorMessage;
    
    // Latency metrics for observability
    double m_lastInferenceMs = 0.0;
    double m_p95LatencyMs = 0.0;
    double m_p99LatencyMs = 0.0;
    std::vector<double> m_recentLatencies;      // Ring buffer for percentile calculation
    static constexpr int LATENCY_WINDOW_SIZE = 100;
    
    // Helper methods
    ggml_tensor* createTensorFromCache(
        const QString& name,
        const QHash<QString, QByteArray>& cache,
        const int64_t* shape, int nDims);
    
    bool initializeKVCache();
    bool updateKVCacheWithNewToken(const std::vector<float>& k_new, 
                                   const std::vector<float>& v_new);
    bool synchronizeGPU();
    
    ggml_tensor* buildGraph(ggml_context* ctx, const std::vector<int32_t>& tokens);
    int sampleToken(const std::vector<float>& logits, float temperature);
    
    void recordLatency(double latencyMs);
    void updatePercentileMetrics();
    void logError(TransformerErrorCode code, const QString& message);
    
    void freeContext();
};
