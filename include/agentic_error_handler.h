#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

class AgenticObservability;
class AgenticLoopState;

/**
 * @class AgenticErrorHandler
 * @brief Non-intrusive centralized error handling with recovery strategies (Qt-free)
 * 
 * Features:
 * - Centralized exception capture at high level
 * - Resource guards (RAII patterns)
 * - Graceful degradation
 * - Automatic recovery strategies
 * - Error classification and metrics
 * - Retry policies with exponential backoff
 * - Function pointer callbacks instead of Qt signals
 */
class AgenticErrorHandler
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

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
        std::string errorId;
        ErrorType type;
        std::string message;
        std::string stackTrace;
        std::string component;
        nlohmann::json systemState;
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

    // Callback types (replaces Qt signals)
    using ErrorRecordedCb      = void(*)(const std::string& errorId, const std::string& message, void* userData);
    using RecoveryStartedCb    = void(*)(const std::string& errorId, void* userData);
    using RecoverySucceededCb  = void(*)(const std::string& errorId, void* userData);
    using RecoveryFailedCb     = void(*)(const std::string& errorId, void* userData);
    using ThresholdExceededCb  = void(*)(const std::string& component, void* userData);
    using CircuitBreakerCb     = void(*)(const std::string& component, void* userData);
    using GracefulDegradationCb= void(*)(const std::string& reason, void* userData);

public:
    AgenticErrorHandler();
    ~AgenticErrorHandler();

    // Initialize with observability system
    void initialize(AgenticObservability* obs, AgenticLoopState* state);

    // ===== ERROR CAPTURE AND HANDLING =====
    
    // Main error handler (high-level exception capture)
    nlohmann::json handleError(
        const std::exception& e,
        const std::string& component,
        const nlohmann::json& context
    );

    // Explicit error recording
    std::string recordError(
        ErrorType type,
        const std::string& message,
        const std::string& component,
        const std::string& stackTrace = "",
        const nlohmann::json& context = nlohmann::json::object()
    );

    // ===== RECOVERY EXECUTION =====
    
    bool executeRecovery(const std::string& errorId);
    bool retry(
        const std::string& errorId,
        std::function<bool()> operation,
        int maxAttempts = 3
    );
    
    // Backtrack to previous known good state
    bool backtrack(const std::string& targetStateId);
    
    // Fallback to alternative execution path
    bool fallback(const std::string& errorId, const std::string& fallbackPlan);
    
    // Escalate error to higher level
    void escalate(const std::string& errorId, const std::string& reason);

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
    nlohmann::json getAllRecoveryPolicies() const;

    // ===== ERROR CLASSIFICATION =====
    ErrorType classifyError(const std::string& errorMessage);
    bool isRetryable(ErrorType type) const;
    bool isFallbackable(ErrorType type) const;

    // ===== MONITORING AND METRICS =====
    nlohmann::json getErrorStatistics() const;
    int getErrorCount(ErrorType type) const;
    float getErrorRate() const;
    std::vector<ErrorContext> getRecentErrors(int limit = 10);
    nlohmann::json getRecoverySuccess() const;

    // ===== CONFIGURATION =====
    void setGracefulDegradation(bool enabled) { m_enableGracefulDegradation = enabled; }
    void setMaxErrorMemory(int size) { m_maxErrorMemory = size; }
    void setErrorThreshold(int count) { m_errorThreshold = count; }
    void enableCircuitBreaker(const std::string& component, int failureThreshold);

    // ===== CALLBACKS (replaces Qt signals) =====
    void setErrorRecordedCb(ErrorRecordedCb cb, void* ud = nullptr) { m_errorRecordedCb = cb; m_errorRecordedUd = ud; }
    void setRecoveryStartedCb(RecoveryStartedCb cb, void* ud = nullptr) { m_recoveryStartedCb = cb; m_recoveryStartedUd = ud; }
    void setRecoverySucceededCb(RecoverySucceededCb cb, void* ud = nullptr) { m_recoverySucceededCb = cb; m_recoverySucceededUd = ud; }
    void setRecoveryFailedCb(RecoveryFailedCb cb, void* ud = nullptr) { m_recoveryFailedCb = cb; m_recoveryFailedUd = ud; }
    void setThresholdExceededCb(ThresholdExceededCb cb, void* ud = nullptr) { m_thresholdExceededCb = cb; m_thresholdExceededUd = ud; }
    void setCircuitBreakerCb(CircuitBreakerCb cb, void* ud = nullptr) { m_circuitBreakerCb = cb; m_circuitBreakerUd = ud; }
    void setGracefulDegradationCb(GracefulDegradationCb cb, void* ud = nullptr) { m_gracefulDegradationCb = cb; m_gracefulDegradationUd = ud; }

private:
    // Helper: generate hex error ID
    static std::string generateErrorId();

    // Case-insensitive substring search
    static bool containsCI(const std::string& haystack, const std::string& needle);

    // Recovery execution helpers
    bool executeRetryStrategy(
        const ErrorContext& context,
        std::function<bool()> operation
    );
    bool executeBacktrackStrategy(const ErrorContext& context);
    bool executeFallbackStrategy(const ErrorContext& context);
    void executeEscalateStrategy(const ErrorContext& context);

    // Error analysis
    std::string analyzeError(const std::exception& e);
    std::string extractStackTrace();
    void recordMetrics(const ErrorContext& context);

    // Circuit breaker
    struct CircuitBreaker {
        std::string component;
        int failureCount;
        int failureThreshold;
        bool isTripped;
        TimePoint tripTime;
    };
    
    CircuitBreaker* getOrCreateCircuitBreaker(const std::string& component);
    void checkCircuitBreaker(const std::string& component);

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

    // Callbacks (function pointers, per project rules — no std::function for callbacks)
    ErrorRecordedCb       m_errorRecordedCb = nullptr;       void* m_errorRecordedUd = nullptr;
    RecoveryStartedCb     m_recoveryStartedCb = nullptr;     void* m_recoveryStartedUd = nullptr;
    RecoverySucceededCb   m_recoverySucceededCb = nullptr;   void* m_recoverySucceededUd = nullptr;
    RecoveryFailedCb      m_recoveryFailedCb = nullptr;      void* m_recoveryFailedUd = nullptr;
    ThresholdExceededCb   m_thresholdExceededCb = nullptr;   void* m_thresholdExceededUd = nullptr;
    CircuitBreakerCb      m_circuitBreakerCb = nullptr;      void* m_circuitBreakerUd = nullptr;
    GracefulDegradationCb m_gracefulDegradationCb = nullptr; void* m_gracefulDegradationUd = nullptr;
};

// RAII wrapper for operations
class ErrorSafeOperation {
public:
    ErrorSafeOperation(
        AgenticErrorHandler* handler,
        const std::string& operationName,
        const std::string& component
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
    std::string m_operationName;
    std::string m_component;
};
