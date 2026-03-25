#include "hybrid_cloud_manager.h"
#include "universal_model_router.h"
<<<<<<< HEAD

#include <algorithm>
#include <limits>
#include <utility>

HybridCloudManager::HybridCloudManager(void* parent) {
    (void)parent;
    healthCheckIntervalMs = HEALTH_CHECK_INTERVAL_MS;
    failoverConfig.enabled = true;
    failoverConfig.maxRetries = MAX_RETRY_ATTEMPTS;
    failoverConfig.fallbackToLocal = true;
    setupDefaultProviders();
}

HybridCloudManager::~HybridCloudManager() = default;
=======
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
>>>>>>> origin/main

HybridCloudManager::~HybridCloudManager() = default;

bool HybridCloudManager::addProvider(const CloudProvider& provider) {
<<<<<<< HEAD
    if (provider.providerId.empty()) {
        return false;
    }
=======
>>>>>>> origin/main
    providers[provider.providerId] = provider;
    return true;
}

<<<<<<< HEAD
bool HybridCloudManager::removeProvider(const std::string& providerId) { return providers.erase(providerId) > 0; }

bool HybridCloudManager::configureProvider(
    const std::string& providerId, const std::string& apiKey, const std::string& endpoint, const std::string& region) {
    auto it = providers.find(providerId);
    if (it == providers.end()) {
        return false;
    }
    it->second.apiKey = apiKey;
    it->second.endpoint = endpoint;
    it->second.region = region;
    it->second.isEnabled = true;
    return true;
}

void HybridCloudManager::updateProvider(const CloudProvider& provider) {
    if (provider.providerId.empty()) {
        return;
    }
    providers[provider.providerId] = provider;
}

std::vector<CloudProvider> HybridCloudManager::getProviders() const {
    std::vector<CloudProvider> out;
    out.reserve(providers.size());
    for (const auto& [_, p] : providers) {
        out.push_back(p);
    }
    return out;
}

std::vector<CloudProvider> HybridCloudManager::getAllProviders() const { return getProviders(); }

CloudProvider HybridCloudManager::getProvider(const std::string& providerId) const {
    auto it = providers.find(providerId);
    if (it == providers.end()) {
        return {};
    }
    return it->second;
}

std::vector<CloudProvider> HybridCloudManager::getHealthyProviders() const {
    std::vector<CloudProvider> out;
    for (const auto& [_, p] : providers) {
        if (p.isHealthy && p.isEnabled) {
            out.push_back(p);
        }
    }
    return out;
}

void HybridCloudManager::checkProviderHealth(const std::string& providerId) {
    auto it = providers.find(providerId);
    if (it == providers.end()) {
        return;
    }
    it->second.isHealthy = it->second.isEnabled;
    it->second.lastHealthCheck = std::chrono::system_clock::now();
    providerHealthChanged(providerId, it->second.isHealthy);
}

void HybridCloudManager::checkAllProvidersHealth() {
    for (auto& [id, p] : providers) {
        p.isHealthy = p.isEnabled;
        p.lastHealthCheck = std::chrono::system_clock::now();
        providerHealthChanged(id, p.isHealthy);
    }
    healthCheckCompleted();
}

bool HybridCloudManager::isProviderHealthy(const std::string& providerId) const {
    auto it = providers.find(providerId);
    return it != providers.end() && it->second.isHealthy;
}

void HybridCloudManager::registerCloudModel(const CloudModel& model) {
    if (model.modelId.empty()) {
        return;
    }
    cloudModels[model.modelId] = model;
}

std::vector<CloudModel> HybridCloudManager::getAvailableCloudModels() const {
    std::vector<CloudModel> out;
    for (const auto& [_, m] : cloudModels) {
        if (m.isAvailable) {
            out.push_back(m);
        }
    }
    return out;
}

std::vector<CloudModel> HybridCloudManager::getModelsForProvider(const std::string& providerId) const {
    std::vector<CloudModel> out;
    for (const auto& [_, m] : cloudModels) {
        if (m.providerId == providerId && m.isAvailable) {
            out.push_back(m);
        }
    }
    return out;
}

