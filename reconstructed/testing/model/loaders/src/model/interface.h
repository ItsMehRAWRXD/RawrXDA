// model_interface.h - Unified Model Generation Interface
// Converted from Qt to pure C++17
#ifndef MODEL_INTERFACE_H
#define MODEL_INTERFACE_H

#include "common/json_types.hpp"
#include "common/callback_system.hpp"
#include "common/string_utils.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>
#include <chrono>

// Generation parameters
struct GenerationParams {
    std::string model;
    std::string prompt;
    std::string systemPrompt;
    float temperature = 0.7f;
    float topP = 0.9f;
    int topK = 40;
    int maxTokens = 2048;
    float repeatPenalty = 1.1f;
    int seed = -1;
    bool stream = false;
    std::vector<std::string> stopSequences;
    std::map<std::string, std::string> extraParams;
};

// Generation result
struct GenerationResult {
    bool success = false;
    std::string text;
    std::string error;
    int promptTokens = 0;
    int completionTokens = 0;
    double durationMs = 0.0;
    std::string model;
    std::string finishReason;
    std::map<std::string, std::string> metadata;

    static GenerationResult ok(const std::string& text) {
        GenerationResult r;
        r.success = true;
        r.text = text;
        return r;
    }
    static GenerationResult fail(const std::string& err) {
        GenerationResult r;
        r.success = false;
        r.error = err;
        return r;
    }
};

// Model information
struct ModelInfo {
    std::string name;
    std::string backend;       // ollama, llamacpp, api
    std::string path;
    std::string quantization;
    int parameterCount = 0;    // billions
    int contextLength = 0;
    bool loaded = false;
    bool available = false;
    std::map<std::string, std::string> capabilities;
};

// Backend types
enum class ModelBackend {
    Ollama,
    LlamaCpp,
    OpenAI,
    Custom,
    Unknown
};

// Chat message
struct ChatMessage {
    std::string role;    // system, user, assistant
    std::string content;
};

// Embedding result
struct EmbeddingResult {
    bool success = false;
    std::vector<float> embedding;
    std::string error;
    int dimensions = 0;
};

class ModelInterface {
public:
    ModelInterface();
    ~ModelInterface();

    // Model management
    bool loadModel(const std::string& modelName, ModelBackend backend = ModelBackend::Ollama);
    bool unloadModel(const std::string& modelName);
    bool isModelLoaded(const std::string& modelName) const;
    std::vector<ModelInfo> getAvailableModels() const;
    ModelInfo getModelInfo(const std::string& modelName) const;

    // Generation
    GenerationResult generate(const GenerationParams& params);
    GenerationResult chat(const std::vector<ChatMessage>& messages,
                          const GenerationParams& params = GenerationParams{});
    void generateAsync(const GenerationParams& params);
    void cancelGeneration();

    // Streaming
    void generateStream(const GenerationParams& params,
                        std::function<void(const std::string&)> tokenCallback);

    // Embeddings
    EmbeddingResult getEmbedding(const std::string& text, const std::string& model = "");

    // Configuration
    void setDefaultModel(const std::string& model);
    std::string getDefaultModel() const;
    void setEndpoint(ModelBackend backend, const std::string& endpoint);
    std::string getEndpoint(ModelBackend backend) const;
    void setApiKey(ModelBackend backend, const std::string& key);

    // Health check
    bool isBackendAvailable(ModelBackend backend) const;
    std::string getBackendStatus(ModelBackend backend) const;

    // Callbacks
    CallbackList<const GenerationResult&> onGenerationComplete;
    CallbackList<const std::string&> onTokenReceived;
    CallbackList<const std::string&> onErrorOccurred;
    CallbackList<const std::string&> onModelLoaded;
    CallbackList<const std::string&> onModelUnloaded;

private:
    GenerationResult generateOllama(const GenerationParams& params);
    GenerationResult generateLlamaCpp(const GenerationParams& params);
    GenerationResult generateOpenAI(const GenerationParams& params);
    std::string buildOllamaPayload(const GenerationParams& params) const;
    std::string buildOpenAIPayload(const GenerationParams& params) const;
    std::string httpPost(const std::string& url, const std::string& body,
                         const std::map<std::string, std::string>& headers = {}) const;

    mutable std::mutex m_mutex;
    std::string m_defaultModel;
    std::map<ModelBackend, std::string> m_endpoints;
    std::map<ModelBackend, std::string> m_apiKeys;
    std::unordered_map<std::string, ModelInfo> m_loadedModels;
    bool m_cancelRequested = false;
};

#endif // MODEL_INTERFACE_H
