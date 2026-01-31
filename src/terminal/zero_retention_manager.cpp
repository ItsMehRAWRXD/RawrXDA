#include "zero_retention_manager.hpp"


ZeroRetentionManager::ZeroRetentionManager(void* parent)
    : void(parent),
      m_cleanupTimer(new void*(this))
{
    logStructured("INFO", "ZeroRetentionManager initializing", void*{{"component", "ZeroRetentionManager"}});
// Qt connect removed
    logStructured("INFO", "ZeroRetentionManager initialized successfully", void*{{"component", "ZeroRetentionManager"}});
}

ZeroRetentionManager::~ZeroRetentionManager()
{
    logStructured("INFO", "ZeroRetentionManager shutting down", void*{{"component", "ZeroRetentionManager"}});
    
    m_cleanupTimer->stop();
    
    // Perform final cleanup
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.dataRetentionDays == 0) {
        purgeAllData(Session);
    }
    
    logStructured("INFO", "ZeroRetentionManager shutdown complete", void*{{"component", "ZeroRetentionManager"}});
}

void ZeroRetentionManager::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    
    // Restart cleanup timer with new interval
    if (config.enableAutoCleanup) {
        m_cleanupTimer->start(config.cleanupIntervalMinutes * 60 * 1000);
    } else {
        m_cleanupTimer->stop();
    }
    
    logStructured("INFO", "Configuration updated", void*{
        {"sessionTtlMinutes", config.sessionTtlMinutes},
        {"dataRetentionDays", config.dataRetentionDays},
        {"enableAutoCleanup", config.enableAutoCleanup},
        {"cleanupIntervalMinutes", config.cleanupIntervalMinutes}
    });
}

ZeroRetentionManager::Config ZeroRetentionManager::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
}

std::string ZeroRetentionManager::registerData(const std::string& path, DataClass classification, int customTtlMinutes)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::string id = generateDataId();
        
        DataEntry entry;
        entry.id = id;
        entry.path = path;
        entry.classification = classification;
        entry.createdAt = std::chrono::system_clock::time_point::currentDateTime();
        entry.expiresAt = calculateExpiry(classification, customTtlMinutes);
        entry.isAnonymized = false;
        
        // Get file size if path exists
        std::filesystem::path fileInfo(path);
        if (fileInfo.exists()) {
            entry.sizeBytes = fileInfo.size();
        } else {
            entry.sizeBytes = 0;
        }
        
        {
            std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
            m_trackedData[id] = entry;
        }
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.dataEntriesTracked++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Data registered for tracking", void*{
            {"id", id},
            {"path", path},
            {"classification", static_cast<int>(classification)},
            {"expiresAt", entry.expiresAt.toString(//ISODate)},
            {"sizeBytes", entry.sizeBytes}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_registered", void*{
                {"id", id},
                {"path", path},
                {"classification", static_cast<int>(classification)},
                {"createdAt", entry.createdAt.toString(//ISODate)},
                {"expiresAt", entry.expiresAt.toString(//ISODate)}
            });
        }
        
        return id;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to register data", void*{{"error", e.what()}});
        errorOccurred(std::string("Registration failed: %1")));
        return std::string();
    }
}

bool ZeroRetentionManager::unregisterData(const std::string& id)
{
    try {
        std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
        
        if (!m_trackedData.contains(id)) {
            logStructured("WARN", "Data ID not found for unregistration", void*{{"id", id}});
            return false;
        }
        
        m_trackedData.remove(id);
        
        logStructured("INFO", "Data unregistered", void*{{"id", id}});
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_unregistered", void*{{"id", id}});
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to unregister data", void*{{"error", e.what()}});
        errorOccurred(std::string("Unregistration failed: %1")));
        return false;
    }
}