CloudModel HybridCloudManager::selectBestCloudModel(const ExecutionRequest& request) {
    const std::string provider = selectOptimalProvider(request);
    auto models = getModelsForProvider(provider);
    if (models.empty()) {
        return {};
    }
    std::sort(models.begin(), models.end(), [](const CloudModel& a, const CloudModel& b) {
        return a.costPerToken < b.costPerToken;
    });
    return models.front();
}

HybridExecution HybridCloudManager::planExecution(const ExecutionRequest& request, const std::string& executionType) {
    HybridExecution plan;
    plan.requestId = request.requestId;

    if (executionType == "local") {
        plan.useCloud = false;
        plan.reasoning = "Forced local execution";
        plan.confidenceScore = 1.0;
        return plan;
    }
    if (executionType == "cloud") {
        plan.useCloud = true;
        plan.selectedProvider = selectOptimalProvider(request);
    } else {
        plan.useCloud = shouldUseCloudExecution(request);
        if (plan.useCloud) {
            plan.selectedProvider = selectOptimalProvider(request);
        }
    }

    const auto selected = selectBestCloudModel(request);
    plan.selectedModel = selected.modelId;
    plan.estimatedCost =
        plan.useCloud ? calculateExecutionCost(plan.selectedProvider, plan.selectedModel, request.maxTokens) : 0.0;
    plan.estimatedLatency = plan.useCloud ? 750.0 : 40.0;
    plan.confidenceScore = plan.useCloud ? 0.78 : 0.92;
    plan.reasoning = plan.useCloud ? "Cloud selected by routing policy" : "Local preferred by policy/cost";
=======
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
    
>>>>>>> origin/main
    return plan;
}
std::string HybridCloudManager::selectOptimalProvider(const ExecutionRequest& req) {
    // 1. Check constraints or preferences
    if (preferLocal && req.taskType != "complex_reasoning" && req.prompt.length() < 8000) {
        return "ollama";
    }

<<<<<<< HEAD
ExecutionResult HybridCloudManager::execute(const ExecutionRequest& request) {
    executionStarted(request.requestId);
    ExecutionResult result;
    if (failoverConfig.enabled) {
        result = executeWithFailover(request);
    } else {
        const auto plan = planExecution(request);
        if (!plan.useCloud || !cloudFallbackEnabled) {
            result = executeLocal(request);
        } else {
            result = executeCloud(request, plan.selectedProvider, plan.selectedModel);
        }
    }
    recordExecution(result);
    executionComplete(result);
    return result;
}

ExecutionResult HybridCloudManager::executeLocal(const ExecutionRequest& request) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "local";
    result.modelUsed = "minimal-local";
    result.response = request.prompt;
    result.tokensUsed = std::max(1, request.maxTokens / 8);
    result.latencyMs = 35.0;
    result.cost = 0.0;
    result.success = localExecutionEnabled;
    result.errorMessage = result.success ? "" : "Local execution disabled.";
    result.completedAt = std::chrono::system_clock::now();
    result.metadata = nlohmann::json{{"mode", "minimal-local"}};
    return result;
}

ExecutionResult HybridCloudManager::executeCloud(
    const ExecutionRequest& request, const std::string& providerId, const std::string& modelId) {
    return executeOnCloud(request, providerId, modelId);
}

ExecutionResult HybridCloudManager::executeOnCloud(
    const ExecutionRequest& request, const std::string& providerId, const std::string& modelId) {
    if (providerId.empty()) {
        ExecutionResult fallback = executeLocal(request);
        fallback.metadata["cloudFallbackReason"] = "No provider selected";
        return fallback;
    }
    if (providerId == "ollama") return executeOnOllama(request);
    if (providerId == "huggingface") return executeOnHuggingFace(request, modelId);
    if (providerId == "aws") return executeOnAWS(request, modelId);
    if (providerId == "azure") return executeOnAzure(request, modelId);
    if (providerId == "gcp") return executeOnGCP(request, modelId);
    ExecutionResult fallback = executeLocal(request);
    fallback.metadata["cloudFallbackReason"] = "Unknown provider";
    return fallback;
}

