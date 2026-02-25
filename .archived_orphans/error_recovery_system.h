// error_recovery_system.h - Enterprise Error Recovery and Auto-Healing
// FULLY SYNCHRONIZED WITH error_recovery_system.cpp
#ifndef ERROR_RECOVERY_SYSTEM_H
#define ERROR_RECOVERY_SYSTEM_H


// Error severity levels
enum class ErrorSeverity {
    Info,
    Warning,
    Error,
    Critical,
    Fatal
};

// Error categories
enum class ErrorCategory {
    System,
    Network,
    FileIO,
    Database,
    AIModel,
    CloudProvider,
    Security,
    Performance,
    UserInput,
    Configuration
};

// Error record - matches cpp usage
struct ErrorRecord {
    std::string errorId;
    std::string component;
    ErrorSeverity severity;
    ErrorCategory category;
    std::string message;
    std::string stackTrace;
    void* context;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point recoveredAt;
    int retryCount = 0;
    bool wasRecovered = false;
    std::string recoveryAction;
};

// Recovery strategy - uses std::vector<ErrorCategory> for applicableCategories
struct RecoveryStrategy {
    std::string strategyId;
    std::string name;
    std::string description;
    std::vector<ErrorCategory> applicableCategories;
    std::vector<std::string> recoverySteps;
    int maxRetries = 3;
    int retryDelayMs = 1000;
    double successRate = 0.5;
    bool isAutomatic = true;
};

// Recovery execution result
struct RecoveryResult {
    std::string errorId;
    std::string strategyId;
    bool success;
    std::string resultMessage;
    std::chrono::system_clock::time_point executedAt;
    int attemptsUsed;
    void* details;
};

// System health status - matches cpp member name currentSystemHealth
struct SystemHealth {
    bool isHealthy = true;
    double healthScore = 100.0;
    int activeErrors = 0;
    int criticalErrors = 0;
    int errorsRecovered = 0;
    int errorsPending = 0;
    std::unordered_map<std::string, int> errorsByComponent;
    std::unordered_map<std::string, int> errorsByCategory;
    std::chrono::system_clock::time_point lastCheckTime;
};

class ErrorRecoverySystem {

public:
    explicit ErrorRecoverySystem(void* parent = nullptr);
    ~ErrorRecoverySystem();

    // Error recording - returns errorId as std::string
    std::string recordError(const std::string& component,
                       ErrorSeverity severity,
                       ErrorCategory category,
                       const std::string& message,
                       const std::string& stackTrace = std::string(),
                       const void*& context = void*());
    
    // Error retrieval
    std::vector<ErrorRecord> getActiveErrors() const;
    std::vector<ErrorRecord> getErrorsByComponent(const std::string& component) const;
    std::vector<ErrorRecord> getErrorsBySeverity(ErrorSeverity severity) const;
    ErrorRecord getError(const std::string& errorId) const;
    
    // Recovery operations
    bool attemptRecovery(const std::string& errorId);
    RecoveryStrategy selectBestStrategy(const ErrorRecord& error);
    
    // Auto-recovery configuration
    void enableAutoRecovery(bool enable);
    void setMaxRetries(int retries);
    void setRetryDelay(int milliseconds);
    bool isAutoRecoveryEnabled() const { return autoRecoveryEnabled; }
    
    // System health
    SystemHealth getSystemHealth() const;
    
    // Error management
    void clearErrorHistory();
    void clearRecoveredErrors();
    void resolveError(const std::string& errorId);
    
    // Statistics
    void* getErrorStatistics() const;
    
    // Utility - member functions (const)
    std::string errorSeverityToString(ErrorSeverity severity) const;
    std::string errorCategoryToString(ErrorCategory category) const;


    void errorRecorded(const ErrorRecord& error);
    void errorRecovered(const std::string& errorId, bool success);
    void errorRecoveredRecord(const ErrorRecord& error);
    void recoveryFailed(const ErrorRecord& error);
    void systemHealthUpdated(const SystemHealth& health);
    
    // Recovery action signals
    void fallbackToLocalRequested(const std::string& component);
    void cacheClearRequested(const std::string& component);
    void componentRestartRequested(const std::string& component);
    void networkReconnectRequested();
    void dataReloadRequested(const std::string& component);
    void resourceReductionRequested();
    void endpointSwitchRequested(const std::string& component);
    void gracefulDegradationEnabled();
    void reauthenticationRequested(const std::string& component);
    void adminEscalationRequired(const ErrorRecord& error);

private:
    void processAutoRecovery();
    void updateSystemHealth();

private:
    // Setup
    void setupDefaultStrategies();
    
    // Recovery execution
    bool executeRecoveryStrategy(ErrorRecord& error, const RecoveryStrategy& strategy);
    
    // Built-in recovery methods
    bool recoverWithRetry(ErrorRecord& error);
    bool recoverFallbackLocal(ErrorRecord& error);
    bool recoverClearCache(ErrorRecord& error);
    bool recoverRestartComponent(ErrorRecord& error);
    bool recoverReconnectNetwork(ErrorRecord& error);
    bool recoverReloadData(ErrorRecord& error);
    bool recoverReduceResources(ErrorRecord& error);
    bool recoverSwitchEndpoint(ErrorRecord& error);
    bool recoverGracefulDegradation(ErrorRecord& error);
    bool recoverReauthenticate(ErrorRecord& error);
    bool recoverEscalateAdmin(ErrorRecord& error);
    
    // ID generation
    std::string generateErrorId();
    
    // Data members - exact names from cpp
    std::unordered_map<std::string, ErrorRecord> activeErrors;
    std::vector<ErrorRecord> recoveredErrors;
    std::vector<ErrorRecord> errorHistory;
    std::unordered_map<std::string, RecoveryStrategy> strategies;
    
    SystemHealth currentSystemHealth;
    
    void** autoRecoveryTimer;
    void** healthCheckTimer;
    
    bool autoRecoveryEnabled;
    int maxRetries;
    int retryDelayMs;
    int healthCheckIntervalMs;
};

#endif // ERROR_RECOVERY_SYSTEM_H

