#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QMutex>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>

class FaultToleranceManager : public QObject {
    Q_OBJECT

public:
    explicit FaultToleranceManager(QObject* parent = nullptr);
    ~FaultToleranceManager();

    // Circuit Breaker Pattern
    struct CircuitBreakerConfig {
        int failure_threshold = 5;
        std::chrono::seconds reset_timeout{30};
        std::chrono::milliseconds request_timeout{5000};
        bool enabled = true;
    };

    bool executeWithCircuitBreaker(const QString& component_id, 
                                  std::function<bool()> operation,
                                  std::function<void()> fallback = nullptr);
    
    void recordSuccess(const QString& component_id);
    void recordFailure(const QString& component_id, const std::string& error_type, const std::string& error_message);
    
    enum class CircuitState {
        CLOSED,     // Normal operation
        OPEN,       // Failing, requests blocked
        HALF_OPEN   // Testing recovery
    };
    
    CircuitState getCircuitState(const QString& component_id) const;
    
    // Retry Logic
    struct RetryPolicy {
        int max_retries = 3;
        std::chrono::milliseconds initial_backoff{100};
        double backoff_multiplier = 2.0;
        std::chrono::milliseconds max_backoff{5000};
        bool jitter = true;
    };
    
    bool executeWithRetry(std::function<bool()> operation, 
                         const RetryPolicy& policy = RetryPolicy());
    
    // Fallback Management
    void registerFallback(const QString& component_id, std::function<void()> fallback_func);
    
    // Health Monitoring
    struct ComponentHealth {
        QString component_id;
        CircuitState state;
        int failure_count;
        int success_count;
        QDateTime last_failure;
        QDateTime last_success;
        double error_rate;
    };
    
    ComponentHealth getComponentHealth(const QString& component_id) const;
    std::vector<ComponentHealth> getAllComponentHealth() const;

signals:
    void circuitOpened(const QString& component_id, const QString& reason);
    void circuitClosed(const QString& component_id);
    void circuitHalfOpened(const QString& component_id);
    void fallbackTriggered(const QString& component_id);
    void retryAttempt(const QString& component_id, int attempt, int max_retries);

private:
    struct CircuitBreakerState {
        CircuitState state = CircuitState::CLOSED;
        int failure_count = 0;
        int success_count = 0;
        std::chrono::steady_clock::time_point last_failure_time;
        std::chrono::steady_clock::time_point last_success_time;
        std::chrono::steady_clock::time_point state_change_time;
        CircuitBreakerConfig config;
    };
    
    mutable QMutex mutex;
    QMap<QString, CircuitBreakerState> circuits;
    QMap<QString, std::function<void()>> fallbacks;
    
    void updateCircuitState(const QString& component_id, CircuitBreakerState& state);
    bool shouldAllowRequest(CircuitBreakerState& state);
};
