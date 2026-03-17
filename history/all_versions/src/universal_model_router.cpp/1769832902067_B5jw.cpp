#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <fstream>
#include <iostream>

// Stub definition for unique_ptr completeness
class QuantizationAwareInferenceEngine {
public:
    QuantizationAwareInferenceEngine() = default;
    virtual ~QuantizationAwareInferenceEngine() = default;
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
    return true; // Stub
}

bool UniversalModelRouter::loadConfigFromJson(const json& j) {
    return true; // Stub
}

bool UniversalModelRouter::saveConfigToFile(const std::string& path) {
    return true; // Stub
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
    json j = json::object();
    j["name"] = name;
    return j;
}
