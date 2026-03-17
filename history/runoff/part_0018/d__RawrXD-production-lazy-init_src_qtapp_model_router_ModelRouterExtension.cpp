#include "ModelRouterExtension.h"
#include <QFile>
#include <QRandomGenerator>
#include <QDebug>
#include <algorithm>
#include <cmath>

// ============ ModelEndpoint Serialization ============

QJsonObject ModelEndpoint::toJson() const {
    QJsonObject obj;
    obj["id"] = id;
    obj["name"] = name;
    obj["type"] = type;
    obj["path"] = path;
    obj["apiKey"] = apiKey;
    obj["priority"] = priority;
    obj["weight"] = weight;
    obj["enabled"] = enabled;
    obj["maxConcurrentRequests"] = maxConcurrentRequests;
    obj["estimatedCostPerToken"] = static_cast<double>(estimatedCostPerToken);
    
    QJsonObject metaObj;
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        metaObj[it.key()] = it.value();
    }
    obj["metadata"] = metaObj;
    
    obj["healthy"] = healthy;
    obj["consecutiveFailures"] = consecutiveFailures;
    obj["lastHealthCheck"] = lastHealthCheck.toString(Qt::ISODate);
    obj["averageResponseTime"] = averageResponseTime;
    obj["activeRequests"] = activeRequests;
    
    return obj;
}

ModelEndpoint ModelEndpoint::fromJson(const QJsonObject& json) {
    ModelEndpoint endpoint;
    endpoint.id = json["id"].toString();
    endpoint.name = json["name"].toString();
    endpoint.type = json["type"].toString();
    endpoint.path = json["path"].toString();
    endpoint.apiKey = json["apiKey"].toString();
    endpoint.priority = json["priority"].toInt(50);
    endpoint.weight = json["weight"].toDouble(1.0);
    endpoint.enabled = json["enabled"].toBool(true);
    endpoint.maxConcurrentRequests = json["maxConcurrentRequests"].toInt(10);
    endpoint.estimatedCostPerToken = json["estimatedCostPerToken"].toDouble(0);
    
    QJsonObject metaObj = json["metadata"].toObject();
    for (auto it = metaObj.begin(); it != metaObj.end(); ++it) {
        endpoint.metadata[it.key()] = it.value().toString();
    }
    
    endpoint.healthy = json["healthy"].toBool(true);
    endpoint.consecutiveFailures = json["consecutiveFailures"].toInt(0);
    endpoint.lastHealthCheck = QDateTime::fromString(json["lastHealthCheck"].toString(), Qt::ISODate);
    endpoint.averageResponseTime = json["averageResponseTime"].toDouble(0);
    endpoint.activeRequests = json["activeRequests"].toInt(0);
    
    return endpoint;
}

// ============ RoutingDecision Serialization ============

QJsonObject RoutingDecision::toJson() const {
    QJsonObject obj;
    obj["endpointId"] = endpointId;
    obj["reason"] = reason;
    obj["confidence"] = confidence;
    
    QJsonArray altArray;
    for (const QString& alt : alternativeEndpoints) {
        altArray.append(alt);
    }
    obj["alternativeEndpoints"] = altArray;
    
    QJsonObject scoresObj;
    for (auto it = scores.begin(); it != scores.end(); ++it) {
        scoresObj[it.key()] = it.value();
    }
    obj["scores"] = scoresObj;
    
    obj["decidedAt"] = decidedAt.toString(Qt::ISODate);
    
    return obj;
}

// ============ ModelHealth Serialization ============

QJsonObject ModelHealth::toJson() const {
    QJsonObject obj;
    obj["endpointId"] = endpointId;
    obj["healthy"] = healthy;
    obj["status"] = status;
    obj["responseTime"] = responseTime;
    obj["errorRate"] = errorRate;
    obj["activeRequests"] = activeRequests;
    obj["totalRequests"] = totalRequests;
    obj["failedRequests"] = failedRequests;
    obj["lastCheck"] = lastCheck.toString(Qt::ISODate);
    obj["failureReason"] = failureReason;
    
    return obj;
}

// ============ CircuitBreaker Implementation ============

CircuitBreaker::CircuitBreaker(int failureThreshold, int timeout)
    : m_state(Closed), m_failureCount(0), m_failureThreshold(failureThreshold), m_timeout(timeout) {}

