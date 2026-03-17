// universal_model_router.cpp - Implementation of Universal Model Router
#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

UniversalModelRouter::UniversalModelRouter()
    : local_engine_ready(false),
      cloud_client_ready(false)
{
    // cloud_client = std::make_unique<CloudApiClient>(this);
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        routerError("Invalid configuration for model: " + model_name);
        return;
    }
    
    model_registry[model_name] = config;
    modelRegistered(model_name, config.backend);
}

void UniversalModelRouter::unregisterModel(const std::string& model_name)
{
    if (model_registry.erase(model_name) > 0) {
        modelUnregistered(model_name);
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& model_name) const
{
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) {
        return it->second;
    }
    
    ModelConfig empty;
    empty.model_id = "";
    return empty;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const
{
    std::vector<std::string> keys;
    for (const auto& pair : model_registry) {
        keys.push_back(pair.first);
    }
    return keys;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    std::vector<std::string> models;
    for (const auto& pair : model_registry) {
        if (pair.second.backend == backend) {
            models.push_back(pair.first);
        }
    }
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& config_file_path)
{
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        routerError("Cannot open config file: " + config_file_path);
        return false;
    }
    
    try {
        json config_json;
        file >> config_json;
        return loadConfigFromJson(config_json);
    } catch (const std::exception& e) {
        routerError("Config file is not valid JSON: " + std::string(e.what()));
        return false;
    }
}

bool UniversalModelRouter::loadConfigFromJson(const json& config_json)
{
    model_registry.clear();
    
    if (!config_json.contains("models") || !config_json["models"].is_object()) {
        routerError("Config missing 'models' section");
        return false;
    }
    
    const auto& models_obj = config_json["models"];
    
    for (auto it = models_obj.begin(); it != models_obj.end(); ++it) {
        const std::string& model_name = it.key();
        const auto& model_json = it.value();
        
        ModelConfig config;
        config.full_config = model_json;
        if (model_json.contains("model_id") && model_json["model_id"].is_string())
            config.model_id = model_json["model_id"].get<std::string>();
        
        if (model_json.contains("description") && model_json["description"].is_string())
            config.description = model_json["description"].get<std::string>();
            
        if (model_json.contains("api_key") && model_json["api_key"].is_string())
            config.api_key = model_json["api_key"].get<std::string>();
            
        if (model_json.contains("endpoint") && model_json["endpoint"].is_string())
            config.endpoint = model_json["endpoint"].get<std::string>();
        
        // Parse backend
        if (model_json.contains("backend") && model_json["backend"].is_string()) {
            std::string backend_str = model_json["backend"].get<std::string>();
            std::transform(backend_str.begin(), backend_str.end(), backend_str.begin(), ::toupper);
            
            if (backend_str == "LOCAL_GGUF") config.backend = ModelBackend::LOCAL_GGUF;
            else if (backend_str == "OLLAMA_LOCAL") config.backend = ModelBackend::OLLAMA_LOCAL;
            else if (backend_str == "ANTHROPIC") config.backend = ModelBackend::ANTHROPIC;
            else if (backend_str == "OPENAI") config.backend = ModelBackend::OPENAI;
            else if (backend_str == "GOOGLE") config.backend = ModelBackend::GOOGLE;
            else if (backend_str == "MOONSHOT") config.backend = ModelBackend::MOONSHOT;
            else if (backend_str == "AZURE_OPENAI") config.backend = ModelBackend::AZURE_OPENAI;
            else if (backend_str == "AWS_BEDROCK") config.backend = ModelBackend::AWS_BEDROCK;
            else {
                routerError("Unknown backend: " + backend_str);
                continue;
            }
        }
        
        // Load parameters
        if (model_json.contains("parameters") && model_json["parameters"].is_object()) {
            for (auto p_it = model_json["parameters"].begin(); p_it != model_json["parameters"].end(); ++p_it) {
                if (p_it.value().is_string())
                    config.parameters[p_it.key()] = p_it.value().get<std::string>();
            }
        }
        
        // Handle environment variable substitution
        if (config.api_key.size() > 3 && config.api_key.substr(0, 2) == "${" && config.api_key.back() == '}') {
            std::string env_var = config.api_key.substr(2, config.api_key.length() - 3);
            const char* env_value = std::getenv(env_var.c_str());
            if (env_value) {
                config.api_key = std::string(env_value);
            }
        }
        
        registerModel(model_name, config);
    }
    
    configLoaded(static_cast<int>(model_registry.size()));
    return true;
}

bool UniversalModelRouter::saveConfigToFile(const std::string& config_file_path)
{
    json root;
    json models_obj = json::object();
    
    for (const auto& pair : model_registry) {
        models_obj[pair.first] = pair.second.full_config;
    }
    
    root["models"] = models_obj;
    
    std::ofstream file(config_file_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << root.dump(4);
    configSaved();
    return true;
}

void UniversalModelRouter::initializeLocalEngine(const std::string& engine_config_path)
{
    local_engine_ready = true;
    modelRegistered("local_engine_ready", ModelBackend::LOCAL_GGUF);
}

void UniversalModelRouter::initializeCloudClient()
{
    cloud_client_ready = true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& model_name) const
{
    return model_registry.find(model_name) != model_registry.end();
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& model_name) const
{
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) {
        return it->second.backend;
    }
    return ModelBackend::LOCAL_GGUF;
}

std::string UniversalModelRouter::getModelDescription(const std::string& model_name) const
{
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) {
        return it->second.description;
    }
    return "";
}

json UniversalModelRouter::getModelInfo(const std::string& model_name) const
{
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) {
        return it->second.full_config;
    }
    return json::object();
}

void UniversalModelRouter::onLocalEngineInitialized()
{
    local_engine_ready = true;
}

void UniversalModelRouter::onCloudClientInitialized()
{
    cloud_client_ready = true;
}

void UniversalModelRouter::onEngineError(const std::string& error)
{
    routerError(error);
}

// Callbacks (placeholders to satisfy interface)
void UniversalModelRouter::modelRegistered(const std::string&, ModelBackend) {}
void UniversalModelRouter::modelUnregistered(const std::string&) {}
void UniversalModelRouter::configLoaded(int) {}
void UniversalModelRouter::configSaved() {}
void UniversalModelRouter::routerError(const std::string&) {}





