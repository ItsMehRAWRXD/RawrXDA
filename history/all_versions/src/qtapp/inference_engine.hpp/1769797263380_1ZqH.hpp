#pragma once
/**
 * inference_engine.hpp
 * Pure C++ inference engine without Qt dependencies
 * Replaces old Qt-based implementation
 */

#include <atomic>
#include <vector>
#include <cstdint>
#include <random>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <map>
#include "gguf_loader.hpp"
#include "transformer_inference.hpp"
#include "bpe_tokenizer.hpp"
#include "sentencepiece_tokenizer.hpp"
#include "vocabulary_loader.hpp"

class InferenceEngine {
public:
    // Constructor
    explicit InferenceEngine(const std::string& ggufPath = std::string());
    
    // Destructor
    ~InferenceEngine();
    
    // Model loading
    bool loadModel(const std::string& path);
    
    // Progress callback
    using ProgressCallback = std::function<void(const std::string&)>;
    void setLoadProgressCallback(ProgressCallback callback) {
        m_loadProgressCallback = callback;
    }
    
    // Model state
    bool isModelLoaded() const;
    std::string modelPath() const;
    std::vector<std::string> tensorNames() const;
    int64_t memoryUsageMB() const;
    double tokensPerSecond() const;
    double temperature() const;
    std::string quantMode() const;
    
    // Inference
    std::vector<int32_t> generate(const std::vector<int32_t>& inputTokens, int maxTokens = 100);
    
    // Streaming generation
    using TokenCallback = std::function<void(const std::string&)>;
    using CompleteCallback = std::function<void()>;
    
    void generateStreaming(const std::vector<int32_t>& inputTokens,
                           int maxTokens,
                           TokenCallback onToken,
                           CompleteCallback onComplete);
    
    void generateStreaming(const std::string& prompt,
                           int maxTokens,
                           TokenCallback onToken,
                           CompleteCallback onComplete);
    
    // Tokenization
    std::vector<int32_t> tokenize(const std::string& text);
    std::string detokenize(const std::vector<int32_t>& tokens);
    
    // Helper methods
    std::string processChat(const std::string& prompt);
    std::string analyzeCode(const std::string& code);
    
    // Model properties
    int vocabSize() const;
    int embeddingDim() const;
    
    // Configuration
    void setQuantMode(const std::string& mode);
    void setLayerQuant(const std::string& tensorName, const std::string& quant);
    void setThreadingEnabled(bool on) { m_threadingEnabled.store(on); }
    bool threadingEnabled() const { return m_threadingEnabled.load(); }
    void setLoadTensors(bool load) { m_loadTensors.store(load); }
    bool shouldLoadTensors() const { return m_loadTensors.load(); }
    
    // Model unload
    void unloadModel();

private:
    struct CachedTensorData {
        std::vector<uint8_t> data;
        int ggml_type_id = 0;
    };
    
    ProgressCallback m_loadProgressCallback;
    std::unique_ptr<GGUFLoader> m_loader;
    std::string m_modelPath;
    
    // Cache and state
    std::map<std::string, CachedTensorData> m_tensorCache;
    std::atomic<bool> m_threadingEnabled{true};
    std::atomic<bool> m_loadTensors{true};
    std::atomic<double> m_tokensPerSecond{0.0};
    std::atomic<double> m_temperature{0.7};
    std::string m_quantMode{"Q4_K_M"};
    
    // Threading
    std::mutex m_mutex;
    std::atomic<bool> m_isLoading{false};
    
