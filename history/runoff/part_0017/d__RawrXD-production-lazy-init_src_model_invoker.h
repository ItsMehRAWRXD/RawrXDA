#ifndef MODEL_INVOKER_H
#define MODEL_INVOKER_H

#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <QMutex>
#include <QSharedPointer>
#include <functional>

namespace RawrXD {

enum class InvokerState {
    CLOSED,
    OPEN,
    HALF_OPEN
};

struct InvokerStats {
    int totalRequests;
    int successfulRequests;
    int failedRequests;
    int consecutiveFailures;
    QDateTime lastFailureTime;
    QDateTime lastSuccessTime;
    
    InvokerStats() : totalRequests(0), successfulRequests(0), failedRequests(0), consecutiveFailures(0) {}
    
    double getSuccessRate() const {
        if (totalRequests == 0) return 0.0;
        return static_cast<double>(successfulRequests) / totalRequests;
    }
    
    void recordSuccess() {
        totalRequests++;
        successfulRequests++;
        consecutiveFailures = 0;
        lastSuccessTime = QDateTime::currentDateTime();
    }
    
    void recordFailure() {
        totalRequests++;
        failedRequests++;
        consecutiveFailures++;
        lastFailureTime = QDateTime::currentDateTime();
    }
};

class CircuitBreaker {
public:
    CircuitBreaker(int failureThreshold = 5, int timeoutSeconds = 60, int halfOpenTimeout = 30);
    
    bool allowRequest();
    void recordSuccess();
    void recordFailure();
    
    InvokerState getState() const { return state_; }
    InvokerStats getStats() const { return stats_; }
    
private:
    void transitionToOpen();
    void transitionToHalfOpen();
    void transitionToClosed();
    
    InvokerState state_ = InvokerState::CLOSED;
    InvokerStats stats_;
    int failureThreshold_;
    int timeoutSeconds_;
    int halfOpenTimeout_;
    QDateTime stateChangeTime_;
    mutable QMutex mutex_;
};

class RetryPolicy {
public:
    RetryPolicy(int maxAttempts = 3, int initialDelayMs = 1000, double backoffMultiplier = 2.0);
    
    bool shouldRetry(int attempt, const std::exception& error) const;
    int getDelayMs(int attempt) const;
    int getMaxAttempts() const { return maxAttempts_; }
    
private:
    int maxAttempts_;
    int initialDelayMs_;
    double backoffMultiplier_;
};

class ModelInvoker {
public:
    static ModelInvoker& instance();
    
    void initialize(const QString& modelName, const QJsonObject& config = QJsonObject());
    void shutdown();
    
    // Primary invocation method with retry and circuit breaker
    QJsonObject invoke(const QJsonObject& request, int timeoutMs = 30000);
    
    // Async invocation
    void invokeAsync(const QJsonObject& request, 
                    std::function<void(const QJsonObject&)> successCallback,
                    std::function<void(const std::exception&)> errorCallback,
                    int timeoutMs = 30000);
    
    // Fallback mechanism
    void setFallbackModel(const QString& fallbackModel);
    void setFallbackHandler(std::function<QJsonObject(const QJsonObject&)> handler);
    
    // Monitoring and statistics
    InvokerStats getStatistics() const;
    bool isHealthy() const;
    void resetStatistics();
    
    // Configuration
    void updateConfig(const QJsonObject& config);
    
private:
    ModelInvoker() = default;
    ~ModelInvoker();
    
    QJsonObject doInvoke(const QJsonObject& request, int timeoutMs);
    QJsonObject invokeFallback(const QJsonObject& request);
    
    QString modelName_;
    QSharedPointer<CircuitBreaker> circuitBreaker_;
    QSharedPointer<RetryPolicy> retryPolicy_;
    QString fallbackModel_;
    std::function<QJsonObject(const QJsonObject&)> fallbackHandler_;
    mutable QMutex mutex_;
    bool initialized_ = false;
    
    static const int DEFAULT_TIMEOUT_MS = 30000;
};

// Convenience macros
#define MODEL_INVOKE(request) RawrXD::ModelInvoker::instance().invoke(request)
#define MODEL_INVOKE_ASYNC(request, success, error) RawrXD::ModelInvoker::instance().invokeAsync(request, success, error)
#define MODEL_STATS RawrXD::ModelInvoker::instance().getStatistics()

} // namespace RawrXD

#endif // MODEL_INVOKER_H