bool ZeroRetentionManager::deleteData(const std::string& id, bool immediate)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        DataEntry entry;
        {
            std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
            if (!m_trackedData.contains(id)) {
                logStructured("WARN", "Data ID not found for deletion", void*{{"id", id}});
                return false;
            }
            entry = m_trackedData[id];
        }
        
        // Check if should delete based on retention policy
        if (!immediate && !isExpired(entry)) {
            logStructured("DEBUG", "Data not yet expired, skipping deletion", void*{
                {"id", id},
                {"expiresAt", entry.expiresAt.toString(//ISODate)}
            });
            return false;
        }
        
        bool deleteSuccess = secureDelete(entry.path);
        
        if (deleteSuccess || immediate) {
            {
                std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
                m_trackedData.remove(id);
            }
            
            {
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.dataEntriesDeleted++;
                m_metrics.bytesDeleted += entry.sizeBytes;
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            logStructured("INFO", "Data deleted", void*{
                {"id", id},
                {"path", entry.path},
                {"sizeBytes", entry.sizeBytes},
                {"latencyMs", duration.count()}
            });
            
            Config config;
            {
                std::lock_guard<std::mutex> configLocker(&m_configMutex);
                config = m_config;
            }
            
            if (config.enableAuditLog) {
                logAudit("data_deleted", void*{
                    {"id", id},
                    {"path", entry.path},
                    {"sizeBytes", entry.sizeBytes},
                    {"classification", static_cast<int>(entry.classification)},
                    {"immediate", immediate}
                });
            }
            
            dataDeleted(id, entry.sizeBytes);
        }
        
        return deleteSuccess;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to delete data", void*{{"error", e.what()}});
        errorOccurred(std::string("Deletion failed: %1")));
        return false;
    }
}

bool ZeroRetentionManager::anonymizeData(const std::string& id)
{
    try {
        std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
        
        if (!m_trackedData.contains(id)) {
            logStructured("WARN", "Data ID not found for anonymization", void*{{"id", id}});
            return false;
        }
        
        DataEntry& entry = m_trackedData[id];
        
        // Read file
        std::fstream file(entry.path);
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for anonymization", void*{
                {"path", entry.path},
                {"error", file.errorString()}
            });
            return false;
        }
        
        std::string content = file.readAll();
        file.seek(0);
        
        // Simple anonymization: hash PII patterns
        // In production, use more sophisticated anonymization
        std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        std::regex ipPattern(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
        
        content.replace(emailPattern, "[EMAIL_REDACTED]");
        content.replace(ipPattern, "[IP_REDACTED]");
        
        file.resize(0);
        file.write(content.toUtf8());
        file.close();
        
        entry.isAnonymized = true;
        entry.classification = Anonymous;
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.anonymizationCount++;
        }
        
        logStructured("INFO", "Data anonymized", void*{
            {"id", id},
            {"path", entry.path}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_anonymized", void*{
                {"id", id},
                {"path", entry.path}
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Failed to anonymize data", void*{{"error", e.what()}});
        errorOccurred(std::string("Anonymization failed: %1")));
        return false;
    }
}

void ZeroRetentionManager::cleanupExpiredData()
{
    auto startTime = std::chrono::steady_clock::now();
    int deletedCount = 0;
    int64_t bytesDeleted = 0;
    
    try {
        std::vector<std::string> expiredIds;
        
        {
            std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (isExpired(it.value())) {
                    expiredIds.append(it.key());
                }
            }
        }
        
        for (const std::string& id : expiredIds) {
            DataEntry entry = getDataEntry(id);
            if (deleteData(id, false)) {
                deletedCount++;
                bytesDeleted += entry.sizeBytes;
                dataExpired(id);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("cleanup", duration);
        
        logStructured("INFO", "Expired data cleanup completed", void*{
            {"deletedCount", deletedCount},
            {"bytesDeleted", bytesDeleted},
            {"latencyMs", duration.count()}
        });
        
        cleanupCompleted(deletedCount, bytesDeleted);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Cleanup failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Cleanup failed: %1")));
    }
}

void ZeroRetentionManager::cleanupSession(const std::string& sessionId)
{
    try {
        std::vector<std::string> sessionDataIds;
        
        {
            std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (it.value().path.contains(sessionId)) {
                    sessionDataIds.append(it.key());
                }
            }
        }
        
        for (const std::string& id : sessionDataIds) {
            deleteData(id, true);
        }
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.sessionsCleanedUp++;
        }
        
        logStructured("INFO", "Session cleaned up", void*{
            {"sessionId", sessionId},
            {"itemsDeleted", sessionDataIds.size()}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("session_cleaned", void*{
                {"sessionId", sessionId},
                {"itemsDeleted", sessionDataIds.size()}
            });
        }
        
        sessionCleaned(sessionId);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Session cleanup failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Session cleanup failed: %1")));
    }
}

