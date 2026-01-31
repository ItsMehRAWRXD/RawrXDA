/**
 * inference_engine_noqt.hpp
 * Pure C++ inference engine without Qt dependencies
 * Replaces inference_engine.hpp with STL equivalents
 */

#pragma once
#include <atomic>
#include <vector>
#include <cstdint>
#include <random>
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
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
    
    // Progress callback (replaces Qt signal)
    using ProgressCallback = std::function<void(const std::string&)>;
    void setLoadProgressCallback(ProgressCallback callback) {
        m_loadProgressCallback = callback;
    }
    
    // Model state
    bool isModelLoaded() const;
    std::string modelPath() const;
    std::vector<std::string> tensorNames() const;  // Replaces QStringList
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
    
    // GPU context
    class VulkanContext;
    const VulkanContext* getGPUContext() const;

private:
    struct CachedTensorData {
        std::vector<uint8_t> data;  // Replaces QByteArray
        int ggml_type_id = 0;
    };
    
    ProgressCallback m_loadProgressCallback;
    std::unique_ptr<GGUFLoader> m_loader;
    std::string m_modelPath;
    
    // Cache and state
    std::map<std::string, CachedTensorData> m_tensorCache;  // Replaces QHash
    std::atomic<bool> m_threadingEnabled{true};
    std::atomic<bool> m_loadTensors{true};
    std::atomic<double> m_tokensPerSecond{0.0};
    std::atomic<double> m_temperature{0.7};
    std::string m_quantMode{"Q4_K_M"};
    
    // Transformer
    TransformerInference m_transformer;
    bool m_kvCacheReady = false;
    
    // Tokens
    std::vector<int32_t> m_systemPromptTokens;
    std::vector<int32_t> m_cachedPromptTokens;
    
    // Threading
    std::mutex m_mutex;
    std::thread m_loaderThread;
    std::atomic<bool> m_isLoading{false};
    
    // Worker methods
    void streamingGenerateWorker(std::vector<int32_t> inputTokens,
                                 int maxTokens,
                                 TokenCallback onToken,
                                 CompleteCallback onComplete);
};

#endif // INFERENCE_ENGINE_NOQT_HPP