ExecutionResult HybridCloudManager::executeOnOllama(const ExecutionRequest& request) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "ollama";
    result.modelUsed = "ollama/minimal";
    result.response = request.prompt;
    result.tokensUsed = std::max(1, request.maxTokens / 6);
    result.latencyMs = 120.0;
    result.cost = 0.0;
    result.success = true;
    result.completedAt = std::chrono::system_clock::now();
    return result;
}

ExecutionResult HybridCloudManager::executeOnHuggingFace(const ExecutionRequest& request, const std::string& modelId) {
    ExecutionResult result = executeLocal(request);
    result.executionLocation = "huggingface";
    result.modelUsed = modelId.empty() ? "hf/minimal" : modelId;
    result.latencyMs = 460.0;
    result.cost = calculateExecutionCost("huggingface", result.modelUsed, request.maxTokens);
    result.metadata["provider"] = "huggingface";
    return result;
}

ExecutionResult HybridCloudManager::executeOnAWS(const ExecutionRequest& request, const std::string& modelId) {
    ExecutionResult result = executeLocal(request);
    result.executionLocation = "aws";
    result.modelUsed = modelId.empty() ? "aws/minimal" : modelId;
    result.latencyMs = 380.0;
    result.cost = calculateExecutionCost("aws", result.modelUsed, request.maxTokens);
    result.metadata["provider"] = "aws";
    return result;
}

ExecutionResult HybridCloudManager::executeOnAzure(const ExecutionRequest& request, const std::string& modelId) {
    ExecutionResult result = executeLocal(request);
    result.executionLocation = "azure";
    result.modelUsed = modelId.empty() ? "azure/minimal" : modelId;
    result.latencyMs = 410.0;
    result.cost = calculateExecutionCost("azure", result.modelUsed, request.maxTokens);
    result.metadata["provider"] = "azure";
    return result;
}

ExecutionResult HybridCloudManager::executeOnGCP(const ExecutionRequest& request, const std::string& modelId) {
    ExecutionResult result = executeLocal(request);
    result.executionLocation = "gcp";
    result.modelUsed = modelId.empty() ? "gcp/minimal" : modelId;
    result.latencyMs = 395.0;
    result.cost = calculateExecutionCost("gcp", result.modelUsed, request.maxTokens);
    result.metadata["provider"] = "gcp";
    return result;
}

bool HybridCloudManager::shouldUseCloudExecution(const ExecutionRequest& request) {
    if (preferLocal || !cloudFallbackEnabled) {
        return false;
    }
    if (!isWithinCostLimits()) {
        return false;
    }
    if (request.maxTokens >= 2048) {
        return true;
    }
    if (request.taskType == "analysis" || request.taskType == "planning") {
        return true;
    }
    return false;
}

std::string HybridCloudManager::selectOptimalProvider(const ExecutionRequest& request) {
    (void)request;
    double bestScore = -std::numeric_limits<double>::infinity();
    std::string bestProvider;
    for (const auto& [id, p] : providers) {
        if (!p.isEnabled || !p.isHealthy) {
            continue;
        }
        const double score = calculateProviderScore(p, request);
        if (score > bestScore) {
            bestScore = score;
            bestProvider = id;
        }
    }
    if (bestProvider.empty() && !providers.empty()) {
        bestProvider = providers.begin()->first;
    }
    return bestProvider;
}

double HybridCloudManager::calculateExecutionCost(
    const std::string& providerId, const std::string& modelId, int estimatedTokens) {
    (void)modelId;
    const int safeTokens = std::max(1, estimatedTokens);
    const auto pIt = providers.find(providerId);
    if (pIt != providers.end() && pIt->second.costPerRequest > 0.0) {
        return pIt->second.costPerRequest;
    }
    return static_cast<double>(safeTokens) * 0.000002;
}

void HybridCloudManager::recordExecution(const ExecutionResult& result) {
    executionHistory.push_back(result);
    updateCostMetrics(result);
    updatePerformanceMetrics(result);
    currentlyUsingCloud = (result.executionLocation != "local");
}

