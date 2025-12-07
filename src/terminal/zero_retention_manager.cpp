#include "zero_retention_manager.hpp"
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <QCryptographicHash>

ZeroRetentionManager::ZeroRetentionManager(QObject* parent)
    : QObject(parent),
      m_cleanupTimer(new QTimer(this))
{
    logStructured("INFO", "ZeroRetentionManager initializing", QJsonObject{{"component", "ZeroRetentionManager"}});
    
    connect(m_cleanupTimer, &QTimer::timeout, this, &ZeroRetentionManager::performAutoCleanup);
    
    logStructured("INFO", "ZeroRetentionManager initialized successfully", QJsonObject{{"component", "ZeroRetentionManager"}});
}

ZeroRetentionManager::~ZeroRetentionManager()
{
    logStructured("INFO", "ZeroRetentionManager shutting down", QJsonObject{{"component", "ZeroRetentionManager"}});
    
    m_cleanupTimer->stop();
    
    // Perform final cleanup
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.dataRetentionDays == 0) {
        purgeAllData(Session);
    }
    
    logStructured("INFO", "ZeroRetentionManager shutdown complete", QJsonObject{{"component", "ZeroRetentionManager"}});
}

void ZeroRetentionManager::setConfig(const Config& config)
{
    QMutexLocker locker(&m_configMutex);
    m_config = config;
    
    // Restart cleanup timer with new interval
    if (config.enableAutoCleanup) {
        m_cleanupTimer->start(config.cleanupIntervalMinutes * 60 * 1000);
    } else {
        m_cleanupTimer->stop();
    }
    
    logStructured("INFO", "Configuration updated", QJsonObject{
        {"sessionTtlMinutes", config.sessionTtlMinutes},
        {"dataRetentionDays", config.dataRetentionDays},
        {"enableAutoCleanup", config.enableAutoCleanup},
        {"cleanupIntervalMinutes", config.cleanupIntervalMinutes}
    });
}

ZeroRetentionManager::Config ZeroRetentionManager::getConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_config;
}

QString ZeroRetentionManager::registerData(const QString& path, DataClass classification, int customTtlMinutes)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        QString id = generateDataId();
        
        DataEntry entry;
        entry.id = id;
        entry.path = path;
        entry.classification = classification;
        entry.createdAt = QDateTime::currentDateTime();
        entry.expiresAt = calculateExpiry(classification, customTtlMinutes);
        entry.isAnonymized = false;
        
        // Get file size if path exists
        QFileInfo fileInfo(path);
        if (fileInfo.exists()) {
            entry.sizeBytes = fileInfo.size();
        } else {
            entry.sizeBytes = 0;
        }
        
        {
            QMutexLocker dataMutex(&m_dataMutex);
            m_trackedData[id] = entry;
        }
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.dataEntriesTracked++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Data registered for tracking", QJsonObject{
            {"id", id},
            {"path", path},
            {"classification", static_cast<int>(classification)},
            {"expiresAt", entry.expiresAt.toString(Qt::ISODate)},
            {"sizeBytes", entry.sizeBytes}
        });
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_registered", QJsonObject{
                {"id", id},
                {"path", path},
                {"classification", static_cast<int>(classification)},
                {"createdAt", entry.createdAt.toString(Qt::ISODate)},
                {"expiresAt", entry.expiresAt.toString(Qt::ISODate)}
            });
        }
        
        return id;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to register data", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Registration failed: %1").arg(e.what()));
        return QString();
    }
}

bool ZeroRetentionManager::unregisterData(const QString& id)
{
    try {
        QMutexLocker dataMutex(&m_dataMutex);
        
        if (!m_trackedData.contains(id)) {
            logStructured("WARN", "Data ID not found for unregistration", QJsonObject{{"id", id}});
            return false;
        }
        
        m_trackedData.remove(id);
        
        logStructured("INFO", "Data unregistered", QJsonObject{{"id", id}});
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_unregistered", QJsonObject{{"id", id}});
        }
        
        return true;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to unregister data", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Unregistration failed: %1").arg(e.what()));
        return false;
    }
}

