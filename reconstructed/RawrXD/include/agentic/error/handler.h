#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <vector>
#include <functional>

class AgenticObservability;
class AgenticLoopState;

/**
 * @class AgenticErrorHandler
 * @brief Non-intrusive centralized error handling with recovery strategies
 * 
 * Features:
 * - Centralized exception capture at high level
 * - Resource guards (RAII patterns)
 * - Graceful degradation
 * - Automatic recovery strategies
 * - Error classification and metrics
 * - Retry policies with exponential backoff
 */
class AgenticErrorHandler : public QObject
{
    Q_OBJECT

public:
    enum class ErrorType {
        ValidationError,
        ExecutionError,
        ResourceError,
        NetworkError,
        TimeoutError,
        StateError,
        ConfigurationError,
        InternalError,
        Unknown
    };

    enum class RecoveryStrategy {
        Retry,
        Backtrack,
        Fallback,
        Escalate,
        Ignore,
        Abort
    };

    struct ErrorContext {
        QString errorId;
        ErrorType type;
        QString message;
        QString stackTrace;
        QString component;
        QJsonObject systemState;
        int retryCount;
        RecoveryStrategy strategy;
        bool recoverySucceeded = false;
    };

    struct RecoveryPolicy {
        ErrorType errorType;
        RecoveryStrategy strategy;
        int maxRetries;
        int initialDelayMs;
        float backoffMultiplier;
        bool fallbackEnabled;
    };

public:
    explicit AgenticErrorHandler(QObject* parent = nullptr);
    ~AgenticErrorHandler();

    // Initialize with observability system
    void initialize(AgenticObservability* obs, AgenticLoopState* state);

    // ===== ERROR CAPTURE AND HANDLING =====
    
    // Main error handler (high-level exception capture)
    QJsonObject handleError(
        const std::exception& e,
        const QString& component,
        const QJsonObject& context
    );

    // Explicit error recording
    QString recordError(
        ErrorType type,
        const QString& message,
        const QString& component,
        const QString& stackTrace = "",
        const QJsonObject& context = QJsonObject()
    );

    // ===== RECOVERY EXECUTION =====
    
    bool executeRecovery(const QString& errorId);
    bool retry(
        const QString& errorId,
        std::function<bool()> operation,
        int maxAttempts = 3
    );
    
    // Backtrack to previous known good state
    bool backtrack(const QString& targetStateId);
    
    // Fallback to alternative execution path
    bool fallback(const QString& errorId, const QString& fallbackPlan);
    
    // Escalate error to higher level
    void escalate(const QString& errorId, const QString& reason);

    // ===== RESOURCE GUARDS (RAII) =====
    
    class ResourceGuard {
    public:
        template<typename Fn>
        ResourceGuard(Fn cleanup) : m_cleanup(cleanup) {}
        
        ~ResourceGuard() {
            if (m_cleanup) m_cleanup();
        }
        
        void release() {
            m_cleanup = nullptr;
        }
        
    private:
        std::function<void()> m_cleanup;
    };

    // Create guarded resource
    template<typename ResourceType>
    std::unique_ptr<ResourceGuard> guardResource(
        ResourceType* resource,
        std::function<void(ResourceType*)> cleanup
    ) {
        return std::make_unique<ResourceGuard>([resource, cleanup]() {
            cleanup(resource);
        });
    }

    // ===== RECOVERY POLICIES =====
    void setRecoveryPolicy(const RecoveryPolicy& policy);
    RecoveryPolicy getRecoveryPolicy(ErrorType type) const;
    QJsonArray getAllRecoveryPolicies() const;

    // ===== ERROR CLASSIFICATION =====
    ErrorType classifyError(const QString& errorMessage);
    bool isRetryable(ErrorType type) const;
    bool isFallbackable(ErrorType type) const;

    // ===== MONITORING AND METRICS =====
    QJsonObject getErrorStatistics() const;
    int getErrorCount(ErrorType type) const;
    float getErrorRate() const;
    std::vector<ErrorContext> getRecentErrors(int limit = 10);
    QJsonObject getRecoverySuccess() const;

    // ===== CONFIGURATION =====
    void setGracefulDegradation(bool enabled) { m_enableGracefulDegradation = enabled; }
    void setMaxErrorMemory(int size) { m_maxErrorMemory = size; }
    void setErrorThreshold(int count) { m_errorThreshold = count; }
    void enableCircuitBreaker(const QString& component, int failureThreshold);

signals:
    void errorRecorded(const QString& errorId, const QString& message);
    void recoveryStarted(const QString& errorId);
    void recoverySucceeded(const QString& errorId);
    void recoveryFailed(const QString& errorId);
    void errorThresholdExceeded(const QString& component);
    void circuitBreakerTripped(const QString& component);
    void gracefulDegradation(const QString& reason);

private:
    // Recovery execution helpers
    bool executeRetryStrategy(
        const ErrorContext& context,
        std::function<bool()> operation
    );
    bool executeBacktrackStrategy(const ErrorContext& context);
    bool executeFallbackStrategy(const ErrorContext& context);
    void executeEscalateStrategy(const ErrorContext& context);

    // Error analysis
    QString analyzeError(const std::exception& e);
    QString extractStackTrace();
    void recordMetrics(const ErrorContext& context);

    // Circuit breaker
    struct CircuitBreaker {
        QString component;
        int failureCount;
        int failureThreshold;
        bool isTripped;
        QDateTime tripTime;
    };
    
    CircuitBreaker* getOrCreateCircuitBreaker(const QString& component);
    void checkCircuitBreaker(const QString& component);

    // Storage
    std::vector<ErrorContext> m_errorHistory;
    std::vector<RecoveryPolicy> m_recoveryPolicies;
    std::unordered_map<std::string, CircuitBreaker> m_circuitBreakers;

    // Configuration
    AgenticObservability* m_observability = nullptr;
    AgenticLoopState* m_state = nullptr;
    bool m_enableGracefulDegradation = true;
    int m_maxErrorMemory = 100;
    int m_errorThreshold = 10;
    float m_circuitBreakerTimeout = 60.0f; // seconds

    // Metrics
    int m_totalErrors = 0;
    int m_totalRecoveries = 0;
    int m_successfulRecoveries = 0;
};

// RAII wrapper for operations
class ErrorSafeOperation {
public:
    ErrorSafeOperation(
        AgenticErrorHandler* handler,
        const QString& operationName,
        const QString& component
    );
    ~ErrorSafeOperation();

    // Execute operation with automatic error handling
    template<typename Fn>
    bool execute(Fn operation) {
        try {
            operation();
            return true;
        } catch (const std::exception& e) {
            // Handler will manage the error
            return false;
        }
    }

private:
    AgenticErrorHandler* m_handler;
    QString m_operationName;
    QString m_component;
};
