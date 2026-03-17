#ifndef CHAT_PRODUCTION_INFRASTRUCTURE_HPP
#define CHAT_PRODUCTION_INFRASTRUCTURE_HPP

/**
 * @file chat_production_infrastructure.hpp
 * @brief Production-ready infrastructure for AI Chat Panel
 * 
 * Implements:
 * - Circuit breaker pattern for external services
 * - Retry logic with exponential backoff
 * - Health checks for chat pane components
 * - Response caching for repeated queries
 * - Real-time metrics dashboard
 * - Performance profiling integration
 * - User interaction analytics
 */

#include <QObject>
#include <QString>
#include <QHash>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QThread>
#include <functional>
#include <atomic>
#include <memory>
#include <algorithm>

namespace RawrXD {
namespace Chat {

/**
 * @class CircuitBreaker
 * @brief Implements the Circuit Breaker pattern for external service calls
 * 
 * States:
 * - CLOSED: Normal operation, requests pass through
 * - OPEN: Failures exceeded threshold, requests fail fast
 * - HALF_OPEN: Testing if service recovered
 */
class CircuitBreaker : public QObject {
    Q_OBJECT
    
public:
    enum State { CLOSED, OPEN, HALF_OPEN };
    Q_ENUM(State)
    
    struct Config {
        int failureThreshold = 5;         // Failures before opening
        int successThreshold = 3;         // Successes before closing from half-open
        int timeoutMs = 30000;            // Timeout for recovery attempt
        int halfOpenMaxAttempts = 3;      // Max concurrent attempts in half-open
    };
    
    explicit CircuitBreaker(const QString& serviceName, QObject* parent = nullptr)
        : QObject(parent), m_serviceName(serviceName), m_state(CLOSED) {}
    
    void setConfig(const Config& config) { m_config = config; }
    
    /**
     * @brief Attempt to execute an operation through the circuit breaker
     * @return true if operation should proceed, false if circuit is open
     */
    bool allowRequest() {
        QMutexLocker locker(&m_mutex);
        
        switch (m_state) {
            case CLOSED:
                return true;
                
            case OPEN:
                if (m_lastFailureTime.msecsTo(QDateTime::currentDateTime()) >= m_config.timeoutMs) {
                    m_state = HALF_OPEN;
                    m_halfOpenAttempts = 0;
                    emit stateChanged(m_serviceName, HALF_OPEN);
                    return true;
                }
                return false;
                
            case HALF_OPEN:
                if (m_halfOpenAttempts < m_config.halfOpenMaxAttempts) {
                    ++m_halfOpenAttempts;
                    return true;
                }
                return false;
        }
        return false;
    }
    
    /**
     * @brief Record a successful operation
     */
    void recordSuccess() {
        QMutexLocker locker(&m_mutex);
        m_failureCount = 0;
        
        if (m_state == HALF_OPEN) {
            ++m_successCount;
            if (m_successCount >= m_config.successThreshold) {
                m_state = CLOSED;
                m_successCount = 0;
                emit stateChanged(m_serviceName, CLOSED);
                emit recovered(m_serviceName);
            }
        }
    }
    
    /**
     * @brief Record a failed operation
     */
    void recordFailure() {
        QMutexLocker locker(&m_mutex);
        ++m_failureCount;
        m_lastFailureTime = QDateTime::currentDateTime();
        
        if (m_state == CLOSED && m_failureCount >= m_config.failureThreshold) {
            m_state = OPEN;
            emit stateChanged(m_serviceName, OPEN);
            emit tripped(m_serviceName, m_failureCount);
        } else if (m_state == HALF_OPEN) {
            m_state = OPEN;
            m_successCount = 0;
            emit stateChanged(m_serviceName, OPEN);
        }
    }
    
    State state() const { return m_state; }
    QString serviceName() const { return m_serviceName; }
    int failureCount() const { return m_failureCount; }
    
signals:
    void stateChanged(const QString& service, State newState);
    void tripped(const QString& service, int failureCount);
    void recovered(const QString& service);
    
private:
    QString m_serviceName;
    Config m_config;
    State m_state;
    QMutex m_mutex;
    int m_failureCount = 0;
    int m_successCount = 0;
    int m_halfOpenAttempts = 0;
    QDateTime m_lastFailureTime;
};

/**
 * @class RetryPolicy
 * @brief Implements retry logic with exponential backoff
 */
class RetryPolicy : public QObject {
    Q_OBJECT
    
public:
    struct Config {
        int maxRetries = 3;
        int initialDelayMs = 100;
        int maxDelayMs = 5000;
        double backoffMultiplier = 2.0;
        bool jitterEnabled = true;
    };
    
