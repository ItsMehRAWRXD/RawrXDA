// error_recovery_system.h - Enterprise Error Recovery and Auto-Healing
// FULLY SYNCHRONIZED WITH error_recovery_system.cpp
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

// Error record - matches cpp usage
struct ErrorRecord {
    QString errorId;
    QString component;
    ErrorSeverity severity;
    ErrorCategory category;
    QString message;
    QString stackTrace;
    QJsonObject context;
    QDateTime timestamp;
    QDateTime recoveredAt;
    int retryCount = 0;
    bool wasRecovered = false;
    QString recoveryAction;
};

// Recovery strategy - uses QVector<ErrorCategory> for applicableCategories
struct RecoveryStrategy {
    QString strategyId;
    QString name;
    QString description;
    QVector<ErrorCategory> applicableCategories;
    QVector<QString> recoverySteps;
    int maxRetries = 3;
    int retryDelayMs = 1000;
    double successRate = 0.5;
    bool isAutomatic = true;
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

// System health status - matches cpp member name currentSystemHealth
struct SystemHealth {
    bool isHealthy = true;
    double healthScore = 100.0;
    int activeErrors = 0;
    int criticalErrors = 0;
    int errorsRecovered = 0;
    int errorsPending = 0;
    QHash<QString, int> errorsByComponent;
    QHash<QString, int> errorsByCategory;
    QDateTime lastCheckTime;
};

class ErrorRecoverySystem : public QObject {
    Q_OBJECT

public:
    explicit ErrorRecoverySystem(QObject* parent = nullptr);
    ~ErrorRecoverySystem();

    // Error recording - returns errorId as QString
    QString recordError(const QString& component,
                       ErrorSeverity severity,
                       ErrorCategory category,
                       const QString& message,
                       const QString& stackTrace = QString(),
                       const QJsonObject& context = QJsonObject());
    
    // Error retrieval
    QVector<ErrorRecord> getActiveErrors() const;
    QVector<ErrorRecord> getErrorsByComponent(const QString& component) const;
    QVector<ErrorRecord> getErrorsBySeverity(ErrorSeverity severity) const;
    ErrorRecord getError(const QString& errorId) const;
    
    // Recovery operations
    bool attemptRecovery(const QString& errorId);
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
    void resolveError(const QString& errorId);
    
    // Statistics
    QJsonObject getErrorStatistics() const;
    
    // Utility - member functions (const)
    QString errorSeverityToString(ErrorSeverity severity) const;
    QString errorCategoryToString(ErrorCategory category) const;

signals:
    void errorRecorded(const ErrorRecord& error);
    void errorRecovered(const QString& errorId, bool success);
    void errorRecoveredRecord(const ErrorRecord& error);
    void recoveryFailed(const ErrorRecord& error);
    void systemHealthUpdated(const SystemHealth& health);
    
    // Recovery action signals
    void fallbackToLocalRequested(const QString& component);
    void cacheClearRequested(const QString& component);
    void componentRestartRequested(const QString& component);
    void networkReconnectRequested();
    void dataReloadRequested(const QString& component);
    void resourceReductionRequested();
    void endpointSwitchRequested(const QString& component);
    void gracefulDegradationEnabled();
    void reauthenticationRequested(const QString& component);
    void adminEscalationRequired(const ErrorRecord& error);

private slots:
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
    QString generateErrorId();
    
    // Data members - exact names from cpp
    QHash<QString, ErrorRecord> activeErrors;
    QVector<ErrorRecord> recoveredErrors;
    QVector<ErrorRecord> errorHistory;
    QHash<QString, RecoveryStrategy> strategies;
    
    SystemHealth currentSystemHealth;
    
    QTimer* autoRecoveryTimer;
    QTimer* healthCheckTimer;
    
    bool autoRecoveryEnabled;
    int maxRetries;
    int retryDelayMs;
    int healthCheckIntervalMs;
};

#endif // ERROR_RECOVERY_SYSTEM_H