bool CircuitBreaker::allowRequest() {
    QMutexLocker locker(&m_mutex);
    
    if (m_state == Closed) {
        return true;
    } else if (m_state == Open) {
        // Check if timeout has elapsed
        qint64 elapsed = m_lastFailureTime.msecsTo(QDateTime::currentDateTime());
        if (elapsed >= m_timeout) {
            m_state = HalfOpen;
            return true;  // Allow one request to test
        }
        return false;
    } else {  // HalfOpen
        return true;
    }
}

void CircuitBreaker::recordSuccess() {
    QMutexLocker locker(&m_mutex);
    m_failureCount = 0;
    if (m_state == HalfOpen) {
        m_state = Closed;
    }
}

void CircuitBreaker::recordFailure() {
    QMutexLocker locker(&m_mutex);
    m_failureCount++;
    m_lastFailureTime = QDateTime::currentDateTime();
    
    if (m_failureCount >= m_failureThreshold) {
        m_state = Open;
    }
}

void CircuitBreaker::reset() {
    QMutexLocker locker(&m_mutex);
    m_state = Closed;
    m_failureCount = 0;
}

// ============ ModelRouterExtension Implementation ============

ModelRouterExtension::ModelRouterExtension(QObject* parent)
    : QObject(parent)
    , m_routingStrategy(RoutingStrategy::RoundRobin)
    , m_roundRobinIndex(0)
    , m_healthCheckInterval(60000)
    , m_healthMonitoringEnabled(false)
    , m_totalRequests(0)
    , m_successfulRequests(0)
    , m_failedRequests(0)
{
    m_healthCheckTimer = new QTimer(this);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &ModelRouterExtension::performHealthCheck);
}

ModelRouterExtension::~ModelRouterExtension() {
    if (m_healthCheckTimer) {
        m_healthCheckTimer->stop();
    }
}

// ============ Endpoint Management ============

bool ModelRouterExtension::registerEndpoint(const ModelEndpoint& endpoint) {
    QMutexLocker locker(&m_mutex);
    
    if (endpoint.id.isEmpty()) {
        qWarning() << "Cannot register endpoint with empty ID";
        return false;
    }
    
    if (m_endpoints.contains(endpoint.id)) {
        qWarning() << "Endpoint already registered:" << endpoint.id;
        return false;
    }
    
    m_endpoints[endpoint.id] = endpoint;
    m_circuitBreakers[endpoint.id] = std::make_shared<CircuitBreaker>();
    m_activeRequests[endpoint.id] = QList<QString>();
    m_endpointRequestCounts[endpoint.id] = 0;
    m_endpointSuccessCounts[endpoint.id] = 0;
    m_endpointFailureCounts[endpoint.id] = 0;
    m_endpointResponseTimes[endpoint.id] = 0.0;
    
    ModelHealth health;
    health.endpointId = endpoint.id;
    health.healthy = endpoint.healthy;
    health.status = endpoint.healthy ? "healthy" : "unknown";
    m_healthStatus[endpoint.id] = health;
    
    qDebug() << "Registered endpoint:" << endpoint.id << endpoint.name;
    
    return true;
}

bool ModelRouterExtension::unregisterEndpoint(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_endpoints.contains(endpointId)) {
        qWarning() << "Endpoint not found:" << endpointId;
        return false;
    }
    
    m_endpoints.remove(endpointId);
    m_circuitBreakers.remove(endpointId);
    m_activeRequests.remove(endpointId);
    m_healthStatus.remove(endpointId);
    m_endpointRequestCounts.remove(endpointId);
    m_endpointSuccessCounts.remove(endpointId);
    m_endpointFailureCounts.remove(endpointId);
    m_endpointResponseTimes.remove(endpointId);
    
    qDebug() << "Unregistered endpoint:" << endpointId;
    
    return true;
}

bool ModelRouterExtension::updateEndpoint(const QString& endpointId, const ModelEndpoint& endpoint) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_endpoints.contains(endpointId)) {
        qWarning() << "Endpoint not found for update:" << endpointId;
        return false;
    }
    
    m_endpoints[endpointId] = endpoint;
    qDebug() << "Updated endpoint:" << endpointId;
    
    return true;
}

ModelEndpoint ModelRouterExtension::getEndpoint(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    return m_endpoints.value(endpointId);
}

QList<ModelEndpoint> ModelRouterExtension::getAllEndpoints() const {
    QMutexLocker locker(&m_mutex);
    return m_endpoints.values();
}