void HybridCloudManager::enableFailover(bool enable) { failoverConfig.enabled = enable; }
void HybridCloudManager::setFailoverConfig(const FailoverConfig& config) { failoverConfig = config; }

ExecutionResult HybridCloudManager::executeWithFailover(const ExecutionRequest& request) {
    const auto plan = planExecution(request);
    if (!plan.useCloud || !cloudFallbackEnabled) {
        return executeLocal(request);
    }

    ExecutionResult result = executeCloud(request, plan.selectedProvider, plan.selectedModel);
    if (result.success) {
        return result;
    }

    for (int attempt = 1; attempt <= failoverConfig.maxRetries; ++attempt) {
        if (!retryWithFallback(request, result)) {
            break;
        }
        if (result.success) {
            break;
        }
        if (!shouldRetry(attempt, result.errorMessage)) {
            break;
        }
    }
    return result;
}

bool HybridCloudManager::retryWithFallback(const ExecutionRequest& request, ExecutionResult& result) {
    if (!failoverConfig.enabled) {
        return false;
    }

    const std::string current = result.executionLocation;
    const std::string next = getNextFallbackProvider(current);
    if (next.empty()) {
        if (failoverConfig.fallbackToLocal) {
            result = executeLocal(request);
            performanceMetrics.failoverCount++;
            failoverTriggered(current, "local");
            return true;
        }
        return false;
    }

    result = executeCloud(request, next, "");
    performanceMetrics.failoverCount++;
    failoverTriggered(current, next);
    return true;
}

CostMetrics HybridCloudManager::getCostMetrics() const { return costMetrics; }
double HybridCloudManager::getTodayCost() const { return costMetrics.todayCostUSD; }
double HybridCloudManager::getMonthCost() const { return costMetrics.monthCostUSD; }
double HybridCloudManager::getTotalCost() const { return costMetrics.totalCostUSD; }
void HybridCloudManager::setCostLimit(double dailyLimitUSD, double monthlyLimitUSD) {
    dailyCostLimit = dailyLimitUSD;
    monthlyCostLimit = monthlyLimitUSD;
}
void HybridCloudManager::setCostThreshold(double thresholdUSD) { costThresholdUSD = thresholdUSD; }
bool HybridCloudManager::isWithinCostLimits() const {
    return costMetrics.todayCostUSD <= dailyCostLimit && costMetrics.monthCostUSD <= monthlyCostLimit;
}
void HybridCloudManager::resetCostMetrics() { costMetrics = {}; }

PerformanceMetrics HybridCloudManager::getPerformanceMetrics() const { return performanceMetrics; }
double HybridCloudManager::getAverageLatency(const std::string& providerId) const {
    if (providerId.empty()) {
        return performanceMetrics.averageLatency;
    }
    auto it = performanceMetrics.latencyByProvider.find(providerId);
    return it == performanceMetrics.latencyByProvider.end() ? 0.0 : it->second;
}
int HybridCloudManager::getSuccessRate() const { return performanceMetrics.successRate; }

std::vector<ExecutionResult> HybridCloudManager::getExecutionHistory(int limit) const {
    if (limit <= 0 || static_cast<size_t>(limit) >= executionHistory.size()) {
        return executionHistory;
    }
    return std::vector<ExecutionResult>(executionHistory.end() - limit, executionHistory.end());
}
void HybridCloudManager::clearExecutionHistory() { executionHistory.clear(); }

void HybridCloudManager::setPreferLocal(bool prefer) { preferLocal = prefer; }
void HybridCloudManager::setCloudFallbackEnabled(bool enabled) { cloudFallbackEnabled = enabled; }
void HybridCloudManager::setCostThresholdPerRequest(double thresholdUSD) { costThresholdPerRequest = thresholdUSD; }
void HybridCloudManager::setLatencyThreshold(int thresholdMs) { latencyThreshold = thresholdMs; }
void HybridCloudManager::setAutoScaling(bool enabled) { autoScalingEnabled = enabled; }
void HybridCloudManager::enableLocalExecution(bool enable) { localExecutionEnabled = enable; }
void HybridCloudManager::setHealthCheckInterval(int milliseconds) { healthCheckIntervalMs = milliseconds; }
void HybridCloudManager::setMaxRetries(int retries) { maxRetries = std::max(1, retries); }

