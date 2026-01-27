#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>

/**
 * @brief Sentry crash reporting integration for production error tracking
 * 
 * PRODUCTION-READY FEATURES:
 * - Centralized error capture with stack traces
 * - Performance monitoring (latency tracking)
 * - Breadcrumb logging for debugging context
 * - Environment-based configuration (DSN from env var)
 * - Privacy-respecting (no PII collection)
 */
class SentryIntegration : public QObject
{
    Q_OBJECT

public:
    static SentryIntegration* instance();
    
    /**
     * @brief Initialize Sentry with DSN from environment variable SENTRY_DSN
     * @return true if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Capture an exception with context
     * @param exception Exception message
     * @param context Additional context (file, function, line)
     */
    void captureException(const QString& exception, const QJsonObject& context = QJsonObject());
    
    /**
     * @brief Capture a message (non-error event)
     * @param message Message to log
     * @param level Severity level (info, warning, error)
     */
    void captureMessage(const QString& message, const QString& level = "info");
    
    /**
     * @brief Add breadcrumb for debugging trail
     * @param message Breadcrumb message
     * @param category Event category (navigation, http, etc.)
     */
    void addBreadcrumb(const QString& message, const QString& category = "default");
    
    /**
     * @brief Start performance transaction
     * @param operation Operation name (e.g., "model.load", "inference.generate")
     * @return Transaction ID
     */
    QString startTransaction(const QString& operation);
    
    /**
     * @brief Finish performance transaction
     * @param transactionId Transaction ID from startTransaction
     */
    void finishTransaction(const QString& transactionId);
    
    /**
     * @brief Set user context (only non-PII data like user ID hash)
     * @param userId Anonymized user identifier
     */
    void setUser(const QString& userId);

signals:
    void errorReported(const QString& errorId);
    void transactionCompleted(const QString& transactionId, qint64 durationMs);

private:
    explicit SentryIntegration(QObject* parent = nullptr);
    ~SentryIntegration();
    
    static SentryIntegration* s_instance;
    
    bool m_initialized;
    QString m_dsn;
    QHash<QString, qint64> m_activeTransactions;  ///< Transaction ID -> start timestamp
    QList<QJsonObject> m_breadcrumbs;  ///< Breadcrumb trail for context
    
    // PRODUCTION-READY: Send event to Sentry HTTP API
    void sendEvent(const QJsonObject& event);
};