QList<ModelEndpoint> ModelRouterExtension::getHealthyEndpoints() const {
    QMutexLocker locker(&m_mutex);
    QList<ModelEndpoint> healthy;
    
    for (const ModelEndpoint& endpoint : m_endpoints) {
        if (endpoint.enabled && endpoint.healthy) {
            healthy.append(endpoint);
        }
    }
    
    return healthy;
}

void ModelRouterExtension::setEndpointEnabled(const QString& endpointId, bool enabled) {
    QMutexLocker locker(&m_mutex);
    
    if (m_endpoints.contains(endpointId)) {
        m_endpoints[endpointId].enabled = enabled;
        qDebug() << "Endpoint" << endpointId << (enabled ? "enabled" : "disabled");
    }
}

// ============ Routing Logic ============

RoutingDecision ModelRouterExtension::routeRequest(const RoutingRequest& request) {
    QMutexLocker locker(&m_mutex);
    
    RoutingDecision decision;
    m_totalRequests++;
    
    // Filter endpoints based on constraints
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    
    if (candidates.isEmpty()) {
        qWarning() << "No available endpoints for routing";
        decision.reason = "No available endpoints";
        decision.confidence = 0.0;
        return decision;
    }
    
    // Check if specific model required
    if (!request.requiredModel.isEmpty()) {
        for (const ModelEndpoint& ep : candidates) {
            if (ep.id == request.requiredModel || ep.name == request.requiredModel) {
                decision.endpointId = ep.id;
                decision.reason = "Required model match";
                decision.confidence = 1.0;
                emit routingDecisionMade(decision);
                return decision;
    }
        }
        qWarning() << "Required model not available:" << request.requiredModel;
        decision.reason = "Required model unavailable";
        decision.confidence = 0.0;
        return decision;
    }
    
    // Route based on strategy
    QString selectedId;
    switch (m_routingStrategy) {
        case RoutingStrategy::RoundRobin:
            selectedId = routeRoundRobin(request);
            decision.reason = "Round-robin selection";
            break;
        case RoutingStrategy::WeightedRandom:
            selectedId = routeWeightedRandom(request);
            decision.reason = "Weighted random selection";
            break;
        case RoutingStrategy::LeastConnections:
            selectedId = routeLeastConnections(request);
            decision.reason = "Least connections";
            break;
        case RoutingStrategy::ResponseTimeBased:
            selectedId = routeResponseTimeBased(request);
            decision.reason = "Best response time";
            break;
        case RoutingStrategy::Adaptive:
            selectedId = routeAdaptive(request);
            decision.reason = "Adaptive routing";
            break;
        case RoutingStrategy::PriorityBased:
            selectedId = routePriorityBased(request);
            decision.reason = "Priority-based selection";
            break;
        case RoutingStrategy::CostOptimized:
            selectedId = routeCostOptimized(request);
            decision.reason = "Cost-optimized selection";
            break;
        case RoutingStrategy::Custom:
            if (m_customRoutingFunction) {
                selectedId = m_customRoutingFunction(request, candidates);
                decision.reason = "Custom routing function";
            } else {
                selectedId = routeRoundRobin(request);
                decision.reason = "Fallback to round-robin";
            }
            break;
    }
    
    decision.endpointId = selectedId;
    decision.confidence = selectedId.isEmpty() ? 0.0 : 0.9;
    
    // Calculate scores for all candidates
    for (const ModelEndpoint& ep : candidates) {
        decision.scores[ep.id] = scoreEndpoint(ep, request);
    }
    
    // Populate alternatives
    for (const ModelEndpoint& ep : candidates) {
        if (ep.id != selectedId) {
            decision.alternativeEndpoints.append(ep.id);
        }
    }
    
    emit routingDecisionMade(decision);
    
    return decision;
}

void ModelRouterExtension::setRoutingStrategy(RoutingStrategy strategy) {
    QMutexLocker locker(&m_mutex);
    m_routingStrategy = strategy;
    qDebug() << "Routing strategy changed to:" << static_cast<int>(strategy);
}

void ModelRouterExtension::setCustomRoutingFunction(std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> func) {
    QMutexLocker locker(&m_mutex);
    m_customRoutingFunction = func;
    qDebug() << "Custom routing function set";
}

