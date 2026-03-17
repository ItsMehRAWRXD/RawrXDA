// ============================================================================
// Phase 5: Advanced Model Router Implementation
// ============================================================================

#include "phase5_model_router.h"
#include "model_router/ModelRouterExtension.h"
#include "../core/local_gguf_loader.hpp"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <algorithm>
#include <random>

// ============================================================================
// ModelPoolManager Implementation
// ============================================================================

class ModelPoolManager {
public:
    ModelPoolManager() : m_cacheSize(1024 * 1024 * 1024) {} // 1GB default
    
    bool addToPool(const QString& modelId, int poolSize) {
        QMutexLocker locker(&m_mutex);
        if (!m_pools.contains(modelId)) {
            m_pools[modelId] = poolSize;
            return true;
        }
        return false;
    }
    
    void removeFromPool(const QString& modelId) {
        QMutexLocker locker(&m_mutex);
        m_pools.remove(modelId);
    }
    
    int getPoolSize(const QString& modelId) const {
        QMutexLocker locker(&m_mutex);
        return m_pools.value(modelId, 0);
    }
    
    void setCacheSize(int sizeBytes) {
        m_cacheSize = sizeBytes;
    }
    
    int getCacheSize() const {
        return m_cacheSize;
    }

private:
    QMap<QString, int> m_pools;
    int m_cacheSize;
    mutable QMutex m_mutex;
};

// ============================================================================
// RoutingAnalytics Implementation
// ============================================================================

class RoutingAnalytics {
public:
    RoutingAnalytics() : m_totalRequests(0), m_totalErrors(0) {}
    
    void recordRequest(const QString& modelId, double latencyMs, int tokens, bool success) {
        QMutexLocker locker(&m_mutex);
        m_totalRequests++;
        if (!success) {
            m_totalErrors++;
        }
        
        auto& stats = m_modelStats[modelId];
        stats.requestCount++;
        stats.totalLatency += latencyMs;
        stats.totalTokens += tokens;
        if (!success) {
            stats.errorCount++;
        }
        
        // Update moving average
        stats.avgLatency = stats.totalLatency / stats.requestCount;
        stats.avgTokensPerSecond = (stats.totalTokens / stats.totalLatency) * 1000.0;
    }
    
    QJsonObject getAnalyticsData() const {
        QMutexLocker locker(&m_mutex);
        QJsonObject obj;
        obj["totalRequests"] = static_cast<qint64>(m_totalRequests);
        obj["totalErrors"] = static_cast<qint64>(m_totalErrors);
        obj["successRate"] = m_totalRequests > 0 ? 
            (1.0 - (double)m_totalErrors / m_totalRequests) * 100.0 : 0.0;
        
        QJsonObject models;
        for (auto it = m_modelStats.constBegin(); it != m_modelStats.constEnd(); ++it) {
            QJsonObject modelObj;
            modelObj["requests"] = static_cast<qint64>(it->requestCount);
            modelObj["avgLatency"] = it->avgLatency;
            modelObj["tokensPerSecond"] = it->avgTokensPerSecond;
            modelObj["errorRate"] = it->requestCount > 0 ? 
                (double)it->errorCount / it->requestCount * 100.0 : 0.0;
            models[it.key()] = modelObj;
        }
        obj["models"] = models;
        
        return obj;
    }
    
    void reset() {
        QMutexLocker locker(&m_mutex);
        m_modelStats.clear();
        m_totalRequests = 0;
        m_totalErrors = 0;
    }

private:
    struct ModelStats {
        int requestCount = 0;
        int errorCount = 0;
        double totalLatency = 0.0;
        int totalTokens = 0;
        double avgLatency = 0.0;
        double avgTokensPerSecond = 0.0;
    };
    
    QMap<QString, ModelStats> m_modelStats;
    int m_totalRequests;
    int m_totalErrors;
    mutable QMutex m_mutex;
};

// ============================================================================
// Phase5ModelRouter Implementation
// ============================================================================

Phase5ModelRouter::Phase5ModelRouter(QObject* parent)
    : QObject(parent)
    , m_poolManager(std::make_unique<ModelPoolManager>())
    , m_analytics(std::make_unique<RoutingAnalytics>())
    , m_currentRoutingStrategy("adaptive")
    , m_modelCacheSize(1024 * 1024 * 1024) // 1GB
    , m_profilingEnabled(true)
    , m_totalRequests(0)
{
    // Set up periodic health checks
    m_healthCheckTimer = new QTimer(this);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &Phase5ModelRouter::performHealthChecks);
    m_healthCheckTimer->start(30000); // Every 30 seconds
    
    // Set up metrics flush
    m_metricsFlushTimer = new QTimer(this);
    connect(m_metricsFlushTimer, &QTimer::timeout, this, [this]() {
        emit metricsUpdated(getDashboardData());
    });
    m_metricsFlushTimer->start(5000); // Every 5 seconds
}

