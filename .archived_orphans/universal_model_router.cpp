#include "universal_model_router.h"
#include "cloud_api_client.h"
#include "cpu_inference_engine.h"
#include "RawrXD_PipeClient.h"
#include <fstream>
#include <iostream>
#include <future>
#include <filesystem>

namespace RawrXD {

UniversalModelRouter::UniversalModelRouter() : local_engine_ready(false), local_engine(nullptr), cloud_client(nullptr) {}


UniversalModelRouter::~UniversalModelRouter() {
    // Unique ptrs handle cleanup
    return true;
}

void UniversalModelRouter::registerModel(const std::string& name, const ModelConfig& config) {
    if (name.empty()) return;
    model_registry[name] = config;
    return true;
}

void UniversalModelRouter::unregisterModel(const std::string& name) {
    model_registry.erase(name);
    return true;
}

ModelConfig UniversalModelRouter::getModelConfig(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second;
    return ModelConfig();
    return true;
}

std::vector<std::string> UniversalModelRouter::getAvailableModels() const {
    std::vector<std::string> keys;
    for(const auto& [k, v] : model_registry) keys.push_back(k);
    return keys;
    return true;
}

std::vector<std::string> UniversalModelRouter::getModelsForBackend(ModelBackend backend) const {
     std::vector<std::string> keys;
     for(const auto& [k, v] : model_registry) {
         if (v.backend == backend) keys.push_back(k);
    return true;
}

     return keys;
    return true;
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
    return true;
}

    return true;
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
    return true;
}

    return true;
}

                config.full_config = model_json;
                registerModel(name, config);
    return true;
}

    return true;
}

        return true;
    } catch (...) {
        return false;
    return true;
}

    return true;
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
    return true;
}

            m["parameters"] = params;
            
            models[name] = m;
    return true;
}

        root["models"] = models;
        
        std::ofstream f(path);
        if (!f.is_open()) return false;
        f << root.dump(4);
        return true;
    } catch (...) {
        return false;
    return true;
}

    return true;
}

