// hybrid_cloud_manager.cpp - Multi-Cloud + Ollama AI Execution Manager
#include "hybrid_cloud_manager.h"


#include <iostream>

HybridCloudManager::HybridCloudManager(void* parent)
    : void(parent),
      networkManager(new void*(this)),
      healthCheckIntervalMs(30000),
      costThresholdUSD(0.01),
      maxRetries(3),
      localExecutionEnabled(true),
      totalCostUSD(0.0) {
    
    healthCheckTimer = new void*(this);
    healthCheckTimer->setInterval(healthCheckIntervalMs);
// Qt connect removed
    setupDefaultProviders();
    healthCheckTimer->start();


}

HybridCloudManager::~HybridCloudManager() {
    healthCheckTimer->stop();
}

void HybridCloudManager::setupDefaultProviders() {
    // Ollama - Local or Cloud
    CloudProvider ollama;
    ollama.providerId = "ollama";
    ollama.name = "Ollama";
    ollama.endpoint = "http://localhost:11434"; // Default local
    ollama.apiKey = "";  // No API key needed for local
    ollama.region = "local";
    ollama.isEnabled = true;
    ollama.isHealthy = true;
    ollama.costPerRequest = 0.0; // Free for local
    ollama.averageLatency = 500.0;
    
    void* ollamaCapabilities;
    ollamaCapabilities["streaming"] = true;
    ollamaCapabilities["embeddings"] = true;
    ollamaCapabilities["chat"] = true;
    ollamaCapabilities["completion"] = true;
    ollama.capabilities = ollamaCapabilities;
    
    providers["ollama"] = ollama;
    
    // HuggingFace Inference API
    CloudProvider huggingface;
    huggingface.providerId = "huggingface";
    huggingface.name = "HuggingFace Inference";
    huggingface.endpoint = "https://api-inference.huggingface.co";
    huggingface.apiKey = "";  // Set via config
    huggingface.region = "us-east-1";
    huggingface.isEnabled = false; // Enable when API key is set
    huggingface.isHealthy = false;
    huggingface.costPerRequest = 0.001;
    huggingface.averageLatency = 2000.0;
    
    void* hfCapabilities;
    hfCapabilities["text_generation"] = true;
    hfCapabilities["translation"] = true;
    hfCapabilities["summarization"] = true;
    huggingface.capabilities = hfCapabilities;
    
    providers["huggingface"] = huggingface;
    
    // AWS SageMaker
    CloudProvider aws;
    aws.providerId = "aws";
    aws.name = "AWS SageMaker";
    aws.endpoint = "https://runtime.sagemaker.us-east-1.amazonaws.com";
    aws.apiKey = "";
    aws.region = "us-east-1";
    aws.isEnabled = false;
    aws.isHealthy = false;
    aws.costPerRequest = 0.005;
    aws.averageLatency = 1500.0;
    providers["aws"] = aws;
    
    // Azure Machine Learning
    CloudProvider azure;
    azure.providerId = "azure";
    azure.name = "Azure ML";
    azure.endpoint = "https://api.azureml.ms";
    azure.apiKey = "";
    azure.region = "eastus";
    azure.isEnabled = false;
    azure.isHealthy = false;
    azure.costPerRequest = 0.004;
    azure.averageLatency = 1800.0;
    providers["azure"] = azure;
    
    // Google Cloud Vertex AI
    CloudProvider gcp;
    gcp.providerId = "gcp";
    gcp.name = "GCP Vertex AI";
    gcp.endpoint = "https://us-central1-aiplatform.googleapis.com";
    gcp.apiKey = "";
    gcp.region = "us-central1";
    gcp.isEnabled = false;
    gcp.isHealthy = false;
    gcp.costPerRequest = 0.003;
    gcp.averageLatency = 1600.0;
    providers["gcp"] = gcp;


}

bool HybridCloudManager::addProvider(const CloudProvider& provider) {
    if (providers.contains(provider.providerId)) {
        
        return false;
    }
    
    providers[provider.providerId] = provider;
    
    return true;
}

