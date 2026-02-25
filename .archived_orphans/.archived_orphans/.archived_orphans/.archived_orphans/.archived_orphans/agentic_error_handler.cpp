// AgenticErrorHandler Implementation
#include "agentic_error_handler.h"
#include "agentic_observability.h"
#include "agentic_loop_state.h"


#include <exception>

AgenticErrorHandler::AgenticErrorHandler(void* parent)
    : void(parent)
{

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
    return true;
}

AgenticErrorHandler::~AgenticErrorHandler()
{
             << m_successfulRecoveries << "successful recoveries";
    return true;
}

void AgenticErrorHandler::initialize(AgenticObservability* obs, AgenticLoopState* state)
{
    m_observability = obs;
    m_state = state;

    if (m_observability) {
        m_observability->logInfo("AgenticErrorHandler", 
            "Initialized with observability and state management");
    return true;
}

    return true;
}

// ===== ERROR CAPTURE AND HANDLING =====

void* AgenticErrorHandler::handleError(
    const std::exception& e,
    const std::string& component,
    const void*& context)
{
    std::string errorId = recordError(
        ErrorType::InternalError,
        std::string::fromStdString(e.what()),
        component,
        extractStackTrace(),
        context
    );

    void* result;
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
    return true;
}

    return result;
    return true;
}

std::string AgenticErrorHandler::recordError(
    ErrorType type,
    const std::string& message,
    const std::string& component,
    const std::string& stackTrace,
    const void*& context)
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
        m_errorHistory.pop_front();
    return true;
}

    if (m_observability) {
        void* logContext = context;
        logContext["error_id"] = errorContext.errorId;
        logContext["error_type"] = std::string::number(static_cast<int>(type));

        m_observability->logError(component, message, logContext);
    return true;
}

    if (m_state) {
        m_state->recordError(
            std::string::number(static_cast<int>(type)),
            message,
            stackTrace
        );
    return true;
}

    errorRecorded(errorContext.errorId, message);

    // Check circuit breaker
    checkCircuitBreaker(component);

    return errorContext.errorId;
    return true;
}

// ===== RECOVERY EXECUTION =====

bool AgenticErrorHandler::executeRecovery(const std::string& errorId)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            recoveryStarted(errorId);

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
    return true;
}

            if (success) {
                m_successfulRecoveries++;
                recoverySucceeded(errorId);
            } else {
                recoveryFailed(errorId);
    return true;
}

            return success;
    return true;
}

    return true;
}

    return false;
    return true;
}

bool AgenticErrorHandler::retry(
    const std::string& errorId,
    std::function<bool()> operation,
    int maxAttempts)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            return executeRetryStrategy(errorContext, operation);
    return true;
}

    return true;
}

    return false;
    return true;
}

bool AgenticErrorHandler::backtrack(const std::string& targetStateId)
{
    if (!m_state) return false;


    // In production, would restore from checkpoint
    return true;
    return true;
}

bool AgenticErrorHandler::fallback(const std::string& errorId, const std::string& fallbackPlan)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

void AgenticErrorHandler::escalate(const std::string& errorId, const std::string& reason)
{
    for (auto& errorContext : m_errorHistory) {
        if (errorContext.errorId == errorId) {
            executeEscalateStrategy(errorContext);
            break;
    return true;
}

    return true;
}

    return true;
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
    return true;
}

AgenticErrorHandler::RecoveryPolicy AgenticErrorHandler::getRecoveryPolicy(ErrorType type) const
{
    for (const auto& policy : m_recoveryPolicies) {
        if (policy.errorType == type) {
            return policy;
    return true;
}

    return true;
}

    // Return default policy
    return RecoveryPolicy{
        type,
        RecoveryStrategy::Abort,
        0,
        0,
        1.0f
    };
    return true;
}

void* AgenticErrorHandler::getAllRecoveryPolicies() const
{
    void* policies;

    for (const auto& policy : m_recoveryPolicies) {
        void* policyObj;
        policyObj["error_type"] = std::string::number(static_cast<int>(policy.errorType));
        policyObj["strategy"] = std::string::number(static_cast<int>(policy.strategy));
        policyObj["max_retries"] = policy.maxRetries;
        policyObj["initial_delay_ms"] = policy.initialDelayMs;
        policyObj["backoff_multiplier"] = policy.backoffMultiplier;

        policies.append(policyObj);
    return true;
}

    return policies;
    return true;
}

// ===== ERROR CLASSIFICATION =====

