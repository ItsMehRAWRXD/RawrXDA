#pragma once


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
class ZeroRetentionManager : public void {

public:
    explicit ZeroRetentionManager(void* parent = nullptr);
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
        std::string auditLogPath;
        std::string dataDirectory;
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
        std::string id;
        std::string path;
        DataClass classification;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point expiresAt;
        qint64 sizeBytes;
        bool isAnonymized;
    };

    // Core functionality
    std::string registerData(const std::string& path, DataClass classification, int customTtlMinutes = -1);
    bool unregisterData(const std::string& id);
    bool deleteData(const std::string& id, bool immediate = false);
    bool anonymizeData(const std::string& id);
    
    void cleanupExpiredData();
    void cleanupSession(const std::string& sessionId);
    void purgeAllData(DataClass classification = Session);
    
    std::vector<DataEntry> getTrackedData(DataClass classification = Session) const;
    DataEntry getDataEntry(const std::string& id) const;

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

    void dataDeleted(const std::string& id, qint64 sizeBytes);
    void dataExpired(const std::string& id);
    void sessionCleaned(const std::string& sessionId);
    void cleanupCompleted(int itemsDeleted, qint64 bytesDeleted);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

private:
    void performAutoCleanup();

private:
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;

    // Data tracking
    std::map<std::string, DataEntry> m_trackedData;
    mutable std::mutex m_dataMutex;

    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Auto cleanup timer
    void** m_cleanupTimer;

    // Helper methods
    void logStructured(const std::string& level, const std::string& message, const void*& context = void*());
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void logAudit(const std::string& action, const void*& details);
    bool secureDelete(const std::string& path);
    std::string generateDataId();
    bool isExpired(const DataEntry& entry) const;
    std::chrono::system_clock::time_point calculateExpiry(DataClass classification, int customTtlMinutes) const;
};