    explicit RetryPolicy(QObject* parent = nullptr) : QObject(parent) {}
    
    void setConfig(const Config& config) { m_config = config; }
    
    /**
     * @brief Execute an operation with retry logic
     * @param operation The operation to execute (returns true on success)
     * @param onRetry Callback called before each retry (receives attempt number)
     * @return true if operation eventually succeeded
     */
    template<typename Operation, typename RetryCallback>
    bool executeWithRetry(Operation operation, RetryCallback onRetry) {
        int attempt = 0;
        int delayMs = m_config.initialDelayMs;
        
        while (attempt <= m_config.maxRetries) {
            try {
                if (operation()) {
                    if (attempt > 0) {
                        emit retrySucceeded(attempt);
                    }
                    return true;
                }
            } catch (const std::exception& e) {
                qWarning() << "[RetryPolicy] Attempt" << attempt << "failed:" << e.what();
            }
            
            if (attempt < m_config.maxRetries) {
                onRetry(attempt + 1);
                emit retrying(attempt + 1, delayMs);
                
                // Wait with exponential backoff
                QThread::msleep(delayMs);
                
                // Calculate next delay
                delayMs = std::min(
                    static_cast<int>(delayMs * m_config.backoffMultiplier),
                    m_config.maxDelayMs
                );
                
                // Add jitter if enabled
                if (m_config.jitterEnabled) {
                    delayMs += (std::rand() % (delayMs / 4));
                }
            }
            
            ++attempt;
        }
        
        emit retryExhausted(m_config.maxRetries);
        return false;
    }
    
    /**
     * @brief Calculate delay for a specific retry attempt
     */
    int calculateDelay(int attempt) const {
        int delayMs = m_config.initialDelayMs;
        for (int i = 0; i < attempt; ++i) {
            delayMs = std::min(
                static_cast<int>(delayMs * m_config.backoffMultiplier),
                m_config.maxDelayMs
            );
        }
        return delayMs;
    }
    
signals:
    void retrying(int attempt, int delayMs);
    void retrySucceeded(int attemptCount);
    void retryExhausted(int maxRetries);
    
private:
    Config m_config;
};

/**
 * @class HealthMonitor
 * @brief Monitors health of chat panel components
 */
class HealthMonitor : public QObject {
    Q_OBJECT
    
public:
    enum ComponentHealth { HEALTHY, DEGRADED, UNHEALTHY, UNKNOWN };
    Q_ENUM(ComponentHealth)
    
    struct HealthStatus {
        ComponentHealth status = UNKNOWN;
        QString message;
        QDateTime lastCheck;
        qint64 latencyMs = 0;
        QJsonObject details;
    };
    
    explicit HealthMonitor(QObject* parent = nullptr) : QObject(parent) {
        m_checkTimer = new QTimer(this);
        connect(m_checkTimer, &QTimer::timeout, this, &HealthMonitor::runHealthChecks);
    }
    
    void registerComponent(const QString& name, std::function<HealthStatus()> checker) {
        m_healthCheckers[name] = checker;
        m_healthStatuses[name] = HealthStatus{UNKNOWN, "Not checked", QDateTime(), 0, {}};
    }
    
    void startMonitoring(int intervalMs = 30000) {
        m_checkTimer->start(intervalMs);
        runHealthChecks();
    }
    
    void stopMonitoring() {
        m_checkTimer->stop();
    }
    
    HealthStatus getComponentHealth(const QString& name) const {
        return m_healthStatuses.value(name, HealthStatus{UNKNOWN, "Component not registered", QDateTime(), 0, {}});
    }
    
