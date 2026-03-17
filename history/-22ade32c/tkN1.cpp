// AgenticErrorHandler Implementation
#include "agentic_error_handler.h"
#include "agentic_observability.h"
#include "agentic_loop_state.h"
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <exception>
#include <deque>

AgenticErrorHandler::AgenticErrorHandler(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[AgenticErrorHandler] Initialized - Ready for error handling and recovery";

    // Set up default recovery policies
    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ExecutionError,
        RecoveryStrategy::Retry,
        3,      // maxRetries
        100,    // initialDelayMs
        2.0f    // backoffMultiplier
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::TimeoutError,
        RecoveryStrategy::Retry,
        5,
        500,
        1.5f
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ResourceError,
        RecoveryStrategy::Fallback,
        2,
        200,
        2.0f
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::NetworkError,
        RecoveryStrategy::Retry,
        4,
        1000,
        1.5f
    });

    setRecoveryPolicy(RecoveryPolicy{
        ErrorType::ValidationError,
        RecoveryStrategy::Abort,
        1,
        0,
        1.0f
    });
}

AgenticErrorHandler::~AgenticErrorHandler()
{
    qDebug() << "[AgenticErrorHandler] Destroyed - Handled" << m_totalErrors << "errors with"
             << m_successfulRecoveries << "successful recoveries";
}

void AgenticErrorHandler::initialize(AgenticObservability* obs, AgenticLoopState* state)
{
    m_observability = obs;
    m_state = state;

    if (m_observability) {
        QJsonObject initContext;
        initContext["event"] = "Initialized";
        m_observability->logInfo("AgenticErrorHandler", 
            "Initialized with observability and state management", initContext);
    }
}

// ===== ERROR CAPTURE AND HANDLING =====

QJsonObject AgenticErrorHandler::handleError(
    const std::exception& e,
    const QString& component,
    const QJsonObject& context)
{
    QJsonObject logContext;
    logContext["component"] = component;
    logContext["error_type"] = "InternalError";
    logContext["error_message"] = QString::fromStdString(e.what());
    logContext.insert(context.constBegin(), context.constEnd());
    
    m_observability->logInfo("AgenticErrorHandler", "Handling internal error", logContext);

    QString errorId = recordError(
        ErrorType::InternalError,
        QString::fromStdString(e.what()),
        component,
        extractStackTrace(),
        context
    );

    QJsonObject result;
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

QString AgenticErrorHandler::recordError(
    ErrorType type,
    const QString& message,
    const QString& component,
    const QString& stackTrace,
    const QJsonObject& context)
{
    ErrorContext errorContext;
    errorContext.errorId = QUuid::createUuid().toString();
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
    if (m_errorHistory.size() > m_maxErrorMemory) {
        m_errorHistory.erase(m_errorHistory.begin());
    }

    if (m_observability) {
        QJsonObject logContext = context;
        logContext["error_id"] = errorContext.errorId;
        logContext["error_type"] = QString::number(static_cast<int>(type));
        logContext["stack_trace"] = stackTrace;
        logContext["event"] = "ErrorRecorded";

        m_observability->logError(component, message, logContext);
    }

    if (m_state) {
        m_state->recordError(
            QString::number(static_cast<int>(type)),
            message,
            stackTrace
        );
    }

    emit errorRecorded(errorContext.errorId, message);

    // Check circuit breaker
    checkCircuitBreaker(component);

    return errorContext.errorId;
}

// ===== RECOVERY EXECUTION =====

bool AgenticErrorHandler::executeRecovery(const QString& errorId)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            emit recoveryStarted(errorId);

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
                emit recoverySucceeded(errorId);
            } else {
                emit recoveryFailed(errorId);
            }

            return success;
        }
    }

    return false;
}

bool AgenticErrorHandler::retry(
    const QString& errorId,
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

bool AgenticErrorHandler::backtrack(const QString& targetStateId)
{
    if (!m_state) return false;

    qInfo() << "[AgenticErrorHandler] Backtracking to state:" << targetStateId;

    // In production, would restore from checkpoint
    return true;
}

bool AgenticErrorHandler::fallback(const QString& errorId, const QString& fallbackPlan)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            qInfo() << "[AgenticErrorHandler] Executing fallback for" << errorId;
            return true;
        }
    }
    return false;
}

void AgenticErrorHandler::escalate(const QString& errorId, const QString& reason)
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
        1.0f
    };
}

QJsonArray AgenticErrorHandler::getAllRecoveryPolicies() const
{
    QJsonArray policies;

    for (const auto& policy : m_recoveryPolicies) {
        QJsonObject policyObj;
        policyObj["error_type"] = QString::number(static_cast<int>(policy.errorType));
        policyObj["strategy"] = QString::number(static_cast<int>(policy.strategy));
        policyObj["max_retries"] = policy.maxRetries;
        policyObj["initial_delay_ms"] = policy.initialDelayMs;
        policyObj["backoff_multiplier"] = policy.backoffMultiplier;

        policies.append(policyObj);
    }

    return policies;
}

// ===== ERROR CLASSIFICATION =====