QJsonObject ModelRouterExtension::getRoutingStatistics() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    stats["totalRequests"] = static_cast<qint64>(m_totalRequests);
    stats["successfulRequests"] = static_cast<qint64>(m_successfulRequests);
    stats["failedRequests"] = static_cast<qint64>(m_failedRequests);
    stats["successRate"] = m_totalRequests > 0 ? 
        static_cast<double>(m_successfulRequests) / m_totalRequests : 0.0;
    
    QJsonObject endpointStats;
    for (auto it = m_endpointRequestCounts.begin(); it != m_endpointRequestCounts.end(); ++it) {
        QJsonObject epStats;
        epStats["requests"] = it.value();
        epStats["successes"] = m_endpointSuccessCounts.value(it.key(), 0);
        epStats["failures"] = m_endpointFailureCounts.value(it.key(), 0);
        epStats["averageResponseTime"] = m_endpointResponseTimes.value(it.key(), 0.0);
        endpointStats[it.key()] = epStats;
    }
    stats["endpointStatistics"] = endpointStats;
    
    return stats;
}

// ============ Health Monitoring ============

ModelHealth ModelRouterExtension::checkEndpointHealth(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    ModelHealth health = m_healthStatus.value(endpointId);
    
    if (!m_endpoints.contains(endpointId)) {
        health.healthy = false;
        health.status = "not_registered";
        health.failureReason = "Endpoint not registered";
        return health;
    }
    
    const ModelEndpoint& endpoint = m_endpoints[endpointId];
    
    // Perform health check
    bool isHealthy = performHealthCheckForEndpoint(endpointId);
    
    health.endpointId = endpointId;
    health.healthy = isHealthy;
    health.status = isHealthy ? "healthy" : "unhealthy";
    health.activeRequests = m_activeRequests.value(endpointId).count();
    health.totalRequests = m_endpointRequestCounts.value(endpointId, 0);
    health.failedRequests = m_endpointFailureCounts.value(endpointId, 0);
    health.responseTime = m_endpointResponseTimes.value(endpointId, 0.0);
    health.errorRate = health.totalRequests > 0 ? 
        static_cast<double>(health.failedRequests) / health.totalRequests : 0.0;
    health.lastCheck = QDateTime::currentDateTime();
    
    if (endpoint.consecutiveFailures >= 3) {
        health.status = "degraded";
    }
    
    m_healthStatus[endpointId] = health;
    
    return health;
}

QMap<QString, ModelHealth> ModelRouterExtension::checkAllEndpointsHealth() {
    QMap<QString, ModelHealth> healthMap;
    
    QStringList endpointIds;
    {
        QMutexLocker locker(&m_mutex);
        endpointIds = m_endpoints.keys();
    }
    
    for (const QString& id : endpointIds) {
        healthMap[id] = checkEndpointHealth(id);
    }
    
    return healthMap;
}

void ModelRouterExtension::enableHealthMonitoring(int intervalMs) {
    QMutexLocker locker(&m_mutex);
    
    m_healthCheckInterval = intervalMs;
    m_healthMonitoringEnabled = true;
    m_healthCheckTimer->start(m_healthCheckInterval);
    
    qDebug() << "Health monitoring enabled with interval:" << intervalMs << "ms";
}

void ModelRouterExtension::disableHealthMonitoring() {
    QMutexLocker locker(&m_mutex);
    
    m_healthMonitoringEnabled = false;
    m_healthCheckTimer->stop();
    
    qDebug() << "Health monitoring disabled";
}

void ModelRouterExtension::setHealthCheckInterval(int intervalMs) {
    QMutexLocker locker(&m_mutex);
    
    m_healthCheckInterval = intervalMs;
    if (m_healthMonitoringEnabled) {
        m_healthCheckTimer->setInterval(m_healthCheckInterval);
    }
}

QMap<QString, ModelHealth> ModelRouterExtension::getHealthStatus() const {
    QMutexLocker locker(&m_mutex);
    return m_healthStatus;
}

// ============ Circuit Breaker ============

CircuitBreaker* ModelRouterExtension::getCircuitBreaker(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_circuitBreakers.contains(endpointId)) {
        return m_circuitBreakers[endpointId].get();
    }
    
    return nullptr;
}

void ModelRouterExtension::resetCircuitBreaker(const QString& endpointId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_circuitBreakers.contains(endpointId)) {
        m_circuitBreakers[endpointId]->reset();
        qDebug() << "Circuit breaker reset for:" << endpointId;
        emit circuitBreakerClosed(endpointId);
    }
}

