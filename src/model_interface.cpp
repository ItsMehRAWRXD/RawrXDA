// model_interface.cpp - Implementation of Unified Model Interface
#include "model_interface.h"
#include "universal_model_router.h"
#include "cloud_api_client.h"


#include <algorithm>

ModelInterface::ModelInterface(void* parent)
    : void(parent),
      default_model(""),
      initialized_flag(false)
{
    // Initialization deferred to initialize() method
}

ModelInterface::~ModelInterface() = default;

void ModelInterface::initialize(const std::string& config_file_path)
{
    router = std::make_shared<UniversalModelRouter>(this);
    cloud_client = std::make_shared<CloudApiClient>(this);
    
    if (router->loadConfigFromFile(config_file_path)) {
        initialized_flag = true;
        initialized();
    }
}

void ModelInterface::initializeWithRouter(std::shared_ptr<UniversalModelRouter> provided_router)
{
    router = provided_router;
    cloud_client = std::make_shared<CloudApiClient>(this);
    initialized_flag = (router != nullptr);
    
    if (initialized_flag) {
        initialized();
    }
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
    // In a full implementation, this would be truly async with threading
    // For now, we use a deferred callback
    auto result = generate(prompt, model_name, options);
    
    void*::singleShot(0, this, [this, result, callback]() {
        callback(result);
    });
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
        results.append(generate(prompt, model_name, options));
    }
    
    return results;
}

void ModelInterface::generateBatchAsync(const std::vector<std::string>& prompts,
                                       const std::string& model_name,
                                       std::function<void(const std::vector<GenerationResult>&)> callback,
                                       const GenerationOptions& options)
{
    auto results = generateBatch(prompts, model_name, options);
    
    void*::singleShot(0, this, [this, results, callback]() {
        callback(results);
    });
}

std::vector<std::string> ModelInterface::getAvailableModels() const
{
    if (!router) return std::vector<std::string>();
    return router->getAvailableModels();
}

std::vector<std::string> ModelInterface::getLocalModels() const
{
    if (!router) return std::vector<std::string>();
    return router->getModelsForBackend(ModelBackend::LOCAL_GGUF)
         + router->getModelsForBackend(ModelBackend::OLLAMA_LOCAL);
}

std::vector<std::string> ModelInterface::getCloudModels() const
{
    if (!router) return std::vector<std::string>();
    
    std::vector<std::string> cloud_models;
    cloud_models += router->getModelsForBackend(ModelBackend::ANTHROPIC);
    cloud_models += router->getModelsForBackend(ModelBackend::OPENAI);
    cloud_models += router->getModelsForBackend(ModelBackend::GOOGLE);
    cloud_models += router->getModelsForBackend(ModelBackend::MOONSHOT);
    cloud_models += router->getModelsForBackend(ModelBackend::AZURE_OPENAI);
    cloud_models += router->getModelsForBackend(ModelBackend::AWS_BEDROCK);
    
    return cloud_models;
}

std::string ModelInterface::getModelDescription(const std::string& model_name) const
{
    if (!router) return "";
    return router->getModelDescription(model_name);
}

void* ModelInterface::getModelInfo(const std::string& model_name) const
{
    if (!router) return void*();
    return router->getModelInfo(model_name);
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
        modelListUpdated(getAvailableModels());
    }
}

void ModelInterface::unregisterModel(const std::string& model_name)
{
    if (router) {
        router->unregisterModel(model_name);
        modelListUpdated(getAvailableModels());
    }
}

std::string ModelInterface::selectBestModel(const std::string& task_type,
                                       const std::string& language,
                                       bool prefer_local)
{
    // Smart model selection logic
    auto available = getAvailableModels();
    
    if (available.isEmpty()) {
        return "";
    }
    
    // Prefer local if requested
    if (prefer_local) {
        auto local_models = getLocalModels();
        if (!local_models.isEmpty()) {
            return local_models.first();
        }
    }
    
    // Default to first available
    return available.first();
}

std::string ModelInterface::selectCostOptimalModel(const std::string& prompt,
                                              double max_cost_usd)
{
    // Select model based on estimated cost
    auto models = getAvailableModels();
    
    for (const auto& model : models) {
        auto config = router->getModelConfig(model);
        // Estimate cost based on token count and backend
        // For now, just return first available
        return model;
    }
    
    return "";
}

