// model_interface.cpp - Implementation of Unified Model Interface
#include "model_interface.h"
#include "universal_model_router.h"
#include "cloud_api_client.h"

#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>

ModelInterface::ModelInterface(void* parent)
    : default_model(""),
      initialized_flag(false)
{
    // Initialization deferred to initialize() method
}

ModelInterface::~ModelInterface() = default;

void ModelInterface::initialize(const std::string& config_file_path)
{
    router = std::make_shared<UniversalModelRouter>(this);
    cloud_client = std::make_shared<CloudApiClient>(this);
    // Initialize Local Engine
    local_engine = std::make_shared<RawrXD::CPUInferenceEngine>();
    
    if (router->loadConfigFromFile(config_file_path)) {
        initialized_flag = true;
    }
}

void ModelInterface::initializeWithRouter(std::shared_ptr<UniversalModelRouter> provided_router)
{
    router = provided_router;
    cloud_client = std::make_shared<CloudApiClient>(this);
    // Initialize Local Engine
    local_engine = std::make_shared<RawrXD::CPUInferenceEngine>();

    initialized_flag = (router != nullptr);
}

bool ModelInterface::isInitialized() const
{
    return initialized_flag;
}

GenerationResult ModelInterface::generate(const std::string& prompt,
                                         const std::string& model_name,
                                         const GenerationOptions& options)
{
    if (!initialized_flag || !router) {
        GenerationResult result;
        result.success = false;
        result.error = "ModelInterface not initialized";
        return result;
    }
    
    return generateInternal(prompt, model_name, options);
}

void ModelInterface::generateAsync(const std::string& prompt,
                                  const std::string& model_name,
                                  std::function<void(const GenerationResult&)> callback,
                                  const GenerationOptions& options)
{
    // Use std::thread for async execution
    std::thread([this, prompt, model_name, callback, options]() {
        auto result = this->generate(prompt, model_name, options);
        if (callback) {
            callback(result);
        }
    }).detach();
}

void ModelInterface::generateStream(const std::string& prompt,
                                   const std::string& model_name,
                                   std::function<void(const std::string&)> on_chunk,
                                   std::function<void(const std::string&)> on_error,
                                   const GenerationOptions& options)
{
    if (!initialized_flag || !router) {
        if (on_error) {
            on_error("ModelInterface not initialized");
        }
        return;
    }
    
    generateStreamInternal(prompt, model_name, on_chunk, on_error, options);
}

std::vector<GenerationResult> ModelInterface::generateBatch(const std::vector<std::string>& prompts,
                                                        const std::string& model_name,
                                                        const GenerationOptions& options)
{
    std::vector<GenerationResult> results;
    for (const auto& prompt : prompts) {
        results.push_back(generate(prompt, model_name, options));
    }
    return results;
}

void ModelInterface::generateBatchAsync(const std::vector<std::string>& prompts,
                                       const std::string& model_name,
                                       std::function<void(const std::vector<GenerationResult>&)> callback,
                                       const GenerationOptions& options)
{
    std::thread([this, prompts, model_name, callback, options]() {
        auto results = this->generateBatch(prompts, model_name, options);
        if (callback) {
            callback(results);
        }
    }).detach();
}

std::vector<std::string> ModelInterface::getAvailableModels() const
{
    if (!router) return {};
    return router->getAvailableModels();
}

std::vector<std::string> ModelInterface::getLocalModels() const
{
    if (!router) return {};
    auto list1 = router->getModelsForBackend(ModelBackend::LOCAL_GGUF);
    auto list2 = router->getModelsForBackend(ModelBackend::OLLAMA_LOCAL);
    list1.insert(list1.end(), list2.begin(), list2.end());
    return list1;
}

std::vector<std::string> ModelInterface::getCloudModels() const
{
    if (!router) return {};
    std::vector<std::string> cloud_models;
    
    auto append = [&](ModelBackend b) {
        auto list = router->getModelsForBackend(b);
        cloud_models.insert(cloud_models.end(), list.begin(), list.end());
    };

    append(ModelBackend::ANTHROPIC);
    append(ModelBackend::OPENAI);
    append(ModelBackend::GOOGLE);
    append(ModelBackend::MOONSHOT);
    append(ModelBackend::AZURE_OPENAI);
    append(ModelBackend::AWS_BEDROCK);
    
    return cloud_models;
}

std::string ModelInterface::getModelDescription(const std::string& model_name) const
{
    if (!router) return "";
    return router->getModelDescription(model_name);
}