Phase5ModelRouter::~Phase5ModelRouter() {
    m_healthCheckTimer->stop();
    m_metricsFlushTimer->stop();
}

// ===== Model Management =====

bool Phase5ModelRouter::registerModel(const ModelEndpoint& endpoint) {
    QMutexLocker locker(&m_endpointMutex);
    
    if (m_endpoints.contains(endpoint.id)) {
        qWarning() << "Model already registered:" << endpoint.id;
        return false;
    }
    
    m_endpoints[endpoint.id] = std::make_shared<ModelEndpoint>(endpoint);
    
    // Initialize metrics
    ModelMetrics metrics;
    metrics.modelId = endpoint.id;
    metrics.modelName = endpoint.name;
    m_metrics[endpoint.id] = metrics;
    
    emit modelLoaded(endpoint.id);
    return true;
}

bool Phase5ModelRouter::unregisterModel(const QString& modelId) {
    QMutexLocker locker(&m_endpointMutex);
    
    if (!m_endpoints.contains(modelId)) {
        return false;
    }
    
    m_endpoints.remove(modelId);
    m_metrics.remove(modelId);
    m_poolManager->removeFromPool(modelId);
    
    return true;
}

bool Phase5ModelRouter::loadCustomGGUFModel(const QString& path, const QString& modelName) {
    ModelEndpoint endpoint;
    endpoint.id = "gguf_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    endpoint.name = modelName;
    endpoint.type = "local";
    endpoint.path = path;
    endpoint.enabled = true;
    endpoint.priority = 5;
    endpoint.maxConcurrentRequests = 1;
    
    return registerModel(endpoint);
}

QStringList Phase5ModelRouter::availableModels() const {
    QMutexLocker locker(&m_endpointMutex);
    QStringList models;
    for (auto it = m_endpoints.constBegin(); it != m_endpoints.constEnd(); ++it) {
        if (it.value()->enabled) {
            models.append(it.value()->name);
        }
    }
    return models;
}

ModelEndpoint* Phase5ModelRouter::getEndpoint(const QString& endpointId) {
    QMutexLocker locker(&m_endpointMutex);
    auto it = m_endpoints.find(endpointId);
    return it != m_endpoints.end() ? it.value().get() : nullptr;
}

const ModelEndpoint* Phase5ModelRouter::getEndpoint(const QString& endpointId) const {
    QMutexLocker locker(&m_endpointMutex);
    auto it = m_endpoints.constFind(endpointId);
    return it != m_endpoints.constEnd() ? it.value().get() : nullptr;
}

// ===== Routing & Inference =====

QString Phase5ModelRouter::executeInference(const InferenceRequest& request) {
    auto decision = routeRequest(request);
    
    if (decision.endpointId.isEmpty()) {
        emit modelUnhealthy("none", "No available models for routing");
        return "Error: No models available";
    }
    
    emit routingDecisionMade(decision.endpointId, decision.reason);
    
    // Execute inference
    QElapsedTimer timer;
    timer.start();
    
    QString result;
    bool success = false;
    
    auto endpoint = getEndpoint(decision.endpointId);
    if (!endpoint) {
        return "Error: Endpoint not found";
    }
    
    try {
        // Simulate inference (in real implementation, call actual model)
        result = QString("Response from %1: Processed prompt with %2 tokens")
            .arg(endpoint->name)
            .arg(request.maxTokens);
        
        success = true;
        
    } catch (const std::exception& e) {
        result = QString("Error: %1").arg(e.what());
        success = false;
    }
    
    double latencyMs = timer.elapsed();
    double tokensPerSecond = request.maxTokens > 0 ? 
        (request.maxTokens / latencyMs) * 1000.0 : 0.0;
    
    // Update metrics
    updateMetrics(decision.endpointId, latencyMs, success);
    m_analytics->recordRequest(decision.endpointId, latencyMs, request.maxTokens, success);
    
    if (success) {
        emit inferenceCompleted(decision.endpointId, result, tokensPerSecond);
    }
    
    return result;
}

void Phase5ModelRouter::executeInferenceAsync(const InferenceRequest& request) {
    // Execute in separate thread
    QTimer::singleShot(0, this, [this, request]() {
        QString result = executeInference(request);
        // Result already emitted via inferenceCompleted signal
    });
}