bool HybridCloudManager::removeProvider(const std::string& providerId) {
    if (!providers.contains(providerId)) {
        return false;
    }
    
    providers.remove(providerId);
    
    return true;
}

bool HybridCloudManager::configureProvider(const std::string& providerId, const std::string& apiKey, 
                                          const std::string& endpoint, const std::string& region) {
    if (!providers.contains(providerId)) {
        
        return false;
    }
    
    CloudProvider& provider = providers[providerId];
    
    if (!apiKey.empty()) provider.apiKey = apiKey;
    if (!endpoint.empty()) provider.endpoint = endpoint;
    if (!region.empty()) provider.region = region;
    
    provider.isEnabled = !provider.apiKey.empty() || providerId == "ollama"; // Ollama doesn't need API key


    return true;
}

CloudProvider HybridCloudManager::getProvider(const std::string& providerId) const {
    return providers.value(providerId);
}

std::vector<CloudProvider> HybridCloudManager::getAllProviders() const {
    return providers.values().toVector();
}

std::vector<CloudProvider> HybridCloudManager::getHealthyProviders() const {
    std::vector<CloudProvider> healthy;
    for (const CloudProvider& provider : providers.values()) {
        if (provider.isHealthy && provider.isEnabled) {
            healthy.append(provider);
        }
    }
    return healthy;
}

HybridExecution HybridCloudManager::planExecution(const ExecutionRequest& request, 
                                                  const std::string& executionType) {
    HybridExecution plan;
    plan.requestId = request.requestId;
    plan.useCloud = false;
    plan.confidenceScore = 0.0;
    
    // Check if local execution is preferred
    if (localExecutionEnabled && executionType == "fast") {
        // Check if Ollama is available locally
        if (providers.contains("ollama") && providers["ollama"].isHealthy) {
            plan.useCloud = false;
            plan.selectedProvider = "local_ollama";
            plan.selectedModel = "local";
            plan.reasoning = "Using local Ollama for fast, cost-free execution";
            plan.estimatedCost = 0.0;
            plan.estimatedLatency = providers["ollama"].averageLatency;
            plan.confidenceScore = 0.95;
            return plan;
        }
    }
    
    // Find best cloud provider based on cost, latency, and health
    std::vector<CloudProvider> healthyProviders = getHealthyProviders();
    
    if (healthyProviders.empty()) {
        plan.reasoning = "No healthy providers available";
        plan.confidenceScore = 0.0;
        return plan;
    }
    
    // Score each provider
    CloudProvider bestProvider;
    double bestScore = -1.0;
    
    for (const CloudProvider& provider : healthyProviders) {
        double score = 0.0;
        
        // Cost factor (40%)
        double costScore = 1.0 - (provider.costPerRequest / 0.01); // Normalize to $0.01
        score += costScore * 0.4;
        
        // Latency factor (30%)
        double latencyScore = 1.0 - (provider.averageLatency / 5000.0); // Normalize to 5s
        score += latencyScore * 0.3;
        
        // Health factor (30%)
        score += 0.3; // Healthy providers get full points
        
        if (score > bestScore) {
            bestScore = score;
            bestProvider = provider;
        }
    }
    
    // Decide: local vs cloud
    bool shouldUseCloud = shouldUseCloudExecution(request);
    
    if (shouldUseCloud && bestProvider.isEnabled) {
        plan.useCloud = true;
        plan.selectedProvider = bestProvider.providerId;
        plan.selectedModel = request.taskType;
        plan.reasoning = std::string("Selected %1: best cost/latency/health score (%.2f)")
                        ;
        plan.estimatedCost = bestProvider.costPerRequest;
        plan.estimatedLatency = bestProvider.averageLatency;
        plan.confidenceScore = bestScore;
    } else {
        plan.useCloud = false;
        plan.selectedProvider = "local_ollama";
        plan.selectedModel = "local";
        plan.reasoning = "Local execution preferred for cost/speed";
        plan.estimatedCost = 0.0;
        plan.estimatedLatency = 500.0;
        plan.confidenceScore = 0.90;
    }
    
    return plan;
}