void* ModelInterface::getModelInfo(const std::string& model_name) const
{
    // void* return type deprecated - returning nullptr
    return nullptr;
}

bool ModelInterface::modelExists(const std::string& model_name) const
{
    if (!router) return false;
    return router->isModelAvailable(model_name);
}

void ModelInterface::registerModel(const std::string& model_name, const ModelConfig& config)
{
    if (router) {
        router->registerModel(model_name, config);
        // modelListUpdated(getAvailableModels());
    }
}

void ModelInterface::unregisterModel(const std::string& model_name)
{
    if (router) {
        router->unregisterModel(model_name);
        // modelListUpdated(getAvailableModels());
    }
}

std::string ModelInterface::selectBestModel(const std::string& task_type,
                                       const std::string& language,
                                       bool prefer_local)
{
    auto available = getAvailableModels();
    if (available.empty()) return "";
    
    // Prefer local if requested
    if (prefer_local) {
        auto local_models = getLocalModels();
        if (!local_models.empty()) {
            return local_models[0];
        }
    }
    
    // Logic: actually selection based on capability
    for (const auto& model : available) {
        if (task_type == "code_completion" && model.find("coder") != std::string::npos) return model;
        if (task_type == "chat" && model.find("chat") != std::string::npos) return model;
    }
    
    return available[0];
}

std::string ModelInterface::selectCostOptimalModel(const std::string& prompt,
                                              double max_cost_usd)
{
    auto models = getAvailableModels();
    
    // Explicit Logic: Real cost estimation based on character count and model pricing
    std::string bestModel = "";
    double lowestCost = 999999.0;
    
    // Approximate token count
    int tokens = estimateTokenCount(prompt);
    
    for (const auto& model : models) {
        double costPer1k = 0.0;
        
        // Pricing table (hardcoded for simplicity, usually from config)
        if (model.find("gpt-4") != std::string::npos) costPer1k = 0.03;
        else if (model.find("gpt-3.5") != std::string::npos) costPer1k = 0.001;
        else if (model.find("claude-3-opus") != std::string::npos) costPer1k = 0.015;
        else if (model.find("local") != std::string::npos || model.find("phi") != std::string::npos || model.find("mistral") != std::string::npos) {
             costPer1k = 0.0; // Local models are free
        } else {
             costPer1k = 0.01; // Default fallback
        }
        
        double estimatedCost = (tokens / 1000.0) * costPer1k;
        
        if (estimatedCost <= max_cost_usd && estimatedCost < lowestCost) {
            lowestCost = estimatedCost;
            bestModel = model;
        }
        
        // Prefer free local models if they fit cost (0 always fits)
        if (costPer1k == 0.0) {
            return model; 
        }
    }
    
    if (bestModel.empty() && !models.empty()) return models[0]; // Fallback
    return bestModel;
}

std::string ModelInterface::selectFastestModel(const std::string& model_type)
{
    auto models = getAvailableModels();
    if (models.empty()) return "";
    
    // Explicit Logic: Latency table lookup
    // Local models are presumed fastest due to no network RTT, unless larger than Phi/Mistral
    
    for (const auto& m : models) {
        if (m.find("phi-3") != std::string::npos || m.find("tiny") != std::string::npos) return m;
    }
    
    // Check local models
    auto local = getLocalModels();
    if (!local.empty()) return local[0];
    
    return models[0];
}

bool ModelInterface::loadConfig(const std::string& config_file_path)
{
    if (!router) router = std::make_shared<UniversalModelRouter>(this);
    return router->loadConfigFromFile(config_file_path);
}

bool ModelInterface::saveConfig(const std::string& config_file_path) const
{
    if (!router) return false;
    return router->saveConfigToFile(config_file_path);
}

void ModelInterface::setDefaultModel(const std::string& model_name)
{
    if (modelExists(model_name)) default_model = model_name;
}

std::string ModelInterface::getDefaultModel() const
{
    return default_model;
}

void* ModelInterface::getUsageStatistics() const { return nullptr; /* Use getAverageLatency/getSuccessRate/getTotalCost directly */ }

void* ModelInterface::getModelStats(const std::string& model_name) const { return nullptr; /* Use per-model methods directly */ }

