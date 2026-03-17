#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <functional>
#include <exception>

namespace RawrXD {

/**
 * @brief Centralized Exception Handler
 * 
 * Provides global exception handling without modifying core application logic.
 * Captures unhandled exceptions, logs them, and optionally invokes recovery callbacks.
 */
class CentralizedExceptionHandler : public QObject {
    Q_OBJECT
    
public:
    static CentralizedExceptionHandler& instance();
    
    // Exception handling
    void installHandler();
    void uninstallHandler();
    
    // Recovery callbacks
    using RecoveryCallback = std::function<bool(const QString& error, const QJsonObject& context)>;
    void registerRecoveryCallback(const QString& errorType, RecoveryCallback callback);
    void unregisterRecoveryCallback(const QString& errorType);
    
    // Exception reporting
    void reportException(const std::exception& ex, const QJsonObject& context = QJsonObject());
    void reportError(const QString& error, const QString& category, const QJsonObject& context = QJsonObject());
    
    // Statistics
    qint64 getTotalExceptions() const { return m_totalExceptions; }
    qint64 getRecoveredExceptions() const { return m_recoveredExceptions; }
    QJsonObject getExceptionStatistics() const;
    
    // Configuration
    void setMaxCapturedExceptions(int max) { m_maxCapturedExceptions = max; }
    void enableAutomaticRecovery(bool enable) { m_automaticRecoveryEnabled = enable; }
    void setLogAllExceptions(bool log) { m_logAllExceptions = log; }
    
signals:
    void exceptionCaptured(const QString& error, const QString& category);
    void recoveryAttempted(const QString& errorType, bool success);
    void criticalError(const QString& error);
    
private:
    explicit CentralizedExceptionHandler(QObject* parent = nullptr);
    ~CentralizedExceptionHandler();
    CentralizedExceptionHandler(const CentralizedExceptionHandler&) = delete;
    CentralizedExceptionHandler& operator=(const CentralizedExceptionHandler&) = delete;
    
    bool attemptRecovery(const QString& errorType, const QString& error, const QJsonObject& context);
    void logException(const QString& error, const QString& category, const QJsonObject& context);
    void captureException(const QString& error, const QString& category, const QJsonObject& context);
    
    static void terminateHandler();
    static void unexpectedHandler();
    
private:
    QMap<QString, RecoveryCallback> m_recoveryCallbacks;
    QVector<QJsonObject> m_capturedExceptions;
    qint64 m_totalExceptions;
    qint64 m_recoveredExceptions;
    int m_maxCapturedExceptions;
    bool m_automaticRecoveryEnabled;
    bool m_logAllExceptions;
    bool m_handlerInstalled;
    
    std::terminate_handler m_previousTerminateHandler;
};

/**
 * @brief RAII Exception Scope Guard
 * 
 * Captures exceptions in a specific scope and reports them with context.
 */
class ExceptionScopeGuard {
public:
    ExceptionScopeGuard(const QString& scopeName, const QJsonObject& context = QJsonObject());
    ~ExceptionScopeGuard();
    
    void addContext(const QString& key, const QJsonValue& value);
    
private:
    QString m_scopeName;
    QJsonObject m_context;
    bool m_exceptionOccurred;
};

} // namespace RawrXD
