#include "hybrid_cloud_manager.h"
#include "universal_model_router.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <cstdio>
#include <array>

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
        else if (providerId == "local") cfg.backend = ModelBackend::LOCAL_GGUF; // Explicit local handling
        
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
    
    CloudProvider& p = providers[id];
    
    // Explicit Logic: Structural Validation
    if (p.endpoint.empty()) {
        p.isEnabled = false;
        return;
    }
    
    // Explicit Logic: Credential Validation
    if (id != "ollama" && id != "local-default" && p.apiKey.empty()) {
        p.isEnabled = false;
        return;
    }

    // Explicit Logic: Connectivity Check (Real)
    // Uses system curl to verify endpoint reachability via HEAD request
    std::string cmd = "curl --head --silent --fail --connect-timeout 1 \"" + p.endpoint + "\"";
    
    // If output is generated, the endpoint responded (curl --fail silent suppressed error output)
    bool reachable = true; // Default to true to allow startup if curl fails to run
    
    // Only perform check if it looks like a URL
    if (p.endpoint.find("http") == 0) {
        reachable = false;
        std::array<char, 128> buffer;
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
        if (pipe) {
            if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                reachable = true;
            }
        }
    } else if (p.endpoint.find("/") != std::string::npos || p.endpoint.find("\\") != std::string::npos) {
         // File path (local model)
         // Check file existence
         FILE* f = fopen(p.endpoint.c_str(), "rb");
         if (f) {
             fclose(f);
             reachable = true;
         } else {
             reachable = false;
         }
    }
    
    p.isEnabled = reachable;
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
    // plan.requestId assigned later or unused
    
    if (type == "auto" || type.empty()) {
        plan.useCloud = shouldUseCloudExecution(req);
    } else {
        plan.useCloud = (type == "cloud");
    }
    
    if (plan.useCloud) {
        std::string prov = selectOptimalProvider(req);
        plan.selectedProvider = prov;
        CloudModel m = selectBestCloudModel(req);
        plan.selectedModel = m.modelId.empty() ? "gpt-4" : m.modelId;
        plan.estimatedCost = calculateExecutionCost(plan.selectedProvider, plan.selectedModel, req.prompt.length() / 4);
    } else {
        plan.selectedProvider = "local";
        plan.selectedModel = "local-default";
        plan.estimatedCost = 0.0;
    }
    
    return plan;
}
std::string HybridCloudManager::selectOptimalProvider(const ExecutionRequest& req) {
    // 1. Check constraints or preferences
    if (preferLocal && req.taskType != "complex_reasoning" && req.prompt.length() < 8000) {
        return "ollama";
    }

    // 2. Select based on task type
    if (req.taskType == "code_completion") return "ollama"; 
    
    if (req.taskType == "complex_reasoning" || req.taskType == "architecture_design") {
        if (providers.count("anthropic") && providers.at("anthropic").isEnabled) return "anthropic";
        if (providers.count("openai") && providers.at("openai").isEnabled) return "openai";
    }
    
    if (req.taskType == "code_generation") {
         // GPT-4 is strong for generation
         if (providers.count("openai") && providers.at("openai").isEnabled) return "openai";
         if (providers.count("anthropic") && providers.at("anthropic").isEnabled) return "anthropic";
    }

    // 3. Fallback
    if (req.prompt.length() > 32000) return "anthropic"; // Handle large context
    
    return "ollama"; // Default
}

double HybridCloudManager::calculateExecutionCost(const std::string& p, const std::string& m, int tokens) {
    double pricePer1k = 0.0;
    
    if (p == "openai") {
        if (m.find("gpt-4") != std::string::npos) pricePer1k = 0.03;
        else if (m.find("gpt-3.5") != std::string::npos) pricePer1k = 0.001;
        else pricePer1k = 0.01; // default assumption
    } else if (p == "anthropic") {
        if (m.find("opus") != std::string::npos) pricePer1k = 0.015;
        else if (m.find("sonnet") != std::string::npos) pricePer1k = 0.003;
        else if (m.find("haiku") != std::string::npos) pricePer1k = 0.00025;
    } else if (p == "azure") {
        pricePer1k = 0.002; // Approximation
    }
    
    // Check if model override exists in router config
    if (router && router->isModelAvailable(m)) {
        // Could look up specific cost metadata if router supported it
    }
    
    return (tokens / 1000.0) * pricePer1k;
}

