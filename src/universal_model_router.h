// universal_model_router.h - Unified Interface for Local GGUF + Cloud Models
#ifndef UNIVERSAL_MODEL_ROUTER_H
#define UNIVERSAL_MODEL_ROUTER_H

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

class CPUInferenceEngine;
class CloudApiClient;
class PipeClient;

// Model backend enumeration
enum class ModelBackend {
    LOCAL_GGUF,        // Local GGUF quantized models
    LOCAL_TITAN,       // Native Assembly Engine (RawrXD_NativeHost)
    OLLAMA_LOCAL,      // Ollama API (local HTTP server)
    ANTHROPIC,         // Claude API (Anthropic)
    OPENAI,            // GPT-4, GPT-3.5 (OpenAI)
    GOOGLE,            // Gemini (Google)
    MOONSHOT,          // Kimi (Moonshot)
    AZURE_OPENAI,      // OpenAI through Azure
    AWS_BEDROCK,       // Claude/Mistral through AWS
    REASONING_ENGINE   // Specialized reasoning engine backend
};

// Model configuration structure
struct ModelConfig {
    ModelBackend backend;
    std::string model_id;                               // Model identifier (e.g., "gpt-4", "claude-3-opus")
    std::string api_key;                                // API key (may be empty for local models)
    std::string endpoint;                               // Custom endpoint URL
    std::map<std::string, std::string> parameters;      // Additional parameters
    std::string description;                            // Human-readable description
    json full_config;                                   // Full JSON configuration
    
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
    explicit UniversalModelRouter();
    ~UniversalModelRouter();

    // Model registration and management
    void registerModel(const std::string& model_name, const ModelConfig& config);
    void unregisterModel(const std::string& model_name);
    ModelConfig getModelConfig(const std::string& model_name) const;
    std::vector<std::string> getAvailableModels() const;
    std::vector<std::string> getModelsForBackend(ModelBackend backend) const;
    
    // Model configuration loading
    bool loadConfigFromFile(const std::string& config_file_path);
    bool loadConfigFromJson(const json& config_json);
    bool saveConfigToFile(const std::string& config_file_path);
    
    // Backend initialization
    void initializeLocalEngine(const std::string& engine_config_path);
    void initializeCloudClient();
    
    // Model hot-swap
    bool hotSwapModel(const std::string& new_model_path, bool preserve_kv_cache = false);
    
    // Direct model access
    ModelConfig getOrLoadModel(const std::string& model_name);
    bool isModelAvailable(const std::string& model_name) const;
    ModelBackend getModelBackend(const std::string& model_name) const;
    
    // Model info retrieval
    std::string getModelDescription(const std::string& model_name) const;
    json getModelInfo(const std::string& model_name) const;

    // Inference
    std::string routeQuery(const std::string& model_name, const std::string& prompt, float temperature = 0.7f);
    void routeStreamQuery(const std::string& model_name, const std::string& prompt, StreamCallback callback, float temperature = 0.7f);
    
    // Additional features
    std::vector<std::string> getAvailableBackends() const;
    void routeRequest(const std::string& model_name, const std::string& prompt, std::function<void(const std::string&)> callback);

    // Callbacks from engine
    void onLocalEngineInitialized();
    void onCloudClientInitialized();
    void onEngineError(const std::string& error);

private:
    std::map<std::string, ModelConfig> m_modelRegistry;
    std::unique_ptr<RawrXD::CPUInferenceEngine> local_engine;
    std::unique_ptr<RawrXD::PipeClient> titan_client;
    std::unique_ptr<CloudApiClient> cloud_client; // Default deleter ok
    bool m_localEngineReady;
    bool m_cloudClientReady;
    
    // Callbacks
    ErrorCallback m_onError;
    std::function<void(const std::string&, ModelBackend)> m_onModelRegistered;
    std::function<void(const std::string&)> m_onModelUnregistered;
    std::function<void(int)> m_onConfigLoaded;
};

} // namespace RawrXD

#endif // UNIVERSAL_MODEL_ROUTER_H

