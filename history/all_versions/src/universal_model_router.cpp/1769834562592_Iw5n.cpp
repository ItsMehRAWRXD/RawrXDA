#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <fstream>
#include <iostream>

// Stub definition for unique_ptr completeness
class QuantizationAwareInferenceEngine {
public:
    QuantizationAwareInferenceEngine() = default;
    virtual ~QuantizationAwareInferenceEngine() = default;
    
    std::string generate(const std::string& prompt, float temperature) {
        return "Local Inference specific logic not implemented yet. (Stub)";
    }
};

// If CloudApiClient functions are needed, include header. 
// We already included it. ensure it has a virtual destructor if we use unique_ptr logic that requires it, 
// but unique_ptr<T> just needs sizeof(T) usually.

UniversalModelRouter::UniversalModelRouter() : local_engine_ready(false) {}
UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const std::string& name, const ModelConfig& config) {
    if (name.empty()) return;
    model_registry[name] = config;
}

void UniversalModelRouter::unregisterModel(const std::string& name) {
    model_registry.erase(name);
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second;
    return ModelConfig();
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const {
    std::vector<std::string> keys;
    for(const auto& [k, v] : model_registry) keys.push_back(k);
    return keys;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const {
     std::vector<std::string> keys;
     for(const auto& [k, v] : model_registry) {
         if (v.backend == backend) keys.push_back(k);
     }
     return keys;
}

bool UniversalModelRouter::loadConfigFromFile(const std::string& path) {
    try {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        json j;
        f >> j;
        return loadConfigFromJson(j);
    } catch (...) {
        return false;
    }
}

bool UniversalModelRouter::loadConfigFromJson(const json& j) {
    try {
        if (!j.is_object()) return false;
        
        if (j.contains("models") && j["models"].is_object()) {
            for (auto& [name, model_json] : j["models"].items()) {
                ModelConfig config;
                // Default to LOCAL_GGUF if not specified
                int backendVal = model_json.value("backend", 0);
                config.backend = static_cast<ModelBackend>(backendVal);
                config.model_id = model_json.value("model_id", "");
                config.api_key = model_json.value("api_key", "");
                config.endpoint = model_json.value("endpoint", "");
                config.description = model_json.value("description", "");
                
                if (model_json.contains("parameters") && model_json["parameters"].is_object()) {
                    for (auto& [pk, pv] : model_json["parameters"].items()) {
                        config.parameters[pk] = pv.get<std::string>();
                    }
                }
                
                config.full_config = model_json;
                registerModel(name, config);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool UniversalModelRouter::saveConfigToFile(const std::string& path) {
    try {
        json root = json::object();
        json models = json::object();
        
        for (const auto& [name, config] : model_registry) {
            json m;
            m["backend"] = static_cast<int>(config.backend);
            m["model_id"] = config.model_id;
            m["api_key"] = config.api_key;
            m["endpoint"] = config.endpoint;
            m["description"] = config.description;
            
            json params = json::object();
            for (const auto& [pk, pv] : config.parameters) {
                params[pk] = pv;
            }
            m["parameters"] = params;
            
            models[name] = m;
        }
        
        root["models"] = models;
        
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << root.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

void UniversalModelRouter::initializeLocalEngine(const std::string& path) {
    local_engine = std::make_unique<QuantizationAwareInferenceEngine>();
    local_engine_ready = true;
}

void UniversalModelRouter::initializeCloudClient() {
    cloud_client = std::make_unique<CloudApiClient>();
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& name) {
    return getModelConfig(name);
}

bool UniversalModelRouter::isModelAvailable(const std::string& name) const {
    return model_registry.find(name) != model_registry.end();
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.backend;
    return ModelBackend::LOCAL_GGUF;
}

std::string UniversalModelRouter::getModelDescription(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.description;
    return "";
}

json UniversalModelRouter::getModelInfo(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.full_config;
    return json::object();
}

std::string UniversalModelRouter::routeQuery(const std::string& model_name, const std::string& prompt, float temperature) {
    if (!isModelAvailable(model_name)) {
        return "Error: Model not found.";
    }

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF) {
        if (!local_engine_ready || !local_engine) {
             return "Error: Local engine not initialized.";
        }
        return local_engine->generate(prompt, temperature);
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        return cloud_client->generate(prompt, config);
    }
}

void UniversalModelRouter::routeStreamQuery(const std::string& model_name, const std::string& prompt, StreamCallback callback, float temperature) {
    if (!isModelAvailable(model_name)) {
        if(callback) callback("Error: Model not found.");
        return;
    }

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF) {
        if (!local_engine_ready || !local_engine) {
             if(callback) callback("Error: Local engine not initialized.");
             return;
        }
        // Local engine doesn't support streaming in this stub yet
        std::string res = local_engine->generate(prompt, temperature);
        if(callback) callback(res);
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        cloud_client->generateStream(prompt, config, callback);
    }
}

json UniversalModelRouter::getModelInfo(const std::string& model_name) const {
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) return it->second.full_config;
    return json::object();
}

std::string UniversalModelRouter::routeQuery(const std::string& model_name, const std::string& prompt, float temperature) {
    if (!isModelAvailable(model_name)) {
        return "Error: Model not found.";
    }

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF) {
        if (!local_engine_ready || !local_engine) {
             return "Error: Local engine not initialized.";
        }
        return local_engine->generate(prompt, temperature);
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        return cloud_client->generate(prompt, config);
    }
}

void UniversalModelRouter::routeStreamQuery(const std::string& model_name, const std::string& prompt, StreamCallback callback, float temperature) {
    if (!isModelAvailable(model_name)) {
        if(callback) callback("Error: Model not found.");
        return;
    }

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF) {
        if (!local_engine_ready || !local_engine) {
             if(callback) callback("Error: Local engine not initialized.");
             return;
        }
        // Local engine doesn't support streaming in this stub yet
        std::string res = local_engine->generate(prompt, temperature);
        if(callback) callback(res);
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        cloud_client->generateStream(prompt, config, callback);
    }
}

json UniversalModelRouter::getModelInfo(const std::string& model_name) const {
    auto it = model_registry.find(model_name);
    if (it != model_registry.end()) return it->second.full_config;
    return json::object();
}
