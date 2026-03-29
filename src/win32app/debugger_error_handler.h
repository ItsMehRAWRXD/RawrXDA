#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>
#include <map>
#include <optional>
#include <thread>
#include <mutex>

// =============================================================================
// Debugger Error Types & Enums
// =============================================================================

/**
 * @brief Error categories for debugger operations
 */
enum class DebuggerErrorType {
    Success = 0,
    
    // Connection errors
    AdapterConnectionFailed = 100,
    AdapterProcessCrashed = 101,
    AdapterCommunicationFailed = 102,
    AdapterTimeout = 103,
    
    // Frame operation errors
    FrameStackWalkFailed = 200,
    FrameIndexOutOfRange = 201,
    FrameDataCorrupted = 202,
    FrameLocalsUnavailable = 203,
    FrameSymbolResolutionFailed = 204,
    FrameNavigationLimitReached = 205,
    FrameNavigationFailed = 206,
    
    // Breakpoint errors
    BreakpointSetFailed = 300,
    BreakpointLocationInvalid = 301,
    BreakpointConditionInvalid = 302,
    
    // Execution control errors
    ExecutionControlFailed = 400,
    StepInFailed = 401,
    StepOutFailed = 402,
    ContinueFailed = 403,
    PauseFailed = 404,
    
    // Expression evaluation errors
    EvaluationFailed = 500,
    ExpressionInvalid = 501,
    ExpressionContextLost = 502,
    ExpressionTimeout = 503,
    
    // Session errors
    SessionNotInitialized = 600,
    SessionCorrupted = 601,
    SessionOperationNotSupported = 602,
    
    // Generic/Unknown errors
    UnknownError = 999
};

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    Info = 0,
    Warning = 1,
    Error = 2,
    Critical = 3
};

/**
 * @brief Recovery strategy for errors
 */
enum class RecoveryStrategy {
    None = 0,
    Retry = 1,
    UseDefaultValue = 2,
    SkipOperation = 3,
    ReattachAdapter = 4,
    RestartSession = 5,
    Fallback = 6
};

// =============================================================================
// Error Information Structures
// =============================================================================

/**
 * @brief Detailed error information
 */
struct DebuggerError {
    DebuggerErrorType type = DebuggerErrorType::UnknownError;
    ErrorSeverity severity = ErrorSeverity::Error;
    std::string message;  // Human-readable error message
    std::string details;  // Extended error details
    std::string context;  // Context about where error occurred
    
    // Error tracking
    std::string functionName;  // Function where error occurred
    int lineNumber = 0;        // Line where error occurred
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
    
    // Recovery information
    RecoveryStrategy suggestedStrategy = RecoveryStrategy::None;
    bool isRecoverable = false;
    int retryCount = 0;
    int maxRetries = 3;
    
    // Diagnostic information
    std::string stackTrace;
    std::map<std::string, std::string> diagnosticData;
    
    /**
     * @brief Convert error to string representation
     */
    std::string toString() const noexcept;
    
    /**
     * @brief Get severity as string
     */
    static std::string severityToString(ErrorSeverity sev) noexcept;
    
    /**
     * @brief Get error type as string
     */
    static std::string errorTypeToString(DebuggerErrorType type) noexcept;
};

/**
 * @brief Result wrapper combining return value with error info
 */
template<typename T>
struct DebuggerResult {
    T value;                                    // Result value (default constructed if failed)
    bool success = false;                       // Operation success
    std::unique_ptr<DebuggerError> error;       // Error details if failed
    
    DebuggerResult() : success(false) {}
    
    DebuggerResult(const T& val) : value(val), success(true) {}
    
    DebuggerResult(T&& val) : value(std::move(val)), success(true) {}
    
    DebuggerResult(const DebuggerError& err) noexcept : success(false) {
        try {
            error = std::make_unique<DebuggerError>(err);
        } catch (...) {
            error.reset();
        }
    }
    
    explicit operator bool() const noexcept { return success; }
};