std::string ModelInterface::selectFastestModel(const std::string& model_type)
{
    // Select model based on latency metrics
    auto models = getAvailableModels();
    
    if (models.isEmpty()) {
        return "";
    }
    
    // Return model with best latency stats
    std::string fastest = models.first();
    double min_latency = getAverageLatency(fastest);
    
    for (const auto& model : models) {
        double latency = getAverageLatency(model);
        if (latency > 0 && latency < min_latency) {
            fastest = model;
            min_latency = latency;
        }
    }
    
    return fastest;
}

bool ModelInterface::loadConfig(const std::string& config_file_path)
{
    if (!router) {
        router = std::make_shared<UniversalModelRouter>(this);
    }
    
    return router->loadConfigFromFile(config_file_path);
}

bool ModelInterface::saveConfig(const std::string& config_file_path) const
{
    if (!router) {
        return false;
    }
    
    return router->saveConfigToFile(config_file_path);
}

void ModelInterface::setDefaultModel(const std::string& model_name)
{
    if (modelExists(model_name)) {
        default_model = model_name;
    }
}

std::string ModelInterface::getDefaultModel() const
{
    return default_model;
}

void* ModelInterface::getUsageStatistics() const
{
    void* stats;
    
    for (const auto& model_name : stats_map.keys()) {
        const auto& model_stats = stats_map[model_name];
        void* model_obj;
        model_obj["calls"] = model_stats.call_count;
        model_obj["successes"] = model_stats.success_count;
        model_obj["failures"] = model_stats.failure_count;
        model_obj["avg_latency_ms"] = model_stats.total_latency_ms / std::max(1, model_stats.call_count);
        model_obj["total_cost"] = model_stats.total_cost;
        model_obj["total_tokens"] = model_stats.total_tokens;
        
        stats[model_name] = model_obj;
    }
    
    return stats;
}

void* ModelInterface::getModelStats(const std::string& model_name) const
{
    void* stats;
    
    if (stats_map.contains(model_name)) {
        const auto& model_stats = stats_map[model_name];
        stats["calls"] = model_stats.call_count;
        stats["successes"] = model_stats.success_count;
        stats["failures"] = model_stats.failure_count;
        stats["avg_latency_ms"] = model_stats.total_latency_ms / std::max(1, model_stats.call_count);
        stats["total_cost"] = model_stats.total_cost;
        stats["total_tokens"] = model_stats.total_tokens;
    }
    
    return stats;
}

double ModelInterface::getAverageLatency(const std::string& model_name) const
{
    if (model_name.isEmpty()) {
        // Return average across all models
        double total_latency = 0.0;
        int total_calls = 0;
        
        for (const auto& model : stats_map.keys()) {
            const auto& stats = stats_map[model];
            total_latency += stats.total_latency_ms;
            total_calls += stats.call_count;
        }
        
        return total_calls > 0 ? total_latency / total_calls : 0.0;
    }
    
    if (stats_map.contains(model_name)) {
        const auto& stats = stats_map[model_name];
        return stats.call_count > 0 ? stats.total_latency_ms / stats.call_count : 0.0;
    }
    
    return 0.0;
}

int ModelInterface::getSuccessRate(const std::string& model_name) const
{
    if (model_name.isEmpty()) {
        // Return success rate across all models
        int total_calls = 0;
        int total_successes = 0;
        
        for (const auto& model : stats_map.keys()) {
            const auto& stats = stats_map[model];
            total_calls += stats.call_count;
            total_successes += stats.success_count;
        }
        
        return total_calls > 0 ? (total_successes * 100) / total_calls : 0;
    }
    
    if (stats_map.contains(model_name)) {
        const auto& stats = stats_map[model_name];
        return stats.call_count > 0 ? (stats.success_count * 100) / stats.call_count : 0;
    }
    
    return 0;
}

double ModelInterface::getTotalCost() const
{
    double total = 0.0;
    
    for (const auto& model : stats_map.keys()) {
        total += stats_map[model].total_cost;
    }
    
    return total;
}