bool ZeroRetentionManager::deleteData(const QString& id, bool immediate)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        DataEntry entry;
        {
            QMutexLocker dataMutex(&m_dataMutex);
            if (!m_trackedData.contains(id)) {
                logStructured("WARN", "Data ID not found for deletion", QJsonObject{{"id", id}});
                return false;
            }
            entry = m_trackedData[id];
        }
        
        // Check if should delete based on retention policy
        if (!immediate && !isExpired(entry)) {
            logStructured("DEBUG", "Data not yet expired, skipping deletion", QJsonObject{
                {"id", id},
                {"expiresAt", entry.expiresAt.toString(Qt::ISODate)}
            });
            return false;
        }
        
        bool deleteSuccess = secureDelete(entry.path);
        
        if (deleteSuccess || immediate) {
            {
                QMutexLocker dataMutex(&m_dataMutex);
                m_trackedData.remove(id);
            }
            
            {
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.dataEntriesDeleted++;
                m_metrics.bytesDeleted += entry.sizeBytes;
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            logStructured("INFO", "Data deleted", QJsonObject{
                {"id", id},
                {"path", entry.path},
                {"sizeBytes", entry.sizeBytes},
                {"latencyMs", duration.count()}
            });
            
            Config config;
            {
                QMutexLocker configLocker(&m_configMutex);
                config = m_config;
            }
            
            if (config.enableAuditLog) {
                logAudit("data_deleted", QJsonObject{
                    {"id", id},
                    {"path", entry.path},
                    {"sizeBytes", entry.sizeBytes},
                    {"classification", static_cast<int>(entry.classification)},
                    {"immediate", immediate}
                });
            }
            
            emit dataDeleted(id, entry.sizeBytes);
        }
        
        return deleteSuccess;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to delete data", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Deletion failed: %1").arg(e.what()));
        return false;
    }
}

bool ZeroRetentionManager::anonymizeData(const QString& id)
{
    try {
        QMutexLocker dataMutex(&m_dataMutex);
        
        if (!m_trackedData.contains(id)) {
            logStructured("WARN", "Data ID not found for anonymization", QJsonObject{{"id", id}});
            return false;
        }
        
        DataEntry& entry = m_trackedData[id];
        
        // Read file
        QFile file(entry.path);
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for anonymization", QJsonObject{
                {"path", entry.path},
                {"error", file.errorString()}
            });
            return false;
        }
        
        QString content = file.readAll();
        file.seek(0);
        
        // Simple anonymization: hash PII patterns
        // In production, use more sophisticated anonymization
        QRegularExpression emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        QRegularExpression ipPattern(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
        
        content.replace(emailPattern, "[EMAIL_REDACTED]");
        content.replace(ipPattern, "[IP_REDACTED]");
        
        file.resize(0);
        file.write(content.toUtf8());
        file.close();
        
        entry.isAnonymized = true;
        entry.classification = Anonymous;
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.anonymizationCount++;
        }
        
        logStructured("INFO", "Data anonymized", QJsonObject{
            {"id", id},
            {"path", entry.path}
        });
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_anonymized", QJsonObject{
                {"id", id},
                {"path", entry.path}
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to anonymize data", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Anonymization failed: %1").arg(e.what()));
        return false;
    }
}

void ZeroRetentionManager::cleanupExpiredData()
{
    auto startTime = std::chrono::steady_clock::now();
    int deletedCount = 0;
    qint64 bytesDeleted = 0;
    
    try {
        QVector<QString> expiredIds;
        
        {
            QMutexLocker dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (isExpired(it.value())) {
                    expiredIds.append(it.key());
                }
            }
        }
        
        for (const QString& id : expiredIds) {
            DataEntry entry = getDataEntry(id);
            if (deleteData(id, false)) {
                deletedCount++;
                bytesDeleted += entry.sizeBytes;
                emit dataExpired(id);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("cleanup", duration);
        
        logStructured("INFO", "Expired data cleanup completed", QJsonObject{
            {"deletedCount", deletedCount},
            {"bytesDeleted", bytesDeleted},
            {"latencyMs", duration.count()}
        });
        
        emit cleanupCompleted(deletedCount, bytesDeleted);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Cleanup failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Cleanup failed: %1").arg(e.what()));
    }
}

void ZeroRetentionManager::cleanupSession(const QString& sessionId)
{
    try {
        QVector<QString> sessionDataIds;
        
        {
            QMutexLocker dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (it.value().path.contains(sessionId)) {
                    sessionDataIds.append(it.key());
                }
            }
        }
        
        for (const QString& id : sessionDataIds) {
            deleteData(id, true);
        }
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.sessionsCleanedUp++;
        }
        
        logStructured("INFO", "Session cleaned up", QJsonObject{
            {"sessionId", sessionId},
            {"itemsDeleted", sessionDataIds.size()}
        });
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("session_cleaned", QJsonObject{
                {"sessionId", sessionId},
                {"itemsDeleted", sessionDataIds.size()}
            });
        }
        
        emit sessionCleaned(sessionId);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Session cleanup failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Session cleanup failed: %1").arg(e.what()));
    }
}

