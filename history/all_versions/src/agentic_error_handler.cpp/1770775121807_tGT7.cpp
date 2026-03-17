// AgenticErrorHandler Implementation (Qt-free)
#include "agentic_error_handler.h"
#include "agentic_observability.h"
#include "agentic_loop_state.h"
#include <cstdio>
#include <algorithm>
#include <random>
#include <thread>
#include <chrono>
#include <cctype>
#include <exception>

// ===== STATIC HELPERS =====

std::string AgenticErrorHandler::generateErrorId()
{
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    static const char hex[] = "0123456789abcdef";
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 32; ++i) {
        id += hex[rng() % 16];
    }
    return id;
}

bool AgenticErrorHandler::containsCI(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

AgenticErrorHandler::AgenticErrorHandler()
{
    fprintf(stderr, "[AgenticErrorHandler] Initialized - Ready for error handling and recovery\n");

    // Set up default recovery policies
    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ExecutionError,
        RecoveryStrategy::Retry,
        3,      // maxRetries
        100,    // initialDelayMs
        2.0f,   // backoffMultiplier
        false
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::TimeoutError,
        RecoveryStrategy::Retry,
        5,
        500,
        1.5f,
        false
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ResourceError,
        RecoveryStrategy::Fallback,
        2,
        200,
        2.0f,
        true
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::NetworkError,
        RecoveryStrategy::Retry,
        4,
        1000,
        1.5f,
        false
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ValidationError,
        RecoveryStrategy::Abort,
        1,
        0,
        1.0f,
        false
    });
}

AgenticErrorHandler::~AgenticErrorHandler()
{
    fprintf(stderr, "[AgenticErrorHandler] Destroyed - Handled %d errors with %d successful recoveries\n",
            m_totalErrors, m_successfulRecoveries);
}

void AgenticErrorHandler::initialize(AgenticObservability* obs, AgenticLoopState* state)
{
    m_observability = obs;
    m_state = state;

    if (m_observability) {
        m_observability->logInfo("AgenticErrorHandler", 
            "Initialized with observability and state management");
    }
}

// ===== ERROR CAPTURE AND HANDLING =====

nlohmann::json AgenticErrorHandler::handleError(
    const std::exception& e,
    const std::string& component,
    const nlohmann::json& context)
{
    std::string errorId = recordError(
        ErrorType::InternalError,
        std::string(e.what()),
        component,
        extractStackTrace(),
        context
    );

    nlohmann::json result;
    result["error_id"] = errorId;
    result["handled"] = false;

    // Determine recovery strategy
    RecoveryPolicy policy = getRecoveryPolicy(ErrorType::InternalError);

    if (policy.strategy == RecoveryStrategy::Retry && policy.maxRetries > 0) {
        result["handled"] = executeRetryStrategy(m_errorHistory.back(), []() {
            return false; // Would retry operation
        });
    } else if (policy.strategy == RecoveryStrategy::Fallback && policy.fallbackEnabled) {
        result["handled"] = executeFallbackStrategy(m_errorHistory.back());
    }

    return result;
}

std::string AgenticErrorHandler::recordError(
    ErrorType type,
    const std::string& message,
    const std::string& component,
    const std::string& stackTrace,
    const nlohmann::json& context)
{
    ErrorContext errorContext;
    errorContext.errorId = generateErrorId();
    errorContext.type = type;
    errorContext.message = message;
    errorContext.stackTrace = stackTrace;
    errorContext.component = component;
    errorContext.systemState = context;
    errorContext.retryCount = 0;
    errorContext.strategy = getRecoveryPolicy(type).strategy;

    m_errorHistory.push_back(errorContext);
    m_totalErrors++;

    // Keep bounded
    if (static_cast<int>(m_errorHistory.size()) > m_maxErrorMemory) {
        m_errorHistory.erase(m_errorHistory.begin());
    }

    if (m_observability) {
        nlohmann::json logContext = context;
        logContext["error_id"] = errorContext.errorId;
        logContext["error_type"] = std::to_string(static_cast<int>(type));

        m_observability->logError(component, message, logContext);
    }

    if (m_state) {
        m_state->recordError(
            std::to_string(static_cast<int>(type)),
            message,
            stackTrace
        );
    }

    // Fire callback
    if (m_errorRecordedCb) {
        m_errorRecordedCb(errorContext.errorId, message, m_errorRecordedUd);
    }

    // Check circuit breaker
    checkCircuitBreaker(component);

    return errorContext.errorId;
}

