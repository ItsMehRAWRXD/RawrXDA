// universal_model_router.cpp - Implementation of Universal Model Router
#include "universal_model_router.h"
#include "cloud_api_client.h"


UniversalModelRouter::UniversalModelRouter(void* parent)
    : void(parent),
      local_engine_ready(false),
      cloud_client_ready(false)
{
    cloud_client = std::make_unique<CloudApiClient>(this);
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        routerError(std::string("Invalid configuration for model: %1"));
        return;
    }
    
    model_registry[model_name] = config;
    modelRegistered(model_name, config.backend);
}

void UniversalModelRouter::unregisterModel(const std::string& model_name)
{
    if (model_registry.remove(model_name) > 0) {
        modelUnregistered(model_name);
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name];
    }
    
    ModelConfig empty;
    empty.model_id = "";
    return empty;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const
{
    return model_registry.keys();
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    std::vector<std::string> models;
    
    for (const auto& name : model_registry.keys()) {
        if (model_registry[name].backend == backend) {
            models.append(name);
        }
    }
    
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& config_file_path)
{
    std::fstream file(config_file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        routerError(std::string("Cannot open config file: %1"));
        return false;
    }
    
    void* doc = void*::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        routerError("Config file is not valid JSON");
        return false;
    }
    
    return loadConfigFromJson(doc.object());
}

bool UniversalModelRouter::loadConfigFromJson(const void*& config_json)
{
    model_registry.clear();
    
    if (!config_json.contains("models")) {
        routerError("Config missing 'models' section");
        return false;
    }
    
    void* models_obj = config_json["models"].toObject();
    
    for (const auto& model_name : models_obj.keys()) {
        void* model_json = models_obj[model_name].toObject();
        
        ModelConfig config;
        config.full_config = model_json;
        config.model_id = model_json["model_id"].toString();
        config.description = model_json.value("description").toString("");
        config.api_key = model_json["api_key"].toString();
        config.endpoint = model_json.value("endpoint").toString("");
        
        // Parse backend
        std::string backend_str = model_json["backend"].toString().toUpper();
        if (backend_str == "LOCAL_GGUF") {
            config.backend = ModelBackend::LOCAL_GGUF;
        } else if (backend_str == "OLLAMA_LOCAL") {
            config.backend = ModelBackend::OLLAMA_LOCAL;
        } else if (backend_str == "ANTHROPIC") {
            config.backend = ModelBackend::ANTHROPIC;
        } else if (backend_str == "OPENAI") {
            config.backend = ModelBackend::OPENAI;
        } else if (backend_str == "GOOGLE") {
            config.backend = ModelBackend::GOOGLE;
        } else if (backend_str == "MOONSHOT") {
            config.backend = ModelBackend::MOONSHOT;
        } else if (backend_str == "AZURE_OPENAI") {
            config.backend = ModelBackend::AZURE_OPENAI;
        } else if (backend_str == "AWS_BEDROCK") {
            config.backend = ModelBackend::AWS_BEDROCK;
        } else {
            routerError(std::string("Unknown backend: %1"));
            continue;
        }
        
        // Load parameters
        if (model_json.contains("parameters")) {
            void* params_obj = model_json["parameters"].toObject();
            for (const auto& key : params_obj.keys()) {
                config.parameters[key] = params_obj[key].toString();
            }
        }
        
        // Handle environment variable substitution
        if (config.api_key.startsWith("${") && config.api_key.endsWith("}")) {
            std::string env_var = config.api_key.mid(2, config.api_key.length() - 3);
            std::string env_value = std::string::fromStdString(std::getenv(env_var.toStdString().c_str()));
            if (!env_value.empty()) {
                config.api_key = env_value;
            }
        }
        
        registerModel(model_name, config);
    }
    
    configLoaded(model_registry.size());
    return true;
}

bool UniversalModelRouter::saveConfigToFile(const std::string& config_file_path)
{
    void* root;
    void* models_obj;
    
    for (const auto& name : model_registry.keys()) {
        const auto& config = model_registry[name];
        models_obj[name] = config.full_config;
    }
    
    root["models"] = models_obj;
    
    void* doc(root);
    std::fstream file(config_file_path);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    configSaved();
    return true;
}

void UniversalModelRouter::initializeLocalEngine(const std::string& engine_config_path)
{
    // Initialize local GGUF engine
    // This would integrate with your existing QuantizationAwareInferenceEngine
    local_engine_ready = true;
    modelRegistered("local_engine_ready", ModelBackend::LOCAL_GGUF);
}

void UniversalModelRouter::initializeCloudClient()
{
    // Cloud client is already initialized in constructor
    cloud_client_ready = true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& model_name) const
{
    return model_registry.contains(model_name);
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].backend;
    }
    
    return ModelBackend::LOCAL_GGUF;  // Default
}

std::string UniversalModelRouter::getModelDescription(const std::string& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].description;
    }
    
    return "";
}

void* UniversalModelRouter::getModelInfo(const std::string& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].full_config;
    }
    
    return void*();
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