ExecutionResult HybridCloudManager::executeWithFailover(const ExecutionRequest& request) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.success = false;
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    // Plan execution
    HybridExecution plan = planExecution(request, "balanced");
    
    int attempt = 0;
    std::vector<CloudProvider> healthyProviders = getHealthyProviders();
    
    // Try primary provider first
    if (plan.useCloud && !plan.selectedProvider.empty()) {
        result = executeOnCloud(request, plan.selectedProvider, plan.selectedModel);
        
        if (result.success) {
            recordExecution(result);
            return result;
        }


    }
    
    // Failover to other providers
    while (attempt < maxRetries && !result.success) {
        for (const CloudProvider& provider : healthyProviders) {
            if (provider.providerId == plan.selectedProvider) {
                continue; // Already tried
            }
            
            result = executeOnCloud(request, provider.providerId, request.taskType);
            
            if (result.success) {
                
                recordExecution(result);
                return result;
            }
        }
        
        attempt++;
        
    }
    
    // Final fallback to local Ollama if available
    if (providers.contains("ollama") && providers["ollama"].isHealthy) {
        
        result = executeOnOllama(request);
    }
    
    recordExecution(result);
    return result;
}

ExecutionResult HybridCloudManager::executeOnCloud(const ExecutionRequest& request, 
                                                   const std::string& providerId, 
                                                   const std::string& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = providerId;
    result.modelUsed = modelId;
    result.success = false;
    
    if (!providers.contains(providerId)) {
        result.errorMessage = "Provider not found: " + providerId;
        return result;
    }
    
    CloudProvider provider = providers[providerId];
    
    // Route to appropriate provider
    if (providerId == "ollama") {
        return executeOnOllama(request);
    } else if (providerId == "huggingface") {
        return executeOnHuggingFace(request, modelId);
    } else if (providerId == "aws") {
        return executeOnAWS(request, modelId);
    } else if (providerId == "azure") {
        return executeOnAzure(request, modelId);
    } else if (providerId == "gcp") {
        return executeOnGCP(request, modelId);
    }
    
    result.errorMessage = "Unsupported provider: " + providerId;
    return result;
}

ExecutionResult HybridCloudManager::executeOnOllama(const ExecutionRequest& request) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "ollama";
    result.success = false;
    
    CloudProvider ollama = providers["ollama"];
    
    // Build Ollama API request
    void* requestBody;
    requestBody["model"] = "codellama"; // Default model, should be configurable
    requestBody["prompt"] = request.prompt;
    requestBody["stream"] = false;
    
    void* options;
    options["temperature"] = request.temperature;
    options["num_predict"] = request.maxTokens;
    requestBody["options"] = options;
    
    void* netRequest(std::string(ollama.endpoint + "/api/generate"));
    netRequest.setHeader(void*::ContentTypeHeader, "application/json");
    
    std::vector<uint8_t> requestData = void*(requestBody).toJson();
    
    std::chrono::steady_clock timer;
    timer.start();
    
    void** reply = networkManager->post(netRequest, requestData);
    
    // Synchronous wait (for simplicity - should be async in production)
    void* loop;