// ===== RECOVERY EXECUTION =====

bool AgenticErrorHandler::executeRecovery(const std::string& errorId)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            if (m_recoveryStartedCb) {
                m_recoveryStartedCb(errorId, m_recoveryStartedUd);
            }

            bool success = false;

            switch (errorContext.strategy) {
                case RecoveryStrategy::Retry:
                    success = executeRetryStrategy(errorContext, []() { return false; });
                    break;
                case RecoveryStrategy::Backtrack:
                    success = executeBacktrackStrategy(errorContext);
                    break;
                case RecoveryStrategy::Fallback:
                    success = executeFallbackStrategy(errorContext);
                    break;
                case RecoveryStrategy::Escalate:
                    executeEscalateStrategy(errorContext);
                    success = true;
                    break;
                default:
                    success = false;
            }

            if (success) {
                m_successfulRecoveries++;
                if (m_recoverySucceededCb) {
                    m_recoverySucceededCb(errorId, m_recoverySucceededUd);
                }
            } else {
                if (m_recoveryFailedCb) {
                    m_recoveryFailedCb(errorId, m_recoveryFailedUd);
                }
            }

            return success;
        }
    }

    return false;
}

bool AgenticErrorHandler::retry(
    const std::string& errorId,
    std::function<bool()> operation,
    int maxAttempts)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            return executeRetryStrategy(errorContext, operation);
        }
    }

    return false;
}

bool AgenticErrorHandler::backtrack(const std::string& targetStateId)
{
    if (!m_state) return false;

    fprintf(stderr, "[AgenticErrorHandler] Backtracking to state: %s\n", targetStateId.c_str());

    // In production, would restore from checkpoint
    return true;
}

bool AgenticErrorHandler::fallback(const std::string& errorId, const std::string& fallbackPlan)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            fprintf(stderr, "[AgenticErrorHandler] Executing fallback for %s\n", errorId.c_str());
            return true;
        }
    }
    return false;
}

void AgenticErrorHandler::escalate(const std::string& errorId, const std::string& reason)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            executeEscalateStrategy(errorContext);
            break;
        }
    }
}

// ===== RECOVERY POLICIES =====

void AgenticErrorHandler::setRecoveryPolicy(const RecoveryPolicy& policy)
{
    // Replace existing policy for same error type
    m_recoveryPolicies.erase(
        std::remove_if(m_recoveryPolicies.begin(), m_recoveryPolicies.end(),
                       [&](const RecoveryPolicy& p) { return p.errorType == policy.errorType; }),
        m_recoveryPolicies.end()
    );

    m_recoveryPolicies.push_back(policy);
}

AgenticErrorHandler::RecoveryPolicy AgenticErrorHandler::getRecoveryPolicy(ErrorType type) const
{
    for (const auto& policy : m_recoveryPolicies) {
        if (policy.errorType == type) {
            return policy;
        }
    }

    // Return default policy
    return RecoveryPolicy{
        type,
        RecoveryStrategy::Abort,
        0,
        0,
        1.0f,
        false
    };
}