void ModelRouterExtension::resetAllCircuitBreakers() {
    QMutexLocker locker(&m_mutex);
    
    for (auto it = m_circuitBreakers.begin(); it != m_circuitBreakers.end(); ++it) {
        it.value()->reset();
    }
    
    qDebug() << "All circuit breakers reset";
}

// ============ Fallback Handling ============

void ModelRouterExtension::setFallbackChain(const QStringList& endpointIds) {
    QMutexLocker locker(&m_mutex);
    m_fallbackChain = endpointIds;
    qDebug() << "Fallback chain set:" << endpointIds;
}

RoutingDecision ModelRouterExtension::routeWithFallback(const RoutingRequest& request) {
    // Try primary routing first
    RoutingDecision decision = routeRequest(request);
    
    if (!decision.endpointId.isEmpty()) {
        CircuitBreaker* cb = getCircuitBreaker(decision.endpointId);
        if (cb && cb->allowRequest()) {
            return decision;
        }
    }
    
    // Try fallback chain
    QMutexLocker locker(&m_mutex);
    for (const QString& fallbackId : m_fallbackChain) {
        if (!m_endpoints.contains(fallbackId)) continue;
        
        const ModelEndpoint& ep = m_endpoints[fallbackId];
        if (!ep.enabled || !ep.healthy) continue;
        
        CircuitBreaker* cb = m_circuitBreakers.value(fallbackId).get();
        if (cb && !cb->allowRequest()) continue;
        
        decision.endpointId = fallbackId;
        decision.reason = "Fallback endpoint";
        decision.confidence = 0.7;
        return decision;
    }
    
    qWarning() << "All fallback endpoints exhausted";
    decision.endpointId.clear();
    decision.reason = "All endpoints unavailable";
    decision.confidence = 0.0;
    
    return decision;
}

// ============ Request Tracking ============

void ModelRouterExtension::recordRequestStart(const QString& endpointId, const QString& requestId) {
    QMutexLocker locker(&m_mutex);
    
    if (m_activeRequests.contains(endpointId)) {
        m_activeRequests[endpointId].append(requestId);
    }
    
    if (m_endpoints.contains(endpointId)) {
        m_endpoints[endpointId].activeRequests++;
    }
    
    m_endpointRequestCounts[endpointId]++;
}

void ModelRouterExtension::recordRequestComplete(const QString& endpointId, const QString& requestId, 
                                                  bool success, double responseTime) {
    QMutexLocker locker(&m_mutex);
    
    // Remove from active requests
    if (m_activeRequests.contains(endpointId)) {
        m_activeRequests[endpointId].removeAll(requestId);
    }
    
    // Update endpoint
    if (m_endpoints.contains(endpointId)) {
        ModelEndpoint& ep = m_endpoints[endpointId];
        ep.activeRequests--;
        
        // Update average response time
        double oldAvg = ep.averageResponseTime;
        int count = m_endpointRequestCounts.value(endpointId, 1);
        ep.averageResponseTime = ((oldAvg * (count - 1)) + responseTime) / count;
        m_endpointResponseTimes[endpointId] = ep.averageResponseTime;
        
        if (success) {
            m_successfulRequests++;
            m_endpointSuccessCounts[endpointId]++;
            ep.consecutiveFailures = 0;
            
            // Record circuit breaker success
            if (m_circuitBreakers.contains(endpointId)) {
                m_circuitBreakers[endpointId]->recordSuccess();
            }
        } else {
            m_failedRequests++;
            m_endpointFailureCounts[endpointId]++;
            ep.consecutiveFailures++;
            
            // Record circuit breaker failure
            if (m_circuitBreakers.contains(endpointId)) {
                m_circuitBreakers[endpointId]->recordFailure();
                if (m_circuitBreakers[endpointId]->getState() == CircuitBreaker::Open) {
                    emit circuitBreakerOpened(endpointId);
                }
            }
            
            updateEndpointHealth(endpointId, false, "Request failed");
            emit endpointFailed(endpointId, "Request failed");
        }
    }
}

int ModelRouterExtension::getActiveRequestCount(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    return m_activeRequests.value(endpointId).count();
}

int ModelRouterExtension::getTotalRequestCount(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    return m_endpointRequestCounts.value(endpointId, 0);
}