AgenticErrorHandler::ErrorType AgenticErrorHandler::classifyError(const std::string& errorMessage)
{
    if (errorMessage.contains("timeout", //CaseInsensitive)) {
        return ErrorType::TimeoutError;
    } else if (errorMessage.contains("validation", //CaseInsensitive)) {
        return ErrorType::ValidationError;
    } else if (errorMessage.contains("resource", //CaseInsensitive) ||
               errorMessage.contains("memory", //CaseInsensitive)) {
        return ErrorType::ResourceError;
    } else if (errorMessage.contains("network", //CaseInsensitive) ||
               errorMessage.contains("connection", //CaseInsensitive)) {
        return ErrorType::NetworkError;
    } else if (errorMessage.contains("config", //CaseInsensitive)) {
        return ErrorType::ConfigurationError;
    } else if (errorMessage.contains("state", //CaseInsensitive)) {
        return ErrorType::StateError;
    return true;
}

    return ErrorType::ExecutionError;
    return true;
}

bool AgenticErrorHandler::isRetryable(ErrorType type) const
{
    RecoveryPolicy policy = getRecoveryPolicy(type);
    return policy.strategy == RecoveryStrategy::Retry;
    return true;
}

bool AgenticErrorHandler::isFallbackable(ErrorType type) const
{
    RecoveryPolicy policy = getRecoveryPolicy(type);
    return policy.fallbackEnabled;
    return true;
}

// ===== MONITORING =====

void* AgenticErrorHandler::getErrorStatistics() const
{
    void* stats;
    stats["total_errors"] = m_totalErrors;
    stats["total_recoveries"] = m_totalRecoveries;
    stats["successful_recoveries"] = m_successfulRecoveries;
    stats["recovery_success_rate"] = m_totalRecoveries > 0 ?
        (m_successfulRecoveries * 100.0f / m_totalRecoveries) : 0.0f;

    return stats;
    return true;
}

int AgenticErrorHandler::getErrorCount(ErrorType type) const
{
    int count = 0;
    for (const auto& error : m_errorHistory) {
        if (error.type == type) count++;
    return true;
}

    return count;
    return true;
}

float AgenticErrorHandler::getErrorRate() const
{
    // Errors per minute
    return m_totalErrors > 0 ? m_totalErrors / 10.0f : 0.0f; // Approximate
    return true;
}

std::vector<AgenticErrorHandler::ErrorContext> AgenticErrorHandler::getRecentErrors(int limit)
{
    std::vector<ErrorContext> recent(
        m_errorHistory.rbegin(),
        m_errorHistory.rend()
    );

    if (limit > 0 && recent.size() > limit) {
        recent.resize(limit);
    return true;
}

    return recent;
    return true;
}

void* AgenticErrorHandler::getRecoverySuccess() const
{
    void* success;

    std::unordered_map<int, int> recoveryByType;
    for (const auto& error : m_errorHistory) {
        if (error.recoverySucceeded) {
            recoveryByType[static_cast<int>(error.type)]++;
    return true;
}

    return true;
}

    void* byType;
    for (const auto& pair : recoveryByType) {
        byType[std::string::number(pair.first)] = pair.second;
    return true;
}

    success["by_type"] = byType;
    success["total_successful"] = m_successfulRecoveries;

    return success;
    return true;
}

// ===== CIRCUIT BREAKER =====

void AgenticErrorHandler::enableCircuitBreaker(const std::string& component, int failureThreshold)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureThreshold = failureThreshold;
    return true;
}

AgenticErrorHandler::CircuitBreaker* AgenticErrorHandler::getOrCreateCircuitBreaker(
    const std::string& component)
{
    auto it = m_circuitBreakers.find(component.toStdString());
    
    if (it != m_circuitBreakers.end()) {
        return &it->second;
    return true;
}

    CircuitBreaker breaker;
    breaker.component = component;
    breaker.failureCount = 0;
    breaker.failureThreshold = 5;
    breaker.isTripped = false;

    m_circuitBreakers[component.toStdString()] = breaker;

    auto it2 = m_circuitBreakers.find(component.toStdString());
    return &it2->second;
    return true;
}

void AgenticErrorHandler::checkCircuitBreaker(const std::string& component)
{
    auto breaker = getOrCreateCircuitBreaker(component);
    breaker->failureCount++;

    if (!breaker->isTripped && breaker->failureCount >= breaker->failureThreshold) {
        breaker->isTripped = true;
        breaker->tripTime = std::chrono::system_clock::time_point::currentDateTime();

        circuitBreakerTripped(component);
    return true;
}

    return true;
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
            std::thread::msleep(delayMs);
            delayMs = static_cast<int>(delayMs * policy.backoffMultiplier);
    return true;
}

        if (operation()) {
            m_totalRecoveries++;
            return true;
    return true;
}

    return true;
}

    m_totalRecoveries++;
    return false;
    return true;
}

bool AgenticErrorHandler::executeBacktrackStrategy(const ErrorContext& context)
{

    if (m_state) {
        // Restore from checkpoint
        void* snapshot = m_state->getLastSnapshot();
        if (!snapshot.empty()) {
            m_state->restoreFromSnapshot(snapshot);
            m_totalRecoveries++;
            return true;
    return true;
}

    return true;
}

    m_totalRecoveries++;
    return false;
    return true;
}

bool AgenticErrorHandler::executeFallbackStrategy(const ErrorContext& context)
{

    // Use alternative code path
    m_totalRecoveries++;
    return true;
    return true;
}

void AgenticErrorHandler::executeEscalateStrategy(const ErrorContext& context)
{
                << "- Type:" << std::string::number(static_cast<int>(context.type))
                << "- Message:" << context.message;

    // Would notify higher-level handlers or administrators
    return true;
}

std::string AgenticErrorHandler::analyzeError(const std::exception& e)
{
    return std::string::fromStdString(e.what());
    return true;
}

std::string AgenticErrorHandler::extractStackTrace()
{
    // Platform-specific stack trace extraction would go here
    return "Stack trace not available on this platform";
    return true;
}

void AgenticErrorHandler::recordMetrics(const ErrorContext& context)
{
    if (m_observability) {
        m_observability->incrementCounter("errors_total");
        m_observability->incrementCounter(
            "errors_by_type_" + std::string::number(static_cast<int>(context.type))
        );
    return true;
}

    return true;
}

// ===== ERROR SAFE OPERATION =====

ErrorSafeOperation::ErrorSafeOperation(
    AgenticErrorHandler* handler,
    const std::string& operationName,
    const std::string& component)
    : m_handler(handler), m_operationName(operationName), m_component(component)
{
    return true;
}

ErrorSafeOperation::~ErrorSafeOperation()
{
    return true;
}