// Specialization for void results
template<>
struct DebuggerResult<void> {
    bool success = false;
    std::unique_ptr<DebuggerError> error;
    
    DebuggerResult() : success(false) {}
    DebuggerResult(bool ok) : success(ok) {}
    DebuggerResult(const DebuggerError& err) noexcept : success(false) {
        try {
            error = std::make_unique<DebuggerError>(err);
        } catch (...) {
            error.reset();
        }
    }
    
    explicit operator bool() const noexcept { return success; }
};

// =============================================================================
// Error Handler System
// =============================================================================

class DebuggerErrorHandler {
public:
    /**
     * @brief Error handler callback type
     * Called when errors occur: (error, can_recover) -> void
     */
    using ErrorCallback = std::function<void(const DebuggerError&, bool)>;
    
    /**
     * @brief Recovery handler callback type
     * Called to attempt recovery: (error) -> bool
     */
    using RecoveryCallback = std::function<bool(DebuggerError&)>;

    class ScopedContext {
    public:
        ScopedContext(DebuggerErrorHandler& handler, const std::string& context) noexcept;
        ~ScopedContext();

        ScopedContext(const ScopedContext&) = delete;
        ScopedContext& operator=(const ScopedContext&) = delete;

        ScopedContext(ScopedContext&& other) noexcept;
        ScopedContext& operator=(ScopedContext&& other) noexcept;

    private:
        DebuggerErrorHandler* handler_ = nullptr;
        bool active_ = false;
    };
    
    DebuggerErrorHandler();
    ~DebuggerErrorHandler() = default;
    
    // =========================================================================
    // Error Registration & Handling
    // =========================================================================
    
    /**
     * @brief Report an error
     * @param error Error information
     * @return true if error was handled/recovered
     */
    bool reportError(const DebuggerError& error) noexcept;
    
    /**
     * @brief Report error with parameters
     */
    bool reportError(
        DebuggerErrorType type,
        ErrorSeverity severity,
        const std::string& message,
        const std::string& context = "") noexcept;
    
    /**
     * @brief Register error callback
     * Called when errors occur
     */
    void setErrorCallback(ErrorCallback callback) noexcept;
    
    /**
     * @brief Register recovery callback
     * Called when attempting recovery
     */
    void setRecoveryCallback(RecoveryCallback callback) noexcept;
    
    // =========================================================================
    // Recovery Operations
    // =========================================================================
    
    /**
     * @brief Attempt to recover from error
     * @param error Error to recover from
     * @return true if recovery succeeded
     */
    bool attemptRecovery(DebuggerError& error) noexcept;
    
    /**
     * @brief Get recovery strategy for error type
     */
    RecoveryStrategy getRecoveryStrategy(DebuggerErrorType type) const noexcept;
    
    /**
     * @brief Set recovery strategy for error type
     */
    void setRecoveryStrategy(DebuggerErrorType type, RecoveryStrategy strategy) noexcept;
    
    /**
     * @brief Retry an operation with exponential backoff
     * @param func Function to retry
     * @param maxRetries Maximum retry attempts
     * @return Result of operation or last error
     */
    template<typename Func>
    auto retryWithBackoff(const Func& func, int maxRetries = 3) noexcept
        -> decltype(func()) {
        
        for (int retry = 0; retry < maxRetries; ++retry) {
            try {
                auto result = func();
                if (result.success) {
                    return result;
                }
                
                // Exponential backoff: 100ms, 200ms, 400ms
                int delayMs = 100 * (1 << retry);
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                
            } catch (...) {
                // Continue to next retry
            }
        }

        try {
            DebuggerError err;
            err.type = DebuggerErrorType::UnknownError;
            err.message = "Operation failed after retries";
            err.isRecoverable = false;
            return {err};
        } catch (...) {
            return {};
        }
    }
    
    // =========================================================================
    // Error History & Statistics
    // =========================================================================
    