void HybridCloudManager::recordExecution(const ExecutionResult& res) {
    executionHistory.push_back(res);
    // Keep history manageable
    if (executionHistory.size() > 1000) {
        executionHistory.erase(executionHistory.begin());
    }
    
    updateCostMetrics(res);
    updatePerformanceMetrics(res);
    
    if (res.success && !res.executionLocation.empty() && res.executionLocation != "local") {
        recordProviderLatency(res.executionLocation, res.latencyMs);
    } else if (!res.success && !res.executionLocation.empty() && res.executionLocation != "local") {
        recordProviderFailure(res.executionLocation);
    }
}

void HybridCloudManager::enableFailover(bool e) { failoverConfig.enabled = e; }
void HybridCloudManager::setFailoverConfig(const FailoverConfig& c) { failoverConfig = c; }

ExecutionResult HybridCloudManager::executeWithFailover(const ExecutionRequest& req) {
    ExecutionResult res = execute(req);
    
    if (!res.success && failoverConfig.enabled) {
        if (retryWithFallback(req, res)) {
            // Updated result is in res
        }
    }
    return res;
}

bool HybridCloudManager::retryWithFallback(const ExecutionRequest& req, ExecutionResult& res) {
    std::string currentProvider = res.executionLocation;
    int attempts = 0;
    
    while (attempts < failoverConfig.maxRetries) {
        std::string nextProvider = getNextFallbackProvider(currentProvider);
        if (nextProvider.empty()) {
            if (failoverConfig.fallbackToLocal && currentProvider != "local") {
                nextProvider = "local";
            } else {
                break; // No more providers
            }
        }
        
        if (nextProvider == "local") {
            res = executeLocal(req);
        } else {
            // Find a model for this provider?
            std::vector<CloudModel> models = getModelsForProvider(nextProvider);
            std::string modelId = models.empty() ? "" : models[0].modelId;
            res = executeCloud(req, nextProvider, modelId);
        }
        
        if (res.success) return true;
        
        currentProvider = nextProvider;
        attempts++;
    }
    
    return false;
}

CostMetrics HybridCloudManager::getCostMetrics() const { return costMetrics; }
double HybridCloudManager::getTodayCost() const { return costMetrics.todayCostUSD; }
double HybridCloudManager::getMonthCost() const { return costMetrics.monthCostUSD; }
double HybridCloudManager::getTotalCost() const { return costMetrics.totalCostUSD; }

void HybridCloudManager::setCostLimit(double d, double m) {
    dailyCostLimit = d;
    monthlyCostLimit = m;
}

void HybridCloudManager::setCostThreshold(double t) { costThresholdUSD = t; }

bool HybridCloudManager::isWithinCostLimits() const {
    if (costMetrics.todayCostUSD >= dailyCostLimit) return false;
    if (costMetrics.monthCostUSD >= monthlyCostLimit) return false;
    return true;
}

void HybridCloudManager::resetCostMetrics() {
    costMetrics = CostMetrics();
    // Persist reset if needed
}

PerformanceMetrics HybridCloudManager::getPerformanceMetrics() const {
    return performanceMetrics;
}

double HybridCloudManager::getAverageLatency(const std::string& p) const {
    if (p.empty()) return performanceMetrics.averageLatency;
    if (performanceMetrics.latencyByProvider.count(p)) return performanceMetrics.latencyByProvider.at(p);
    return 0.0;
}

int HybridCloudManager::getSuccessRate() const { return performanceMetrics.successRate; }

