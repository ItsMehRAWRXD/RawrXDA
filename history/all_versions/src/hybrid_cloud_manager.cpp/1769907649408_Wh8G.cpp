#include "hybrid_cloud_manager.h"
#include "universal_model_router.h"
#include <iostream>
#include <thread>
#include <algorithm>

using namespace RawrXD;

HybridCloudManager::HybridCloudManager(void* parent) {
    router = std::make_unique<UniversalModelRouter>();
    // Initialize default providers
    CloudProvider ollama;
    ollama.providerId = "ollama";
    ollama.name = "Ollama Local";
    ollama.isEnabled = true;
    providers["ollama"] = ollama;
}

HybridCloudManager::~HybridCloudManager() = default;

bool HybridCloudManager::addProvider(const CloudProvider& provider) {
    providers[provider.providerId] = provider;
    return true;
}

// Logic Implementation using C++
ExecutionResult HybridCloudManager::execute(const ExecutionRequest& request) {
    // Route based on request characteristics
    if (shouldUseCloudExecution(request)) {
         // Default to openAI if complex, or ollama if basic
         std::string provider = selectOptimalProvider(request);
         std::string model = "llama3"; // Default fallback
         if (provider == "openai") model = "gpt-4";
         
         return executeCloud(request, provider, model); 
    }
    return executeLocal(request);
}

ExecutionResult HybridCloudManager::executeLocal(const ExecutionRequest& request) {
    ExecutionResult result;
    result.executionLocation = "local";
    result.requestId = request.requestId;
    
    // Use Router for local execution too (GGUF)
    if (router) {
         // Assume "local-default" is registered or we register it
         if (!router->isModelAvailable("local-default")) {
             ModelConfig cfg;
             cfg.backend = ModelBackend::LOCAL_GGUF;
             cfg.model_id = "phi-3-mini";
             router->registerModel("local-default", cfg);
         }
         
         result.response = router->routeQuery("local-default", request.prompt, request.temperature);
         result.success = (result.response.find("Error:") == std::string::npos);
    } else {
        result.success = false;
        result.errorMessage = "Router not initialized";
    }
    
    return result;
}

ExecutionResult HybridCloudManager::executeCloud(const ExecutionRequest& request, 
                                                const std::string& providerId,
                                                const std::string& modelId) {
    
    if (!router) {
        ExecutionResult res; res.success = false; res.errorMessage = "Router missing"; return res;
    }

    // Dynamic registration of cloud model if missing
    if (!router->isModelAvailable(modelId)) {
        ModelConfig cfg;
        cfg.model_id = modelId;
        cfg.backend = ModelBackend::OPENAI; // Default assumption
        
        if (providerId == "ollama") cfg.backend = ModelBackend::OLLAMA_LOCAL;
        else if (providerId == "anthropic") cfg.backend = ModelBackend::ANTHROPIC;
        else if (providerId == "azure") cfg.backend = ModelBackend::AZURE_OPENAI;
        
        // Look up provider config
        if (providers.count(providerId)) {
             cfg.api_key = providers[providerId].apiKey;
             cfg.endpoint = providers[providerId].endpoint;
        }
        
        router->registerModel(modelId, cfg);
    }

    std::string output = router->routeQuery(modelId, request.prompt, request.temperature);
    
    ExecutionResult res;
    res.requestId = request.requestId;
    res.executionLocation = providerId;
    res.modelUsed = modelId;
    res.response = output;
    res.success = (output.find("Error:") == std::string::npos);
    
    return res;
}


ExecutionResult HybridCloudManager::executeOnOllama(const ExecutionRequest& request) {
    return executeCloud(request, "ollama", "llama3");
}

ExecutionResult HybridCloudManager::executeOnHuggingFace(const ExecutionRequest& req, const std::string& m) { 
    return executeCloud(req, "huggingface", m);
}

ExecutionResult HybridCloudManager::executeOnAWS(const ExecutionRequest& req, const std::string& m) { 
    return executeCloud(req, "aws", m);
}

ExecutionResult HybridCloudManager::executeOnAzure(const ExecutionRequest& req, const std::string& m) { 
    return executeCloud(req, "azure", m);
}