RoutingDecision Phase5ModelRouter::routeRequest(const InferenceRequest& request) {
    RoutingDecision decision;
    
    QMutexLocker locker(&m_endpointMutex);
    
    if (m_endpoints.isEmpty()) {
        decision.reason = "No endpoints registered";
        return decision;
    }
    
    // Filter enabled endpoints
    QList<QString> availableEndpoints;
    for (auto it = m_endpoints.constBegin(); it != m_endpoints.constEnd(); ++it) {
        if (it.value()->enabled && it.value()->activeRequests < it.value()->maxConcurrentRequests) {
            availableEndpoints.append(it.key());
        }
    }
    
    if (availableEndpoints.isEmpty()) {
        decision.reason = "All endpoints at capacity";
        return decision;
    }
    
    // Apply routing strategy
    if (m_currentRoutingStrategy == "round-robin") {
        // Simple round-robin
        static int rrIndex = 0;
        decision.endpointId = availableEndpoints[rrIndex % availableEndpoints.size()];
        rrIndex++;
        decision.reason = "Round-robin selection";
        
    } else if (m_currentRoutingStrategy == "least-connections") {
        // Least active requests
        QString bestEndpoint;
        int minRequests = INT_MAX;
        for (const QString& id : availableEndpoints) {
            int active = m_endpoints[id]->activeRequests;
            if (active < minRequests) {
                minRequests = active;
                bestEndpoint = id;
            }
        }
        decision.endpointId = bestEndpoint;
        decision.reason = "Least connections";
        
    } else if (m_currentRoutingStrategy == "weighted") {
        // Weighted by priority
        QList<std::pair<QString, int>> weighted;
        for (const QString& id : availableEndpoints) {
            weighted.append({id, m_endpoints[id]->priority});
        }
        
        // Sort by priority (higher is better)
        std::sort(weighted.begin(), weighted.end(), 
            [](const auto& a, const auto& b) { return a.second > b.second; });
        
        decision.endpointId = weighted.first().first;
        decision.reason = "Weighted by priority";
        
    } else { // adaptive (default)
        // Adaptive based on metrics
        QString bestEndpoint;
        double bestScore = -1.0;
        
        QMutexLocker metricsLocker(&m_metricsMutex);
        for (const QString& id : availableEndpoints) {
            const auto& metrics = m_metrics[id];
            
            // Calculate score (lower latency, higher tokens/sec = better)
            double score = 100.0;
            if (metrics.avgLatencyMs > 0) {
                score -= (metrics.avgLatencyMs / 1000.0) * 10.0; // Latency penalty
            }
            score += metrics.tokensPerSecond; // Speed bonus
            score -= metrics.errorCount * 5.0; // Error penalty
            
            if (score > bestScore) {
                bestScore = score;
                bestEndpoint = id;
            }
        }
        
        decision.endpointId = bestEndpoint;
        decision.reason = QString("Adaptive (score: %1)").arg(bestScore, 0, 'f', 2);
    }
    
    // Update endpoint state
    if (!decision.endpointId.isEmpty()) {
        m_endpoints[decision.endpointId]->activeRequests++;
    }
    
    decision.decidedAt = QDateTime::currentDateTime();
    return decision;
}

RoutingDecision Phase5ModelRouter::routeWeightedRandom(const InferenceRequest& request) {
    RoutingDecision decision;
    
    QList<ModelEndpoint*> enabledEndpoints;
    double totalWeight = 0;
    
    for (auto endpoint : this->m_endpoints.values()) {
        if (endpoint->enabled) {
            enabledEndpoints.append(endpoint.get());
            totalWeight += endpoint->weight;
        }
    }
    
    if (enabledEndpoints.isEmpty()) {
        return decision;
    }
    
    // Weighted random selection
    double random = QRandomGenerator::global()->generateDouble() * totalWeight;
    double currentWeight = 0;
    
    for (auto endpoint : enabledEndpoints) {
        currentWeight += endpoint->weight;
        if (random <= currentWeight) {
            decision.endpointId = endpoint->id;
            decision.reason = QString("Weighted random selection (weight: %1)").arg(endpoint->weight);
            break;
        }
    }
    
    if (decision.endpointId.isEmpty()) {
        decision.endpointId = enabledEndpoints.first()->id;
    }
    
    return decision;
}

RoutingDecision Phase5ModelRouter::routeLeastConnections(const InferenceRequest& request) {
    RoutingDecision decision;
    
    ModelEndpoint* bestEndpoint = nullptr;
    int minConnections = std::numeric_limits<int>::max();
    
    for (auto endpoint : this->m_endpoints.values()) {
        if (endpoint->enabled && endpoint->activeRequests < minConnections) {
            if (endpoint->activeRequests < endpoint->maxConcurrentRequests) {
                minConnections = endpoint->activeRequests;
                bestEndpoint = endpoint.get();
            }
        }
    }
    
    if (bestEndpoint) {
        decision.endpointId = bestEndpoint->id;
        decision.reason = QString("Least connections (%1 active)").arg(minConnections);
    }
    
    return decision;
}