// ============ Load Balancing ============

bool ModelRouterExtension::canAcceptRequest(const QString& endpointId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_endpoints.contains(endpointId)) return false;
    
    const ModelEndpoint& ep = m_endpoints[endpointId];
    if (!ep.enabled || !ep.healthy) return false;
    
    // Check circuit breaker
    auto cbIt = m_circuitBreakers.find(endpointId);
    if (cbIt != m_circuitBreakers.end()) {
        if (!const_cast<CircuitBreaker*>(cbIt.value().get())->allowRequest()) {
            return false;
        }
    }
    
    // Check concurrency limit
    int activeCount = m_activeRequests.value(endpointId).count();
    return activeCount < ep.maxConcurrentRequests;
}

QString ModelRouterExtension::getEndpointWithLeastConnections() const {
    QMutexLocker locker(&m_mutex);
    
    QString bestId;
    int minConnections = INT_MAX;
    
    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it) {
        const ModelEndpoint& ep = it.value();
        if (!ep.enabled || !ep.healthy) continue;
        
        int activeCount = m_activeRequests.value(it.key()).count();
        if (activeCount < minConnections) {
            minConnections = activeCount;
            bestId = it.key();
        }
    }
    
    return bestId;
}

QString ModelRouterExtension::getEndpointWithBestResponseTime() const {
    QMutexLocker locker(&m_mutex);
    
    QString bestId;
    double minResponseTime = std::numeric_limits<double>::max();
    
    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it) {
        const ModelEndpoint& ep = it.value();
        if (!ep.enabled || !ep.healthy) continue;
        
        if (ep.averageResponseTime < minResponseTime && ep.averageResponseTime > 0) {
            minResponseTime = ep.averageResponseTime;
            bestId = it.key();
        }
    }
    
    return bestId;
}

// ============ Configuration ============

QJsonObject ModelRouterExtension::saveConfiguration() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject config;
    config["routingStrategy"] = static_cast<int>(m_routingStrategy);
    config["healthCheckInterval"] = m_healthCheckInterval;
    config["healthMonitoringEnabled"] = m_healthMonitoringEnabled;
    
    QJsonArray endpointsArray;
    for (const ModelEndpoint& ep : m_endpoints) {
        endpointsArray.append(ep.toJson());
    }
    config["endpoints"] = endpointsArray;
    
    QJsonArray fallbackArray;
    for (const QString& id : m_fallbackChain) {
        fallbackArray.append(id);
    }
    config["fallbackChain"] = fallbackArray;
    
    return config;
}

bool ModelRouterExtension::loadConfiguration(const QJsonObject& config) {
    QMutexLocker locker(&m_mutex);
    
    m_routingStrategy = static_cast<RoutingStrategy>(config["routingStrategy"].toInt());
    m_healthCheckInterval = config["healthCheckInterval"].toInt(60000);
    bool healthEnabled = config["healthMonitoringEnabled"].toBool(false);
    
    // Load endpoints
    QJsonArray endpointsArray = config["endpoints"].toArray();
    for (const QJsonValue& val : endpointsArray) {
        ModelEndpoint ep = ModelEndpoint::fromJson(val.toObject());
        // Unlock temporarily to call registerEndpoint
        m_mutex.unlock();
        registerEndpoint(ep);
        m_mutex.lock();
    }
    
    // Load fallback chain
    QJsonArray fallbackArray = config["fallbackChain"].toArray();
    m_fallbackChain.clear();
    for (const QJsonValue& val : fallbackArray) {
        m_fallbackChain.append(val.toString());
    }
    
    // Enable health monitoring if needed
    if (healthEnabled) {
        m_mutex.unlock();
        enableHealthMonitoring(m_healthCheckInterval);
        m_mutex.lock();
    }
    
    qDebug() << "Configuration loaded successfully";
    
    return true;
}

bool ModelRouterExtension::saveToFile(const QString& filePath) const {
    QJsonObject config = saveConfiguration();
    QJsonDocument doc(config);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "Configuration saved to:" << filePath;
    
    return true;
}

bool ModelRouterExtension::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for reading:" << filePath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Failed to parse JSON from file:" << filePath;
        return false;
    }
    
    return loadConfiguration(doc.object());
}

// ============ Statistics ============

QJsonObject ModelRouterExtension::getMetrics() const {
    return getRoutingStatistics();
}

