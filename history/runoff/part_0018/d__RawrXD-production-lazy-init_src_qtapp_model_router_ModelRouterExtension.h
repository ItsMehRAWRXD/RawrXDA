#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QHash>
#include <QMutex>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <functional>
#include <memory>
#include <atomic>

// Forward declarations
class InferenceEngine;
class LoadBalancingStrategies;
class ModelPoolManager;
class RoutingAnalytics;

/**
 * @brief Routing strategy types for model selection
 */
enum class RoutingStrategy {
    RoundRobin,           // Cycle through models sequentially
    WeightedRandom,       // Random selection weighted by priority
    LeastConnections,     // Route to model with fewest active requests
    ResponseTimeBased,    // Route to fastest responding model
    Adaptive,             // Machine learning based routing
    PriorityBased,        // Use priority queue
    CostOptimized,        // Minimize inference cost
    Custom                // User-defined strategy
};

/**
 * @brief Model endpoint configuration
 */
struct ModelEndpoint {
    QString id;                    // Unique identifier
    QString name;                  // Human-readable name
    QString type;                  // "local", "ollama", "openai", "anthropic", etc.
    QString path;                  // Model path or URL
    QString apiKey;                // Optional API key
    int priority;                  // Higher = more priority (1-100)
    double weight;                 // Weight for weighted routing (0.0-1.0)
    bool enabled;                  // Is endpoint active?
    int maxConcurrentRequests;     // Concurrency limit
    qint64 estimatedCostPerToken;  // Cost in microcents
    QMap<QString, QString> metadata; // Additional metadata
    
    // Health tracking
    bool healthy;
    int consecutiveFailures;
    QDateTime lastHealthCheck;
    double averageResponseTime;    // milliseconds
    int activeRequests;
    
    ModelEndpoint() : priority(50), weight(1.0), enabled(true), 
                     maxConcurrentRequests(10), estimatedCostPerToken(0),
                     healthy(true), consecutiveFailures(0),
                     averageResponseTime(0), activeRequests(0) {}
    
    QJsonObject toJson() const;
    static ModelEndpoint fromJson(const QJsonObject& json);
};

/**
 * @brief Routing request context
 */
struct RoutingRequest {
    QString requestId;
    QString prompt;
    int maxTokens;
    double temperature;
    QString requiredModel;         // Specific model requirement (empty = any)
    QStringList preferredModels;   // Preference list
    QMap<QString, QString> constraints; // Custom constraints
    QDateTime timestamp;
    int priority;                  // Request priority (1-10)
    
    RoutingRequest() : maxTokens(1024), temperature(0.7), priority(5) {
        timestamp = QDateTime::currentDateTime();
    }
};

/**
 * @brief Routing decision result
 */
struct RoutingDecision {
    QString endpointId;
    QString reason;                // Why this endpoint was chosen
    double confidence;             // Confidence in decision (0.0-1.0)
    QStringList alternativeEndpoints; // Backup options
    QMap<QString, double> scores;  // Scoring breakdown
    QDateTime decidedAt;
    