RoutingDecision Phase5ModelRouter::routeAdaptive(const InferenceRequest& request) {
    // Phase 5 uses a scoring system for adaptive routing
    return routeRequest(request); 
}

RoutingDecision Phase5ModelRouter::routePriorityBased(const InferenceRequest& request) {
    RoutingDecision decision;
    
    ModelEndpoint* bestEndpoint = nullptr;
    int maxPriority = -1;
    
    for (auto endpoint : this->m_endpoints.values()) {
        if (endpoint->enabled && endpoint->priority > maxPriority) {
            if (endpoint->activeRequests < endpoint->maxConcurrentRequests) {
                maxPriority = endpoint->priority;
                bestEndpoint = endpoint.get();
            }
        }
    }
    
    if (bestEndpoint) {
        decision.endpointId = bestEndpoint->id;
        decision.reason = QString("Priority-based selection (priority: %1)").arg(maxPriority);
    }
    
    return decision;
}

RoutingDecision Phase5ModelRouter::routeCostOptimized(const InferenceRequest& request) {
    RoutingDecision decision;
    
    ModelEndpoint* bestEndpoint = nullptr;
    qint64 minCost = std::numeric_limits<qint64>::max();
    
    for (auto endpoint : this->m_endpoints.values()) {
        if (endpoint->enabled && endpoint->estimatedCostPerToken < minCost) {
            if (endpoint->activeRequests < endpoint->maxConcurrentRequests) {
                minCost = endpoint->estimatedCostPerToken;
                bestEndpoint = endpoint.get();
            }
        }
    }
    
    if (bestEndpoint) {
        decision.endpointId = bestEndpoint->id;
        decision.reason = QString("Cost optimized selection (%1 microcents/token)").arg(minCost);
    }
    
    return decision;
}

// ===== Inference Modes =====

QString Phase5ModelRouter::executeMax(const QString& prompt, int maxTokens) {
    InferenceRequest request;
    request.prompt = prompt;
    request.maxTokens = maxTokens;
    request.temperature = 0.9;
    request.contextWindow = 4096;
    request.mode = InferenceMode::Max;
    return executeInference(request);
}

QString Phase5ModelRouter::executeResearch(const QString& prompt, int maxTokens) {
    InferenceRequest request;
    request.prompt = prompt;
    request.maxTokens = maxTokens;
    request.temperature = 0.8;
    request.contextWindow = 8192;
    request.mode = InferenceMode::Research;
    return executeInference(request);
}

QString Phase5ModelRouter::executeDeepResearch(const QString& prompt, int maxTokens) {
    InferenceRequest request;
    request.prompt = prompt;
    request.maxTokens = maxTokens;
    request.temperature = 0.85;
    request.contextWindow = 16384;
    request.mode = InferenceMode::DeepResearch;
    return executeInference(request);
}

QString Phase5ModelRouter::executeThinking(const QString& prompt, int maxTokens) {
    InferenceRequest request;
    request.prompt = "Think step by step:\n" + prompt;
    request.maxTokens = maxTokens;
    request.temperature = 0.7;
    request.contextWindow = 8192;
    request.mode = InferenceMode::Thinking;
    return executeInference(request);
}

QString Phase5ModelRouter::executeCustom(const QString& prompt, const QJsonObject& params) {
    InferenceRequest request;
    request.prompt = prompt;
    request.maxTokens = params.value("max_tokens").toInt(512);
    request.temperature = params.value("temperature").toDouble(0.7);
    request.contextWindow = params.value("context_window").toInt(2048);
    request.mode = InferenceMode::Custom;
    return executeInference(request);
}

// ===== Performance Monitoring =====

ModelMetrics Phase5ModelRouter::getMetrics(const QString& modelId) const {
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics.value(modelId, ModelMetrics());
}

QList<ModelMetrics> Phase5ModelRouter::getAllMetrics() const {
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics.values();
}