    QJsonObject getFullHealthReport() const {
        QJsonObject report;
        report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        QJsonObject components;
        bool allHealthy = true;
        
        for (auto it = m_healthStatuses.begin(); it != m_healthStatuses.end(); ++it) {
            QJsonObject comp;
            comp["status"] = it.value().status == HEALTHY ? "healthy" : 
                            (it.value().status == DEGRADED ? "degraded" : 
                            (it.value().status == UNHEALTHY ? "unhealthy" : "unknown"));
            comp["message"] = it.value().message;
            comp["lastCheck"] = it.value().lastCheck.toString(Qt::ISODate);
            comp["latencyMs"] = it.value().latencyMs;
            comp["details"] = it.value().details;
            components[it.key()] = comp;
            
            if (it.value().status != HEALTHY) {
                allHealthy = false;
            }
        }
        
        report["components"] = components;
        report["overallHealthy"] = allHealthy;
        
        return report;
    }
    
public slots:
    void runHealthChecks() {
        for (auto it = m_healthCheckers.begin(); it != m_healthCheckers.end(); ++it) {
            QElapsedTimer timer;
            timer.start();
            
            HealthStatus status = it.value()();
            status.latencyMs = timer.elapsed();
            status.lastCheck = QDateTime::currentDateTime();
            
            ComponentHealth oldStatus = m_healthStatuses[it.key()].status;
            m_healthStatuses[it.key()] = status;
            
            if (status.status != oldStatus) {
                emit healthChanged(it.key(), status.status, status.message);
            }
            
            if (status.status == UNHEALTHY) {
                emit componentUnhealthy(it.key(), status.message);
            }
        }
        
        emit healthCheckCompleted(getFullHealthReport());
    }
    
signals:
    void healthChanged(const QString& component, ComponentHealth newHealth, const QString& message);
    void componentUnhealthy(const QString& component, const QString& reason);
    void healthCheckCompleted(const QJsonObject& report);
    
private:
    QTimer* m_checkTimer;
    QHash<QString, std::function<HealthStatus()>> m_healthCheckers;
    QHash<QString, HealthStatus> m_healthStatuses;
};

/**
 * @class ResponseCache
 * @brief Caches responses for repeated queries
 */
class ResponseCache : public QObject {
    Q_OBJECT
    
public:
    struct CacheEntry {
        QString response;
        QDateTime timestamp;
        int hitCount = 0;
        qint64 ttlMs;
    };
    
    struct Config {
        int maxEntries = 1000;
        qint64 defaultTtlMs = 300000;  // 5 minutes
        bool compressionEnabled = true;
        qint64 maxEntrySizeBytes = 1048576;  // 1 MB
    };
    
    explicit ResponseCache(QObject* parent = nullptr) : QObject(parent) {
        m_cleanupTimer = new QTimer(this);
        connect(m_cleanupTimer, &QTimer::timeout, this, &ResponseCache::evictExpired);
        m_cleanupTimer->start(60000);  // Cleanup every minute
    }
    
    void setConfig(const Config& config) { m_config = config; }
    
    /**
     * @brief Get a cached response if available
     */
    bool get(const QString& key, QString& response) {
        QMutexLocker locker(&m_mutex);
        
        if (!m_cache.contains(key)) {
            emit cacheMiss(key);
            return false;
        }
        
        CacheEntry& entry = m_cache[key];
        
        // Check if expired
        if (entry.timestamp.msecsTo(QDateTime::currentDateTime()) > entry.ttlMs) {
            m_cache.remove(key);
            emit cacheMiss(key);
            return false;
        }
        
        ++entry.hitCount;
        response = entry.response;
        emit cacheHit(key, entry.hitCount);
        return true;
    }
    
    /**
     * @brief Store a response in cache
     */
    void put(const QString& key, const QString& response, qint64 ttlMs = -1) {
        QMutexLocker locker(&m_mutex);
        
        // Check size limits
        if (response.size() > m_config.maxEntrySizeBytes) {
            qWarning() << "[ResponseCache] Response too large to cache:" << response.size() << "bytes";
            return;
        }
        
        // Evict if at capacity
        if (m_cache.size() >= m_config.maxEntries) {
            evictLeastUsed();
        }
        
        CacheEntry entry;
        entry.response = response;
        entry.timestamp = QDateTime::currentDateTime();
        entry.hitCount = 0;
        entry.ttlMs = ttlMs > 0 ? ttlMs : m_config.defaultTtlMs;
        
        m_cache[key] = entry;
        emit entryAdded(key, entry.response.size());
    }
    
