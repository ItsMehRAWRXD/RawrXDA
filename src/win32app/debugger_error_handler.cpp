#include "debugger_error_handler.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <thread>
#include <iomanip>

// =============================================================================
// DebuggerError Implementation
// =============================================================================

std::string DebuggerError::toString() const noexcept {
    try {
        std::ostringstream oss;

        oss << "[" << severityToString(severity) << "] "
            << errorTypeToString(type) << "\n"
            << "Message: " << message << "\n";

        if (!context.empty()) {
            oss << "Context: " << context << "\n";
        }

        if (!details.empty()) {
            oss << "Details: " << details << "\n";
        }

        if (!functionName.empty()) {
            oss << "Function: " << functionName << ":" << lineNumber << "\n";
        }

        return oss.str();
    } catch (...) {
        return "[ERROR] Error formatting unavailable\n";
    }
}

std::string DebuggerError::severityToString(ErrorSeverity sev) noexcept {
    try {
        switch (sev) {
            case ErrorSeverity::Info:
                return "INFO";
            case ErrorSeverity::Warning:
                return "WARNING";
            case ErrorSeverity::Error:
                return "ERROR";
            case ErrorSeverity::Critical:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    } catch (...) {
        return {};
    }
}

std::string DebuggerError::errorTypeToString(DebuggerErrorType type) noexcept {
    try {
        switch (type) {
            case DebuggerErrorType::Success:
                return "Success";
            case DebuggerErrorType::AdapterConnectionFailed:
                return "Adapter Connection Failed";
            case DebuggerErrorType::AdapterProcessCrashed:
                return "Adapter Process Crashed";
            case DebuggerErrorType::AdapterCommunicationFailed:
                return "Adapter Communication Failed";
            case DebuggerErrorType::AdapterTimeout:
                return "Adapter Timeout";
            case DebuggerErrorType::FrameStackWalkFailed:
                return "Frame Stack Walk Failed";
            case DebuggerErrorType::FrameIndexOutOfRange:
                return "Frame Index Out Of Range";
            case DebuggerErrorType::FrameDataCorrupted:
                return "Frame Data Corrupted";
            case DebuggerErrorType::FrameLocalsUnavailable:
                return "Frame Locals Unavailable";
            case DebuggerErrorType::FrameSymbolResolutionFailed:
                return "Frame Symbol Resolution Failed";
            case DebuggerErrorType::FrameNavigationLimitReached:
                return "Frame Navigation Limit Reached";
            case DebuggerErrorType::FrameNavigationFailed:
                return "Frame Navigation Failed";
            case DebuggerErrorType::BreakpointSetFailed:
                return "Breakpoint Set Failed";
            case DebuggerErrorType::BreakpointLocationInvalid:
                return "Breakpoint Location Invalid";
            case DebuggerErrorType::BreakpointConditionInvalid:
                return "Breakpoint Condition Invalid";
            case DebuggerErrorType::ExecutionControlFailed:
                return "Execution Control Failed";
            case DebuggerErrorType::StepInFailed:
                return "Step In Failed";
            case DebuggerErrorType::StepOutFailed:
                return "Step Out Failed";
            case DebuggerErrorType::ContinueFailed:
                return "Continue Failed";
            case DebuggerErrorType::PauseFailed:
                return "Pause Failed";
            case DebuggerErrorType::EvaluationFailed:
                return "Evaluation Failed";
            case DebuggerErrorType::ExpressionInvalid:
                return "Expression Invalid";
            case DebuggerErrorType::ExpressionContextLost:
                return "Expression Context Lost";
            case DebuggerErrorType::ExpressionTimeout:
                return "Expression Timeout";
            case DebuggerErrorType::SessionNotInitialized:
                return "Session Not Initialized";
            case DebuggerErrorType::SessionCorrupted:
                return "Session Corrupted";
            case DebuggerErrorType::SessionOperationNotSupported:
                return "Session Operation Not Supported";
            case DebuggerErrorType::UnknownError:
            default:
                return "Unknown Error";
        }
    } catch (...) {
        return {};
    }
}

// =============================================================================
// DebuggerErrorHandler Implementation
// =============================================================================

DebuggerErrorHandler::ScopedContext::ScopedContext(
    DebuggerErrorHandler& handler,
    const std::string& context) noexcept
    : handler_(&handler), active_(true) {
    handler_->pushContext(context);
}

DebuggerErrorHandler::ScopedContext::~ScopedContext() {
    if (active_ && handler_) {
        handler_->popContext();
    }
}

DebuggerErrorHandler::ScopedContext::ScopedContext(ScopedContext&& other) noexcept
    : handler_(other.handler_), active_(other.active_) {
    other.handler_ = nullptr;
    other.active_ = false;
}

DebuggerErrorHandler::ScopedContext& DebuggerErrorHandler::ScopedContext::operator=(
    ScopedContext&& other) noexcept {
    if (this != &other) {
        if (active_ && handler_) {
            handler_->popContext();
        }
        handler_ = other.handler_;
        active_ = other.active_;
        other.handler_ = nullptr;
        other.active_ = false;
    }
    return *this;
}

DebuggerErrorHandler::DebuggerErrorHandler()
    : totalErrors_(0), recoveredErrors_(0) {
    initializeDefaultStrategies();
}

bool DebuggerErrorHandler::reportError(const DebuggerError& error) noexcept {
    try {
        DebuggerError mutableError = normalizeError(error);
        ErrorCallback errorCb;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            errorCb = errorCallback_;
        }

        // Call error callback if registered
        if (errorCb) {
            try {
                errorCb(mutableError, mutableError.isRecoverable);
            } catch (...) {
                // Swallow callback exceptions
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            addToHistory(mutableError);
            ++totalErrors_;
        }
        
        // Attempt recovery if configured
        if (mutableError.isRecoverable &&
            mutableError.suggestedStrategy != RecoveryStrategy::None) {
            return attemptRecovery(mutableError);
        }

        // Error was successfully normalized, callback-dispatched, and persisted.
        // Even without a recovery strategy, this still counts as handled.
        return true;
        
    } catch (...) {
        return false;
    }
}

bool DebuggerErrorHandler::reportError(
    DebuggerErrorType type,
    ErrorSeverity severity,
    const std::string& message,
    const std::string& context) noexcept {
    try {
        DebuggerError err;
        err.type = type;
        err.severity = severity;
        err.message = message;
        err.context = context.empty() ? getCurrentContext() : context;
        err.suggestedStrategy = getRecoveryStrategy(type);
        err.isRecoverable = (severity < ErrorSeverity::Critical) &&
                            (err.suggestedStrategy != RecoveryStrategy::None);
        err.timestamp = std::chrono::system_clock::now();

        return reportError(err);
    } catch (...) {
        return false;
    }
}

void DebuggerErrorHandler::setErrorCallback(ErrorCallback callback) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        errorCallback_ = std::move(callback);
    } catch (...) {
        // Keep previous callback on failure
    }
}