bool HybridCloudManager::switchToCloud(const std::string& reason) {
    (void)reason;
    currentlyUsingCloud = true;
    cloudSwitched(true);
    return true;
}

bool HybridCloudManager::switchToLocal(const std::string& reason) {
    (void)reason;
    currentlyUsingCloud = false;
    cloudSwitched(false);
    return true;
}
bool HybridCloudManager::isUsingCloud() const { return currentlyUsingCloud; }

void HybridCloudManager::setAPIKey(const std::string& providerId, const std::string& apiKey) {
    auto it = providers.find(providerId);
    if (it == providers.end()) {
        return;
    }
    it->second.apiKey = apiKey;
}
void HybridCloudManager::setAWSCredentials(
    const std::string& accessKey, const std::string& secretKey, const std::string& region) {
    auto it = providers.find("aws");
    if (it == providers.end()) return;
    it->second.apiKey = accessKey + ":" + secretKey;
    it->second.region = region;
}
void HybridCloudManager::setAzureCredentials(const std::string& subscriptionId, const std::string& apiKey) {
    auto it = providers.find("azure");
    if (it == providers.end()) return;
    it->second.apiKey = subscriptionId + ":" + apiKey;
}
void HybridCloudManager::setGCPCredentials(const std::string& projectId, const std::string& apiKey) {
    auto it = providers.find("gcp");
    if (it == providers.end()) return;
    it->second.apiKey = projectId + ":" + apiKey;
}
void HybridCloudManager::setHuggingFaceKey(const std::string& apiKey) { setAPIKey("huggingface", apiKey); }

void HybridCloudManager::queueRequest(const ExecutionRequest& request) { requestQueue.push_back(request); }
std::vector<ExecutionRequest> HybridCloudManager::getPendingRequests() const { return requestQueue; }
void HybridCloudManager::processPendingRequests() {
    for (const auto& req : requestQueue) {
        (void)execute(req);
    }
    requestQueue.clear();
}

void HybridCloudManager::executionStarted(const std::string& requestId) { (void)requestId; }
void HybridCloudManager::executionComplete(const ExecutionResult& result) { (void)result; }
void HybridCloudManager::providerHealthChanged(const std::string& providerId, bool isHealthy) {
    (void)providerId;
    (void)isHealthy;
}
void HybridCloudManager::costLimitReached(const std::string& limitType) { (void)limitType; }
void HybridCloudManager::failoverTriggered(const std::string& fromProvider, const std::string& toProvider) {
    (void)fromProvider;
    (void)toProvider;
}
void HybridCloudManager::cloudSwitched(bool usingCloud) { (void)usingCloud; }
void HybridCloudManager::errorOccurred(const std::string& error) { (void)error; }
void HybridCloudManager::healthCheckCompleted() {}

void HybridCloudManager::onNetworkReplyFinished(void** reply) { (void)reply; }
void HybridCloudManager::onHealthCheckTimerTimeout() { checkAllProvidersHealth(); }

void HybridCloudManager::setupDefaultProviders() {
    addProvider({"ollama", "Ollama", "http://127.0.0.1:11434", "", "local", true, true});
    addProvider({"huggingface", "HuggingFace", "https://api-inference.huggingface.co", "", "global", false, false});
    addProvider({"aws", "AWS Bedrock", "https://bedrock-runtime.amazonaws.com", "", "us-east-1", false, false});
    addProvider({"azure", "Azure OpenAI", "https://azure.microsoft.com", "", "eastus", false, false});
    addProvider({"gcp", "GCP Vertex", "https://vertexai.googleapis.com", "", "us-central1", false, false});
}

ExecutionResult HybridCloudManager::sendCloudRequest(
    const std::string& providerId, const std::string& modelId, const ExecutionRequest& request) {
    return executeCloud(request, providerId, modelId);
}

void* HybridCloudManager::createRequestPayload(const ExecutionRequest& request, const std::string& providerId) {
    (void)request;
    (void)providerId;
    return nullptr;
}