    /**
     * @brief Generate cache key from query
     */
    static QString generateKey(const QString& query, const QString& model, const QString& context = "") {
        return QString("%1|%2|%3").arg(query).arg(model).arg(context.left(100));
    }
    
    void clear() {
        QMutexLocker locker(&m_mutex);
        m_cache.clear();
        emit cacheCleared();
    }
    
    QJsonObject getStats() const {
        QJsonObject stats;
        stats["entries"] = m_cache.size();
        stats["maxEntries"] = m_config.maxEntries;
        
        int totalHits = 0;
        qint64 totalSize = 0;
        for (const auto& entry : m_cache) {
            totalHits += entry.hitCount;
            totalSize += entry.response.size();
        }
        stats["totalHits"] = totalHits;
        stats["totalSizeBytes"] = totalSize;
        
        return stats;
    }
    
signals:
    void cacheHit(const QString& key, int hitCount);
    void cacheMiss(const QString& key);
    void entryAdded(const QString& key, int sizeBytes);
    void entryEvicted(const QString& key, const QString& reason);
    void cacheCleared();
    
private slots:
    void evictExpired() {
        QMutexLocker locker(&m_mutex);
        QDateTime now = QDateTime::currentDateTime();
        
        QList<QString> toRemove;
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (it.value().timestamp.msecsTo(now) > it.value().ttlMs) {
                toRemove.append(it.key());
            }
        }
        
        for (const QString& key : toRemove) {
            m_cache.remove(key);
            emit entryEvicted(key, "expired");
        }
    }
    
    void evictLeastUsed() {
        // Find entry with lowest hit count
        QString leastUsedKey;
        int minHits = INT_MAX;
        
        for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
            if (it.value().hitCount < minHits) {
                minHits = it.value().hitCount;
                leastUsedKey = it.key();
            }
        }
        
        if (!leastUsedKey.isEmpty()) {
            m_cache.remove(leastUsedKey);
            emit entryEvicted(leastUsedKey, "lru");
        }
    }
    
private:
    Config m_config;
    QHash<QString, CacheEntry> m_cache;
    QMutex m_mutex;
    QTimer* m_cleanupTimer;
};

/**
 * @class ChatMetrics
 * @brief Collects and reports chat metrics for monitoring
 */
class ChatMetrics : public QObject {
    Q_OBJECT
    
public:
    struct MessageMetrics {
        qint64 totalMessages = 0;
        qint64 userMessages = 0;
        qint64 assistantMessages = 0;
        qint64 errorMessages = 0;
        qint64 retriedMessages = 0;
    };
    
    struct LatencyMetrics {
        double avgLatencyMs = 0;
        qint64 minLatencyMs = LLONG_MAX;
        qint64 maxLatencyMs = 0;
        qint64 p50LatencyMs = 0;
        qint64 p95LatencyMs = 0;
        qint64 p99LatencyMs = 0;
    };
    
    struct ThroughputMetrics {
        double messagesPerSecond = 0;
        double tokensPerSecond = 0;
        qint64 totalTokens = 0;
    };
    
    explicit ChatMetrics(QObject* parent = nullptr) : QObject(parent) {
        m_metricsTimer = new QTimer(this);
        connect(m_metricsTimer, &QTimer::timeout, this, &ChatMetrics::calculateMetrics);
        m_metricsTimer->start(5000);  // Calculate every 5 seconds
        m_startTime = QDateTime::currentDateTime();
    }
    
    void recordMessage(const QString& role, qint64 latencyMs = 0) {
        QMutexLocker locker(&m_mutex);
        ++m_messageMetrics.totalMessages;
        
        if (role == "user") {
            ++m_messageMetrics.userMessages;
        } else if (role == "assistant") {
            ++m_messageMetrics.assistantMessages;
        } else if (role == "error") {
            ++m_messageMetrics.errorMessages;
        }
        
        if (latencyMs > 0) {
            m_latencies.append(latencyMs);
            // Keep only last 1000 samples
            if (m_latencies.size() > 1000) {
                m_latencies.removeFirst();
            }
        }
    }
    