void DebuggerErrorHandler::setRecoveryCallback(RecoveryCallback callback) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        recoveryCallback_ = std::move(callback);
    } catch (...) {
        // Keep previous callback on failure
    }
}

bool DebuggerErrorHandler::attemptRecovery(DebuggerError& error) noexcept {
    try {
        if (!error.isRecoverable) {
            return false;
        }

        const auto recoveryStart = std::chrono::steady_clock::now();

        auto markRecovered = [this, recoveryStart]() noexcept {
            try {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - recoveryStart).count();
                std::lock_guard<std::mutex> lock(mutex_);
                ++recoveredErrors_;
                totalRecoveryTimeMs_ += static_cast<uint64_t>(std::max<int64_t>(0, elapsed));
            } catch (...) {
                // Best effort only; recovery success should still propagate.
            }
            return true;
        };

        RecoveryCallback recoveryCb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            recoveryCb = recoveryCallback_;
        }
        
        // Call custom recovery callback if registered
        if (recoveryCb) {
            try {
                if (recoveryCb(error)) {
                    return markRecovered();
                }
            } catch (...) {
                // Continue with default recovery
            }
        }
        
        // Default recovery strategies
        switch (error.suggestedStrategy) {
            case RecoveryStrategy::Retry:
                if (error.retryCount < error.maxRetries) {
                    ++error.retryCount;
                    // Retry indicates caller should re-attempt the operation;
                    // no recovery has occurred yet, so do not count success.
                    return false;
                }
                break;
                
            case RecoveryStrategy::UseDefaultValue:
                return markRecovered();
                
            case RecoveryStrategy::SkipOperation:
                return markRecovered();
                
            case RecoveryStrategy::Fallback:
                return markRecovered();

            case RecoveryStrategy::ReattachAdapter:
            case RecoveryStrategy::RestartSession:
                // These strategies require the registered recovery callback to post
                // an async action (e.g. WM_DEBUGGER_REATTACH_SAFE) to the UI thread.
                // If no recovery callback is registered, we cannot reattach from here.
                return false;
                
            default:
                break;
        }
        
        return false;
        
    } catch (...) {
        return false;
    }
}

