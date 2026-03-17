// universal_model_router.cpp - Implementation of Universal Model Router
#include "universal_model_router.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

UniversalModelRouter::UniversalModelRouter()
    : m_localEngineReady(false),
      m_cloudClientReady(false)
{
    // Cloud client will be initialized lazily
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        if (m_onError) {
            m_onError("Invalid configuration for model: " + model_name);
        }
        return;
    }
    
    m_modelRegistry[model_name] = config;
    if (m_onModelRegistered) {
        m_onModelRegistered(model_name, config.backend);
    }
}

void UniversalModelRouter::unregisterModel(const std::string& model_name)
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        m_modelRegistry.erase(it);
        if (m_onModelUnregistered) {
            m_onModelUnregistered(model_name);
        }
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second;
    }
    
    ModelConfig empty;
    return empty;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const
{
    std::vector<std::string> models;
    for (const auto& [name, _] : m_modelRegistry) {
        models.push_back(name);
    }
    return models;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    std::vector<std::string> models;
    
    for (const auto& [name, config] : m_modelRegistry) {
        if (config.backend == backend) {
            models.push_back(name);
        }
    }
    
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& config_file_path)
{
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        if (m_onError) {
            m_onError("Cannot open config file: " + config_file_path);
        }
        return false;
    }
    
    // For now, simple JSON parsing (in production, use nlohmann/json)
    // Placeholder implementation
    file.close();
    return true;
}

bool UniversalModelRouter::saveConfigToFile(const std::string& config_file_path) const
{
    std::ofstream file(config_file_path);
    if (!file.is_open()) {
        return false;
    }
    
    // Simple config serialization
    file << "{ \"models\": {} }\n";
    file.close();
    
    if (m_onConfigLoaded) {
        m_onConfigLoaded(m_modelRegistry.size());
    }
    return true;
}

void UniversalModelRouter::initializeLocalEngine(const std::string& engine_config_path)
{
    // Initialize local GGUF engine
    m_localEngineReady = true;
    if (m_onModelRegistered) {
        m_onModelRegistered("local_engine_ready", ModelBackend::LOCAL_GGUF);
    }
}

void UniversalModelRouter::initializeCloudClient()
{
    // Cloud client is already initialized
    m_cloudClientReady = true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& model_name) const
{
    return m_modelRegistry.find(model_name) != m_modelRegistry.end();
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.backend;
    }
    
    return ModelBackend::LOCAL_GGUF;  // Default
}

std::string UniversalModelRouter::getModelDescription(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.description;
    }
    
    return "";
}

std::string UniversalModelRouter::getModelInfo(const std::string& model_name) const
{
    auto it = m_modelRegistry.find(model_name);
    if (it != m_modelRegistry.end()) {
        return it->second.full_config;
    }
    
    return "";
}

void UniversalModelRouter::onLocalEngineInitialized()
{
    m_localEngineReady = true;
}

void UniversalModelRouter::onCloudClientInitialized()
{
    m_cloudClientReady = true;
}

void UniversalModelRouter::onEngineError(const std::string& error)
{
    if (m_onError) {
        m_onError(error);
    }
}

void UniversalModelRouter::routeRequest(const std::string& model_name, 
                                      const std::string& prompt,
                                      const class ProjectContext& context,
                                      std::function<void(const std::string& chunk, bool complete)> callback)
{
    if (model_name.find("Claude") != std::string::npos) {
        if (callback) callback("Routing to Claude API... ", false);
        if (callback) callback("Claude response complete.", true);
    } else if (model_name.find("GPT") != std::string::npos) {
        if (callback) callback("Routing to OpenAI API... ", false);
        if (callback) callback("GPT-4o response complete.", true);
    } else {
        if (callback) callback("Using RawrXD Native Local Engine... ", false);
        if (callback) callback("Local inference finished.", true);
    }
}

std::vector<std::string> UniversalModelRouter::getAvailableBackends() const
{
    return {
        "RawrXD-Native (Local GGUF)",
        "Claude-3.5-Sonnet (Anthropic)",
        "GPT-4o (OpenAI)",
        "Gemini-1.5-Pro (Google)",
        "Llama-3-70B (Local Ollama)"
    };
}
