#pragma once

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QHash>
#include <QByteArray>
#include <QString>
#include <vector>
#include <memory>
#include <chrono>

// Forward declarations
class TransformerInference;
class BPETokenizer;
class SentencePieceTokenizer;
class VocabularyLoader;

/**
 * @brief Comprehensive error codes for inference engine (4000-4999)
 * 
 * Client error codes for REST API responses and operation results
 */
enum class InferenceErrorCode {
    // Success
    SUCCESS = 0,
    
    // Model loading errors (4000-4099)
    MODEL_LOAD_FAILED = 4001,
    INVALID_MODEL_PATH = 4002,
    UNSUPPORTED_MODEL_FORMAT = 4003,
    MODEL_CORRUPTION_DETECTED = 4004,
    
    // Tokenization errors (4100-4199)
    TOKENIZER_NOT_INITIALIZED = 4101,
    TOKENIZATION_FAILED = 4102,
    INVALID_TOKEN_SEQUENCE = 4103,
    VOCABULARY_LOAD_FAILED = 4104,
    
    // Request validation errors (4200-4299)
    EMPTY_REQUEST = 4201,
    PROMPT_TOO_LONG = 4202,
    INVALID_GENERATION_PARAMETERS = 4203,
    REQUEST_TIMEOUT = 4204,
    
    // Resource allocation errors (4300-4399)
    INSUFFICIENT_MEMORY = 4301,
    REQUEST_QUEUE_FULL = 4302,
    
    // Internal server errors (4400-4499)
    TRANSFORMER_ERROR = 4401,
    INFERENCE_FAILURE = 4402,
    OUTPUT_GENERATION_FAILURE = 4403,
};

/**
 * @brief Inference request with unique tracking
 * 
 * Allows tracking per-request state and latencies
 */
struct InferenceRequest {
    QString requestId;                      // Unique identifier for tracking
    QString prompt;                          // Raw text prompt
    int maxTokens = 256;                    // Maximum generation length
    float temperature = 0.8f;               // Sampling temperature
    std::chrono::system_clock::time_point enqueueTime;  // When request was enqueued
};

/**
 * @brief Health status for production monitoring
 * 
 * Used by /health endpoint to report server state
 */
struct HealthStatus {
    bool model_loaded = false;
    bool gpu_available = false;
    bool inference_ready = false;
    
    size_t total_vram_mb = 0;
    size_t used_vram_mb = 0;
    
    double avg_latency_ms = 0.0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
    
    int pending_requests = 0;
    int total_requests_processed = 0;
    
    QString last_error;
};

/**
 * @brief Production-grade inference engine
 * 
 * Features:
 * - Thread-safe request queuing
 * - Mutex-protected resource access
 * - Comprehensive error tracking
 * - Performance metrics collection
 * - GPU memory management
 * - Health status reporting
 */
class InferenceEngine : public QObject {
    Q_OBJECT

public:
    explicit InferenceEngine(QObject* parent = nullptr);
    ~InferenceEngine();

    // Model management
    bool loadModel(const QString& modelPath, const QString& tokenizePath);
    bool isModelLoaded() const;
    
    // Synchronous inference
    QString infer(const QString& prompt, int maxTokens = 256);
    
    // Asynchronous queuing (for production)
    QString queueInferenceRequest(const QString& prompt, int maxTokens = 256, float temperature = 0.8f);
    
    // Status and diagnostics
    HealthStatus getHealthStatus();
    InferenceErrorCode getLastError() const;
    QString getLastErrorMessage() const;
    
    // Performance metrics
    double getAverageLatencyMs() const;
    double getTokensPerSecond() const;
    size_t getGPUMemoryUsedMB() const;
    
    // Resource management
    void clearAllCaches();
    void resetMetrics();

signals:
    void modelLoaded();
    void modelLoadFailed(const QString& reason);
    void inferenceProgress(int currentToken, int totalTokens);
    void inferenceComplete(const QString& result);
    void errorOccurred(InferenceErrorCode code, const QString& message);
    void gpuMemoryWarning(const QString& message);
    void healthStatusChanged(const HealthStatus& status);

public slots:
    void processNextRequest();

private:
    // ==================== CONCURRENCY & SYNCHRONIZATION ====================
    mutable QMutex m_mutex;                 // Main resource lock
    mutable QMutex m_tokenizermutex;        // Tokenizer access lock
    mutable QMutex m_cacheMutex;            // Cache access lock
    
    // ==================== REQUEST QUEUE ====================
    QQueue<InferenceRequest> m_requestQueue;
    bool m_isProcessingInference = false;   // Atomic flag to prevent concurrent inference
    static constexpr int MAX_QUEUE_SIZE = 100;
    
    // ==================== CORE COMPONENTS ====================
    std::unique_ptr<TransformerInference> m_transformer;
    std::unique_ptr<BPETokenizer> m_bpeTokenizer;
    std::unique_ptr<SentencePieceTokenizer> m_spTokenizer;
    std::unique_ptr<VocabularyLoader> m_vocab;
    
    // ==================== STATE ====================
    bool m_modelLoaded = false;
    bool m_gpuAvailable = false;
    
    // ==================== CACHED DATA ====================
    QHash<QString, QString> m_perLayerQuant;  // Per-layer quantization modes
    QHash<QString, QByteArray> m_tensorCache;  // Cached weight tensors
    
    // ==================== PERFORMANCE METRICS ====================
    struct PerformanceMetrics {
        int total_requests = 0;
        int successful_requests = 0;
        int failed_requests = 0;
        
        double total_latency_ms = 0.0;
        double total_tokens_generated = 0;
        
        double avg_latency_ms = 0.0;
        double p95_latency_ms = 0.0;
        double p99_latency_ms = 0.0;
        
        std::vector<double> recent_latencies;
        static constexpr int LATENCY_WINDOW = 100;
    } m_metrics;
    
    // ==================== MEMORY TRACKING ====================
    struct MemoryState {
        size_t total_vram_mb = 0;
        size_t model_vram_mb = 0;
        size_t cache_vram_mb = 0;
        size_t available_vram_mb = 0;
    } m_memory;
    
    // ==================== ERROR TRACKING ====================
    InferenceErrorCode m_lastError = InferenceErrorCode::SUCCESS;
    QString m_lastErrorMessage;
    
    // ==================== HELPER METHODS ====================
    
    // Tokenization (thread-safe)
    std::vector<int32_t> tokenizeWithLocking(const QString& text);
    QString detokenizeWithLocking(const std::vector<int32_t>& tokens);
    
    // Error handling
    void logError(InferenceErrorCode code, const QString& message);
    void recordLatency(double latencyMs);
    void updateMetrics();
    
    // Resource management
    bool ensureGPUMemoryAvailable(size_t requestedMB);
    bool validateRequest(const InferenceRequest& request);
    
    // Health status
    void emitHealthStatus();
};
