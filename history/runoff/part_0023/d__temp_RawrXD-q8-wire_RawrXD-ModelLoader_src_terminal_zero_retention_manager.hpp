#pragma once

#include <QString>
#include <QJsonObject>
#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QVector>
#include <QPair>
#include <QDateTime>
#include <chrono>
#include <memory>

/**
 * @class ZeroRetentionManager
 * @brief Production-ready GDPR/privacy-compliant data retention manager
 * 
 * Features:
 * - Automatic data deletion based on retention policies
 * - Session cleanup with configurable TTL
 * - Comprehensive audit logging for compliance
 * - Privacy-first data handling with secure deletion
 * - Structured logging with metrics
 * - Configuration-driven retention policies
 * - Support for data anonymization
 */
class ZeroRetentionManager : public QObject {
    Q_OBJECT

public:
    explicit ZeroRetentionManager(QObject* parent = nullptr);
    ~ZeroRetentionManager() override;

    // Configuration
    struct Config {
        int sessionTtlMinutes = 60;
        int dataRetentionDays = 0;         // 0 = immediate deletion
        int auditRetentionDays = 90;
        bool enableAutoCleanup = true;
        int cleanupIntervalMinutes = 15;
        bool enableSecureWipe = true;      // Overwrite data before deletion
        bool enableAuditLog = true;
        QString auditLogPath;
        QString dataDirectory;
        bool enableMetrics = true;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Data classification
    enum DataClass {
        Sensitive,       // PII, credentials, etc.
        Session,         // Temporary session data
        Cached,          // Cached results
        Audit,           // Audit logs
        Anonymous        // Anonymized/aggregated data
    };

    // Data entry tracking
    struct DataEntry {
        QString id;
        QString path;
        DataClass classification;
        QDateTime createdAt;
        QDateTime expiresAt;
        qint64 sizeBytes;
        bool isAnonymized;
    };

    // Core functionality
    QString registerData(const QString& path, DataClass classification, int customTtlMinutes = -1);
    bool unregisterData(const QString& id);
    bool deleteData(const QString& id, bool immediate = false);
    bool anonymizeData(const QString& id);
    
    void cleanupExpiredData();
    void cleanupSession(const QString& sessionId);
    void purgeAllData(DataClass classification = Session);
    
    QVector<DataEntry> getTrackedData(DataClass classification = Session) const;
    DataEntry getDataEntry(const QString& id) const;

    // Metrics
    struct Metrics {
        qint64 dataEntriesTracked = 0;
        qint64 dataEntriesDeleted = 0;
        qint64 bytesDeleted = 0;
        qint64 sessionsCleanedUp = 0;
        qint64 anonymizationCount = 0;
        qint64 auditEntriesCreated = 0;
        qint64 errorCount = 0;
        double avgCleanupLatencyMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

signals:
    void dataDeleted(const QString& id, qint64 sizeBytes);
    void dataExpired(const QString& id);
    void sessionCleaned(const QString& sessionId);
    void cleanupCompleted(int itemsDeleted, qint64 bytesDeleted);
    void errorOccurred(const QString& error);
    void metricsUpdated(const Metrics& metrics);

private slots:
    void performAutoCleanup();

private:
    // Configuration
    Config m_config;
    mutable QMutex m_configMutex;

    // Data tracking
    QMap<QString, DataEntry> m_trackedData;
    mutable QMutex m_dataMutex;

    // Metrics
    Metrics m_metrics;
    mutable QMutex m_metricsMutex;

    // Auto cleanup timer
    QTimer* m_cleanupTimer;

    // Helper methods
    void logStructured(const QString& level, const QString& message, const QJsonObject& context = QJsonObject());
    void recordLatency(const QString& operation, const std::chrono::milliseconds& duration);
    void logAudit(const QString& action, const QJsonObject& details);
    bool secureDelete(const QString& path);
    QString generateDataId();
    bool isExpired(const DataEntry& entry) const;
    QDateTime calculateExpiry(DataClass classification, int customTtlMinutes) const;
};
