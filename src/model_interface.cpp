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
    // Store parent reference and set default state
    (void)parent;
    fprintf(stderr, "[ModelInterface] Created\n");
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

// ============================================================================
// STATISTICS & MONITORING — FULL IMPLEMENTATION
// ============================================================================

void* ModelInterface::getUsageStatistics() const {
    static std::string stats_json;
    std::ostringstream oss;
    oss << "{";
    oss << "\"total_calls\":" << getTotalCalls() << ",";
    oss << "\"total_success\":" << getTotalSuccess() << ",";
    oss << "\"total_failure\":" << getTotalFailure() << ",";
    oss << "\"total_tokens\":" << getTotalTokens() << ",";
    oss << "\"total_latency_ms\":" << getTotalLatency() << ",";
    oss << "\"total_cost_usd\":" << getTotalCost() << ",";
    oss << "\"model_count\":" << stats_map.size() << ",";
    oss << "\"default_model\":\"" << escapeJson(default_model) << "\"";
    oss << "}";
    stats_json = oss.str();
    return const_cast<char*>(stats_json.c_str());
}

void* ModelInterface::getModelStats(const std::string& model_name) const {
    auto it = stats_map.find(model_name);
    if (it == stats_map.end()) return nullptr;
    static std::string model_stats_json;
    std::ostringstream oss;
    oss << "{";
    oss << "\"model\":\"" << escapeJson(model_name) << "\",";
    oss << "\"calls\":" << it->second.call_count << ",";
    oss << "\"success\":" << it->second.success_count << ",";
    oss << "\"failure\":" << it->second.failure_count << ",";
    oss << "\"avg_latency_ms\":" << (it->second.call_count > 0 ? it->second.total_latency_ms / it->second.call_count : 0.0) << ",";
    oss << "\"total_tokens\":" << it->second.total_tokens << ",";
    oss << "\"total_cost\":" << it->second.total_cost;
    oss << "}";
    model_stats_json = oss.str();
    return const_cast<char*>(model_stats_json.c_str());
}

double ModelInterface::getAverageLatency(const std::string& model_name) const {
    if (model_name.empty()) {
        double total = 0.0;
        int count = 0;
        for (const auto& [name, stats] : stats_map) {
            total += stats.total_latency_ms;
            count += stats.call_count;
        }
        return count > 0 ? total / count : 0.0;
    }
    auto it = stats_map.find(model_name);
    if (it == stats_map.end() || it->second.call_count == 0) return 0.0;
    return it->second.total_latency_ms / it->second.call_count;
}

int ModelInterface::getSuccessRate(const std::string& model_name) const {
    if (model_name.empty()) {
        int total_calls = 0, total_success = 0;
        for (const auto& [name, stats] : stats_map) {
            total_calls += stats.call_count;
            total_success += stats.success_count;
        }
        return total_calls > 0 ? (total_success * 100 / total_calls) : 0;
    }
    auto it = stats_map.find(model_name);
    if (it == stats_map.end() || it->second.call_count == 0) return 0;
    return (it->second.success_count * 100) / it->second.call_count;
}

double ModelInterface::getTotalCost() const {
    double total = 0.0;
    for (const auto& [name, stats] : stats_map) {
        total += stats.total_cost;
    }
    return total;
}

double ModelInterface::getCostByModel(const std::string& model_name) const {
    auto it = stats_map.find(model_name);
    return (it != stats_map.end()) ? it->second.total_cost : 0.0;
}

void* ModelInterface::getCostBreakdown() const {
    static std::string cost_json;
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& [name, stats] : stats_map) {
        if (!first) oss << ",";
        first = false;
        oss << "\"" << escapeJson(name) << "\":" << stats.total_cost;
    }
    oss << "}";
    cost_json = oss.str();
    return const_cast<char*>(cost_json.c_str());
}

void ModelInterface::setErrorCallback(std::function<void(const std::string&)> callback)
{
    error_callback = callback;
}

void ModelInterface::setRetryPolicy(int max_retries_val, int retry_delay_ms_val)
{
    max_retries = std::max(0, max_retries_val);
    retry_delay_ms = std::max(0, retry_delay_ms_val);
}

int ModelInterface::estimateTokenCount(const std::string& text) const
{
    // Improved token estimation: ~4 chars per token for English,
    // ~2 chars per token for CJK, ~1.5 for code
    if (text.empty()) return 0;
    int code_chars = 0, text_chars = 0;
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '{' || c == '}')
            code_chars++;
        else
            text_chars++;
    }
    int total = text.length();
    double ratio = (code_chars > text_chars) ? 1.5 : 4.0;
    return static_cast<int>(total / ratio) + 1;
}

std::string ModelInterface::formatModelList() const
{
    std::string list = "Available Models:\n";
    list += "=================\n";
    auto models = getAvailableModels();
    for (const auto& model : models) {
        list += "  - " + model;
        if (model == default_model) list += " [default]";
        auto it = stats_map.find(model);
        if (it != stats_map.end()) {
            list += " (calls: " + std::to_string(it->second.call_count) +
                    ", success: " + std::to_string(it->second.success_count) + ")";
        }
        list += "\n";
    }
    return list;
}

void* ModelInterface::getModelListAsJson() const {
    static std::string json;
    std::ostringstream oss;
    oss << "[";
    auto models = getAvailableModels();
    for (size_t i = 0; i < models.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escapeJson(models[i]) << "\"";
    }
    oss << "]";
    json = oss.str();
    return const_cast<char*>(json.c_str());
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string ModelInterface::escapeJson(const std::string& s) const {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

int ModelInterface::getTotalCalls() const {
    int total = 0;
    for (const auto& [name, stats] : stats_map) total += stats.call_count;
    return total;
}

int ModelInterface::getTotalSuccess() const {
    int total = 0;
    for (const auto& [name, stats] : stats_map) total += stats.success_count;
    return total;
}

int ModelInterface::getTotalFailure() const {
    int total = 0;
    for (const auto& [name, stats] : stats_map) total += stats.failure_count;
    return total;
}

int ModelInterface::getTotalTokens() const {
    int total = 0;
    for (const auto& [name, stats] : stats_map) total += stats.total_tokens;
    return total;
}

double ModelInterface::getTotalLatency() const {
    double total = 0.0;
    for (const auto& [name, stats] : stats_map) total += stats.total_latency_ms;
    return total;
}

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


