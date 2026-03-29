#include "hybrid_cloud_manager.h"
#include "universal_model_router.h"

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

bool HybridCloudManager::addProvider(const CloudProvider& provider) {
    if (provider.providerId.empty()) {
        return false;
    }
    providers[provider.providerId] = provider;
    return true;
}

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
    return plan;
}
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
