// hybrid_cloud_manager.cpp - Multi-Cloud + Ollama AI Execution Manager
#include "hybrid_cloud_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <iostream>

HybridCloudManager::HybridCloudManager(QObject* parent)
    : QObject(parent),
      networkManager(new QNetworkAccessManager(this)),
      healthCheckIntervalMs(30000),
      costThresholdUSD(0.01),
      maxRetries(3),
      localExecutionEnabled(true),
      totalCostUSD(0.0) {
    
    healthCheckTimer = new QTimer(this);
    healthCheckTimer->setInterval(healthCheckIntervalMs);
    connect(healthCheckTimer, &QTimer::timeout, this, &HybridCloudManager::checkAllProvidersHealth);
    
    setupDefaultProviders();
    healthCheckTimer->start();
    
    std::cout << "[HybridCloudManager] Initialized with multi-cloud + Ollama support" << std::endl;
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
    
    QJsonObject ollamaCapabilities;
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
    
    QJsonObject hfCapabilities;
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
    
    std::cout << "[HybridCloudManager] Configured 5 providers (Ollama, HuggingFace, AWS, Azure, GCP)" << std::endl;
}

bool HybridCloudManager::addProvider(const CloudProvider& provider) {
    if (providers.contains(provider.providerId)) {
        std::cout << "[HybridCloudManager] Provider already exists: " << provider.providerId.toStdString() << std::endl;
        return false;
    }
    
    providers[provider.providerId] = provider;
    std::cout << "[HybridCloudManager] Added provider: " << provider.name.toStdString() << std::endl;
    return true;
}

bool HybridCloudManager::removeProvider(const QString& providerId) {
    if (!providers.contains(providerId)) {
        return false;
    }
    
    providers.remove(providerId);
    std::cout << "[HybridCloudManager] Removed provider: " << providerId.toStdString() << std::endl;
    return true;
}

bool HybridCloudManager::configureProvider(const QString& providerId, const QString& apiKey, 
                                          const QString& endpoint, const QString& region) {
    if (!providers.contains(providerId)) {
        std::cout << "[HybridCloudManager] Provider not found: " << providerId.toStdString() << std::endl;
        return false;
    }
    
    CloudProvider& provider = providers[providerId];
    
    if (!apiKey.isEmpty()) provider.apiKey = apiKey;
    if (!endpoint.isEmpty()) provider.endpoint = endpoint;
    if (!region.isEmpty()) provider.region = region;
    
    provider.isEnabled = !provider.apiKey.isEmpty() || providerId == "ollama"; // Ollama doesn't need API key
    
    std::cout << "[HybridCloudManager] Configured " << provider.name.toStdString() 
              << " - Enabled: " << (provider.isEnabled ? "yes" : "no") << std::endl;
    
    return true;
}

CloudProvider HybridCloudManager::getProvider(const QString& providerId) const {
    return providers.value(providerId);
}

QVector<CloudProvider> HybridCloudManager::getAllProviders() const {
    return providers.values().toVector();
}

QVector<CloudProvider> HybridCloudManager::getHealthyProviders() const {
    QVector<CloudProvider> healthy;
    for (const CloudProvider& provider : providers.values()) {
        if (provider.isHealthy && provider.isEnabled) {
            healthy.append(provider);
        }
    }
    return healthy;
}

