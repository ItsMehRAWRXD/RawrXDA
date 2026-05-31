#pragma once
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <shared_mutex>

namespace RawrXD::Inference {

enum class ModelType {
    UNKNOWN,
    GGUF_VULKAN,
    GGUF_CUDA,
    GGUF_CPU,
    OLLAMA_REMOTE,
    ONNX_RUNTIME
};

// Forward declarations
class InferenceEngine;
using TokenCallback = std::function<void(uint32_t token, float confidence, bool done, const std::string& error)>;

class ModelHotSwap {
public:
    static ModelHotSwap& getInstance();
    
    // Model loading/unloading
    bool loadModel(const std::string& path, ModelType type);
    void unloadModel();
    bool isModelLoaded() const;
    
    // Generation
    void generate(const std::vector<uint32_t>& prompt, TokenCallback cb);
    void generate(const std::string& prompt, TokenCallback cb);
    
    // Model info
    ModelType getCurrentModelType() const;
    std::string getCurrentModelPath() const;
    size_t getModelMemoryUsage() const;
    
    // Hot-swap with fallback
    bool hotSwapTo(const std::string& path, ModelType type, bool keepFallback = true);
    void restoreFallback();

private:
    ModelHotSwap() = default;
    ~ModelHotSwap() = default;
    
    ModelHotSwap(const ModelHotSwap&) = delete;
    ModelHotSwap& operator=(const ModelHotSwap&) = delete;
    
    mutable std::shared_mutex m_mutex;
    std::unique_ptr<InferenceEngine> m_currentEngine;
    std::unique_ptr<InferenceEngine> m_fallbackEngine;
    
    ModelType m_currentType = ModelType::UNKNOWN;
    std::string m_currentPath;
    std::string m_fallbackPath;
    ModelType m_fallbackType = ModelType::UNKNOWN;
};

// Base inference engine interface
class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual bool loadModel(const std::string& path) = 0;
    virtual void shutdown() = 0;
    virtual void generateStream(uint32_t maxTokens, std::function<void(uint32_t, float, bool)> cb) = 0;
    virtual size_t getMemoryUsage() const = 0;
};

// Remote Ollama engine
class RemoteOllamaEngine : public InferenceEngine {
public:
    bool loadModel(const std::string& path) override;
    void shutdown() override;
    void generateStream(uint32_t maxTokens, std::function<void(uint32_t, float, bool)> cb) override;
    size_t getMemoryUsage() const override { return 0; }
};

} // namespace RawrXD::Inference
