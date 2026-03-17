// error_recovery_system.h - Enterprise Error Recovery and Auto-Healing
#ifndef ERROR_RECOVERY_SYSTEM_H
#define ERROR_RECOVERY_SYSTEM_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>

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

// Error record
struct ErrorRecord {
    QString errorId;
    QString component;
    ErrorSeverity severity;
    ErrorCategory category;
    QString message;
    QString stackTrace;
    QJsonObject context;
    QDateTime timestamp;
    int retryCount;
    bool wasRecovered;
    QString recoveryAction;
};

// Recovery strategy
struct RecoveryStrategy {
    QString strategyId;
    QString name;
    QString description;
    ErrorCategory applicableCategory;
    QVector<QString> recoverySteps;
    int maxRetries;
    int retryDelayMs;
    double successRate;
    bool isAutomatic;
};

// Recovery execution result
struct RecoveryResult {
    QString errorId;
    QString strategyId;
    bool success;
    QString resultMessage;
    QDateTime executedAt;
    int attemptsUsed;
    QJsonObject details;
};

// System health status
struct SystemHealth {
    bool isHealthy;
    double healthScore;        // 0-100
    int activeErrors;
    int criticalErrors;
    int errorsRecovered;
    int errorsPending;
    QHash<QString, int> errorsByComponent;
    QHash<QString, int> errorsByCategory;
    QDateTime lastCheckTime;
};

class ErrorRecoverySystem : public QObject {
    Q_OBJECT

public:
    explicit ErrorRecoverySystem(QObject* parent = nullptr);
    ~ErrorRecoverySystem();

    // Error recording
    void recordError(const QString& component,
                    ErrorSeverity severity,
                    ErrorCategory category,
                    const QString& message,
                    const QJsonObject& context = QJsonObject());
    void recordException(const QString& component,
                        const std::exception& exception,
                        const QJsonObject& context = QJsonObject());
    
    // Error retrieval
    QVector<ErrorRecord> getActiveErrors() const;
    QVector<ErrorRecord> getErrorsByComponent(const QString& component) const;
    QVector<ErrorRecord> getErrorsByCategory(ErrorCategory category) const;
    QVector<ErrorRecord> getCriticalErrors() const;
    ErrorRecord getError(const QString& errorId) const;
    
    // Recovery operations
    bool attemptRecovery(const QString& errorId);
    bool recoverAllErrors();
    RecoveryResult executeRecovery(const QString& errorId,
                                   const QString& strategyId);
    
    // Recovery strategies
    void registerRecoveryStrategy(const RecoveryStrategy& strategy);
    void removeRecoveryStrategy(const QString& strategyId);
    QVector<RecoveryStrategy> getRecoveryStrategies() const;
    QVector<RecoveryStrategy> getStrategiesForCategory(ErrorCategory category) const;
    RecoveryStrategy selectBestStrategy(const ErrorRecord& error) const;
    
    // Auto-recovery
    void enableAutoRecovery(bool enable);
    void setAutoRecoveryDelay(int delayMs);
    void setMaxAutoRetries(int maxRetries);
    bool isAutoRecoveryEnabled() const;
    
    // System health
    SystemHealth getSystemHealth() const;
    double calculateHealthScore() const;
    bool isSystemHealthy() const;
    void performHealthCheck();
    
    // Error patterns
    void detectErrorPatterns();
    QVector<QString> getCommonErrorPatterns() const;
    bool isRecurringError(const QString& errorId) const;
    int getErrorFrequency(const QString& component) const;
    
    // Error history
    void clearErrorHistory();
    void archiveOldErrors(int daysOld = 30);
    QVector<ErrorRecord> getErrorHistory(int lastNDays = 7) const;
    int getTotalErrorCount() const;
    int getRecoverySuccessRate() const;
    
    // Notifications
    void enableErrorNotifications(bool enable);
    void setNotificationThreshold(ErrorSeverity threshold);
    
    // Logging
    void enableLogging(bool enable);
    void setLogFilePath(const QString& path);
    void exportErrorReport(const QString& filePath);

signals:
    void errorRecorded(const ErrorRecord& error);
    void errorRecovered(const QString& errorId, bool success);
    void criticalErrorDetected(const ErrorRecord& error);
    void systemHealthChanged(double healthScore);
    void autoRecoveryAttempted(const QString& errorId, bool success);
    void recoveryStrategyExecuted(const RecoveryResult& result);

private slots:
    void onAutoRecoveryTimerTimeout();
    void onHealthCheckTimerTimeout();

private:
    // Recovery execution
    bool executeRecoverySequence(const QString& errorId,
                                const RecoveryStrategy& strategy);
    bool executeRecoveryStep(const QString& step,
                            const ErrorRecord& error);
    void logRecoveryAttempt(const QString& errorId,
                           const QString& strategyId,
                           bool success);
    
    // Built-in recovery strategies
    void initializeDefaultStrategies();
    bool recoverNetworkError(const ErrorRecord& error);
    bool recoverFileIOError(const ErrorRecord& error);
    bool recoverDatabaseError(const ErrorRecord& error);
    bool recoverAIModelError(const ErrorRecord& error);
    bool recoverCloudProviderError(const ErrorRecord& error);
    
    // Error analysis
    QString generateErrorId(const QString& component, const QString& message);
    ErrorSeverity categorizeSeverity(const QString& message);
    bool shouldAutoRecover(const ErrorRecord& error) const;
    
    // Health monitoring
    void updateSystemHealth();
    void checkComponentHealth(const QString& component);
    bool detectAnomalousErrorRate();
    
    // Pattern detection
    void analyzeErrorSequence();
    bool detectErrorCascade();
    QVector<QString> findSimilarErrors(const ErrorRecord& error) const;
    
    // Data members
    QHash<QString, ErrorRecord> activeErrors;
    QVector<ErrorRecord> errorHistory;
    QHash<QString, RecoveryStrategy> recoveryStrategies;
    QVector<RecoveryResult> recoveryHistory;
    
    SystemHealth systemHealth;
    
    QTimer* autoRecoveryTimer;
    QTimer* healthCheckTimer;
    
    bool autoRecoveryEnabled;
    int autoRecoveryDelayMs;
    int maxAutoRetries;
    bool errorNotificationsEnabled;
    ErrorSeverity notificationThreshold;
    bool loggingEnabled;
    QString logFilePath;
    
    // Statistics
    int totalErrors;
    int totalRecoveries;
    int successfulRecoveries;
    
    // Configuration
    static constexpr int DEFAULT_AUTO_RECOVERY_DELAY = 5000; // 5 seconds
    static constexpr int DEFAULT_MAX_RETRIES = 3;
    static constexpr int HEALTH_CHECK_INTERVAL = 30000; // 30 seconds
};

// Utility functions for error severity conversion
QString errorSeverityToString(ErrorSeverity severity);
ErrorSeverity stringToErrorSeverity(const QString& str);
QString errorCategoryToString(ErrorCategory category);
ErrorCategory stringToErrorCategory(const QString& str);

#endif // ERROR_RECOVERY_SYSTEM_H