ExecutionResult HybridCloudManager::parseCloudResponse(const std::vector<uint8_t>& data, const std::string& providerId) {
    (void)data;
    ExecutionResult result;
    result.executionLocation = providerId;
    result.success = true;
    return result;
}

double HybridCloudManager::calculateProviderScore(const CloudProvider& provider, const ExecutionRequest& request) {
    (void)request;
    return calculateCostEfficiency(provider, request.maxTokens) + calculateLatencyScore(provider) +
           calculateReliabilityScore(provider);
}
double HybridCloudManager::calculateCostEfficiency(const CloudProvider& provider, int estimatedTokens) {
    const double cost = provider.costPerRequest > 0.0 ? provider.costPerRequest
                                                       : static_cast<double>(estimatedTokens) * 0.000002;
    return 1.0 / std::max(0.0001, cost);
}
double HybridCloudManager::calculateLatencyScore(const CloudProvider& provider) {
    return 1.0 / std::max(1.0, provider.averageLatency);
}
double HybridCloudManager::calculateReliabilityScore(const CloudProvider& provider) {
    return provider.isHealthy ? 1.0 : 0.2;
}

bool HybridCloudManager::validateCloudHealth(const std::string& providerId) {
    auto it = providers.find(providerId);
    if (it == providers.end()) return false;
    return it->second.isEnabled;
}
void HybridCloudManager::recordProviderLatency(const std::string& providerId, double latencyMs) {
    performanceMetrics.latencyByProvider[providerId] = latencyMs;
}
void HybridCloudManager::recordProviderFailure(const std::string& providerId) {
    auto it = providers.find(providerId);
    if (it != providers.end()) {
        it->second.isHealthy = false;
    }
}

void HybridCloudManager::recordCost(const std::string& providerId, double cost) {
    costMetrics.totalCostUSD += cost;
    costMetrics.todayCostUSD += cost;
    costMetrics.monthCostUSD += cost;
    totalCostUSD = costMetrics.totalCostUSD;
    costMetrics.costByProvider[providerId] += cost;
    if (!isWithinCostLimits()) {
        costLimitReached("daily_or_monthly");
    }
}

void HybridCloudManager::updateCostMetrics(const ExecutionResult& result) {
    costMetrics.totalRequests++;
    if (result.executionLocation == "local") {
        costMetrics.localRequests++;
    } else {
        costMetrics.cloudRequests++;
    }
    costMetrics.requestsByProvider[result.executionLocation]++;
    recordCost(result.executionLocation, result.cost);
}

void HybridCloudManager::recordLatency(double latencyMs) {
    const int n = std::max(1, costMetrics.totalRequests);
    performanceMetrics.averageLatency =
        ((performanceMetrics.averageLatency * (n - 1)) + latencyMs) / static_cast<double>(n);
    performanceMetrics.p95Latency = std::max(performanceMetrics.p95Latency, latencyMs);
    performanceMetrics.p99Latency = std::max(performanceMetrics.p99Latency, latencyMs);
}

void HybridCloudManager::recordSuccess(bool success) {
    static int successes = 0;
    static int total = 0;
    ++total;
    if (success) ++successes;
    performanceMetrics.successRate = static_cast<int>((100.0 * successes) / std::max(1, total));
}

void HybridCloudManager::updatePerformanceMetrics(const ExecutionResult& result) {
    recordLatency(result.latencyMs);
    recordSuccess(result.success);
    recordProviderLatency(result.executionLocation, result.latencyMs);
}

std::string HybridCloudManager::getNextFallbackProvider(const std::string& currentProvider) {
    for (const auto& id : failoverConfig.providerPriority) {
        if (id != currentProvider && isProviderHealthy(id)) {
            return id;
        }
    }
    for (const auto& [id, p] : providers) {
        if (id != currentProvider && p.isEnabled && p.isHealthy) {
            return id;
        }
    }
    return "";
}

bool HybridCloudManager::shouldRetry(int attemptNumber, const std::string& errorType) {
    (void)errorType;
    return attemptNumber < std::min(maxRetries, failoverConfig.maxRetries);
}
=======
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


>>>>>>> origin/main
