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
    
    return available[0];
}

std::string ModelInterface::selectCostOptimalModel(const std::string& prompt,
                                              double max_cost_usd)
{
    auto models = getAvailableModels();
    // For now, just return first available
    if (!models.empty()) return models[0];
    return "";
}

std::string ModelInterface::selectFastestModel(const std::string& model_type)
{
    auto models = getAvailableModels();
    if (models.empty()) return "";
    
    // Simple heuristic: local models are usually faster latency-wise if loaded
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

void* ModelInterface::getUsageStatistics() const { return nullptr; }
void* ModelInterface::getModelStats(const std::string& model_name) const { return nullptr; }
double ModelInterface::getAverageLatency(const std::string& model_name) const { return 0.0; }
int ModelInterface::getSuccessRate(const std::string& model_name) const { return 0; }
double ModelInterface::getTotalCost() const { return 0.0; }
double ModelInterface::getCostByModel(const std::string& model_name) const { return 0.0; }
void* ModelInterface::getCostBreakdown() const { return nullptr; }

void ModelInterface::setErrorCallback(std::function<void(const std::string&)> callback)
{
    // error_callback = callback; // Needs member variable
}

void ModelInterface::setRetryPolicy(int max_retries, int retry_delay_ms)
{
    // Storing in member variables assumed to exist in header but strictly private
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

void* ModelInterface::getModelListAsJson() const { return nullptr; }

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
    result.model_name = model_name;
    
    // Check if local model
    if (isLocalModel(model_name)) {
        if (!local_engine) {
             result.success = false;
             result.error = "Local engine not initialized";
             return result;
        }
        
        // Use local engine - Tokenize -> Generate -> Detokenize
        auto tokens = local_engine->Tokenize(prompt);
        // Assuming Generate takes tokens and returns tokens (std::vector<int32_t>)
        auto output_tokens = local_engine->Generate(tokens, options.max_tokens); 
        std::string text = local_engine->Detokenize(output_tokens);
        
        result.success = true;
        result.content = text;
        result.backend = "local-cpu";
        return result;
    }
    
    if (cloud_client) {
        return cloud_client->generate(prompt, model_name, options);
    }
    
    result.success = false;
    result.error = "No backend available";
    return result;
}

void ModelInterface::generateStreamInternal(const std::string& prompt,
                                           const std::string& model_name,
                                           std::function<void(const std::string&)> on_chunk,
                                           std::function<void(const std::string&)> on_error,
                                           const GenerationOptions& options)
{
    if (isLocalModel(model_name)) {
        if (local_engine) {
            auto tokens = local_engine->Tokenize(prompt);
            local_engine->GenerateStreaming(tokens, options.max_tokens, 
                [on_chunk](const std::string& text) {
                    if (on_chunk) on_chunk(text);
                },
                []() { /* complete */ }
            );
        } else {
            if (on_error) on_error("Local engine not initialized");
        }
        return;
    }

    if (cloud_client) {
        cloud_client->generateStream(prompt, model_name, on_chunk, on_error, options);
    } else {
        if (on_error) on_error("No backend available");
    }
}