    // Placeholder members if needed by legacy code
    std::vector<int32_t> m_systemPromptTokens;
};
    std::vector<int32_t> generate(const std::vector<int32_t>& inputTokens, int maxTokens = 100);
    
    // Streaming generation for chat
    void generateStreaming(const std::vector<int32_t>& inputTokens,
                           int maxTokens,
                           std::function<void(const std::string&)> tokenCallback,
                           std::function<void()> completeCallback);
    
    // Convenience method for string input
    void generateStreaming(const std::string& prompt,
                           int maxTokens,
                           std::function<void(const std::string&)> tokenCallback,
                           std::function<void()> completeCallback);
    
    /**
     * @brief Tokenize text (public for server API)
     */
    std::vector<int32_t> tokenize(const std::string& text);
    
    /**
     * @brief Detokenize tokens to text (public for server API)
     */
    std::string detokenize(const std::vector<int32_t>& tokens);

    // Server-oriented helper APIs
    std::string processChat(const std::string& prompt);
    std::string analyzeCode(const std::string& code);

    // Streaming generation APIs
    using TokenCallback = std::function<void(const std::string&)>;
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
    void generateStreaming(const std::string& prompt,
                           int maxTokens,
                           TokenCallback onToken,
                           CompleteCallback onComplete);

    /**
     * @brief Stream tokens for a request (emits signals)
     * @param reqId Request ID for signal emission
     * @param prompt Input prompt
     * @param maxTokens Maximum tokens to generate
     */
    void generateStreaming(int64_t reqId, const std::string& prompt, int maxTokens = 128);

\npublic:\n    /**
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

\npublic:\n    /**
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
    void unsupportedQuantizationTypeDetected(const std::stringList& unsupportedTypes, 
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
    
    // ========== INTERPRETABILITY SIGNALS ==========
    
    /**
     * @brief Emitted when attention weights are available after inference
     * @param attentionData JSON array containing multi-head attention weights
     * 
     * JSON format:
     * [
     *   {
     *     "layer": 0, "head": 0,
     *     "weights": [[0.1, 0.2, ...], ...],
     *     "mean": 0.05, "max": 0.9, "entropy": 2.3
     *   },
     *   ...
     * ]
     */
    void attentionDataAvailable(const nlohmann::json& attentionData);
    
    /**
     * @brief Emitted when gradient flow metrics are available
     * @param gradientData JSON array containing per-layer gradient metrics
     * 
     * JSON format:
     * [
     *   {
     *     "layer": 0, "norm": 0.05, "variance": 0.001,
     *     "min": -0.1, "max": 0.1, "dead_ratio": 0.01,
     *     "is_vanishing": false, "is_exploding": false
     *   },
     *   ...
     * ]
     */
    void gradientDataAvailable(const nlohmann::json& gradientData);
    
    /**
     * @brief Emitted when activation statistics are available
     * @param activationData JSON array containing per-layer activation stats
     * 
     * JSON format:
     * [
     *   {
     *     "layer": 0, "mean": 0.5, "variance": 0.1,
     *     "min": -1.0, "max": 1.0, "sparsity": 0.3,
     *     "dead_neurons": 5, "distribution": [0.1, 0.2, ...]
     *   },
     *   ...
     * ]
     */
    void activationDataAvailable(const nlohmann::json& activationData);
    
    /**
     * @brief Emitted when layer contribution data is available
     * @param layerData JSON array containing per-layer contribution metrics
     */
    void layerContributionAvailable(const nlohmann::json& layerData);
    
    /**
     * @brief Emitted when token logits are available during generation
     * @param tokenIdx Token position in sequence
     * @param logits JSON array of logit values
     */
    void tokenLogitsAvailable(int tokenIdx, const nlohmann::json& logits);

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
    int64_t m_memoryUsageMB{0};
    double m_tokensPerSecond{0.0};
    double m_temperature{0.0};   // Enterprise deterministic default
    double m_topP{1.0};          // Enterprise deterministic default (full nucleus)
    std::string m_quantMode{"Q4_0"};  // Default quantization
    std::map<std::string, CachedTensorData> m_tensorCache;  // Cached quantized tensors with type info
    std::map<std::string, std::string> m_perLayerQuant;  // Tensor-specific quants
    std::chrono::steady_clock m_inferenceTimer;
    std::atomic<bool> m_threadingEnabled{true};  // default threaded
    std::atomic<bool> m_loadTensors{true};       // default load tensors (can be disabled for headless mode)
    
    // FIX 6: Add request queue and processing state
    std::queue<InferenceRequest> m_requestQueue;
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