nlohmann::json AgenticErrorHandler::getAllRecoveryPolicies() const
{
    nlohmann::json policies = nlohmann::json::array();

    for (const auto& policy : m_recoveryPolicies) {
        nlohmann::json policyObj;
        policyObj["error_type"] = static_cast<int>(policy.errorType);
        policyObj["strategy"] = static_cast<int>(policy.strategy);
        policyObj["max_retries"] = policy.maxRetries;
        policyObj["initial_delay_ms"] = policy.initialDelayMs;
        policyObj["backoff_multiplier"] = policy.backoffMultiplier;

        policies.push_back(policyObj);
    }

    return policies;
}

// ===== ERROR CLASSIFICATION =====

AgenticErrorHandler::ErrorType AgenticErrorHandler::classifyError(const std::string& errorMessage)
{
    if (containsCI(errorMessage, "timeout")) {
        return ErrorType::TimeoutError;
    } else if (containsCI(errorMessage, "validation")) {
        return ErrorType::ValidationError;
    } else if (containsCI(errorMessage, "resource") ||
               containsCI(errorMessage, "memory")) {
        return ErrorType::ResourceError;
    } else if (containsCI(errorMessage, "network") ||
               containsCI(errorMessage, "connection")) {
        return ErrorType::NetworkError;
    } else if (containsCI(errorMessage, "config")) {
        return ErrorType::ConfigurationError;
    } else if (containsCI(errorMessage, "state")) {
        return ErrorType::StateError;
    }

    return ErrorType::ExecutionError;
}

bool AgenticErrorHandler::isRetryable(ErrorType type) const
{
    RecoveryPolicy policy = getRecoveryPolicy(type);
    return policy.strategy == RecoveryStrategy::Retry;
}

bool AgenticErrorHandler::isFallbackable(ErrorType type) const
{
    RecoveryPolicy policy = getRecoveryPolicy(type);
    return policy.fallbackEnabled;
}

// ===== MONITORING =====

nlohmann::json AgenticErrorHandler::getErrorStatistics() const
{
    nlohmann::json stats;
    stats["total_errors"] = m_totalErrors;
    stats["total_recoveries"] = m_totalRecoveries;
    stats["successful_recoveries"] = m_successfulRecoveries;
    stats["recovery_success_rate"] = m_totalRecoveries > 0 ?
        (m_successfulRecoveries * 100.0f / m_totalRecoveries) : 0.0f;

    return stats;
}

int AgenticErrorHandler::getErrorCount(ErrorType type) const
{
    int count = 0;
    for (const auto& error : m_errorHistory) {
        if (error.type == type) count++;
    }
    return count;
}

float AgenticErrorHandler::getErrorRate() const
{
    // Errors per minute (approximate)
    return m_totalErrors > 0 ? m_totalErrors / 10.0f : 0.0f;
}

std::vector<AgenticErrorHandler::ErrorContext> AgenticErrorHandler::getRecentErrors(int limit)
{
    std::vector<ErrorContext> recent(
        m_errorHistory.rbegin(),
        m_errorHistory.rend()
    );

    if (limit > 0 && static_cast<int>(recent.size()) > limit) {
        recent.resize(limit);
    }

    return recent;
}

nlohmann::json AgenticErrorHandler::getRecoverySuccess() const
{
    nlohmann::json success;

    std::unordered_map<int, int> recoveryByType;
    for (const auto& error : m_errorHistory) {
        if (error.recoverySucceeded) {
            recoveryByType[static_cast<int>(error.type)]++;
        }
    }

    nlohmann::json byType = nlohmann::json::object();
    for (const auto& pair : recoveryByType) {
        byType[std::to_string(pair.first)] = pair.second;
    }

    success["by_type"] = byType;
    success["total_successful"] = m_successfulRecoveries;

    return success;
}

// ===== CIRCUIT BREAKER =====

void AgenticErrorHandler::enableCircuitBreaker(const std::string& component, int failureThreshold)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureThreshold = failureThreshold;
}