    void recordRetry() {
        QMutexLocker locker(&m_mutex);
        ++m_messageMetrics.retriedMessages;
    }
    
    void recordTokens(qint64 tokens) {
        QMutexLocker locker(&m_mutex);
        m_throughputMetrics.totalTokens += tokens;
    }
    
    MessageMetrics getMessageMetrics() const { return m_messageMetrics; }
    LatencyMetrics getLatencyMetrics() const { return m_latencyMetrics; }
    ThroughputMetrics getThroughputMetrics() const { return m_throughputMetrics; }
    
    QJsonObject getFullMetricsReport() const {
        QJsonObject report;
        report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        report["uptimeSeconds"] = m_startTime.secsTo(QDateTime::currentDateTime());
        
        QJsonObject messages;
        messages["total"] = m_messageMetrics.totalMessages;
        messages["user"] = m_messageMetrics.userMessages;
        messages["assistant"] = m_messageMetrics.assistantMessages;
        messages["errors"] = m_messageMetrics.errorMessages;
        messages["retried"] = m_messageMetrics.retriedMessages;
        report["messages"] = messages;
        
        QJsonObject latency;
        latency["avgMs"] = m_latencyMetrics.avgLatencyMs;
        latency["minMs"] = m_latencyMetrics.minLatencyMs == LLONG_MAX ? 0 : m_latencyMetrics.minLatencyMs;
        latency["maxMs"] = m_latencyMetrics.maxLatencyMs;
        latency["p50Ms"] = m_latencyMetrics.p50LatencyMs;
        latency["p95Ms"] = m_latencyMetrics.p95LatencyMs;
        latency["p99Ms"] = m_latencyMetrics.p99LatencyMs;
        report["latency"] = latency;
        
        QJsonObject throughput;
        throughput["messagesPerSec"] = m_throughputMetrics.messagesPerSecond;
        throughput["tokensPerSec"] = m_throughputMetrics.tokensPerSecond;
        throughput["totalTokens"] = m_throughputMetrics.totalTokens;
        report["throughput"] = throughput;
        
        return report;
    }
    
signals:
    void metricsUpdated(const QJsonObject& metrics);
    void latencyAlert(qint64 latencyMs, qint64 thresholdMs);
    void errorRateAlert(double errorRate, double thresholdPercent);
    
private slots:
    void calculateMetrics() {
        QMutexLocker locker(&m_mutex);
        
        if (!m_latencies.isEmpty()) {
            // Calculate latency percentiles
            QList<qint64> sorted = m_latencies;
            std::sort(sorted.begin(), sorted.end());
            
            qint64 sum = 0;
            for (qint64 lat : sorted) {
                sum += lat;
                if (lat < m_latencyMetrics.minLatencyMs) m_latencyMetrics.minLatencyMs = lat;
                if (lat > m_latencyMetrics.maxLatencyMs) m_latencyMetrics.maxLatencyMs = lat;
            }
            
            m_latencyMetrics.avgLatencyMs = static_cast<double>(sum) / sorted.size();
            m_latencyMetrics.p50LatencyMs = sorted[sorted.size() / 2];
            m_latencyMetrics.p95LatencyMs = sorted[static_cast<int>(sorted.size() * 0.95)];
            m_latencyMetrics.p99LatencyMs = sorted[static_cast<int>(sorted.size() * 0.99)];
        }
        
        // Calculate throughput
        qint64 uptimeSeconds = m_startTime.secsTo(QDateTime::currentDateTime());
        if (uptimeSeconds > 0) {
            m_throughputMetrics.messagesPerSecond = 
                static_cast<double>(m_messageMetrics.totalMessages) / uptimeSeconds;
            m_throughputMetrics.tokensPerSecond = 
                static_cast<double>(m_throughputMetrics.totalTokens) / uptimeSeconds;
        }
        
        emit metricsUpdated(getFullMetricsReport());
        
        // Check for alerts
        if (m_latencyMetrics.avgLatencyMs > 5000) {
            emit latencyAlert(static_cast<qint64>(m_latencyMetrics.avgLatencyMs), 5000);
        }
        
        if (m_messageMetrics.totalMessages > 0) {
            double errorRate = static_cast<double>(m_messageMetrics.errorMessages) / 
                              m_messageMetrics.totalMessages * 100;
            if (errorRate > 10.0) {
                emit errorRateAlert(errorRate, 10.0);
            }
        }
    }
    
private:
    QTimer* m_metricsTimer;
    QMutex m_mutex;
    QDateTime m_startTime;
    MessageMetrics m_messageMetrics;
    LatencyMetrics m_latencyMetrics;
    ThroughputMetrics m_throughputMetrics;
    QList<qint64> m_latencies;
};

