#pragma once
// zero_retention_manager.hpp – Qt-free GDPR/privacy data retention (C++20 / Win32)
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;

/**
 * Production-ready GDPR/privacy-compliant data retention manager.
 *   - Automatic data deletion based on retention policies
 *   - Session cleanup with configurable TTL
 *   - Audit logging for compliance
 *   - Privacy-first data handling with secure deletion
 *   - Timer-based auto-cleanup via background thread
 */
class ZeroRetentionManager {
public:
    ZeroRetentionManager();
    ~ZeroRetentionManager();

    // ---------- Configuration ----------
    struct Config {
        int         sessionTtlMinutes       = 60;
        int         dataRetentionDays       = 0;  // 0 = immediate deletion
        int         auditRetentionDays      = 90;
        bool        enableAutoCleanup       = true;
        int         cleanupIntervalMinutes  = 15;
        bool        enableSecureWipe        = true;
        bool        enableAuditLog          = true;
        std::string auditLogPath;
        std::string dataDirectory;
        bool        enableMetrics           = true;
    };

    void   setConfig(const Config& config);
    Config getConfig() const;

    // ---------- Data classification ----------
    enum DataClass {
        Sensitive,
        Session,
        Cached,
        Audit,
        Anonymous
    };

    struct DataEntry {
        std::string id;
        std::string path;
        DataClass   classification = Session;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point expiresAt;
        int64_t     sizeBytes     = 0;
        bool        isAnonymized  = false;
    };

    // ---------- Core functionality ----------
    std::string registerData(const std::string& path, DataClass classification, int customTtlMinutes = -1);
    bool        unregisterData(const std::string& id);
    bool        deleteData(const std::string& id, bool immediate = false);
    bool        anonymizeData(const std::string& id);

    void cleanupExpiredData();
    void cleanupSession(const std::string& sessionId);
    void purgeAllData(DataClass classification = Session);

    std::vector<DataEntry> getTrackedData(DataClass classification = Session) const;
    DataEntry              getDataEntry(const std::string& id) const;

    // ---------- Metrics ----------
    struct Metrics {
        int64_t dataEntriesTracked    = 0;
        int64_t dataEntriesDeleted    = 0;
        int64_t bytesDeleted          = 0;
        int64_t sessionsCleanedUp     = 0;
        int64_t anonymizationCount    = 0;
        int64_t auditEntriesCreated   = 0;
        int64_t errorCount            = 0;
        double  avgCleanupLatencyMs   = 0.0;
    };

    Metrics getMetrics() const;
    void    resetMetrics();

    // ---------- Callbacks (replaces Qt signals) ----------
    using DataDeletedCb   = void(*)(void* ctx, const char* id, int64_t sizeBytes);
    using DataExpiredCb   = void(*)(void* ctx, const char* id);
    using SessionCleanCb  = void(*)(void* ctx, const char* sessionId);
    using CleanupDoneCb   = void(*)(void* ctx, int itemsDeleted, int64_t bytesDeleted);
    using ErrorCb         = void(*)(void* ctx, const char* error);
    using MetricsUpdCb    = void(*)(void* ctx, const Metrics* m);

    void setDataDeletedCb(DataDeletedCb cb, void* ctx)      { m_delCb = cb; m_delCtx = ctx; }
    void setDataExpiredCb(DataExpiredCb cb, void* ctx)       { m_expCb = cb; m_expCtx = ctx; }
    void setSessionCleanCb(SessionCleanCb cb, void* ctx)     { m_sesCb = cb; m_sesCtx = ctx; }
    void setCleanupDoneCb(CleanupDoneCb cb, void* ctx)       { m_clnCb = cb; m_clnCtx = ctx; }
    void setErrorCb(ErrorCb cb, void* ctx)                    { m_errCb = cb; m_errCtx = ctx; }
    void setMetricsUpdatedCb(MetricsUpdCb cb, void* ctx)      { m_metCb = cb; m_metCtx = ctx; }

private:
    // State
    Config                           m_config;
    mutable std::mutex               m_configMutex;

    std::map<std::string, DataEntry> m_trackedData;
    mutable std::mutex               m_dataMutex;

    Metrics                          m_metrics;
    mutable std::mutex               m_metricsMutex;

    // Background cleanup thread
    std::atomic<bool>                m_running{false};
    std::thread                      m_cleanupThread;
    void cleanupThreadFunc();
    void performAutoCleanup();

    // Helpers
    void logStructured(const std::string& level, const std::string& message,
                       const JsonObject& context = {});
    void recordLatency(const std::string& operation, std::chrono::milliseconds duration);
    void logAudit(const std::string& action, const JsonObject& details);
    bool secureDelete(const std::string& path);
    std::string generateDataId();
    bool isExpired(const DataEntry& entry) const;
    std::chrono::system_clock::time_point calculateExpiry(DataClass classification, int customTtlMinutes) const;

    // Callback state
    DataDeletedCb  m_delCb = nullptr;  void* m_delCtx = nullptr;
    DataExpiredCb  m_expCb = nullptr;  void* m_expCtx = nullptr;
    SessionCleanCb m_sesCb = nullptr;  void* m_sesCtx = nullptr;
    CleanupDoneCb  m_clnCb = nullptr;  void* m_clnCtx = nullptr;
    ErrorCb        m_errCb = nullptr;  void* m_errCtx = nullptr;
    MetricsUpdCb   m_metCb = nullptr;  void* m_metCtx = nullptr;
};