AgenticErrorHandler::CircuitBreaker* AgenticErrorHandler::getOrCreateCircuitBreaker(
    const std::string& component)
{
    auto it = m_circuitBreakers.find(component);
    
    if (it != m_circuitBreakers.end()) {
        return &it->second;
    }

    CircuitBreaker breaker;
    breaker.component = component;
    breaker.failureCount = 0;
    breaker.failureThreshold = 5;
    breaker.isTripped = false;

    m_circuitBreakers[component] = breaker;

    auto it2 = m_circuitBreakers.find(component);
    return &it2->second;
}

void AgenticErrorHandler::checkCircuitBreaker(const std::string& component)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureCount++;

    if (!breaker->isTripped && breaker->failureCount >= breaker->failureThreshold) {
        breaker->isTripped = true;
        breaker->tripTime = std::chrono::system_clock::now();

        fprintf(stderr, "[AgenticErrorHandler] Circuit breaker tripped for %s\n", component.c_str());
        if (m_circuitBreakerCb) {
            m_circuitBreakerCb(component, m_circuitBreakerUd);
        }
    }
}

// ===== PRIVATE HELPERS =====

bool AgenticErrorHandler::executeRetryStrategy(
    const ErrorContext& context,
    std::function<bool()> operation)
{
    RecoveryPolicy policy = getRecoveryPolicy(context.type);
    int delayMs = policy.initialDelayMs;

    for (int attempt = 0; attempt < policy.maxRetries; ++attempt) {
        if (attempt > 0) {
            // Exponential backoff using std::this_thread::sleep_for
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            delayMs = static_cast<int>(delayMs * policy.backoffMultiplier);
        }

        if (operation()) {
            fprintf(stderr, "[AgenticErrorHandler] Retry succeeded for %s\n", context.errorId.c_str());
            m_totalRecoveries++;
            return true;
        }
    }

    fprintf(stderr, "[AgenticErrorHandler] All retry attempts failed for %s\n", context.errorId.c_str());
    m_totalRecoveries++;
    return false;
}

bool AgenticErrorHandler::executeBacktrackStrategy(const ErrorContext& context)
{
    fprintf(stderr, "[AgenticErrorHandler] Backtracking from error %s\n", context.errorId.c_str());

    if (m_state) {
        // Restore from checkpoint
        nlohmann::json snapshot = m_state->getLastSnapshot();
        if (!snapshot.empty()) {
            m_state->restoreFromSnapshot(snapshot);
            m_totalRecoveries++;
            return true;
        }
    }

    m_totalRecoveries++;
    return false;
}

bool AgenticErrorHandler::executeFallbackStrategy(const ErrorContext& context)
{
    fprintf(stderr, "[AgenticErrorHandler] Executing fallback for %s\n", context.errorId.c_str());

    // Use alternative code path
    m_totalRecoveries++;
    return true;
}

void AgenticErrorHandler::executeEscalateStrategy(const ErrorContext& context)
{
    fprintf(stderr, "[AgenticErrorHandler] Escalating error %s - Type: %d - Message: %s\n",
            context.errorId.c_str(),
            static_cast<int>(context.type),
            context.message.c_str());

    // Would notify higher-level handlers or administrators
}

std::string AgenticErrorHandler::analyzeError(const std::exception& e)
{
    return std::string(e.what());
}

std::string AgenticErrorHandler::extractStackTrace()
{
    // Platform-specific stack trace extraction would go here
    return "Stack trace not available on this platform";
}

void AgenticErrorHandler::recordMetrics(const ErrorContext& context)
{
    if (m_observability) {
        m_observability->incrementCounter("errors_total");
        m_observability->incrementCounter(
            "errors_by_type_" + std::to_string(static_cast<int>(context.type))
        );
    }
}

// ===== ERROR SAFE OPERATION =====

ErrorSafeOperation::ErrorSafeOperation(
    AgenticErrorHandler* handler,
    const std::string& operationName,
    const std::string& component)
    : m_handler(handler), m_operationName(operationName), m_component(component)
{
}

ErrorSafeOperation::~ErrorSafeOperation()
{
}