RecoveryStrategy DebuggerErrorHandler::getRecoveryStrategy(
    DebuggerErrorType type) const noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = recoveryStrategies_.find(type);
        if (it != recoveryStrategies_.end()) {
            return it->second;
        }
        return RecoveryStrategy::None;
    } catch (...) {
        return RecoveryStrategy::None;
    }
}

void DebuggerErrorHandler::setRecoveryStrategy(
    DebuggerErrorType type, RecoveryStrategy strategy) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        recoveryStrategies_[type] = strategy;
    } catch (...) {
        // Leave recovery strategy map unchanged on failure
    }
}

std::vector<DebuggerError> DebuggerErrorHandler::getRecentErrors(
    size_t maxCount) const noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DebuggerError> result;

        size_t start = (errorHistory_.size() > maxCount)
                       ? (errorHistory_.size() - maxCount)
                       : 0;

        for (size_t i = start; i < errorHistory_.size(); ++i) {
            result.push_back(errorHistory_[i]);
        }

        return result;
    } catch (...) {
        return {};
    }
}

DebuggerErrorHandler::ErrorStats DebuggerErrorHandler::getStatistics() const noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        ErrorStats stats;
        stats.totalErrors = totalErrors_;
        stats.recoveredErrors = recoveredErrors_;
        stats.unrecoveredErrors = (totalErrors_ >= recoveredErrors_)
            ? (totalErrors_ - recoveredErrors_)
            : 0;

        for (const auto& err : errorHistory_) {
            stats.errorCounts[err.type]++;
        }

        if (recoveredErrors_ > 0) {
            stats.avgRecoveryTime = std::chrono::milliseconds(
                static_cast<long long>(totalRecoveryTimeMs_ / recoveredErrors_));
        }

        return stats;
    } catch (...) {
        return ErrorStats{};
    }
}

void DebuggerErrorHandler::clear() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        errorHistory_.clear();
        contextStack_.clear();
        totalErrors_ = 0;
        recoveredErrors_ = 0;
        totalRecoveryTimeMs_ = 0;
    } catch (...) {
        // Best effort only
    }
}

std::string DebuggerErrorHandler::getErrorReport() const noexcept {
    try {
        size_t totalErrors = 0;
        size_t recoveredErrors = 0;
        std::vector<DebuggerError> recent;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            totalErrors = totalErrors_;
            recoveredErrors = recoveredErrors_;
            const size_t maxCount = 10;
            const size_t start = (errorHistory_.size() > maxCount)
                ? (errorHistory_.size() - maxCount)
                : 0;
            for (size_t i = start; i < errorHistory_.size(); ++i) {
                recent.push_back(errorHistory_[i]);
            }
        }

        std::ostringstream oss;

        oss << "=== Debugger Error Report ===\n"
            << "Total Errors: " << totalErrors << "\n"
            << "Recovered: " << recoveredErrors << "\n"
            << "Unrecovered: " << ((totalErrors >= recoveredErrors) ? (totalErrors - recoveredErrors) : 0) << "\n"
            << "\nRecent Errors:\n";

        for (size_t i = 0; i < recent.size(); ++i) {
            oss << "\n[" << (i + 1) << "]\n" << buildFullErrorMessage(recent[i]);
        }

        return oss.str();
    } catch (...) {
        return "=== Debugger Error Report ===\n<unavailable: memory or formatting failure>\n";
    }
}