    RoutingDecision() : confidence(1.0) {
        decidedAt = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief Model health status
 */
struct ModelHealth {
    QString endpointId;
    bool healthy;
    QString status;                // "healthy", "degraded", "unhealthy", "unknown"
    double responseTime;           // Average response time (ms)
    double errorRate;              // Error rate (0.0-1.0)
    int activeRequests;
    int totalRequests;
    int failedRequests;
    QDateTime lastCheck;
    QString failureReason;
    
    ModelHealth() : healthy(true), status("unknown"), responseTime(0),
                   errorRate(0), activeRequests(0), totalRequests(0),
                   failedRequests(0) {
        lastCheck = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief Circuit breaker for fault tolerance
 */
class CircuitBreaker {
public:
    enum State {
        Closed,       // Normal operation
        Open,         // Rejecting requests
        HalfOpen      // Testing recovery
    };
    
    CircuitBreaker(int failureThreshold = 5, int timeout = 30000);
    
    bool allowRequest();
    void recordSuccess();
    void recordFailure();
    State getState() const { return m_state; }
    void reset();
    
private:
    State m_state;
    int m_failureCount;
    int m_failureThreshold;
    int m_timeout;               // milliseconds
    QDateTime m_lastFailureTime;
    QMutex m_mutex;
};

/**
 * @brief Advanced Model Router Extension
 * 
 * Provides intelligent routing, load balancing, health monitoring,
 * and failover capabilities for multiple model endpoints.
 */
class ModelRouterExtension : public QObject {
    Q_OBJECT
    
public:
    explicit ModelRouterExtension(QObject* parent = nullptr);
    ~ModelRouterExtension();
    
    // ============ Endpoint Management ============
    
    /**
     * @brief Register a new model endpoint
     */
    bool registerEndpoint(const ModelEndpoint& endpoint);
    
    /**
     * @brief Unregister an endpoint
     */
    bool unregisterEndpoint(const QString& endpointId);
    
    /**
     * @brief Update an existing endpoint
     */
    bool updateEndpoint(const QString& endpointId, const ModelEndpoint& endpoint);
    
    /**
     * @brief Get endpoint by ID
     */
    ModelEndpoint getEndpoint(const QString& endpointId) const;
    
    /**
     * @brief Get all registered endpoints
     */
    QList<ModelEndpoint> getAllEndpoints() const;
    
    /**
     * @brief Get all healthy endpoints
     */
    QList<ModelEndpoint> getHealthyEndpoints() const;
    
    /**
     * @brief Enable/disable an endpoint
     */
    void setEndpointEnabled(const QString& endpointId, bool enabled);
    
    // ============ Routing Logic ============
    
    /**
     * @brief Route a request to the best endpoint
     */
    RoutingDecision routeRequest(const RoutingRequest& request);
    
    /**
     * @brief Set routing strategy
     */
    void setRoutingStrategy(RoutingStrategy strategy);
    
    /**
     * @brief Get current routing strategy
     */
    RoutingStrategy getRoutingStrategy() const { return m_routingStrategy; }
    
    /**
     * @brief Set custom routing function
     */
    void setCustomRoutingFunction(std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> func);
    
    /**
     * @brief Get routing statistics
     */
    QJsonObject getRoutingStatistics() const;
    
    // ============ Health Monitoring ============
    
    /**
     * @brief Check health of specific endpoint
     */
    ModelHealth checkEndpointHealth(const QString& endpointId);
    
    /**
     * @brief Check health of all endpoints
     */
    QMap<QString, ModelHealth> checkAllEndpointsHealth();
    
    /**
     * @brief Enable automatic health checks
     */
    void enableHealthMonitoring(int intervalMs = 60000);
    
    /**
     * @brief Disable automatic health checks
     */
    void disableHealthMonitoring();
    
    /**
     * @brief Set health check interval
     */
    void setHealthCheckInterval(int intervalMs);
    
    /**
     * @brief Get health status of all endpoints
     */
    QMap<QString, ModelHealth> getHealthStatus() const;
    
    // ============ Circuit Breaker ============
    
    /**
     * @brief Get circuit breaker for endpoint
     */
    CircuitBreaker* getCircuitBreaker(const QString& endpointId);
    
    /**
     * @brief Reset circuit breaker for endpoint
     */
    void resetCircuitBreaker(const QString& endpointId);
    
    /**
     * @brief Reset all circuit breakers
     */
    void resetAllCircuitBreakers();
    
    // ============ Fallback Handling ============
    
    /**
     * @brief Set fallback endpoints
     */
    void setFallbackChain(const QStringList& endpointIds);
    
    /**
     * @brief Get fallback chain
     */
    QStringList getFallbackChain() const { return m_fallbackChain; }
    
    /**
     * @brief Route with automatic fallback
     */
    RoutingDecision routeWithFallback(const RoutingRequest& request);
    
    // ============ Request Tracking ============
    
    /**
     * @brief Record request start
     */
    void recordRequestStart(const QString& endpointId, const QString& requestId);
    
    /**
     * @brief Record request completion
     */
    void recordRequestComplete(const QString& endpointId, const QString& requestId, bool success, double responseTime);
    
    /**
     * @brief Get active requests for endpoint
     */
    int getActiveRequestCount(const QString& endpointId) const;
    
    /**
     * @brief Get total requests for endpoint
     */
    int getTotalRequestCount(const QString& endpointId) const;
    
    // ============ Load Balancing ============
    
    /**
     * @brief Check if endpoint can accept request
     */
    bool canAcceptRequest(const QString& endpointId) const;
    
    /**
     * @brief Get endpoint with least connections
     */
    QString getEndpointWithLeastConnections() const;
    
    /**
     * @brief Get endpoint with best response time
     */
    QString getEndpointWithBestResponseTime() const;
    
    // ============ Configuration ============
    
    /**
     * @brief Save configuration to JSON
     */
    QJsonObject saveConfiguration() const;
    
    /**
     * @brief Load configuration from JSON
     */
    bool loadConfiguration(const QJsonObject& config);
    
    /**
     * @brief Save to file
     */
    bool saveToFile(const QString& filePath) const;
    
    /**
     * @brief Load from file
     */
    bool loadFromFile(const QString& filePath);
    
    // ============ Statistics ============
    
    /**
     * @brief Get routing metrics
     */
    QJsonObject getMetrics() const;
    
    /**
     * @brief Reset all statistics
     */
    void resetStatistics();
    
signals:
    /**
     * @brief Emitted when endpoint health changes
     */
    void endpointHealthChanged(const QString& endpointId, bool healthy);
    
    /**
     * @brief Emitted when routing decision is made
     */
    void routingDecisionMade(const RoutingDecision& decision);
    
    /**
     * @brief Emitted when endpoint fails
     */
    void endpointFailed(const QString& endpointId, const QString& reason);
    
    /**
     * @brief Emitted when circuit breaker opens
     */
    void circuitBreakerOpened(const QString& endpointId);
    
    /**
     * @brief Emitted when circuit breaker closes
     */
    void circuitBreakerClosed(const QString& endpointId);
    
private slots:
    void performHealthCheck();
    
private:
    // Routing implementation
    QString routeRoundRobin(const RoutingRequest& request);
    QString routeWeightedRandom(const RoutingRequest& request);
    QString routeLeastConnections(const RoutingRequest& request);
    QString routeResponseTimeBased(const RoutingRequest& request);
    QString routeAdaptive(const RoutingRequest& request);
    QString routePriorityBased(const RoutingRequest& request);
    QString routeCostOptimized(const RoutingRequest& request);
    
    // Scoring functions
    double scoreEndpoint(const ModelEndpoint& endpoint, const RoutingRequest& request) const;
    double calculateHealthScore(const ModelEndpoint& endpoint) const;
    double calculateLoadScore(const ModelEndpoint& endpoint) const;
    double calculateCostScore(const ModelEndpoint& endpoint, const RoutingRequest& request) const;
    
    // Health check implementation
    bool performHealthCheckForEndpoint(const QString& endpointId);
    void updateEndpointHealth(const QString& endpointId, bool healthy, const QString& reason);
    
    // Helper functions
    bool matchesConstraints(const ModelEndpoint& endpoint, const RoutingRequest& request) const;
    QList<ModelEndpoint> filterEndpoints(const RoutingRequest& request) const;
    
    // Data members
    QMap<QString, ModelEndpoint> m_endpoints;
    QMap<QString, std::shared_ptr<CircuitBreaker>> m_circuitBreakers;
    QMap<QString, ModelHealth> m_healthStatus;
    QMap<QString, QList<QString>> m_activeRequests;  // endpointId -> requestIds
    
    RoutingStrategy m_routingStrategy;
    std::function<QString(const RoutingRequest&, const QList<ModelEndpoint>&)> m_customRoutingFunction;
    
    QStringList m_fallbackChain;
    int m_roundRobinIndex;
    
    // Health monitoring
    QTimer* m_healthCheckTimer;
    int m_healthCheckInterval;
    bool m_healthMonitoringEnabled;
    
    // Statistics
    std::atomic<qint64> m_totalRequests;
    std::atomic<qint64> m_successfulRequests;
    std::atomic<qint64> m_failedRequests;
    QMap<QString, qint64> m_endpointRequestCounts;
    QMap<QString, qint64> m_endpointSuccessCounts;
    QMap<QString, qint64> m_endpointFailureCounts;
    QMap<QString, double> m_endpointResponseTimes;
    
    mutable QMutex m_mutex;
};