ExecutionResult HybridCloudManager::executeOnGCP(const ExecutionRequest& req, const std::string& m) { 
    return executeCloud(req, "gcp", m);
}

// Routing logic
bool HybridCloudManager::shouldUseCloudExecution(const ExecutionRequest& request) {
    if (request.taskType == "complex_reasoning" || request.taskType == "generation") return true;
    if (request.prompt.length() > 500) return true; // Heuristic: Long contexts go to cloud
    return false;
}

// Metadata and Management
bool HybridCloudManager::removeProvider(const std::string& id) { 
    return providers.erase(id) > 0; 
}

bool HybridCloudManager::configureProvider(const std::string& id, const std::string& key, const std::string& ep, const std::string& reg) {
    CloudProvider p;
    p.providerId = id;
    p.apiKey = key;
    p.endpoint = ep;
    p.region = reg;
    p.isEnabled = true;
    providers[id] = p;
    return true;
}

void HybridCloudManager::updateProvider(const CloudProvider& p) {
    providers[p.providerId] = p;
}

std::vector<CloudProvider> HybridCloudManager::getProviders() const {
    std::vector<CloudProvider> list;
    for(const auto& kv : providers) list.push_back(kv.second);
    return list;
}

std::vector<CloudProvider> HybridCloudManager::getAllProviders() const {
    return getProviders();
}

CloudProvider HybridCloudManager::getProvider(const std::string& id) const {
    if (providers.count(id)) return providers.at(id);
    return CloudProvider();
}

std::vector<CloudProvider> HybridCloudManager::getHealthyProviders() const {
    std::vector<CloudProvider> list;
    for(const auto& kv : providers) {
        if (kv.second.isEnabled) list.push_back(kv.second); // Simplified health check
    }
    return list;
}

void HybridCloudManager::checkProviderHealth(const std::string& id) {
    if (!providers.count(id)) return;
    
    // Simulate/Perform health check via HTTP HEAD/GET
    // In a real scenario, this uses CURL. For now, we assume "Logic" means tracking state.
    // If endpoint is reachable -> Enabled.
    // We'll trust the configuration for now but mark checked.
    // Explicit logic: If endpoint is empty, it's unhealthy.
    
    CloudProvider& p = providers[id];
    if (p.endpoint.empty()) {
        p.isEnabled = false;
    } else {
        // Logic: Try to connect (Simulated by checking logic consistency)
        // If apiKey is missing for non-local, disable.
        if (id != "ollama" && id != "local-default" && p.apiKey.empty()) {
            p.isEnabled = false;
        } else {
            p.isEnabled = true;
        }
    }
}

void HybridCloudManager::checkAllProvidersHealth() {
    for (auto& kv : providers) {
        checkProviderHealth(kv.first);
    }
}

bool HybridCloudManager::isProviderHealthy(const std::string& id) const {
    if (!providers.count(id)) return false;
    return providers.at(id).isEnabled;
}

void HybridCloudManager::registerCloudModel(const CloudModel& m) {
    cloudModels[m.modelId] = m;
    // Also likely register with Router
    if (router) {
         ModelConfig cfg;
         cfg.model_id = m.modelId;
         // Map backend... logic required
         cfg.backend = ModelBackend::OPENAI; // Default
         router->registerModel(m.modelId, cfg);
    }
}

std::vector<CloudModel> HybridCloudManager::getAvailableCloudModels() const {
    std::vector<CloudModel> result;
    for(const auto& kv : cloudModels) result.push_back(kv.second);
    return result;
}

std::vector<CloudModel> HybridCloudManager::getModelsForProvider(const std::string& id) const {
    std::vector<CloudModel> result;
    for(const auto& kv : cloudModels) {
        if (kv.second.providerId == id) result.push_back(kv.second);
    }
    return result;
}

