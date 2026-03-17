#pragma once


#include <atomic>
#include <vector>
#include <cstdint>
#include <random>
#include <string>
#include <mutex>
#include "gguf_loader.hpp"
#include "transformer_inference.hpp"
#include "bpe_tokenizer.hpp"
#include "sentencepiece_tokenizer.hpp"
#include "vocabulary_loader.hpp"

class InferenceEngine : public void {


public:
    /**
     * @brief Construct an inference engine
     * @param ggufPath Path to the GGUF model file (can be empty, loaded later)
     * @param parent void parent
     */
    explicit InferenceEngine(const std::string& ggufPath = std::string(), void* parent = nullptr);
    // Overload used by server components expecting void* only
    explicit InferenceEngine(void* parent);
    
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
     * QMetaObject::invokeMethod(..., //QueuedConnection)
     */
    Q_INVOKABLE bool loadModel(const std::string& path);
    
    /**
     * @brief Set a progress callback for model loading
     * @param callback Function to call with progress updates
     */
    void setLoadProgressCallback(std::function<void(const std::string&)> callback) {
        m_loadProgressCallback = callback;
    }
    
    /**
     * @brief Check if a model is currently loaded
     */
    bool isModelLoaded() const;
    
    /**
     * @brief Get the current model path
     */
    std::string modelPath() const;
    
    /**
     * @brief Get list of tensor names from the loaded model
     */
    std::vector<std::string> tensorNames() const;
    
    /**
     * @brief Get memory usage in MB
     */
    int64_t memoryUsageMB() const;
    
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
    std::string quantMode() const;

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

private:
    // IPC / Agent Communication
    void* m_hPipe = nullptr;
    bool connectToAgent();
    void disconnectAgent();
    std::string sendToAgent(const std::string& prompt);

private:
    /**
     * @brief Process an inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     */
    void request(const std::string& prompt, int64_t reqId);
    
    /**
     * @brief Process a streaming inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     * @param streaming Whether to use streaming mode
     */
    void request(const std::string& prompt, int64_t reqId, bool streaming);
    
    /**
     * @brief Unload the current model
     */
    void unloadModel();
    
    /**
     * @brief Change quantization mode at runtime
     * @param mode Quantization type: Q4_0, Q4_1, Q5_0, Q5_1, Q6_K, Q8_K, F16, F32
     */
    void setQuantMode(const std::string& mode);
    
    /**
     * @brief Set quantization for a specific tensor layer
     * @param tensorName Name of the tensor
     * @param quant Quantization type for this layer
     */
    void setLayerQuant(const std::string& tensorName, const std::string& quant);

private:
    // Background worker for streaming
    void streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                 int maxTokens,
                                 TokenCallback onToken,
                                 CompleteCallback onComplete);

    /**
     * @brief Emitted when inference completes successfully
     * @param reqId Request ID
     * @param answer Generated response
     */
    void resultReady(int64_t reqId, const std::string& answer);
    
    /**
     * @brief Emitted when an error occurs
     * @param reqId Request ID
     * @param errorMsg Error description
     */
    void error(int64_t reqId, const std::string& errorMsg);
    
    /**
     * @brief Emitted when model loading status changes
     * @param loaded true if model is loaded
     * @param modelName Name of the loaded model
     */
    void modelLoadedChanged(bool loaded, const std::string& modelName);
    
    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request ID
     * @param token Single token string
     */
    void streamToken(int64_t reqId, const std::string& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request ID
     */
    void streamFinished(int64_t reqId);
    
    /**
     * @brief Emitted when quantization mode changes
     * @param mode New quantization mode
     */
    void quantChanged(const std::string& mode);
    
    /**
     * @brief Emitted when inference completes (alias for resultReady)
     * @param requestId Request ID
     * @param result Generated text
     */
    void inferenceComplete(const std::string& requestId, const std::string& result);
    
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
    void unsupportedQuantizationTypeDetected(const std::vector<std::string>& unsupportedTypes, 
                                             const std::string& recommendedConversion,
                                             const std::string& modelPath);
    
    /**
     * @brief Emitted when inference error occurs (alias for error)
     * @param requestId Request ID  
     * @param errorMessage Error description
     */
    void inferenceError(const std::string& requestId, const std::string& errorMessage);
    
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
        std::vector<uint8_t> data;
        int ggml_type_id;  // Stores the enum ggml_type as an integer
    };
    
    // Define structure for inference requests
    struct InferenceRequest {
        std::string prompt;
        int64_t requestId;
        bool streaming{false};
    };
    
    mutable std::mutex m_mutex;
    std::string m_modelPath;
    double m_temperature = 0.7;
    double m_topP = 0.9;
    std::string m_quantMode = "Q4_0";
    
    // Named Pipe Handle for Agent Communication
    void* m_hPipe = nullptr;
    bool connectToAgent();
    void disconnectAgent();
    std::string sendToAgent(const std::string& request);

    std::atomic<bool> m_threadingEnabled { true };  // default threaded
    std::atomic<bool> m_loadTensors{true};       // default load tensors (can be disabled for headless mode)
    
    // FIX 6: Add request queue and processing state
    QQueue<InferenceRequest> m_requestQueue;
    bool m_isProcessingInference{false};
    
    // Progress callback for model loading (used by background threads)
    std::function<void(const std::string&)> m_loadProgressCallback;
    
    enum TokenizerMode {
        TOKENIZER_FALLBACK,  // Simple word-based fallback
        TOKENIZER_BPE,       // BPE (GPT-2/GPT-3 style)
        TOKENIZER_SP         // SentencePiece (LLaMA/Mistral)
    } m_tokenizerMode{TOKENIZER_FALLBACK};
    
    // Helper methods
    std::string extractModelName(const std::string& path) const;
    void rebuildTensorCache();
    void initializeTokenizer();
    bool initializeTokenizerWithTimeout(int timeoutMs = 5000);
    bool loadFallbackTokenizer();
    std::vector<int32_t> tokenizeInternal(const std::string& text, bool includeSystemPrompt = true, bool includeSpecialTokens = true);
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
    void streamingGenerateWorkerSignals(int64_t reqId, const std::string& prompt, int maxTokens);
};


