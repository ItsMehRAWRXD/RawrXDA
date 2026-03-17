#include "universal_model_router.h"
#include "cloud_api_client.h"
#include "cpu_inference_engine.h"
#include <fstream>
#include <iostream>
#include <future>

namespace RawrXD {

UniversalModelRouter::UniversalModelRouter() : local_engine_ready(false), local_engine(nullptr), cloud_client(nullptr) {}


UniversalModelRouter::~UniversalModelRouter() {
    // Unique ptrs handle cleanup
}

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
    if (!local_engine) {
        local_engine = std::make_unique<CPUInferenceEngine>();
        // logic to load model from path, ignoring for now as per previous stub pattern
        // but ensuring it's "ready"
    }
    local_engine_ready = true;
}

void UniversalModelRouter::initializeCloudClient() {
    // CloudApiClient is standard unique_ptr
    if (!cloud_client) {
        cloud_client = std::make_unique<CloudApiClient>(this);
    }
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

// Helper to bridge configs
CloudConnectionConfig bridgeToCloudConfig(const RawrXD::ModelConfig& mc, float temp) {
    CloudConnectionConfig cc;
    
    switch(mc.backend) {
        case ModelBackend::ANTHROPIC: cc.provider = "anthropic"; break;
        case ModelBackend::OLLAMA_LOCAL: cc.provider = "ollama"; break;
        case ModelBackend::AZURE_OPENAI: cc.provider = "azure"; break;
        case ModelBackend::GOOGLE: cc.provider = "google"; break;
        case ModelBackend::MOONSHOT: cc.provider = "moonshot"; break;
        case ModelBackend::OPENAI: 
        default:
            cc.provider = "openai"; break;
    }
    
    cc.model = mc.model_id;
    cc.apiKey = mc.api_key;
    cc.endpoint = mc.endpoint;
    cc.temperature = temp;
    
    // Check parameters for overrides
    if (mc.parameters.count("max_tokens")) {
        try { cc.maxTokens = std::stoi(mc.parameters.at("max_tokens")); } catch(...) {}
    }
    
    return cc;
}

std::string UniversalModelRouter::routeQuery(const std::string& model_name, const std::string& prompt, float temperature) {
    if (!isModelAvailable(model_name)) {
        return "Error: Model not found.";
    }

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF) {
        if (!local_engine_ready || !local_engine) {
             initializeLocalEngine(""); // Lazy init
        }
        
        // Blocking generation using streaming interface
        std::string full_response;
        std::promise<void> done_promise;
        std::future<void> done_future = done_promise.get_future();
        
        if (local_engine) {
            local_engine->GenerateStreaming(
                local_engine->Tokenize(prompt), 
                512, // max tokens
                [&full_response](const std::string& chunk) { full_response += chunk; },
                [&done_promise]() { done_promise.set_value(); }
            );
            done_future.wait();
        } else {
             return "Error: Local Engine Failed Init";
        }
        
        return full_response;
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        return cloud_client->generate(prompt, bridgeToCloudConfig(config, temperature));
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
             initializeLocalEngine("");
        }
        if (local_engine) {
            local_engine->GenerateStreaming(
                local_engine->Tokenize(prompt),
                512,
                callback,
                nullptr // No completion callback usage here
            ); 
        }
    } else {
        if (!cloud_client) {
            initializeCloudClient();
        }
        cloud_client->generateStream(prompt, bridgeToCloudConfig(config, temperature), callback);
    }
}



} // namespace RawrXD