double ModelInterface::getAverageLatency(const std::string& model_name) const {
    if (model_name.empty()) {
        // Average across all models
        double total = 0.0; int count = 0;
        for (const auto& [name, s] : stats_map) {
            if (s.call_count > 0) { total += s.total_latency_ms; count += s.call_count; }
        }
        return count > 0 ? total / count : 0.0;
    }
    auto it = stats_map.find(model_name);
    if (it == stats_map.end() || it->second.call_count == 0) return 0.0;
    return it->second.total_latency_ms / it->second.call_count;
}

int ModelInterface::getSuccessRate(const std::string& model_name) const {
    if (model_name.empty()) {
        int total = 0, success = 0;
        for (const auto& [name, s] : stats_map) { total += s.call_count; success += s.success_count; }
        return total > 0 ? (success * 100) / total : 0;
    }
    auto it = stats_map.find(model_name);
    if (it == stats_map.end() || it->second.call_count == 0) return 0;
    return (it->second.success_count * 100) / it->second.call_count;
}

double ModelInterface::getTotalCost() const {
    double total = 0.0;
    for (const auto& [name, s] : stats_map) total += s.total_cost;
    return total;
}

double ModelInterface::getCostByModel(const std::string& model_name) const {
    auto it = stats_map.find(model_name);
    return (it != stats_map.end()) ? it->second.total_cost : 0.0;
}

void* ModelInterface::getCostBreakdown() const { return nullptr; /* Use getCostByModel per model */ }

void ModelInterface::setErrorCallback(std::function<void(const std::string&)> callback)
{
    error_callback = std::move(callback);
}

void ModelInterface::setRetryPolicy(int max_retries_val, int retry_delay_ms_val)
{
    max_retries = max_retries_val;
    retry_delay_ms = retry_delay_ms_val;
}

int ModelInterface::estimateTokenCount(const std::string& text) const
{
    return text.length() / 4;
}

std::string ModelInterface::formatModelList() const
{
    std::string list;
    auto models = getAvailableModels();
    for (const auto& model : models) {
        list += model + "\n";
    }
    return list;
}

void* ModelInterface::getModelListAsJson() const { return nullptr; /* Use formatModelList() or getAvailableModels() */ }

// ============ PRIVATE METHODS ============

// Helper: Check model type
// Implemented locally as it was missing from the class definition in some views
bool ModelInterface::isLocalModel(const std::string& model_name) const {
    // Basic check - assuming router knows.
    // If not, check if it starts with "local-"
    return model_name.find("local") != std::string::npos || model_name.find("gguf") != std::string::npos;
}

ModelConfig ModelInterface::getModelConfigOrThrow(const std::string& model_name) const {
    if (router) return router->getModelConfig(model_name);
    return ModelConfig();
}

GenerationResult ModelInterface::generateInternal(const std::string& prompt,
                                                 const std::string& model_name,
                                                 const GenerationOptions& options)
{
    GenerationResult result;
    result.success = false;
    
    std::string target_model = model_name.empty() ? default_model : model_name;
    if (target_model.empty()) {
        target_model = selectBestModel("chat", "en", false);
    }
    
    if (target_model.empty()) {
        result.error = "No suitable model found";
        return result;
    }
    
    try {
        // Use Universal Router to execute
        std::string text_output = router->routeQuery(target_model, prompt, options.temperature);
        
        // Check for specific error prefix from router
        if (text_output.rfind("Error:", 0) == 0) {
            result.success = false;
            result.error = text_output;
        } else {
            result.success = true;
            result.text = text_output;
            result.model_used = target_model;
            result.finish_reason = "stop"; // Default
        }
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    
    return result;
}

void ModelInterface::generateStreamInternal(const std::string& prompt,
                                           const std::string& model_name,
                                           std::function<void(const std::string&)> on_chunk,
                                           std::function<void(const std::string&)> on_error,
                                           const GenerationOptions& options)
{
    std::string target_model = model_name.empty() ? default_model : model_name;
     if (target_model.empty()) {
        target_model = selectBestModel("chat", "en", false);
    }
    
    if (target_model.empty()) {
        if(on_error) on_error("No suitable model found");
        return;
    }
    
    // Define callback wrapper to handle errors inside stream if possible, 
    // though router usually just streams text.
    auto safe_chunk = [on_chunk, on_error](const std::string& chunk) {
        if (chunk.rfind("Error:", 0) == 0) {
            if(on_error) on_error(chunk);
        } else {
            if(on_chunk) on_chunk(chunk);
        }
    };

    router->routeStreamQuery(target_model, prompt, safe_chunk, options.temperature);
}