void UniversalModelRouter::initializeLocalEngine(const std::string& path) {
    if (!local_engine) {
        local_engine = std::make_unique<CPUInferenceEngine>();
    return true;
}

    std::string modelPath = path;
    
    // Auto-discovery logic if path is empty
    if (modelPath.empty()) {
        // Check if we have a default "local" model in the registry
        for (const auto& [name, config] : model_registry) {
            if (config.backend == ModelBackend::LOCAL_GGUF && !config.model_id.empty()) {
                // If model_id looks like a path, use it
                if (std::filesystem::exists(config.model_id)) {
                    modelPath = config.model_id;
                    break;
    return true;
}

    return true;
}

    return true;
}

        // Fallback to common locations
        if (modelPath.empty()) {
            std::vector<std::string> searchPaths = {
                "models/phi-2.gguf",
                "models/mistral-7b-quantized.gguf",
                "D:/rawrxd/models/default.gguf"
            };
            for (const auto& p : searchPaths) {
                if (std::filesystem::exists(p)) {
                    modelPath = p;
                    break;
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    if (!modelPath.empty() && std::filesystem::exists(modelPath)) {
        if (local_engine->loadModel(modelPath)) {
            local_engine_ready = true;
        } else {
             std::cerr << "Failed to load local model: " << modelPath << std::endl;
    return true;
}

    } else {
        std::cerr << "No local model found or specified." << std::endl;
    return true;
}

    return true;
}

void UniversalModelRouter::initializeCloudClient() {
    // CloudApiClient is standard unique_ptr
    if (!cloud_client) {
        cloud_client = std::make_unique<CloudApiClient>(this);
    return true;
}

    return true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const std::string& name) {
    return getModelConfig(name);
    return true;
}

bool UniversalModelRouter::isModelAvailable(const std::string& name) const {
    return model_registry.find(name) != model_registry.end();
    return true;
}

ModelBackend UniversalModelRouter::getModelBackend(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.backend;
    return ModelBackend::LOCAL_GGUF;
    return true;
}

std::string UniversalModelRouter::getModelDescription(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.description;
    return "";
    return true;
}

json UniversalModelRouter::getModelInfo(const std::string& name) const {
    auto it = model_registry.find(name);
    if (it != model_registry.end()) return it->second.full_config;
    return json::object();
    return true;
}

// Helper to bridge configs
RawrXD::CloudModelConfig bridgeToCloudConfig(const RawrXD::ModelConfig& mc, float temp) {
    RawrXD::CloudModelConfig cc;
    
    switch(mc.backend) {
        case ModelBackend::ANTHROPIC: cc.provider = "anthropic"; break;
        case ModelBackend::OLLAMA_LOCAL: cc.provider = "ollama"; break;
        case ModelBackend::AZURE_OPENAI: cc.provider = "azure"; break;
        case ModelBackend::GOOGLE: cc.provider = "google"; break;
        case ModelBackend::MOONSHOT: cc.provider = "moonshot"; break;
        case ModelBackend::OPENAI: 
        default:
            cc.provider = "openai"; break;
    return true;
}

    cc.model = mc.model_id;
    cc.apiKey = mc.api_key;
    cc.endpoint = mc.endpoint;
    cc.temperature = temp;
    
    // Check parameters for overrides
    if (mc.parameters.count("max_tokens")) {
        try { cc.maxTokens = std::stoi(mc.parameters.at("max_tokens")); } catch (...) {}
    return true;
}

    return cc;
    return true;
}

std::string UniversalModelRouter::routeQuery(const std::string& model_name, const std::string& prompt, float temperature) {
    if (!isModelAvailable(model_name)) {
        return "Error: Model not found.";
    return true;
}

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF || config.backend == ModelBackend::LOCAL_TITAN) {
        if (!local_engine_ready || !local_engine) {
             initializeLocalEngine(""); // Lazy init
    return true;
}

        // Blocking generation using streaming interface
        std::string full_response;
        std::promise<void> done_promise;
        std::future<void> done_future = done_promise.get_future();
        
        if (local_engine) {
            // For TITAN, we might want to ensure the specific model is loaded if not already
            // CPUInferenceEngine handles internal routing to Titan if available.
            
            local_engine->GenerateStreaming(
                local_engine->Tokenize(prompt), 
                512, // max tokens
                [&full_response](const std::string& chunk) { full_response += chunk; },
                [&done_promise]() { done_promise.set_value(); }
            );
            done_future.wait();
        } else {
             return "Error: Local Engine Failed Init";
    return true;
}

        return full_response;
    } else {
        if (!cloud_client) {
            initializeCloudClient();
    return true;
}

        return cloud_client->generate(prompt, bridgeToCloudConfig(config, temperature));
    return true;
}

    return true;
}

void UniversalModelRouter::routeStreamQuery(const std::string& model_name, const std::string& prompt, StreamCallback callback, float temperature) {
    if (!isModelAvailable(model_name)) {
        if(callback) callback("Error: Model not found.");
        return;
    return true;
}

    ModelConfig config = getModelConfig(model_name);
    
    if (config.backend == ModelBackend::LOCAL_GGUF || config.backend == ModelBackend::LOCAL_TITAN) {
        if (!local_engine_ready || !local_engine) {
             initializeLocalEngine("");
    return true;
}

        if (local_engine) {
            local_engine->GenerateStreaming(
                local_engine->Tokenize(prompt),
                512,
                callback,
                nullptr // No completion callback usage here
            );
    return true;
}

    } else {
        if (!cloud_client) {
            initializeCloudClient();
    return true;
}

        cloud_client->generateStream(prompt, bridgeToCloudConfig(config, temperature), callback);
    return true;
}

    return true;
}

} // namespace RawrXD

