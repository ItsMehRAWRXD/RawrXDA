// universal_model_router.h - Unified Interface for Local GGUF + Cloud Models
#ifndef UNIVERSAL_MODEL_ROUTER_H
#define UNIVERSAL_MODEL_ROUTER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>
#include <stdexcept>

class QuantizationAwareInferenceEngine;
class CloudApiClient;

// Model backend enumeration
enum class ModelBackend {
    LOCAL_GGUF,        // Local GGUF quantized models
    OLLAMA_LOCAL,      // Ollama API (local HTTP server)
    ANTHROPIC,         // Claude API (Anthropic)
    OPENAI,            // GPT-4, GPT-3.5 (OpenAI)
    GOOGLE,            // Gemini (Google)
    MOONSHOT,          // Kimi (Moonshot)
    AZURE_OPENAI,      // OpenAI through Azure
    AWS_BEDROCK,       // Claude/Mistral through AWS
    REASONING_ENGINE   // Local C++20/ASM Expert Reasoning Engine
};

// Model configuration structure
struct ModelConfig {
    ModelBackend backend = ModelBackend::LOCAL_GGUF;
    std::string model_id;                           // Model identifier (e.g., "gpt-4", "claude-3-opus")
    std::string api_key;                            // API key (may be empty for local models)
    std::string endpoint;                           // Custom endpoint URL
    std::unordered_map<std::string, std::string> parameters;  // Additional parameters
    std::string description;                        // Human-readable description
    std::string full_config;                        // Full JSON configuration (as string)
    
    // Validation
    bool isValid() const {
        if (model_id.empty()) return false;
        
        // Cloud models require API key
        if (backend != ModelBackend::LOCAL_GGUF && 
            backend != ModelBackend::OLLAMA_LOCAL && 
            api_key.empty()) {
            return false;
        }
        
        return true;
    }
};

// Streaming callback type
using StreamCallback = std::function<void(const std::string&)>;

// Error callback type
using ErrorCallback = std::function<void(const std::string&)>;

// Universal Model Router - Routes requests to appropriate backend
class UniversalModelRouter {
public:
    // Callback function types
    using OnModelRegisteredCallback = std::function<void(const std::string&, ModelBackend)>;
    using OnModelUnregisteredCallback = std::function<void(const std::string&)>;
    using OnConfigLoadedCallback = std::function<void(int)>;
    using OnErrorCallback = std::function<void(const std::string&)>;

    UniversalModelRouter();
    ~UniversalModelRouter();

    // Delete copy operations
    UniversalModelRouter(const UniversalModelRouter&) = delete;
    UniversalModelRouter& operator=(const UniversalModelRouter&) = delete;

    // Allow move operations
    UniversalModelRouter(UniversalModelRouter&&) = default;
    UniversalModelRouter& operator=(UniversalModelRouter&&) = default;

    // Model registration and management
    void registerModel(const std::string& model_name, const ModelConfig& config);
    void unregisterModel(const std::string& model_name);
    ModelConfig getModelConfig(const std::string& model_name) const;
    std::vector<std::string> getAvailableModels() const;
    std::vector<std::string> getModelsForBackend(ModelBackend backend) const;
    
    // Model configuration loading
    bool loadConfigFromFile(const std::string& config_file_path);
    bool saveConfigToFile(const std::string& config_file_path) const;
    
    // Backend initialization
    void initializeLocalEngine(const std::string& model_path);
    void initializeCloudClient();
    
    // Direct model access
    ModelConfig getOrLoadModel(const std::string& model_name);
    bool isModelAvailable(const std::string& model_name) const;
    ModelBackend getModelBackend(const std::string& model_name) const;
    
    // Model info retrieval
    std::string getModelDescription(const std::string& model_name) const;
    std::string getModelInfo(const std::string& model_name) const;

    // Unified inference entry point
    void routeRequest(const std::string& model_name, 
                      const std::string& prompt,
                      const class ProjectContext& context,
                      std::function<void(const std::string& chunk, bool complete)> callback);

    // Backend helper for UI
    std::vector<std::string> getAvailableBackends() const;

    // Callback setters for event notifications
    void setOnModelRegisteredCallback(OnModelRegisteredCallback cb) { m_onModelRegistered = cb; }
    void setOnModelUnregisteredCallback(OnModelUnregisteredCallback cb) { m_onModelUnregistered = cb; }
    void setOnConfigLoadedCallback(OnConfigLoadedCallback cb) { m_onConfigLoaded = cb; }
    void setOnErrorCallback(OnErrorCallback cb) { m_onError = cb; }

private:
    void onLocalEngineInitialized();
    void onCloudClientInitialized();
    void onEngineError(const std::string& error);

    std::unordered_map<std::string, ModelConfig> m_modelRegistry;
    std::unique_ptr<class QuantizationAwareInferenceEngine> m_localEngine;
    std::unique_ptr<class CloudApiClient> m_cloudClient;
    bool m_localEngineReady = false;
    bool m_cloudClientReady = false;

    // Callbacks
    OnModelRegisteredCallback m_onModelRegistered;
    OnModelUnregisteredCallback m_onModelUnregistered;
    OnConfigLoadedCallback m_onConfigLoaded;
    OnErrorCallback m_onError;
};

#endif // UNIVERSAL_MODEL_ROUTER_H
