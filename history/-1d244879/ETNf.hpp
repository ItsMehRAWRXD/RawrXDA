#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QHash>
#include <QElapsedTimer>
#include <QQueue>
#include <atomic>
#include <vector>
#include <cstdint>
#include <random>
#include "gguf_loader.hpp"
#include "transformer_inference.hpp"
#include "bpe_tokenizer.hpp"
#include "sentencepiece_tokenizer.hpp"
#include "vocabulary_loader.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"

class AgenticFailureDetector;
class AgenticPuppeteer;
class OllamaProxy;

class InferenceEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath CONSTANT)
    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(QString quantMode READ quantMode WRITE setQuantMode NOTIFY quantChanged)

public:
    /**
     * @brief Construct an inference engine
     * @param ggufPath Path to the GGUF model file (can be empty, loaded later)
     * @param parent QObject parent
     */
    explicit InferenceEngine(const QString& ggufPath = QString(), QObject* parent = nullptr);
    // Overload used by server components expecting QObject* only
    explicit InferenceEngine(QObject* parent);
    
    /**
     * @brief Destructor - cleans up GGUFLoader resources
     */
    ~InferenceEngine();
    
    /**
     * @brief Load a GGUF model file
     * @param path Path to the GGUF file
     * @return true if loaded successfully
     *
     * This is declared Q_INVOKABLE so it can be called via
     * QMetaObject::invokeMethod(..., Qt::QueuedConnection)
     */
    Q_INVOKABLE bool loadModel(const QString& path);
    
    /**
     * @brief Set a progress callback for model loading
     * @param callback Function to call with progress updates
     */
    void setLoadProgressCallback(std::function<void(const QString&)> callback) {
        m_loadProgressCallback = callback;
    }
    
    /**
     * @brief Check if a model is currently loaded
     */
    bool isModelLoaded() const;
    
    /**
     * @brief Get the current model path
     */
    QString modelPath() const;
    
    /**
     * @brief Get list of tensor names from the loaded model
     */
    QStringList tensorNames() const;
    
    /**
     * @brief Get memory usage in MB
     */
    qint64 memoryUsageMB() const;
    
    /**
     * @brief Get tokens per second performance metric
     */
    double tokensPerSecond() const;
    
    /**
     * @brief Get current temperature setting
     */
    double temperature() const;
    
    /**
     * @brief Get current quantization mode
     */
    QString quantMode() const;

    /**
     * @struct HealthStatus
     * @brief Comprehensive health status for monitoring
     */
    struct HealthStatus {
        // Model status
        bool model_loaded = false;
        bool inference_ready = false;
        QString model_name;
        QString quantization;
        
        // GPU status
        bool gpu_available = false;
        double total_vram_mb = 0.0;
        double used_vram_mb = 0.0;
        
        // Performance metrics
        double tokens_per_second = 0.0;
        double avg_latency_ms = 0.0;
        double p95_latency_ms = 0.0;
        double p99_latency_ms = 0.0;
        
        // Request tracking
        int pending_requests = 0;
        int active_requests = 0;
        int total_requests = 0;
        int total_requests_processed = 0;
        double memory_usage_mb = 0.0;
        
        // Error tracking
        QString last_error;
        QString backend = "Vulkan";
    };

    /**
     * @brief Get comprehensive health status
     * @return Current system health metrics
     */
    HealthStatus getHealthStatus() const;

    /**
     * @brief Get real-time tokens per second metric
     * @return Current TPS based on recent inference
     */
    double getTokensPerSecond() const;

    void setThreadingEnabled(bool on) { m_threadingEnabled.store(on); }
    bool threadingEnabled() const { return m_threadingEnabled.load(); }

    void setLoadTensors(bool load) { m_loadTensors.store(load); }
    bool shouldLoadTensors() const { return m_loadTensors.load(); }

    /**
     * @brief Generate tokens synchronously (for server API)
     * @param inputTokens Input token sequence
     * @param maxTokens Maximum number of tokens to generate
     * @return Generated token sequence
     */
    std::vector<int32_t> generate(const std::vector<int32_t>& inputTokens, int maxTokens = 100);
    
    // Streaming generation for chat
    void generateStreaming(const std::vector<int32_t>& inputTokens,
                           int maxTokens,
                           std::function<void(const std::string&)> tokenCallback,
                           std::function<void()> completeCallback);
    
    // Convenience method for string input
    void generateStreaming(const QString& prompt,
                           int maxTokens,
                           std::function<void(const std::string&)> tokenCallback,
                           std::function<void()> completeCallback);
    
    /**
     * @brief Tokenize text (public for server API)
     */
    std::vector<int32_t> tokenize(const QString& text);
    
    /**
     * @brief Detokenize tokens to text (public for server API)
     */
    QString detokenize(const std::vector<int32_t>& tokens);

    // Server-oriented helper APIs
    QString processChat(const QString& prompt);
    QString analyzeCode(const QString& code);

    // Streaming generation APIs
    using TokenCallback = std::function<void(const QString&)>;
    using CompleteCallback = std::function<void()>;

    /**
     * @brief Stream tokens for a tokenized prompt
     * @param inputTokens Input token sequence
     * @param maxTokens Max tokens to generate
     * @param onToken Callback per detokenized token fragment
     * @param onComplete Callback on stream completion
     */
    void generateStreaming(const std::vector<int32_t>& inputTokens,
                           int maxTokens,
                           TokenCallback onToken,
                           CompleteCallback onComplete);

    /**
     * @brief Stream tokens for a text prompt (convenience)
     */
    void generateStreaming(const QString& prompt,
                           int maxTokens,
                           TokenCallback onToken,
                           CompleteCallback onComplete);

    /**
     * @brief Stream tokens for a request (emits signals)
     * @param reqId Request ID for signal emission
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     */
    void generateStreaming(qint64 reqId, const QString& prompt, int maxTokens = 128);