std::vector<ExecutionResult> HybridCloudManager::getExecutionHistory(int l) const {
    if (l <= 0 || l >= executionHistory.size()) return executionHistory;
    return std::vector<ExecutionResult>(executionHistory.end() - l, executionHistory.end());
}

void HybridCloudManager::clearExecutionHistory() { executionHistory.clear(); }

void HybridCloudManager::setPreferLocal(bool p) { preferLocal = p; }
void HybridCloudManager::setCloudFallbackEnabled(bool e) { cloudFallbackEnabled = e; }
void HybridCloudManager::setCostThresholdPerRequest(double t) { costThresholdPerRequest = t; }
void HybridCloudManager::setLatencyThreshold(int t) { latencyThreshold = t; }
void HybridCloudManager::setAutoScaling(bool e) { autoScalingEnabled = e; }
void HybridCloudManager::enableLocalExecution(bool e) { localExecutionEnabled = e; }
void HybridCloudManager::setHealthCheckInterval(int ms) { healthCheckIntervalMs = ms; }
void HybridCloudManager::setMaxRetries(int r) { maxRetries = r; }

ExecutionResult HybridCloudManager::executeOnCloud(const ExecutionRequest& request, 
                                   const std::string& providerId,
                                   const std::string& modelId) {
    return executeCloud(request, providerId, modelId);
}

// --- Private Implementation ---

void HybridCloudManager::updateCostMetrics(const ExecutionResult& result) {
    double cost = result.cost;
    costMetrics.totalCostUSD += cost;
    costMetrics.todayCostUSD += cost; // Simplify: Assuming reset logic elsewhere or monotonic
    costMetrics.monthCostUSD += cost;
    costMetrics.totalRequests++;
    
    if (result.executionLocation == "local") {
        costMetrics.localRequests++;
    } else {
        costMetrics.cloudRequests++;
        costMetrics.costByProvider[result.executionLocation] += cost;
        costMetrics.requestsByProvider[result.executionLocation]++;
    }
}

void HybridCloudManager::updatePerformanceMetrics(const ExecutionResult& result) {
    // Basic moving average
    double n = (double)costMetrics.totalRequests; // approx
    if (n > 0) {
        performanceMetrics.averageLatency = (performanceMetrics.averageLatency * (n - 1) + result.latencyMs) / n;
    } else {
        performanceMetrics.averageLatency = result.latencyMs;
    }
    
    // Update success rate
    static int successes = 0;
    static int totals = 0;
    totals++;
    if (result.success) successes++;
    performanceMetrics.successRate = (int)((double)successes / totals * 100.0);
}

void HybridCloudManager::recordProviderLatency(const std::string& providerId, double latencyMs) {
    if (providers.count(providerId)) {
        CloudProvider& p = providers[providerId];
        // Exponential moving average for smoothness
        p.averageLatency = p.averageLatency * 0.8 + latencyMs * 0.2;
        performanceMetrics.latencyByProvider[providerId] = p.averageLatency;
    }
}

void HybridCloudManager::recordProviderFailure(const std::string& providerId) {
    // Could track failure counts per provider
    performanceMetrics.failoverCount++; // Increment global failover count as approximation of trouble
}

std::string HybridCloudManager::getNextFallbackProvider(const std::string& current) {
    // Simple round-robin or priority based on config
    if (!failoverConfig.providerPriority.empty()) {
        for (const auto& p : failoverConfig.providerPriority) {
            if (p != current && isProviderHealthy(p)) return p;
        }
    }
    
    // Fallback to any healthy cloud provider
    for (const auto& kv : providers) {
        if (kv.first != current && kv.second.isEnabled && kv.first != "local" && kv.first != "ollama") { // Valid cloud
             return kv.first;
        }
    }
    return "";
}

void HybridCloudManager::recordCost(const std::string& providerId, double cost) {}
void HybridCloudManager::recordSuccess(bool success) {}
void HybridCloudManager::recordLatency(double latencyMs) {}