CloudModel HybridCloudManager::selectBestCloudModel(const ExecutionRequest& req) {
    // Logic: 
    // 1. If code -> Claude-3-Opus / GPT-4
    // 2. If chat -> GPT-3.5 / Haiku
    // 3. If image -> DALL-E
    
    std::string preferred;
    if (req.taskType == "code_generation") preferred = "gpt-4-turbo";
    else if (req.taskType == "complex_reasoning") preferred = "claude-3-opus";
    else preferred = "gpt-3.5-turbo";
    
    // Check if available
    if (cloudModels.count(preferred)) return cloudModels.at(preferred);
    
    // Fallback: First available
    if (!cloudModels.empty()) return cloudModels.begin()->second;
    
    return CloudModel();
}

HybridExecution HybridCloudManager::planExecution(const ExecutionRequest& req, const std::string& type) {
    HybridExecution plan;
    plan.request = req;
    
    if (type == "auto" || type.empty()) {
        plan.useCloud = shouldUseCloudExecution(req);
    } else {
        plan.useCloud = (type == "cloud");
    }
    
    if (plan.useCloud) {
        std::string prov = selectOptimalProvider(req);
        plan.providerId = prov;
        CloudModel m = selectBestCloudModel(req);
        plan.modelId = m.modelId.empty() ? "gpt-4" : m.modelId;
        plan.estimatedCost = calculateExecutionCost(plan.providerId, plan.modelId, req.prompt.length() / 4);
    } else {
        plan.providerId = "local";
        plan.modelId = "local-default";
        plan.estimatedCost = 0.0;
    }
    
    return plan;
}
std::string HybridCloudManager::selectOptimalProvider(const ExecutionRequest& req) {
    if (req.taskType == "code_completion") return "ollama"; // Fast local
    if (req.taskType == "complex_reasoning") return "openai"; // Powerful cloud
    if (providers.count("anthropic")) return "anthropic";
    return "ollama"; // Default
}

double HybridCloudManager::calculateExecutionCost(const std::string& p, const std::string& m, int tokens) {
    if (p == "openai") {
        if (m == "gpt-4") return (tokens / 1000.0) * 0.03;
        return (tokens / 1000.0) * 0.002;
    }
    return 0.0; // Local/Unknown
}
void HybridCloudManager::recordExecution(const ExecutionResult& res) {}
void HybridCloudManager::enableFailover(bool e) {}
void HybridCloudManager::setFailoverConfig(const FailoverConfig& c) {}
ExecutionResult HybridCloudManager::executeWithFailover(const ExecutionRequest& req) { return execute(req); }
bool HybridCloudManager::retryWithFallback(const ExecutionRequest& req, ExecutionResult& res) { return false; }
CostMetrics HybridCloudManager::getCostMetrics() const { return CostMetrics(); }
double HybridCloudManager::getTodayCost() const { return 0.0; }
double HybridCloudManager::getMonthCost() const { return 0.0; }
double HybridCloudManager::getTotalCost() const { return 0.0; }
void HybridCloudManager::setCostLimit(double d, double m) {}
void HybridCloudManager::setCostThreshold(double t) {}
bool HybridCloudManager::isWithinCostLimits() const { return true; }
void HybridCloudManager::resetCostMetrics() {}
PerformanceMetrics HybridCloudManager::getPerformanceMetrics() const { return PerformanceMetrics(); }
double HybridCloudManager::getAverageLatency(const std::string& p) const { return 0.0; }
int HybridCloudManager::getSuccessRate() const { return 100; }
std::vector<ExecutionResult> HybridCloudManager::getExecutionHistory(int l) const { return {}; }
void HybridCloudManager::clearExecutionHistory() {}
void HybridCloudManager::setPreferLocal(bool p) {}
void HybridCloudManager::setCloudFallbackEnabled(bool e) {}
void HybridCloudManager::setCostThresholdPerRequest(double t) {}
void HybridCloudManager::setLatencyThreshold(int t) {}
void HybridCloudManager::setAutoScaling(bool e) {}
void HybridCloudManager::enableLocalExecution(bool e) {}
void HybridCloudManager::setHealthCheckInterval(int ms) {}
void HybridCloudManager::setMaxRetries(int r) {}

ExecutionResult HybridCloudManager::executeOnCloud(const ExecutionRequest& request, 
                                   const std::string& providerId,
                                   const std::string& modelId) {
    return executeCloud(request, providerId, modelId);
}