void DebuggerErrorHandler::pushContext(const std::string& context) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        contextStack_.push_back(context);
    } catch (...) {
        // Best effort only
    }
}

void DebuggerErrorHandler::popContext() noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!contextStack_.empty()) {
            contextStack_.pop_back();
        }
    } catch (...) {
        // Best effort only
    }
}

std::string DebuggerErrorHandler::getCurrentContext() const noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (contextStack_.empty()) {
            return "";
        }

        std::ostringstream oss;
        for (size_t i = 0; i < contextStack_.size(); ++i) {
            if (i > 0) oss << " > ";
            oss << contextStack_[i];
        }
        return oss.str();
    } catch (...) {
        return "";
    }
}

DebuggerErrorHandler::ScopedContext DebuggerErrorHandler::scopedContext(
    const std::string& context) noexcept {
    return ScopedContext(*this, context);
}

bool DebuggerErrorHandler::validateIndex(
    size_t index, size_t maxSize,
    const std::string& context) noexcept {
    try {
        if (index >= maxSize) {
            DebuggerError err;
            err.type = DebuggerErrorType::FrameIndexOutOfRange;
            err.severity = ErrorSeverity::Error;
            std::string rangeText = (maxSize == 0)
                ? "[empty]"
                : ("[0, " + std::to_string(maxSize - 1) + "]");
            err.message = "Index " + std::to_string(index) +
                          " out of range " + rangeText;
            err.context = context.empty() ? getCurrentContext() : context;
            err.isRecoverable = false;

            reportError(err);
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool DebuggerErrorHandler::validateCondition(
    bool condition, DebuggerErrorType errorType,
    const std::string& message,
    const std::string& context) noexcept {
    try {
        if (!condition) {
            reportError(errorType, ErrorSeverity::Error, message, context);
            return false;
        }

        return true;
    } catch (...) {
        return false;
    }
}

DebuggerError DebuggerErrorHandler::normalizeError(
    const DebuggerError& error) const noexcept {
    try {
        DebuggerError normalized = error;

        if (normalized.context.empty()) {
            normalized.context = getCurrentContext();
        }

        if (normalized.message.empty()) {
            normalized.message = "Unknown debugger error";
        }

        if (normalized.timestamp.time_since_epoch().count() == 0) {
            normalized.timestamp = std::chrono::system_clock::now();
        }

        if (normalized.suggestedStrategy == RecoveryStrategy::None) {
            normalized.suggestedStrategy = getRecoveryStrategy(normalized.type);
        }

        if (!normalized.isRecoverable &&
            normalized.severity < ErrorSeverity::Critical &&
            normalized.suggestedStrategy != RecoveryStrategy::None) {
            normalized.isRecoverable = true;
        }

        return normalized;
    } catch (...) {
        DebuggerError fallback;
        fallback.type = error.type;
        fallback.severity = error.severity;
        fallback.isRecoverable = false;
        fallback.suggestedStrategy = RecoveryStrategy::None;
        return fallback;
    }
}

void DebuggerErrorHandler::addToHistory(const DebuggerError& error) noexcept {
    try {
        // Enrich the stored copy with the full context chain so that historical
        // errors carry their call-path even after the context stack is unwound.
        // (Called while mutex_ is held - do not re-lock here.)
        DebuggerError stored = error;
        if (!contextStack_.empty()) {
            std::string chain;
            for (size_t i = 0; i < contextStack_.size(); ++i) {
                if (i > 0) chain += " > ";
                chain += contextStack_[i];
            }
            if (stored.context.empty()) {
                stored.context = chain;
            } else if (stored.context.find(chain) == std::string::npos) {
                stored.context = chain + " > " + stored.context;
            }
        }
        errorHistory_.push_back(std::move(stored));

        // Maintain max history size
        if (errorHistory_.size() > MAX_ERROR_HISTORY) {
            errorHistory_.erase(errorHistory_.begin());
        }
    } catch (...) {
        // Best effort only: never allow history bookkeeping to terminate process.
        try {
            DebuggerError fallback;
            fallback.type = error.type;
            fallback.severity = error.severity;
            fallback.message = error.message.empty() ? "<error message unavailable>" : error.message;
            fallback.timestamp = error.timestamp;
            errorHistory_.push_back(std::move(fallback));
            if (errorHistory_.size() > MAX_ERROR_HISTORY) {
                errorHistory_.erase(errorHistory_.begin());
            }
        } catch (...) {
            // Intentionally swallow; preserving noexcept contract.
        }
    }
}

std::string DebuggerErrorHandler::buildFullErrorMessage(
    const DebuggerError& error) const noexcept {
    // Formats the error for human-readable output, including the full context
    // chain stored on the error.  Does NOT take mutex_ (safe to call while
    // the lock is already held, or from error report paths on snapshots).
    try {
        std::ostringstream oss;
        oss << "[" << DebuggerError::severityToString(error.severity) << "] "
            << DebuggerError::errorTypeToString(error.type) << "\n"
            << "Message: " << error.message << "\n";

        if (!error.context.empty()) {
            oss << "Context Chain: " << error.context << "\n";
        }

        if (!error.details.empty()) {
            oss << "Details: " << error.details << "\n";
        }

        if (!error.functionName.empty()) {
            oss << "Location: " << error.functionName << ":" << error.lineNumber << "\n";
        }

        return oss.str();
    } catch (...) {
        return error.toString();
    }
}

void DebuggerErrorHandler::initializeDefaultStrategies() noexcept {
    try {
        // Frame operation errors - retry strategy
        recoveryStrategies_[DebuggerErrorType::FrameStackWalkFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::FrameLocalsUnavailable] = RecoveryStrategy::UseDefaultValue;
        recoveryStrategies_[DebuggerErrorType::FrameSymbolResolutionFailed] = RecoveryStrategy::Fallback;

        // Adapter errors - retry with reattach
        recoveryStrategies_[DebuggerErrorType::AdapterTimeout] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::AdapterCommunicationFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::AdapterProcessCrashed] = RecoveryStrategy::ReattachAdapter;

        // Execution control - retry
        recoveryStrategies_[DebuggerErrorType::ExecutionControlFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::StepInFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::StepOutFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::ContinueFailed] = RecoveryStrategy::Retry;
        recoveryStrategies_[DebuggerErrorType::PauseFailed] = RecoveryStrategy::Retry;

        // Expression evaluation - use default or skip
        recoveryStrategies_[DebuggerErrorType::EvaluationFailed] = RecoveryStrategy::UseDefaultValue;
        recoveryStrategies_[DebuggerErrorType::ExpressionInvalid] = RecoveryStrategy::SkipOperation;
        recoveryStrategies_[DebuggerErrorType::ExpressionTimeout] = RecoveryStrategy::SkipOperation;

        // Session errors - restart
        recoveryStrategies_[DebuggerErrorType::SessionCorrupted] = RecoveryStrategy::RestartSession;
        recoveryStrategies_[DebuggerErrorType::SessionNotInitialized] = RecoveryStrategy::RestartSession;
    } catch (...) {
        // Best effort only; map may be partially populated.
    }
}

// =============================================================================
// Singleton Implementation
// =============================================================================

static DebuggerErrorHandler g_errorHandler;

DebuggerErrorHandler& DebuggerErrorHandlerInstance::instance() noexcept {
    return g_errorHandler;
}
