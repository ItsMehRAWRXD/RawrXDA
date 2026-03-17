#pragma once
#include "ProdIntegration.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMutex>
#include <QQueue>
#include <QPointer>
#include <exception>
#include <vector>

namespace RawrXD {
namespace Integration {
namespace Errors {

// Error severity levels
enum class Severity {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

// Structured error entry
struct ErrorEntry {
    QString component;
    QString operation;
    QString message;
    QString details;
    Severity severity = Severity::Error;
    QDateTime timestamp;
    QString stackTrace;
    int occurrenceCount = 1;
};

// Error aggregator for collecting and reporting errors
class ErrorAggregator {
public:
    static ErrorAggregator& instance() {
        static ErrorAggregator aggregator;
        return aggregator;
    }

    void recordError(const QString& component, const QString& operation, const QString& message,
                    Severity severity = Severity::Error, const QString& details = QString()) {
        QMutexLocker lock(&m_mutex);
        
        const QString key = component + QStringLiteral("::") + operation;
        const auto now = QDateTime::currentDateTime();
        
        // Check if similar error exists
        auto it = m_errors.find(key);
        if (it != m_errors.end()) {
            // Update existing error
            it->occurrenceCount++;
            it->timestamp = now;
            if (!details.isEmpty()) {
                it->details = details;
            }
        } else {
            // Create new error entry
            ErrorEntry entry;
            entry.component = component;
            entry.operation = operation;
            entry.message = message;
            entry.details = details;
            entry.severity = severity;
            entry.timestamp = now;
            entry.stackTrace = captureStackTrace();
            m_errors[key] = entry;
        }

        // Log the error
        logError(component, operation, message, details);
        
        // Update metrics
        if (Config::metricsEnabled()) {
            recordMetric("error_recorded", 1);
            if (severity == Severity::Critical) {
                recordMetric("critical_error", 1);
            }
        }

        // Emit signal if registered
        if (!m_errorHandler.isNull()) {
            QMetaObject::invokeMethod(m_errorHandler, "onErrorRecorded", Qt::QueuedConnection,
                                    Q_ARG(QString, component), Q_ARG(QString, operation), 
                                    Q_ARG(QString, message));
        }
    }

    void recordException(const QString& component, const QString& operation, const std::exception& ex) {
        recordError(component, operation, QString::fromUtf8(ex.what()), Severity::Error);
    }

    void recordException(const QString& component, const QString& operation) {
        recordError(component, operation, QStringLiteral("Unknown exception"), Severity::Error);
    }

    QJsonArray getRecentErrors(int maxCount = 100) const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray result;
        int count = 0;
        
        for (auto it = m_errors.begin(); it != m_errors.end() && count < maxCount; ++it, ++count) {
            QJsonObject obj;
            obj.insert(QStringLiteral("component"), it->component);
            obj.insert(QStringLiteral("operation"), it->operation);
            obj.insert(QStringLiteral("message"), it->message);
            obj.insert(QStringLiteral("details"), it->details);
            obj.insert(QStringLiteral("severity"), severityToString(it->severity));
            obj.insert(QStringLiteral("timestamp"), it->timestamp.toString(Qt::ISODate));
            obj.insert(QStringLiteral("stack_trace"), it->stackTrace);
            obj.insert(QStringLiteral("count"), it->occurrenceCount);
            result.append(obj);
        }
        
        return result;
    }

    QJsonObject getErrorSummary() const {
        QMutexLocker lock(&m_mutex);
        
        QJsonObject summary;
        summary.insert(QStringLiteral("total_errors"), static_cast<int>(m_errors.size()));
        summary.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
        
        // Count by severity
        QJsonObject bySeverity;
        for (int i = 0; i < static_cast<int>(Severity::Critical) + 1; ++i) {
            bySeverity.insert(severityToString(static_cast<Severity>(i)), 0);
        }
        
        for (const auto& error : m_errors) {
            const QString sev = severityToString(error.severity);
            bySeverity[sev] = bySeverity[sev].toInt() + 1;
        }
        
        summary.insert(QStringLiteral("by_severity"), bySeverity);
        
        return summary;
    }

    void clear() {
        QMutexLocker lock(&m_mutex);
        m_errors.clear();
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("ErrorAggregator"), QStringLiteral("cleared"), 
                    QStringLiteral("All errors cleared"));
        }
    }

    void setErrorHandler(QObject* handler) {
        m_errorHandler = handler;
    }

private:
    ErrorAggregator() = default;
    ~ErrorAggregator() = default;

    QString severityToString(Severity severity) const {
        switch (severity) {
            case Severity::Debug: return QStringLiteral("DEBUG");
            case Severity::Info: return QStringLiteral("INFO");
            case Severity::Warning: return QStringLiteral("WARNING");
            case Severity::Error: return QStringLiteral("ERROR");
            case Severity::Critical: return QStringLiteral("CRITICAL");
            default: return QStringLiteral("UNKNOWN");
        }
    }

    QString captureStackTrace() const {
        // Simple stack capture - in production, integrate with debug symbols
        // For now, just return a placeholder
        return QStringLiteral("Stack trace not available");
    }

    void logError(const QString& component, const QString& operation, 
                  const QString& message, const QString& details) {
        const QString fullMessage = details.isEmpty() ? message 
            : QStringLiteral("%1: %2").arg(message, details);
        
        logWithLevel(QStringLiteral("ERROR"), component, operation, fullMessage);
    }

    mutable QMutex m_mutex;
    QMap<QString, ErrorEntry> m_errors;
    QPointer<QObject> m_errorHandler;
};

// Global convenience functions
inline void recordError(const QString& component, const QString& operation, 
                       const QString& message, Severity severity = Severity::Error,
                       const QString& details = QString()) {
    ErrorAggregator::instance().recordError(component, operation, message, severity, details);
}

inline void recordException(const QString& component, const QString& operation, 
                           const std::exception& ex) {
    ErrorAggregator::instance().recordException(component, operation, ex);
}

inline void recordException(const QString& component, const QString& operation) {
    ErrorAggregator::instance().recordException(component, operation);
}

inline QJsonArray getRecentErrors(int maxCount = 100) {
    return ErrorAggregator::instance().getRecentErrors(maxCount);
}

inline QJsonObject getErrorSummary() {
    return ErrorAggregator::instance().getErrorSummary();
}

inline void clearErrors() {
    ErrorAggregator::instance().clear();
}

// RAII error guard - automatically records errors on destruction if not dismissed
class ErrorGuard {
public:
    ErrorGuard(const QString& component, const QString& operation)
        : m_component(component), m_operation(operation), m_active(true) {}

    ~ErrorGuard() {
        if (m_active && m_error) {
            recordError(m_component, m_operation, m_error.value());
        }
    }

    void dismiss() { m_active = false; }
    void setError(const QString& error) { m_error = error; }
    void setException(const std::exception& ex) { m_error = QString::fromUtf8(ex.what()); }

    // Non-copyable
    ErrorGuard(const ErrorGuard&) = delete;
    ErrorGuard& operator=(const ErrorGuard&) = delete;

private:
    QString m_component;
    QString m_operation;
    bool m_active;
    QOptional<QString> m_error;
};

} // namespace Errors
} // namespace Integration
} // namespace RawrXD