HybridExecution HybridCloudManager::planExecution(const ExecutionRequest& request, 
                                                  const QString& executionType) {
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
    QVector<CloudProvider> healthyProviders = getHealthyProviders();
    
    if (healthyProviders.isEmpty()) {
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
        plan.reasoning = QString("Selected %1: best cost/latency/health score (%.2f)")
                        .arg(bestProvider.name).arg(bestScore);
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
    result.completedAt = QDateTime::currentDateTime();
    
    // Plan execution
    HybridExecution plan = planExecution(request, "balanced");
    
    int attempt = 0;
    QVector<CloudProvider> healthyProviders = getHealthyProviders();
    
    // Try primary provider first
    if (plan.useCloud && !plan.selectedProvider.isEmpty()) {
        result = executeOnCloud(request, plan.selectedProvider, plan.selectedModel);
        
        if (result.success) {
            recordExecution(result);
            return result;
        }
        
        std::cout << "[HybridCloudManager] Primary provider failed, trying failover..." << std::endl;
    }
    
    // Failover to other providers
    while (attempt < maxRetries && !result.success) {
        for (const CloudProvider& provider : healthyProviders) {
            if (provider.providerId == plan.selectedProvider) {
                continue; // Already tried
            }
            
            result = executeOnCloud(request, provider.providerId, request.taskType);
            
            if (result.success) {
                std::cout << "[HybridCloudManager] Failover successful with " 
                         << provider.name.toStdString() << std::endl;
                recordExecution(result);
                return result;
            }
        }
        
        attempt++;
        std::cout << "[HybridCloudManager] Retry attempt " << attempt << "/" << maxRetries << std::endl;
    }
    
    // Final fallback to local Ollama if available
    if (providers.contains("ollama") && providers["ollama"].isHealthy) {
        std::cout << "[HybridCloudManager] All cloud providers failed, falling back to local Ollama" << std::endl;
        result = executeOnOllama(request);
    }
    
    recordExecution(result);
    return result;
}

ExecutionResult HybridCloudManager::executeOnCloud(const ExecutionRequest& request, 
                                                   const QString& providerId, 
                                                   const QString& modelId) {
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
    QJsonObject requestBody;
    requestBody["model"] = "codellama"; // Default model, should be configurable
    requestBody["prompt"] = request.prompt;
    requestBody["stream"] = false;
    
    QJsonObject options;
    options["temperature"] = request.temperature;
    options["num_predict"] = request.maxTokens;
    requestBody["options"] = options;
    
    QNetworkRequest netRequest(QUrl(ollama.endpoint + "/api/generate"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QByteArray requestData = QJsonDocument(requestBody).toJson();
    
    QElapsedTimer timer;
    timer.start();
    
    QNetworkReply* reply = networkManager->post(netRequest, requestData);
    
    // Synchronous wait (for simplicity - should be async in production)
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    result.latencyMs = timer.elapsed();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        QJsonObject responseObj = responseDoc.object();
        
        result.response = responseObj["response"].toString();
        result.modelUsed = responseObj["model"].toString();
        result.tokensUsed = responseObj["eval_count"].toInt();
        result.cost = 0.0; // Free for local Ollama
        result.success = true;
        
        std::cout << "[HybridCloudManager] Ollama execution successful - Model: " 
                  << result.modelUsed.toStdString() << ", Tokens: " << result.tokensUsed 
                  << ", Latency: " << result.latencyMs << "ms" << std::endl;
    } else {
        result.errorMessage = reply->errorString();
        result.success = false;
        
        std::cout << "[HybridCloudManager] Ollama execution failed: " 
                  << result.errorMessage.toStdString() << std::endl;
    }
    
    reply->deleteLater();
    result.completedAt = QDateTime::currentDateTime();
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnHuggingFace(const ExecutionRequest& request, 
                                                         const QString& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "huggingface";
    result.modelUsed = modelId.isEmpty() ? "bigcode/starcoder" : modelId;
    result.success = false;
    
    CloudProvider hf = providers["huggingface"];
    
    if (hf.apiKey.isEmpty()) {
        result.errorMessage = "HuggingFace API key not configured";
        return result;
    }
    
    // Build HuggingFace API request
    QString apiUrl = hf.endpoint + "/models/" + result.modelUsed;
    
    QJsonObject requestBody;
    requestBody["inputs"] = request.prompt;
    
    QJsonObject parameters;
    parameters["max_new_tokens"] = request.maxTokens;
    parameters["temperature"] = request.temperature;
    parameters["return_full_text"] = false;
    requestBody["parameters"] = parameters;
    
    QUrl requestUrl(apiUrl);
    QNetworkRequest netRequest(requestUrl);
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netRequest.setRawHeader("Authorization", ("Bearer " + hf.apiKey).toUtf8());
    
    QByteArray requestData = QJsonDocument(requestBody).toJson();
    
    QElapsedTimer timer;
    timer.start();
    
    QNetworkReply* reply = networkManager->post(netRequest, requestData);
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    result.latencyMs = timer.elapsed();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
        
        if (responseDoc.isArray()) {
            QJsonArray responseArray = responseDoc.array();
            if (!responseArray.isEmpty()) {
                QJsonObject firstResult = responseArray[0].toObject();
                result.response = firstResult["generated_text"].toString();
                result.success = true;
            }
        }
        
        result.tokensUsed = request.maxTokens; // Estimate
        result.cost = hf.costPerRequest;
        
        std::cout << "[HybridCloudManager] HuggingFace execution successful - Latency: " 
                  << result.latencyMs << "ms" << std::endl;
    } else {
        result.errorMessage = reply->errorString();
        std::cout << "[HybridCloudManager] HuggingFace execution failed: " 
                  << result.errorMessage.toStdString() << std::endl;
    }
    
    reply->deleteLater();
    result.completedAt = QDateTime::currentDateTime();
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnAWS(const ExecutionRequest& request, 
                                                 const QString& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "aws";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "AWS SageMaker integration not yet implemented";
    result.completedAt = QDateTime::currentDateTime();
    
    // TODO: Implement AWS SageMaker Runtime API integration
    // Requires AWS SDK or manual signing of requests
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnAzure(const ExecutionRequest& request, 
                                                   const QString& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "azure";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "Azure ML integration not yet implemented";
    result.completedAt = QDateTime::currentDateTime();
    
    // TODO: Implement Azure ML REST API integration
    
    return result;
}

ExecutionResult HybridCloudManager::executeOnGCP(const ExecutionRequest& request, 
                                                 const QString& modelId) {
    ExecutionResult result;
    result.requestId = request.requestId;
    result.executionLocation = "gcp";
    result.modelUsed = modelId;
    result.success = false;
    result.errorMessage = "GCP Vertex AI integration not yet implemented";
    result.completedAt = QDateTime::currentDateTime();
    
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

void HybridCloudManager::checkProviderHealth(const QString& providerId) {
    if (!providers.contains(providerId)) {
        return;
    }
    
    CloudProvider& provider = providers[providerId];
    
    // Special handling for Ollama - check /api/tags endpoint
    if (providerId == "ollama") {
        QNetworkRequest request(QUrl(provider.endpoint + "/api/tags"));
        QNetworkReply* reply = networkManager->get(request);
        
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(5000); // 5 second timeout
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        timeout.start();
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError) {
            provider.isHealthy = true;
            std::cout << "[HybridCloudManager] Ollama health check: HEALTHY" << std::endl;
        } else {
            provider.isHealthy = false;
            std::cout << "[HybridCloudManager] Ollama health check: FAILED - " 
                     << reply->errorString().toStdString() << std::endl;
        }
        
        reply->deleteLater();
        provider.lastHealthCheck = QDateTime::currentDateTime();
        return;
    }
    
    // For other providers, do a simple ping
    QNetworkRequest request(QUrl(provider.endpoint));
    if (!provider.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + provider.apiKey).toUtf8());
    }
    
    QNetworkReply* reply = networkManager->get(request);
    
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    
    timeout.start();
    loop.exec();
    
    provider.isHealthy = (reply->error() == QNetworkReply::NoError || 
                         reply->error() == QNetworkReply::AuthenticationRequiredError);
    provider.lastHealthCheck = QDateTime::currentDateTime();
    
    reply->deleteLater();
    
    std::cout << "[HybridCloudManager] " << provider.name.toStdString() 
              << " health check: " << (provider.isHealthy ? "HEALTHY" : "FAILED") << std::endl;
}

void HybridCloudManager::checkAllProvidersHealth() {
    std::cout << "[HybridCloudManager] Running health checks for all providers..." << std::endl;
    
    for (const QString& providerId : providers.keys()) {
        if (providers[providerId].isEnabled) {
            checkProviderHealth(providerId);
        }
    }
    
    emit healthCheckCompleted();
}

CostMetrics HybridCloudManager::getCostMetrics() const {
    CostMetrics metrics;
    metrics.totalCostUSD = totalCostUSD;
    
    QDateTime now = QDateTime::currentDateTime();
    QDateTime todayStart = QDateTime(now.date(), QTime(0, 0));
    QDateTime monthStart = QDateTime(QDate(now.date().year(), now.date().month(), 1), QTime(0, 0));
    
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

QVector<ExecutionResult> HybridCloudManager::getExecutionHistory(int limit) const {
    if (limit <= 0 || limit > executionHistory.size()) {
        return executionHistory;
    }
    
    return executionHistory.mid(executionHistory.size() - limit);
}

void HybridCloudManager::clearExecutionHistory() {
    executionHistory.clear();
    std::cout << "[HybridCloudManager] Execution history cleared" << std::endl;
}

void HybridCloudManager::enableLocalExecution(bool enable) {
    localExecutionEnabled = enable;
    std::cout << "[HybridCloudManager] Local execution " 
              << (enable ? "enabled" : "disabled") << std::endl;
}

void HybridCloudManager::setCostThreshold(double thresholdUSD) {
    costThresholdUSD = thresholdUSD;
    std::cout << "[HybridCloudManager] Cost threshold set to $" << thresholdUSD << std::endl;
}

void HybridCloudManager::setHealthCheckInterval(int milliseconds) {
    healthCheckIntervalMs = milliseconds;
    healthCheckTimer->setInterval(milliseconds);
    std::cout << "[HybridCloudManager] Health check interval set to " 
              << milliseconds << "ms" << std::endl;
}

void HybridCloudManager::setMaxRetries(int retries) {
    maxRetries = retries;
}

double HybridCloudManager::getTotalCost() const {
    return totalCostUSD;
}

void HybridCloudManager::onNetworkReplyFinished(QNetworkReply* reply) {
    // Handle async network replies if needed
    if (reply) {
        QString requestId = reply->property("requestId").toString();
        if (!requestId.isEmpty()) {
            activeRequests.remove(requestId);
        }
        reply->deleteLater();
    }
}

void HybridCloudManager::onHealthCheckTimerTimeout() {
    checkAllProvidersHealth();
}