/**
 * @class UserAnalytics
 * @brief Tracks user interaction patterns for optimization
 */
class UserAnalytics : public QObject {
    Q_OBJECT
    
public:
    struct UserSession {
        QString sessionId;
        QDateTime startTime;
        QDateTime lastActivity;
        int messageCount = 0;
        int codeApprovals = 0;
        int codeRejections = 0;
        QStringList queriedModels;
        QMap<QString, int> intentDistribution;
    };
    
    explicit UserAnalytics(QObject* parent = nullptr) : QObject(parent) {
        m_sessionId = QString::number(QDateTime::currentMSecsSinceEpoch());
        m_currentSession.sessionId = m_sessionId;
        m_currentSession.startTime = QDateTime::currentDateTime();
        m_currentSession.lastActivity = QDateTime::currentDateTime();
    }
    
    void trackMessage(const QString& intent, const QString& model) {
        m_currentSession.lastActivity = QDateTime::currentDateTime();
        ++m_currentSession.messageCount;
        
        if (!m_currentSession.queriedModels.contains(model)) {
            m_currentSession.queriedModels.append(model);
        }
        
        m_currentSession.intentDistribution[intent] = 
            m_currentSession.intentDistribution.value(intent, 0) + 1;
        
        emit interactionTracked("message", intent);
    }
    
    void trackCodeApproval() {
        m_currentSession.lastActivity = QDateTime::currentDateTime();
        ++m_currentSession.codeApprovals;
        emit interactionTracked("code_approval", "");
    }
    
    void trackCodeRejection() {
        m_currentSession.lastActivity = QDateTime::currentDateTime();
        ++m_currentSession.codeRejections;
        emit interactionTracked("code_rejection", "");
    }
    
    void trackError(const QString& errorType) {
        emit interactionTracked("error", errorType);
    }
    
    void trackRetry() {
        emit interactionTracked("retry", "");
    }
    
    UserSession getCurrentSession() const { return m_currentSession; }
    
    QJsonObject getSessionReport() const {
        QJsonObject report;
        report["sessionId"] = m_currentSession.sessionId;
        report["startTime"] = m_currentSession.startTime.toString(Qt::ISODate);
        report["lastActivity"] = m_currentSession.lastActivity.toString(Qt::ISODate);
        report["durationMinutes"] = m_currentSession.startTime.secsTo(
            m_currentSession.lastActivity) / 60.0;
        report["messageCount"] = m_currentSession.messageCount;
        report["codeApprovals"] = m_currentSession.codeApprovals;
        report["codeRejections"] = m_currentSession.codeRejections;
        
        QJsonArray models;
        for (const QString& model : m_currentSession.queriedModels) {
            models.append(model);
        }
        report["queriedModels"] = models;
        
        QJsonObject intents;
        for (auto it = m_currentSession.intentDistribution.begin(); 
             it != m_currentSession.intentDistribution.end(); ++it) {
            intents[it.key()] = it.value();
        }
        report["intentDistribution"] = intents;
        
        // Calculate engagement metrics
        double approvalRate = 0;
        int totalCodeActions = m_currentSession.codeApprovals + m_currentSession.codeRejections;
        if (totalCodeActions > 0) {
            approvalRate = static_cast<double>(m_currentSession.codeApprovals) / totalCodeActions * 100;
        }
        report["codeApprovalRate"] = approvalRate;
        
        return report;
    }
    
signals:
    void interactionTracked(const QString& type, const QString& detail);
    void sessionEnded(const QJsonObject& sessionReport);
    
private:
    QString m_sessionId;
    UserSession m_currentSession;
};

/**
 * @class ChatProductionInfrastructure
 * @brief Master class integrating all production infrastructure components
 */