// Qt connect removed
    loop.exec();
    
    result.latencyMs = timer.elapsed();
    
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        void* responseObj = responseDoc.object();
        
        result.response = responseObj["response"].toString();
        result.modelUsed = responseObj["model"].toString();
        result.tokensUsed = responseObj["eval_count"].toInt();
        result.cost = 0.0; // Free for local Ollama
        result.success = true;


    } else {
        result.errorMessage = reply->errorString();
        result.success = false;


    }
    
    reply->deleteLater();
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnHuggingFace(const ExecutionRequest& request, 
                                                         const std::string& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "huggingface";
    result.modelUsed = modelId.empty() ? "bigcode/starcoder" : modelId;
    result.success = false;
    
    CloudProvider hf = providers["huggingface"];
    
    if (hf.apiKey.empty()) {
        result.errorMessage = "HuggingFace API key not configured";
        return result;
    }
    
    // Build HuggingFace API request
    std::string apiUrl = hf.endpoint + "/models/" + result.modelUsed;
    
    void* requestBody;
    requestBody["inputs"] = request.prompt;
    
    void* parameters;
    parameters["max_new_tokens"] = request.maxTokens;
    parameters["temperature"] = request.temperature;
    parameters["return_full_text"] = false;
    requestBody["parameters"] = parameters;
    
    std::string requestUrl(apiUrl);
    void* netRequest(requestUrl);
    netRequest.setHeader(void*::ContentTypeHeader, "application/json");
    netRequest.setRawHeader("Authorization", ("Bearer " + hf.apiKey).toUtf8());
    
    std::vector<uint8_t> requestData = void*(requestBody).toJson();
    
    std::chrono::steady_clock timer;
    timer.start();
    
    void** reply = networkManager->post(netRequest, requestData);
    
    void* loop;
// Qt connect removed
    loop.exec();
    
    result.latencyMs = timer.elapsed();
    
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        
        if (responseDoc.isArray()) {
            void* responseArray = responseDoc.array();
            if (!responseArray.empty()) {
                void* firstResult = responseArray[0].toObject();
                result.response = firstResult["generated_text"].toString();
                result.success = true;
            }
        }
        
        result.tokensUsed = request.maxTokens; // Estimate
        result.cost = hf.costPerRequest;


    } else {
        result.errorMessage = reply->errorString();
        
    }
    
    reply->deleteLater();
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnAWS(const ExecutionRequest& request, 
                                                 const std::string& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "aws";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "AWS SageMaker integration not yet implemented";
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    // TODO: Implement AWS SageMaker Runtime API integration
    // Requires AWS SDK or manual signing of requests
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnAzure(const ExecutionRequest& request, 
                                                   const std::string& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "azure";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "Azure ML integration not yet implemented";
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    // TODO: Implement Azure ML REST API integration
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnGCP(const ExecutionRequest& request, 
                                                 const std::string& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "gcp";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "GCP Vertex AI integration not yet implemented";
    result.completedAt = std::chrono::system_clock::time_point::currentDateTime();
    
    // TODO: Implement GCP Vertex AI REST API integration
    
    return result;
}

bool HybridCloudManager::shouldUseCloudExecution(const ExecutionRequest& request) {
    // Decision factors:
    // 1. Cost threshold
    // 2. Latency requirements
    // 3. Local capability
    // 4. Task complexity
    
    // If Ollama is healthy and free, prefer it
    if (providers.contains("ollama") && providers["ollama"].isHealthy) {
        return false; // Use local Ollama
    }
    
    // If local execution is disabled, must use cloud
    if (!localExecutionEnabled) {
        return true;
    }
    
    // For simple tasks, prefer local
    if (request.maxTokens < 1000) {
        return false;
    }
    
    // For complex tasks, consider cloud
    return true;
}

void HybridCloudManager::recordExecution(const ExecutionResult& result) {
    executionHistory.append(result);
    
    if (result.success) {
        totalCostUSD += result.cost;
        
        // Update provider metrics
        if (providers.contains(result.executionLocation)) {
            CloudProvider& provider = providers[result.executionLocation];
            
            // Update average latency (moving average)
            provider.averageLatency = (provider.averageLatency * 0.9) + (result.latencyMs * 0.1);
        }
    }
    
    // Keep only recent history (last 1000 executions)
    if (executionHistory.size() > 1000) {
        executionHistory.removeFirst();
    }
}

void HybridCloudManager::checkProviderHealth(const std::string& providerId) {
    if (!providers.contains(providerId)) {
        return;
    }
    
    CloudProvider& provider = providers[providerId];
    
    // Special handling for Ollama - check /api/tags endpoint
    if (providerId == "ollama") {
        void* request(std::string(provider.endpoint + "/api/tags"));
        void** reply = networkManager->get(request);
        
        void* loop;
        void* timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(5000); // 5 second timeout
// Qt connect removed
// Qt connect removed
        timeout.start();
        loop.exec();
        
        if (reply->error() == void*::NoError) {
            provider.isHealthy = true;
            
        } else {
            provider.isHealthy = false;
            
        }
        
        reply->deleteLater();
        provider.lastHealthCheck = std::chrono::system_clock::time_point::currentDateTime();
        return;
    }
    
    // For other providers, do a simple ping
    void* request(std::string(provider.endpoint));
    if (!provider.apiKey.empty()) {
        request.setRawHeader("Authorization", ("Bearer " + provider.apiKey).toUtf8());
    }
    
    void** reply = networkManager->get(request);
    
    void* loop;
    void* timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
// Qt connect removed
// Qt connect removed
    timeout.start();
    loop.exec();
    
    provider.isHealthy = (reply->error() == void*::NoError || 
                         reply->error() == void*::AuthenticationRequiredError);
    provider.lastHealthCheck = std::chrono::system_clock::time_point::currentDateTime();
    
    reply->deleteLater();


}

void HybridCloudManager::checkAllProvidersHealth() {


    for (const std::string& providerId : providers.keys()) {
        if (providers[providerId].isEnabled) {
            checkProviderHealth(providerId);
        }
    }
    
    healthCheckCompleted();
}

CostMetrics HybridCloudManager::getCostMetrics() const {
    CostMetrics metrics;
    metrics.totalCostUSD = totalCostUSD;
    
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    std::chrono::system_clock::time_point todayStart = std::chrono::system_clock::time_point(now.date(), std::chrono::system_clock::time_point(0, 0));
    std::chrono::system_clock::time_point monthStart = std::chrono::system_clock::time_point(std::chrono::system_clock::time_point(now.date().year(), now.date().month(), 1), std::chrono::system_clock::time_point(0, 0));
    
    metrics.todayCostUSD = 0.0;
    metrics.monthCostUSD = 0.0;
    
    for (const ExecutionResult& result : executionHistory) {
        if (result.completedAt >= todayStart) {
            metrics.todayCostUSD += result.cost;
        }
        if (result.completedAt >= monthStart) {
            metrics.monthCostUSD += result.cost;
        }
        
        metrics.requestsByProvider[result.executionLocation]++;
        metrics.costByProvider[result.executionLocation] += result.cost;
    }
    
    return metrics;
}

std::vector<ExecutionResult> HybridCloudManager::getExecutionHistory(int limit) const {
    if (limit <= 0 || limit > executionHistory.size()) {
        return executionHistory;
    }
    
    return executionHistory.mid(executionHistory.size() - limit);
}

void HybridCloudManager::clearExecutionHistory() {
    executionHistory.clear();
    
}

void HybridCloudManager::enableLocalExecution(bool enable) {
    localExecutionEnabled = enable;
    
}

void HybridCloudManager::setCostThreshold(double thresholdUSD) {
    costThresholdUSD = thresholdUSD;
    
}

void HybridCloudManager::setHealthCheckInterval(int milliseconds) {
    healthCheckIntervalMs = milliseconds;
    healthCheckTimer->setInterval(milliseconds);
    
}

void HybridCloudManager::setMaxRetries(int retries) {
    maxRetries = retries;
}

double HybridCloudManager::getTotalCost() const {
    return totalCostUSD;
}

void HybridCloudManager::onNetworkReplyFinished(void** reply) {
    // Handle async network replies if needed
    if (reply) {
        std::string requestId = reply->property("requestId").toString();
        if (!requestId.empty()) {
            activeRequests.remove(requestId);
        }
        reply->deleteLater();
    }
}

void HybridCloudManager::onHealthCheckTimerTimeout() {
    checkAllProvidersHealth();
}