public slots:
    /**
     * @brief Process an inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     */
    void request(const QString& prompt, qint64 reqId);
    
    /**
     * @brief Process a streaming inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     * @param streaming Whether to use streaming mode
     */
    void request(const QString& prompt, qint64 reqId, bool streaming);

    // Test-friendly hook: handle an incoming request. Default implementation
    // delegates to the existing request(...) slot. Tests can override this to
    // implement mock behavior without changing the slot signatures.
    virtual void handleRequest(const QString& prompt, qint64 reqId) {
        // Default: dispatch to request slot asynchronously
        QMetaObject::invokeMethod(this, "request", Qt::QueuedConnection,
                                  Q_ARG(QString, prompt), Q_ARG(qint64, reqId));
    }
    
    /**
     * @brief Unload the current model
     */
    void unloadModel();
    
    /**
     * @brief Change quantization mode at runtime
     * @param mode Quantization type: Q4_0, Q4_1, Q5_0, Q5_1, Q6_K, Q8_K, F16, F32
     */
    void setQuantMode(const QString& mode);
    
    /**
     * @brief Set quantization for a specific tensor layer
     * @param tensorName Name of the tensor
     * @param quant Quantization type for this layer
     */
    void setLayerQuant(const QString& tensorName, const QString& quant);

private:
    // Background worker for streaming
    void streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                 int maxTokens,
                                 TokenCallback onToken,
                                 CompleteCallback onComplete);