void ModelRouterExtension::resetStatistics() {
    QMutexLocker locker(&m_mutex);
    
    m_totalRequests = 0;
    m_successfulRequests = 0;
    m_failedRequests = 0;
    m_endpointRequestCounts.clear();
    m_endpointSuccessCounts.clear();
    m_endpointFailureCounts.clear();
    
    for (auto it = m_endpoints.begin(); it != m_endpoints.end(); ++it) {
        it.value().consecutiveFailures = 0;
        it.value().averageResponseTime = 0;
    }
    
    qDebug() << "Statistics reset";
}

// ============ Private Slots ============

void ModelRouterExtension::performHealthCheck() {
    checkAllEndpointsHealth();
}

// ============ Private Routing Implementation ============

QString ModelRouterExtension::routeRoundRobin(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    m_roundRobinIndex = (m_roundRobinIndex + 1) % candidates.size();
    return candidates[m_roundRobinIndex].id;
}

QString ModelRouterExtension::routeWeightedRandom(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    double totalWeight = 0;
    for (const ModelEndpoint& ep : candidates) {
        totalWeight += ep.weight;
    }
    
    double random = QRandomGenerator::global()->generateDouble() * totalWeight;
    double cumulative = 0;
    
    for (const ModelEndpoint& ep : candidates) {
        cumulative += ep.weight;
        if (random <= cumulative) {
            return ep.id;
        }
    }
    
    return candidates.last().id;
}

QString ModelRouterExtension::routeLeastConnections(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    QString bestId;
    int minConnections = INT_MAX;
    
    for (const ModelEndpoint& ep : candidates) {
        int activeCount = m_activeRequests.value(ep.id).count();
        if (activeCount < minConnections) {
            minConnections = activeCount;
            bestId = ep.id;
        }
    }
    
    return bestId;
}

QString ModelRouterExtension::routeResponseTimeBased(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    QString bestId;
    double minResponseTime = std::numeric_limits<double>::max();
    
    for (const ModelEndpoint& ep : candidates) {
        if (ep.averageResponseTime < minResponseTime && ep.averageResponseTime > 0) {
            minResponseTime = ep.averageResponseTime;
            bestId = ep.id;
        }
    }
    
    // If no response time data, use round-robin
    if (bestId.isEmpty()) {
        return routeRoundRobin(request);
    }
    
    return bestId;
}

QString ModelRouterExtension::routeAdaptive(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    // Score each endpoint
    QString bestId;
    double bestScore = -1.0;
    
    for (const ModelEndpoint& ep : candidates) {
        double score = scoreEndpoint(ep, request);
        if (score > bestScore) {
            bestScore = score;
            bestId = ep.id;
        }
    }
    
    return bestId;
}

QString ModelRouterExtension::routePriorityBased(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    // Sort by priority (highest first)
    std::sort(candidates.begin(), candidates.end(), [](const ModelEndpoint& a, const ModelEndpoint& b) {
        return a.priority > b.priority;
    });
    
    return candidates.first().id;
}

QString ModelRouterExtension::routeCostOptimized(const RoutingRequest& request) {
    QList<ModelEndpoint> candidates = filterEndpoints(request);
    if (candidates.isEmpty()) return QString();
    
    QString bestId;
    qint64 minCost = LLONG_MAX;
    
    for (const ModelEndpoint& ep : candidates) {
        qint64 estimatedCost = ep.estimatedCostPerToken * request.maxTokens;
        if (estimatedCost < minCost) {
            minCost = estimatedCost;
            bestId = ep.id;
        }
    }
    
    return bestId;
}

// ============ Private Scoring Functions ============

double ModelRouterExtension::scoreEndpoint(const ModelEndpoint& endpoint, const RoutingRequest& request) const {
    double healthScore = calculateHealthScore(endpoint);
    double loadScore = calculateLoadScore(endpoint);
    double costScore = calculateCostScore(endpoint, request);
    
    // Weighted combination
    double score = (healthScore * 0.5) + (loadScore * 0.3) + (costScore * 0.2);
    
    // Boost by priority
    score *= (1.0 + (endpoint.priority / 100.0));
    
    return score;
}

double ModelRouterExtension::calculateHealthScore(const ModelEndpoint& endpoint) const {
    if (!endpoint.healthy) return 0.0;
    
    // Lower consecutive failures = higher score
    double failurePenalty = 1.0 - (endpoint.consecutiveFailures * 0.1);
    failurePenalty = std::max(0.0, failurePenalty);
    
    return failurePenalty;
}

