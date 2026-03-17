#pragma once
// zero_retention_manager.hpp — Qt-free GDPR/privacy data retention (Win32 + STL)
// Purged: prior GUI-framework types (object, string, mutex, timer, vector, map,
//         json, crypto, uuid, meta-object, signals/slots).
// Replaced with: std::mutex, std::thread, std::chrono, std::unordered_map,
//                std::function callbacks, Win32 crypto RNG
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <cstdint>

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
 *
 * Zero Qt dependency — pure Win32/C++20.
 */
class ZeroRetentionManager {
public:
    // ---------- Callbacks (replace Qt signals) ----------
    using IdSizeCb   = std::function<void(const std::string& id, int64_t sizeBytes)>;
    using IdCb       = std::function<void(const std::string& id)>;
    using SessionCb  = std::function<void(const std::string& sessionId)>;
    using CleanupCb  = std::function<void(int itemsDeleted, int64_t bytesDeleted)>;
    using ErrorCb    = std::function<void(const std::string& error)>;

    IdSizeCb   onDataDeleted      = nullptr;
    IdCb       onDataExpired      = nullptr;
    SessionCb  onSessionCleaned   = nullptr;
    CleanupCb  onCleanupCompleted = nullptr;
    ErrorCb    onErrorOccurred    = nullptr;

    struct Metrics;
    using MetricsCb = std::function<void(const Metrics&)>;
    MetricsCb onMetricsUpdated = nullptr;

    // ---------- Configuration ----------
    struct Config {
        int  sessionTtlMinutes       = 60;
        int  dataRetentionDays       = 0;    // 0 = immediate deletion
        int  auditRetentionDays      = 90;
        bool enableAutoCleanup       = true;
        int  cleanupIntervalMinutes  = 15;
        bool enableSecureWipe        = true;
        bool enableAuditLog          = true;
        std::string auditLogPath;
        std::string dataDirectory;
        bool enableMetrics           = true;
    };

    void   setConfig(const Config& config);
    Config getConfig() const;

    // ---------- Data classification ----------
    enum DataClass {
        Sensitive,   // PII, credentials
        Session,     // Temporary session data
        Cached,      // Cached results
        Audit,       // Audit logs
        Anonymous    // Anonymized/aggregated
    };

    // ---------- Data entry tracking ----------
    struct DataEntry {
        std::string   id;
        std::string   path;
        DataClass     classification = Session;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point expiresAt;
        int64_t       sizeBytes    = 0;
        bool          isAnonymized = false;
    };

    // ---------- Core functionality ----------
    std::string registerData(const std::string& path, DataClass classification, int customTtlMinutes = -1);
    bool unregisterData(const std::string& id);
    bool deleteData(const std::string& id, bool immediate = false);
    bool anonymizeData(const std::string& id);

    void cleanupExpiredData();
    void cleanupSession(const std::string& sessionId);
    void purgeAllData(DataClass classification = Session);

    std::vector<DataEntry> getTrackedData(DataClass classification = Session) const;
    DataEntry getDataEntry(const std::string& id) const;

    // ---------- Metrics ----------
    struct Metrics {
        int64_t dataEntriesTracked   = 0;
        int64_t dataEntriesDeleted   = 0;
        int64_t bytesDeleted         = 0;
        int64_t sessionsCleanedUp    = 0;
        int64_t anonymizationCount   = 0;
        int64_t auditEntriesCreated  = 0;
        int64_t errorCount           = 0;
        double  avgCleanupLatencyMs  = 0.0;
    };

    Metrics getMetrics() const;
    void    resetMetrics();

    ZeroRetentionManager();
    ~ZeroRetentionManager();

    // Non-copyable
    ZeroRetentionManager(const ZeroRetentionManager&) = delete;
    ZeroRetentionManager& operator=(const ZeroRetentionManager&) = delete;

private:
    Config                                       m_config;
    mutable std::mutex                           m_configMutex;

    std::unordered_map<std::string, DataEntry>   m_trackedData;
    mutable std::mutex                           m_dataMutex;

    Metrics                                      m_metrics;
    mutable std::mutex                           m_metricsMutex;

    // Cleanup thread (replaces QTimer)
    std::thread    m_cleanupThread;
    std::atomic<bool> m_running{false};
    std::function<void()>* m_cleanupTimer = nullptr;
    void cleanupThreadFunc();

    // Callback emitters (invoked internally, dispatch to public callbacks)
    void performAutoCleanup();
    void dataDeleted(const std::string& id, int64_t sizeBytes);
    void dataExpired(const std::string& id);
    void sessionCleaned(const std::string& sessionId);
    void cleanupCompleted(int itemsDeleted, int64_t bytesDeleted);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

    // Helpers
    void        logStructured(const char* level, const std::string& message,
                              const std::string& contextJson = "{}");
    void        logStructured(const std::string& level, const std::string& message,
                              const std::string& contextJson = "{}");
    void        logAudit(const std::string& action, const std::string& detailsJson);
    void        recordLatency(const std::string& operation,
                              const std::chrono::milliseconds& duration);
    bool        secureDelete(const std::string& path);
    std::string generateDataId();
    bool        isExpired(const DataEntry& entry) const;

    std::chrono::system_clock::time_point
    calculateExpiry(DataClass classification, int customTtlMinutes) const;

    static std::string escapeJson(const std::string& s);
    static std::string nowISO8601();
    static std::string timeToISO8601(std::chrono::system_clock::time_point tp);
};