    /**
     * @brief Get recent errors
     * @param maxCount Maximum number of errors to return
     */
    std::vector<DebuggerError> getRecentErrors(size_t maxCount = 10) const noexcept;
    
    /**
     * @brief Get error statistics
     */
    struct ErrorStats {
        size_t totalErrors = 0;
        size_t recoveredErrors = 0;
        size_t unrecoveredErrors = 0;
        std::map<DebuggerErrorType, int> errorCounts;
        std::chrono::milliseconds avgRecoveryTime = std::chrono::milliseconds(0);
    };
    ErrorStats getStatistics() const noexcept;
    
    /**
     * @brief Clear error history
     */
    void clear() noexcept;
    
    /**
     * @brief Get error report
     */
    std::string getErrorReport() const noexcept;
    
    // =========================================================================
    // Error Context Management
    // =========================================================================
    
    /**
     * @brief Push error context (for nested operations)
     */
    void pushContext(const std::string& context) noexcept;
    
    /**
     * @brief Pop error context
     */
    void popContext() noexcept;
    
    /**
     * @brief Get current error context
     */
    std::string getCurrentContext() const noexcept;

    /**
     * @brief Create a scoped context that is automatically popped
     */
    [[nodiscard]] ScopedContext scopedContext(const std::string& context) noexcept;
    
    // =========================================================================
    // Validation & Assertion Helpers
    // =========================================================================
    
    /**
     * @brief Validate pointer and report error if null
     */
    template<typename T>
    bool validatePointer(T* ptr, const std::string& ptrName,
                         const std::string& context = "") noexcept {
        try {
            if (!ptr) {
                DebuggerError err;
                err.type = DebuggerErrorType::UnknownError;
                err.severity = ErrorSeverity::Error;
                err.message = "Null pointer: " + ptrName;
                err.context = context;
                err.isRecoverable = false;
                reportError(err);
                return false;
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    
    /**
     * @brief Validate index and report error if out of range
     */
    bool validateIndex(size_t index, size_t maxSize,
                       const std::string& context = "") noexcept;
    
    /**
     * @brief Validate condition and report error if false
     */
    bool validateCondition(bool condition, DebuggerErrorType errorType,
                           const std::string& message,
                           const std::string& context = "") noexcept;

private:
    std::vector<DebuggerError> errorHistory_;
    static constexpr size_t MAX_ERROR_HISTORY = 100;
    mutable std::mutex mutex_;
    
    std::map<DebuggerErrorType, RecoveryStrategy> recoveryStrategies_;
    ErrorCallback errorCallback_;
    RecoveryCallback recoveryCallback_;
    
    std::vector<std::string> contextStack_;
    
    // Statistics
    size_t totalErrors_ = 0;
    size_t recoveredErrors_ = 0;
    uint64_t totalRecoveryTimeMs_ = 0;
    
    DebuggerError normalizeError(const DebuggerError& error) const noexcept;
    void addToHistory(const DebuggerError& error) noexcept;
    std::string buildFullErrorMessage(const DebuggerError& error) const noexcept;
    void initializeDefaultStrategies() noexcept;
};

// =============================================================================
// Singleton Access
// =============================================================================

class DebuggerErrorHandlerInstance {
public:
    static DebuggerErrorHandler& instance() noexcept;
    
private:
    DebuggerErrorHandlerInstance() = delete;
};

// =============================================================================
// Helper Macros for Error Handling
// =============================================================================

#define DEBUGGER_REPORT_ERROR(type, severity, message, context) \
    DebuggerErrorHandlerInstance::instance().reportError((type), (severity), (message), (context))

#define DEBUGGER_VALIDATE_INDEX(index, maxSize, context) \
    DebuggerErrorHandlerInstance::instance().validateIndex((index), (maxSize), (context))

#define DEBUGGER_CHECK_CONDITION(cond, type, message, context) \
    DebuggerErrorHandlerInstance::instance().validateCondition((cond), (type), (message), (context))