QJsonObject Phase5ModelRouter::getHealthStatus() const {
    QJsonObject status;
    status["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    status["totalRequests"] = static_cast<qint64>(m_totalRequests);
    status["routingStrategy"] = m_currentRoutingStrategy;
    status["profilingEnabled"] = m_profilingEnabled;
    
    QJsonArray endpoints;
    QMutexLocker locker(&m_endpointMutex);
    for (auto it = m_endpoints.constBegin(); it != m_endpoints.constEnd(); ++it) {
        QJsonObject ep;
        ep["id"] = it.value()->id;
        ep["name"] = it.value()->name;
        ep["enabled"] = it.value()->enabled;
        ep["active"] = it.value()->activeRequests;
        ep["maxConcurrent"] = it.value()->maxConcurrentRequests;
        endpoints.append(ep);
    }
    status["endpoints"] = endpoints;
    
    return status;
}

QJsonObject Phase5ModelRouter::getDashboardData() const {
    QJsonObject data;
    data["health"] = getHealthStatus();
    data["analytics"] = m_analytics->getAnalyticsData();
    
    QJsonArray metricsArray;
    QMutexLocker locker(&m_metricsMutex);
    for (auto it = m_metrics.constBegin(); it != m_metrics.constEnd(); ++it) {
        QJsonObject m;
        m["modelId"] = it.value().modelId;
        m["modelName"] = it.value().modelName;
        m["tokensPerSecond"] = it.value().tokensPerSecond;
        m["avgLatency"] = it.value().avgLatencyMs;
        m["requests"] = it.value().requestCount;
        m["errors"] = it.value().errorCount;
        metricsArray.append(m);
    }
    data["metrics"] = metricsArray;
    
    return data;
}

void Phase5ModelRouter::enableProfiling(bool enable) {
    m_profilingEnabled = enable;
}

// ===== Model Pooling =====

bool Phase5ModelRouter::ensureModelPooled(const QString& modelId, int poolSize) {
    return m_poolManager->addToPool(modelId, poolSize);
}

void Phase5ModelRouter::releaseModelPool(const QString& modelId) {
    m_poolManager->removeFromPool(modelId);
}

void Phase5ModelRouter::setModelCacheSize(int sizeBytes) {
    m_modelCacheSize = sizeBytes;
    m_poolManager->setCacheSize(sizeBytes);
}

// ===== Configuration =====

bool Phase5ModelRouter::saveConfiguration(const QString& path) const {
    QJsonObject config = getConfiguration();
    QJsonDocument doc(config);
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool Phase5ModelRouter::loadConfiguration(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    QJsonObject config = doc.object();
    m_currentRoutingStrategy = config.value("routingStrategy").toString("adaptive");
    m_modelCacheSize = config.value("cacheSize").toInt(1024 * 1024 * 1024);
    m_profilingEnabled = config.value("profilingEnabled").toBool(true);
    
    return true;
}

QJsonObject Phase5ModelRouter::getConfiguration() const {
    QJsonObject config;
    config["routingStrategy"] = m_currentRoutingStrategy;
    config["cacheSize"] = m_modelCacheSize;
    config["profilingEnabled"] = m_profilingEnabled;
    config["totalRequests"] = static_cast<qint64>(m_totalRequests);
    return config;
}

// ===== Private Methods =====

void Phase5ModelRouter::updateMetrics(const QString& modelId, double latencyMs, bool success) {
    QMutexLocker locker(&m_endpointMutex);
    
    auto it = m_endpoints.find(modelId);
    if (it == m_endpoints.end()) return;
    
    auto endpoint = it.value();
    endpoint->averageResponseTime = (endpoint->averageResponseTime * 0.9) + (latencyMs * 0.1);
    
    if (!success) {
        endpoint->consecutiveFailures++;
        if (endpoint->consecutiveFailures > 3) {
            endpoint->healthy = false;
        }
    } else {
        endpoint->consecutiveFailures = 0;
        endpoint->healthy = true;
    }
}

void Phase5ModelRouter::performHealthChecks() {
    QMutexLocker locker(&m_endpointMutex);
    
    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it) {
        auto& endpoint = it.value();
        
        // Check if model is unhealthy (too many errors, high latency, etc.)
        QMutexLocker metricsLocker(&m_metricsMutex);
        const auto& metrics = m_metrics[endpoint->id];
        
        double errorRate = metrics.requestCount > 0 ? 
            (double)metrics.errorCount / metrics.requestCount : 0.0;
        
        if (errorRate > 0.5) {
            emit modelUnhealthy(endpoint->id, QString("High error rate: %1%").arg(errorRate * 100.0));
            endpoint->enabled = false;
        }
        
        if (metrics.avgLatencyMs > 30000) { // > 30 seconds
            emit modelUnhealthy(endpoint->id, QString("High latency: %1ms").arg(metrics.avgLatencyMs));
        }
    }
}

QString Phase5ModelRouter::getConfigurationPath() const {
    return QDir::homePath() + "/.rawrxd/model_router_config.json";
}