signals:
    /**
     * @brief Emitted when inference completes successfully
     * @param reqId Request ID
     * @param answer Generated response
     */
    void resultReady(qint64 reqId, const QString& answer);
    
    /**
     * @brief Emitted when an error occurs
     * @param reqId Request ID
     * @param errorMsg Error description
     */
    void error(qint64 reqId, const QString& errorMsg);
    
    /**
     * @brief Emitted when model loading status changes
     * @param loaded true if model is loaded
     * @param modelName Name of the loaded model
     */
    void modelLoadedChanged(bool loaded, const QString& modelName);
    
    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request ID
     * @param token Single token string
     */
    void streamToken(qint64 reqId, const QString& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request ID
     */
    void streamFinished(qint64 reqId);
    
    /**
     * @brief Emitted when quantization mode changes
     * @param mode New quantization mode
     */
    void quantChanged(const QString& mode);
    
    /**
     * @brief Emitted when inference completes (alias for resultReady)
     * @param requestId Request ID
     * @param result Generated text
     */
    void inferenceComplete(const QString& requestId, const QString& result);
    
    /**
     * @brief Emitted when model uses unsupported quantization types
     * 
     * This signal is emitted during loadModel() if the GGUF uses quantization types
     * that are not supported by the current GGML library (e.g., IQ4_NL type 39).
     * 
     * The IDE can connect to this signal to show a conversion prompt dialog.
     * 
     * @param unsupportedTypes List of type descriptions (e.g., "IQ4_NL (type 39): 145 tensors")
     * @param recommendedConversion Type to convert to (e.g., "Q5_K")
     * @param modelPath Path to the model that needs conversion
     */
    void unsupportedQuantizationTypeDetected(const QStringList& unsupportedTypes, 
                                             const QString& recommendedConversion,
                                             const QString& modelPath);
    
    /**
     * @brief Emitted when inference error occurs (alias for error)
     * @param requestId Request ID  
     * @param errorMessage Error description
     */
    void inferenceError(const QString& requestId, const QString& errorMessage);
    
    /**
     * @brief Emitted when the transformer is fully ready for inference
     */
    void transformerReady();

private:
    GGUFLoaderQt* m_loader;
    TransformerInference m_transformer;
    BPETokenizer m_bpeTokenizer;
    SentencePieceTokenizer m_spTokenizer;
    VocabularyLoader m_vocab;

    // Define structure to store quantized tensor data and its type
    struct CachedTensorData {
        QByteArray data;
        int ggml_type_id;  // Stores the enum ggml_type as an integer
    };
    
    // Define structure for inference requests
    struct InferenceRequest {
        QString prompt;
        qint64 requestId;
        bool streaming{false};
    };
    
    mutable QMutex m_mutex;
    QString m_modelPath;
    qint64 m_memoryUsageMB{0};
    double m_tokensPerSecond{0.0};
    double m_temperature{0.0};   // Enterprise deterministic default
    double m_topP{1.0};          // Enterprise deterministic default (full nucleus)
    QString m_quantMode{"Q4_0"};  // Default quantization
    QHash<QString, CachedTensorData> m_tensorCache;  // Cached quantized tensors with type info
    QHash<QString, QString> m_perLayerQuant;  // Tensor-specific quants
    QElapsedTimer m_inferenceTimer;
    std::atomic<bool> m_threadingEnabled{true};  // default threaded
    std::atomic<bool> m_loadTensors{true};       // default load tensors (can be disabled for headless mode)
    
    // FIX 6: Add request queue and processing state
    QQueue<InferenceRequest> m_requestQueue;
    bool m_isProcessingInference{false};
    
    // Performance tracking for health monitoring
    mutable std::atomic<int> m_totalRequests{0};
    mutable std::atomic<int> m_activeRequests{0};
    mutable std::atomic<double> m_avgLatencyMs{0.0};
    mutable std::atomic<double> m_realtimeTokensPerSecond{0.0};
    
    // Progress callback for model loading (used by background threads)
    std::function<void(const QString&)> m_loadProgressCallback;
    
    enum TokenizerMode {
        TOKENIZER_FALLBACK,  // Simple word-based fallback
        TOKENIZER_BPE,       // BPE (GPT-2/GPT-3 style)
        TOKENIZER_SP         // SentencePiece (LLaMA/Mistral)
    } m_tokenizerMode{TOKENIZER_FALLBACK};
    
    /**
     * @brief Stop current inference
     */
    void stopInference();

    /**
     * @brief Set the directory to scan for Ollama models/blobs
     */
    void setModelDirectory(const QString& dir);

    /**
     * @brief Get list of detected Ollama models
     */
    QStringList detectedOllamaModels() const;
    
    std::vector<std::string> detectedOllamaModelsStd() { 
        return {}; 
    }

    // Helper methods
    QString extractModelName(const QString& path) const;
    void rebuildTensorCache();
    void initializeTokenizer();
    bool initializeTokenizerWithTimeout(int timeoutMs = 5000);
    bool loadFallbackTokenizer();
    std::vector<int32_t> tokenizeInternal(const QString& text, bool includeSystemPrompt = true, bool includeSpecialTokens = true);
    void buildSystemPromptTokens();
    
    // FIX 6: Request queue processing
    void processNextRequest();
    
    // Elegant two-phase inference with KV-cache

    // Advanced sampling for more natural text generation
    int32_t sampleNextToken(std::vector<float>& logits, double temperature, double topP);

    // Thread-safe random number generation
    float getRandomFloat(float min, float max);
    
    // Sampling configuration
    bool m_kvCacheReady{false};  // Track if KV-cache is prefilled
    std::mt19937 m_randomEngine;  // Thread-safe random number generator
    std::vector<int32_t> m_systemPromptTokens;  // Cached tokens for system prompt injection

    // Thread-safe streaming generation worker
    void streamingGenerateWorker(const std::vector<int32_t> inputTokens,
                                 int maxTokens,
                                 std::function<void(const std::string&)> tokenCallback,
                                 std::function<void()> completeCallback);
    
    // Thread-safe streaming generation worker that emits signals
    void streamingGenerateWorkerSignals(qint64 reqId, const QString& prompt, int maxTokens);

    // Agentic failure detection and correction
    AgenticFailureDetector* m_failureDetector{};
    AgenticPuppeteer* m_puppeteer{};

    // Ollama integration
    class OllamaProxy* m_ollamaProxy{};
    bool m_useOllama{false};
    qint64 m_currentRequestId{0};
};