double ModelRouterExtension::calculateLoadScore(const ModelEndpoint& endpoint) const {
    if (endpoint.maxConcurrentRequests == 0) return 1.0;
    
    double utilization = static_cast<double>(endpoint.activeRequests) / endpoint.maxConcurrentRequests;
    return 1.0 - utilization;  // Lower load = higher score
}

double ModelRouterExtension::calculateCostScore(const ModelEndpoint& endpoint, const RoutingRequest& request) const {
    if (endpoint.estimatedCostPerToken == 0) return 1.0;
    
    qint64 estimatedCost = endpoint.estimatedCostPerToken * request.maxTokens;
    
    // Normalize to 0-1 range (assuming max cost of 1000 microcents per request)
    double normalizedCost = std::min(1.0, static_cast<double>(estimatedCost) / 1000.0);
    
    return 1.0 - normalizedCost;  // Lower cost = higher score
}

// ============ Private Health Check Implementation ============

bool ModelRouterExtension::performHealthCheckForEndpoint(const QString& endpointId) {
    if (!m_endpoints.contains(endpointId)) return false;
    
    const ModelEndpoint& endpoint = m_endpoints[endpointId];
    
    // Simple health check: enabled and not too many failures
    bool healthy = endpoint.enabled && endpoint.consecutiveFailures < 5;
    
    // Check circuit breaker
    if (m_circuitBreakers.contains(endpointId)) {
        auto state = m_circuitBreakers[endpointId]->getState();
        if (state == CircuitBreaker::Open) {
            healthy = false;
        }
    }
    
    return healthy;
}

void ModelRouterExtension::updateEndpointHealth(const QString& endpointId, bool healthy, const QString& reason) {
    if (m_endpoints.contains(endpointId)) {
        ModelEndpoint& ep = m_endpoints[endpointId];
        bool wasHealthy = ep.healthy;
        ep.healthy = healthy;
        ep.lastHealthCheck = QDateTime::currentDateTime();
        
        if (wasHealthy != healthy) {
            emit endpointHealthChanged(endpointId, healthy);
        }
    }
    
    if (m_healthStatus.contains(endpointId)) {
        ModelHealth& health = m_healthStatus[endpointId];
        health.healthy = healthy;
        health.status = healthy ? "healthy" : "unhealthy";
        health.failureReason = healthy ? QString() : reason;
        health.lastCheck = QDateTime::currentDateTime();
    }
}

// ============ Private Helper Functions ============

bool ModelRouterExtension::matchesConstraints(const ModelEndpoint& endpoint, const RoutingRequest& request) const {
    // Check if endpoint matches request constraints
    for (auto it = request.constraints.begin(); it != request.constraints.end(); ++it) {
        QString constraintKey = it.key();
        QString constraintValue = it.value();
        
        if (endpoint.metadata.contains(constraintKey)) {
            if (endpoint.metadata[constraintKey] != constraintValue) {
                return false;
            }
        }
    }
    
    return true;
}

QList<ModelEndpoint> ModelRouterExtension::filterEndpoints(const RoutingRequest& request) const {
    QList<ModelEndpoint> filtered;
    
    for (const ModelEndpoint& ep : m_endpoints) {
        // Must be enabled and healthy
        if (!ep.enabled || !ep.healthy) continue;
        
        // Check circuit breaker
        auto cbIt = m_circuitBreakers.find(ep.id);
        if (cbIt != m_circuitBreakers.end()) {
            if (!const_cast<CircuitBreaker*>(cbIt.value().get())->allowRequest()) {
                continue;
            }
        }
        
        // Check concurrency limit
        int activeCount = m_activeRequests.value(ep.id).count();
        if (activeCount >= ep.maxConcurrentRequests) continue;
        
        // Check constraints
        if (!matchesConstraints(ep, request)) continue;
        
        // Check preferred models
        if (!request.preferredModels.isEmpty()) {
            bool isPreferred = false;
            for (const QString& preferred : request.preferredModels) {
                if (ep.id == preferred || ep.name == preferred) {
                    isPreferred = true;
                    break;
                }
            }
            if (!isPreferred) continue;
        }
        
        filtered.append(ep);
    }
    
    return filtered;
}