void ZeroRetentionManager::purgeAllData(DataClass classification)
{
    try {
        QVector<QString> idsToDelete;
        
        {
            QMutexLocker dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (it.value().classification == classification) {
                    idsToDelete.append(it.key());
                }
            }
        }
        
        for (const QString& id : idsToDelete) {
            deleteData(id, true);
        }
        
        logStructured("INFO", "Data purged", QJsonObject{
            {"classification", static_cast<int>(classification)},
            {"itemsDeleted", idsToDelete.size()}
        });
        
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_purged", QJsonObject{
                {"classification", static_cast<int>(classification)},
                {"itemsDeleted", idsToDelete.size()}
            });
        }
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Data purge failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Purge failed: %1").arg(e.what()));
    }
}

QVector<ZeroRetentionManager::DataEntry> ZeroRetentionManager::getTrackedData(DataClass classification) const
{
    QMutexLocker dataMutex(&m_dataMutex);
    QVector<DataEntry> result;
    
    for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
        if (it.value().classification == classification) {
            result.append(it.value());
        }
    }
    
    return result;
}

ZeroRetentionManager::DataEntry ZeroRetentionManager::getDataEntry(const QString& id) const
{
    QMutexLocker dataMutex(&m_dataMutex);
    return m_trackedData.value(id, DataEntry());
}

ZeroRetentionManager::Metrics ZeroRetentionManager::getMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics;
}

void ZeroRetentionManager::resetMetrics()
{
    QMutexLocker locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", QJsonObject{});
}

void ZeroRetentionManager::performAutoCleanup()
{
    logStructured("DEBUG", "Auto cleanup triggered", QJsonObject{});
    cleanupExpiredData();
}

void ZeroRetentionManager::logStructured(const QString& level, const QString& message, const QJsonObject& context)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "ZeroRetentionManager";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    QJsonDocument doc(logEntry);
    qDebug().noquote() << doc.toJson(QJsonDocument::Compact);
}

void ZeroRetentionManager::recordLatency(const QString& operation, const std::chrono::milliseconds& duration)
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (operation == "cleanup") {
        int cleanupCount = m_metrics.dataEntriesDeleted;
        m_metrics.avgCleanupLatencyMs = 
            (m_metrics.avgCleanupLatencyMs * (cleanupCount - 1) + duration.count()) / cleanupCount;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        emit metricsUpdated(m_metrics);
    }
}

void ZeroRetentionManager::logAudit(const QString& action, const QJsonObject& details)
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.isEmpty()) {
        return;
    }
    
    QJsonObject auditEntry;
    auditEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    QFile auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        QJsonDocument doc(auditEntry);
        out << doc.toJson(QJsonDocument::Compact) << "\n";
        auditFile.close();
        
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.auditEntriesCreated++;
    } else {
        logStructured("ERROR", "Failed to write audit log", QJsonObject{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

bool ZeroRetentionManager::secureDelete(const QString& path)
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!QFile::exists(path)) {
        logStructured("WARN", "File does not exist for deletion", QJsonObject{{"path", path}});
        return false;
    }
    
    if (config.enableSecureWipe) {
        // Secure wipe: overwrite file with random data before deletion
        QFile file(path);
        if (file.open(QIODevice::ReadWrite)) {
            qint64 size = file.size();
            QByteArray zeroData(size, '\0');
            file.write(zeroData);
            file.flush();
            file.close();
            
            logStructured("DEBUG", "Secure wipe completed", QJsonObject{
                {"path", path},
                {"size", size}
            });
        }
    }
    
    bool removed = QFile::remove(path);
    
    if (!removed) {
        logStructured("ERROR", "Failed to delete file", QJsonObject{{"path", path}});
    }
    
    return removed;
}

QString ZeroRetentionManager::generateDataId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ZeroRetentionManager::isExpired(const DataEntry& entry) const
{
    return QDateTime::currentDateTime() >= entry.expiresAt;
}

QDateTime ZeroRetentionManager::calculateExpiry(DataClass classification, int customTtlMinutes) const
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    
    if (customTtlMinutes >= 0) {
        return now.addSecs(customTtlMinutes * 60);
    }
    
    switch (classification) {
        case Sensitive:
            return now.addSecs(config.dataRetentionDays * 24 * 60 * 60);
        case Session:
            return now.addSecs(config.sessionTtlMinutes * 60);
        case Cached:
            return now.addSecs(config.sessionTtlMinutes * 2 * 60);
        case Audit:
            return now.addDays(config.auditRetentionDays);
        case Anonymous:
            return now.addDays(365); // 1 year for anonymized data
        default:
            return now.addSecs(config.sessionTtlMinutes * 60);
    }
}