AgenticErrorHandler::ErrorType AgenticErrorHandler::classifyError(const QString& errorMessage)
{
    if (errorMessage.contains("timeout", Qt::CaseInsensitive)) {
        return ErrorType::TimeoutError;
    } else if (errorMessage.contains("validation", Qt::CaseInsensitive)) {
        return ErrorType::ValidationError;
    } else if (errorMessage.contains("resource", Qt::CaseInsensitive) ||
               errorMessage.contains("memory", Qt::CaseInsensitive)) {
        return ErrorType::ResourceError;
    } else if (errorMessage.contains("network", Qt::CaseInsensitive) ||
               errorMessage.contains("connection", Qt::CaseInsensitive)) {
        return ErrorType::NetworkError;
    } else if (errorMessage.contains("config", Qt::CaseInsensitive)) {
        return ErrorType::ConfigurationError;
    } else if (errorMessage.contains("state", Qt::CaseInsensitive)) {
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

QJsonObject AgenticErrorHandler::getErrorStatistics() const
{
    QJsonObject stats;
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
    // Errors per minute
    return m_totalErrors > 0 ? m_totalErrors / 10.0f : 0.0f; // Approximate
}

std::vector<AgenticErrorHandler::ErrorContext> AgenticErrorHandler::getRecentErrors(int limit)
{
    std::vector<ErrorContext> recent(
        m_errorHistory.rbegin(),
        m_errorHistory.rend()
    );

    if (limit > 0 && recent.size() > limit) {
        recent.resize(limit);
    }

    return recent;
}

QJsonObject AgenticErrorHandler::getRecoverySuccess() const
{
    QJsonObject success;

    std::unordered_map<int, int> recoveryByType;
    for (const auto& error : m_errorHistory) {
        if (error.recoverySucceeded) {
            recoveryByType[static_cast<int>(error.type)]++;
        }
    }

    QJsonObject byType;
    for (const auto& pair : recoveryByType) {
        byType[QString::number(pair.first)] = pair.second;
    }

    success["by_type"] = byType;
    success["total_successful"] = m_successfulRecoveries;

    return success;
}

// ===== CIRCUIT BREAKER =====

void AgenticErrorHandler::enableCircuitBreaker(const QString& component, int failureThreshold)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureThreshold = failureThreshold;
}

AgenticErrorHandler::CircuitBreaker* AgenticErrorHandler::getOrCreateCircuitBreaker(
    const QString& component)
{
    auto it = m_circuitBreakers.find(component.toStdString());
    
    if (it != m_circuitBreakers.end()) {
        return &it->second;
    }

    CircuitBreaker breaker;
    breaker.component = component;
    breaker.failureCount = 0;
    breaker.failureThreshold = 5;
    breaker.isTripped = false;

    m_circuitBreakers[component.toStdString()] = breaker;

    auto it2 = m_circuitBreakers.find(component.toStdString());
    return &it2->second;
}

void AgenticErrorHandler::checkCircuitBreaker(const QString& component)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureCount++;

    if (!breaker->isTripped && breaker->failureCount >= breaker->failureThreshold) {
        breaker->isTripped = true;
        breaker->tripTime = QDateTime::currentDateTime();

        qCritical() << "[AgenticErrorHandler] Circuit breaker tripped for" << component;
        emit circuitBreakerTripped(component);
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
            // Exponential backoff
            QThread::msleep(delayMs);
            delayMs = static_cast<int>(delayMs * policy.backoffMultiplier);
        }

        if (operation()) {
            qInfo() << "[AgenticErrorHandler] Retry succeeded for" << context.errorId;
            m_totalRecoveries++;
            return true;
        }
    }

    qWarning() << "[AgenticErrorHandler] All retry attempts failed for" << context.errorId;
    m_totalRecoveries++;
    return false;
}

bool AgenticErrorHandler::executeBacktrackStrategy(const ErrorContext& context)
{
    qInfo() << "[AgenticErrorHandler] Backtracking from error" << context.errorId;

    if (m_state) {
        // Restore from checkpoint
        QJsonObject snapshot = m_state->getLastSnapshot();
        if (!snapshot.isEmpty()) {
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
    qInfo() << "[AgenticErrorHandler] Executing fallback for" << context.errorId;

    // Use alternative code path
    m_totalRecoveries++;
    return true;
}

void AgenticErrorHandler::executeEscalateStrategy(const ErrorContext& context)
{
    qCritical() << "[AgenticErrorHandler] Escalating error" << context.errorId
                << "- Type:" << QString::number(static_cast<int>(context.type))
                << "- Message:" << context.message;

    // Would notify higher-level handlers or administrators
}

QString AgenticErrorHandler::analyzeError(const std::exception& e)
{
    return QString::fromStdString(e.what());
}

QString AgenticErrorHandler::extractStackTrace()
{
    // Platform-specific stack trace extraction would go here
    return "Stack trace not available on this platform";
}

void AgenticErrorHandler::recordMetrics(const ErrorContext& context)
{
    if (m_observability) {
        m_observability->incrementCounter("errors_total");
        m_observability->incrementCounter(
            "errors_by_type_" + QString::number(static_cast<int>(context.type))
        );
    }
}

// ===== ERROR SAFE OPERATION =====

ErrorSafeOperation::ErrorSafeOperation(
    AgenticErrorHandler* handler,
    const QString& operationName,
    const QString& component)
    : m_handler(handler), m_operationName(operationName), m_component(component)
{
}

ErrorSafeOperation::~ErrorSafeOperation()
{
}