void ZeroRetentionManager::purgeAllData(DataClass classification)
{
    try {
        std::vector<std::string> idsToDelete;
        
        {
            std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
            for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
                if (it.value().classification == classification) {
                    idsToDelete.append(it.key());
                }
            }
        }
        
        for (const std::string& id : idsToDelete) {
            deleteData(id, true);
        }
        
        logStructured("INFO", "Data purged", void*{
            {"classification", static_cast<int>(classification)},
            {"itemsDeleted", idsToDelete.size()}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("data_purged", void*{
                {"classification", static_cast<int>(classification)},
                {"itemsDeleted", idsToDelete.size()}
            });
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Data purge failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Purge failed: %1")));
    }
}

std::vector<ZeroRetentionManager::DataEntry> ZeroRetentionManager::getTrackedData(DataClass classification) const
{
    std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
    std::vector<DataEntry> result;
    
    for (auto it = m_trackedData.begin(); it != m_trackedData.end(); ++it) {
        if (it.value().classification == classification) {
            result.append(it.value());
        }
    }
    
    return result;
}

ZeroRetentionManager::DataEntry ZeroRetentionManager::getDataEntry(const std::string& id) const
{
    std::lock_guard<std::mutex> dataMutex(&m_dataMutex);
    return m_trackedData.value(id, DataEntry());
}

ZeroRetentionManager::Metrics ZeroRetentionManager::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
}

void ZeroRetentionManager::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
}

void ZeroRetentionManager::performAutoCleanup()
{
    logStructured("DEBUG", "Auto cleanup triggered", void*{});
    cleanupExpiredData();
}

void ZeroRetentionManager::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "ZeroRetentionManager";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
}

void ZeroRetentionManager::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "cleanup") {
        int cleanupCount = m_metrics.dataEntriesDeleted;
        m_metrics.avgCleanupLatencyMs = 
            (m_metrics.avgCleanupLatencyMs * (cleanupCount - 1) + duration.count()) / cleanupCount;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    }
}

void ZeroRetentionManager::logAudit(const std::string& action, const void*& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }
    
    void* auditEntry;
    auditEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    std::fstream auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        void* doc(auditEntry);
        out << doc.toJson(void*::Compact) << "\n";
        auditFile.close();
        
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.auditEntriesCreated++;
    } else {
        logStructured("ERROR", "Failed to write audit log", void*{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

bool ZeroRetentionManager::secureDelete(const std::string& path)
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!std::fstream::exists(path)) {
        logStructured("WARN", "File does not exist for deletion", void*{{"path", path}});
        return false;
    }
    
    if (config.enableSecureWipe) {
        // Secure wipe: overwrite file with random data before deletion
        std::fstream file(path);
        if (file.open(QIODevice::ReadWrite)) {
            int64_t size = file.size();
            std::vector<uint8_t> zeroData(size, '\0');
            file.write(zeroData);
            file.flush();
            file.close();
            
            logStructured("DEBUG", "Secure wipe completed", void*{
                {"path", path},
                {"size", size}
            });
        }
    }
    
    bool removed = std::fstream::remove(path);
    
    if (!removed) {
        logStructured("ERROR", "Failed to delete file", void*{{"path", path}});
    }
    
    return removed;
}

std::string ZeroRetentionManager::generateDataId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ZeroRetentionManager::isExpired(const DataEntry& entry) const
{
    return std::chrono::system_clock::time_point::currentDateTime() >= entry.expiresAt;
}

std::chrono::system_clock::time_point ZeroRetentionManager::calculateExpiry(DataClass classification, int customTtlMinutes) const
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    
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