double ModelInterface::getCostByModel(const std::string& model_name) const
{
    if (stats_map.contains(model_name)) {
        return stats_map[model_name].total_cost;
    }
    
    return 0.0;
}

void* ModelInterface::getCostBreakdown() const
{
    void* breakdown;
    
    for (const auto& model : stats_map.keys()) {
        breakdown[model] = stats_map[model].total_cost;
    }
    
    return breakdown;
}

void ModelInterface::setErrorCallback(std::function<void(const std::string&)> callback)
{
    error_callback = callback;
}

void ModelInterface::setRetryPolicy(int max_retries, int retry_delay_ms)
{
    this->max_retries = max_retries;
    this->retry_delay_ms = retry_delay_ms;
}

int ModelInterface::estimateTokenCount(const std::string& text) const
{
    // Simple estimation: ~4 characters per token on average
    return text.length() / 4;
}

std::string ModelInterface::formatModelList() const
{
    std::string list;
    
    auto models = getAvailableModels();
    for (const auto& model : models) {
        auto config = router->getModelConfig(model);
        std::string backend_str = std::string::number(static_cast<int>(config.backend));
        list += std::string("%1 (%2)\n");
    }
    
    return list;
}

void* ModelInterface::getModelListAsJson() const
{
    void* array;
    
    auto models = getAvailableModels();
    for (const auto& model : models) {
        auto info = getModelInfo(model);
        if (!info.isEmpty()) {
            array.append(info);
        }
    }
    
    return array;
}

// ============ PRIVATE METHODS ============

GenerationResult ModelInterface::generateInternal(const std::string& prompt,
                                                 const std::string& model_name,
                                                 const GenerationOptions& options)
{
    GenerationResult result;
    result.model_name = model_name;
    
    auto config = getModelConfigOrThrow(model_name);
    
    if (isLocalModel(model_name)) {
        // Use local engine
        result.content = "Local model generation not yet fully implemented";
        result.backend = "LOCAL";
    } else {
        // Use cloud client
        result.content = cloud_client->generate(prompt, config);
        result.backend = "CLOUD";
    }
    
    result.success = !result.content.isEmpty();
    
    // Update statistics
    if (stats_map.contains(model_name)) {
        stats_map[model_name].call_count++;
        if (result.success) {
            stats_map[model_name].success_count++;
        } else {
            stats_map[model_name].failure_count++;
        }
    } else {
        ModelStats new_stats;
        new_stats.call_count = 1;
        new_stats.success_count = result.success ? 1 : 0;
        new_stats.failure_count = result.success ? 0 : 1;
        stats_map[model_name] = new_stats;
    }
    
    return result;
}

void ModelInterface::generateStreamInternal(const std::string& prompt,
                                           const std::string& model_name,
                                           std::function<void(const std::string&)> on_chunk,
                                           std::function<void(const std::string&)> on_error,
                                           const GenerationOptions& options)
{
    auto config = getModelConfigOrThrow(model_name);
    
    if (isLocalModel(model_name)) {
        // Use local engine streaming
        on_chunk("Local streaming not yet implemented");
    } else {
        // Use cloud client streaming
        cloud_client->generateStream(prompt, config, on_chunk, on_error);
    }
}

bool ModelInterface::isLocalModel(const std::string& model_name) const
{
    if (!router) return false;
    
    auto backend = router->getModelBackend(model_name);
    return backend == ModelBackend::LOCAL_GGUF || backend == ModelBackend::OLLAMA_LOCAL;
}

bool ModelInterface::isCloudModel(const std::string& model_name) const
{
    return !isLocalModel(model_name);
}

ModelConfig ModelInterface::getModelConfigOrThrow(const std::string& model_name) const
{
    if (!router) {
        throw std::runtime_error("Router not initialized");
    }
    
    auto config = router->getModelConfig(model_name);
    if (config.model_id.isEmpty()) {
        throw std::runtime_error(std::string("Model not found: %1").toStdString());
    }
    
    return config;
}

void ModelInterface::onRouterInitialized()
{
    initialized_flag = true;
    initialized();
}

void ModelInterface::onModelRegistered(const std::string& model_name)
{
    modelListUpdated(getAvailableModels());
}