class ChatProductionInfrastructure : public QObject {
    Q_OBJECT
    
public:
    explicit ChatProductionInfrastructure(QObject* parent = nullptr) : QObject(parent) {
        m_circuitBreaker = std::make_unique<CircuitBreaker>("InferenceEngine", this);
        m_retryPolicy = std::make_unique<RetryPolicy>(this);
        m_healthMonitor = std::make_unique<HealthMonitor>(this);
        m_responseCache = std::make_unique<ResponseCache>(this);
        m_metrics = std::make_unique<ChatMetrics>(this);
        m_analytics = std::make_unique<UserAnalytics>(this);
        
        connectSignals();
    }
    
    // Access components
    CircuitBreaker* circuitBreaker() { return m_circuitBreaker.get(); }
    RetryPolicy* retryPolicy() { return m_retryPolicy.get(); }
    HealthMonitor* healthMonitor() { return m_healthMonitor.get(); }
    ResponseCache* responseCache() { return m_responseCache.get(); }
    ChatMetrics* metrics() { return m_metrics.get(); }
    UserAnalytics* analytics() { return m_analytics.get(); }
    
    void initialize() {
        // Register health checks
        m_healthMonitor->registerComponent("CircuitBreaker", [this]() {
            HealthMonitor::HealthStatus status;
            status.status = m_circuitBreaker->state() == CircuitBreaker::CLOSED ? 
                           HealthMonitor::HEALTHY : 
                           (m_circuitBreaker->state() == CircuitBreaker::HALF_OPEN ?
                           HealthMonitor::DEGRADED : HealthMonitor::UNHEALTHY);
            status.message = QString("State: %1, Failures: %2")
                            .arg(m_circuitBreaker->state())
                            .arg(m_circuitBreaker->failureCount());
            return status;
        });
        
        m_healthMonitor->registerComponent("ResponseCache", [this]() {
            HealthMonitor::HealthStatus status;
            auto stats = m_responseCache->getStats();
            int entries = stats["entries"].toInt();
            int maxEntries = stats["maxEntries"].toInt();
            
            double utilization = static_cast<double>(entries) / maxEntries;
            status.status = utilization < 0.9 ? HealthMonitor::HEALTHY : HealthMonitor::DEGRADED;
            status.message = QString("Entries: %1/%2").arg(entries).arg(maxEntries);
            status.details = stats;
            return status;
        });
        
        m_healthMonitor->startMonitoring(30000);
    }
    
    QJsonObject getFullStatusReport() const {
        QJsonObject report;
        report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        report["health"] = m_healthMonitor->getFullHealthReport();
        report["metrics"] = m_metrics->getFullMetricsReport();
        report["cache"] = m_responseCache->getStats();
        report["analytics"] = m_analytics->getSessionReport();
        return report;
    }
    
signals:
    void infrastructureAlert(const QString& component, const QString& message);
    
private:
    void connectSignals() {
        connect(m_circuitBreaker.get(), &CircuitBreaker::tripped,
                this, [this](const QString& service, int failures) {
            emit infrastructureAlert("CircuitBreaker", 
                QString("Service %1 tripped after %2 failures").arg(service).arg(failures));
        });
        
        connect(m_healthMonitor.get(), &HealthMonitor::componentUnhealthy,
                this, [this](const QString& component, const QString& reason) {
            emit infrastructureAlert("Health", 
                QString("Component %1 unhealthy: %2").arg(component).arg(reason));
        });
        
        connect(m_metrics.get(), &ChatMetrics::latencyAlert,
                this, [this](qint64 latency, qint64 threshold) {
            emit infrastructureAlert("Performance", 
                QString("High latency: %1ms (threshold: %2ms)").arg(latency).arg(threshold));
        });
    }
    
    std::unique_ptr<CircuitBreaker> m_circuitBreaker;
    std::unique_ptr<RetryPolicy> m_retryPolicy;
    std::unique_ptr<HealthMonitor> m_healthMonitor;
    std::unique_ptr<ResponseCache> m_responseCache;
    std::unique_ptr<ChatMetrics> m_metrics;
    std::unique_ptr<UserAnalytics> m_analytics;
};

} // namespace Chat
} // namespace RawrXD

#endif // CHAT_PRODUCTION_INFRASTRUCTURE_HPP
