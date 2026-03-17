// ============================================================================
// license_audit_trail.h — Enterprise License Audit Trail System
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include <cstdint>
#include <ctime>
#include <vector>
#include <array>

namespace RawrXD::License {

// ============================================================================
// Audit Trail Constants
// ============================================================================

// Extended audit entry statistics window
static constexpr uint32_t AUDIT_STATS_WINDOW_SECONDS = 3600;  // 1 hour
static constexpr uint32_t AUDIT_ANOMALY_THRESHOLD = 10;       // 10+ denials in window
static constexpr uint32_t AUDIT_LOG_ROTATION_SIZE = 1024 * 1024;  // 1 MB

// ============================================================================
// Audit Trail Types
// ============================================================================

enum class AuditEventType : uint8_t {
    FEATURE_GRANTED           = 0x01,
    FEATURE_DENIED            = 0x02,
    LICENSE_ACTIVATED         = 0x03,
    LICENSE_EXPIRED           = 0x04,
    LICENSE_REVOKED           = 0x05,
    TAMPERING_DETECTED        = 0x06,
    OFFLINE_VALIDATION        = 0x07,
    ONLINE_SYNC               = 0x08,
    GRACE_PERIOD_ENTERED      = 0x09,
    GRACE_PERIOD_EXCEEDED     = 0x0A,
    TIER_DOWNGRADE            = 0x0B,
    FEATURE_REMOVAL           = 0x0C,
    CLOCK_SKEW_DETECTED       = 0x0D,
    UNAUTHORIZED_ACCESS       = 0x0E,
    AUDIT_LOG_ROTATION        = 0x0F,
    SYSTEM_EVENT              = 0x10
};

enum class AnomalyType : uint8_t {
    EXCESSIVE_DENIALS         = 0x01,
    RAPID_TIER_CHANGES        = 0x02,
    SUSPICIOUS_FEATURE_PATTERN= 0x03,
    HWID_MISMATCH             = 0x04,
    CERTIFICATE_TAMPERING     = 0x05,
    GRACE_PERIOD_ABUSE        = 0x06,
    CLOCK_ROLLBACK            = 0x07,
    CACHE_CORRUPTION          = 0x08
};

// ============================================================================
// Extended Audit Statistics
// ============================================================================

struct FeatureAuditStats {
    uint32_t featureID;
    uint32_t grantCount;
    uint32_t denyCount;
    uint32_t lastAccessTime;
    float    denialRate;  // Denials / (Grants + Denials)
};

struct TierChangeRecord {
    uint32_t       timestamp;
    LicenseTierV2  oldTier;
    LicenseTierV2  newTier;
    const char*    reason;
};

struct AnomalyEvent {
    uint32_t     timestamp;
    AnomalyType  type;
    float        severity;  // 0.0 - 1.0
    const char*  description;
    uint32_t     affectedFeature;  // 0 if N/A
};

// ============================================================================
// SIEM Export Format
// ============================================================================

enum class SIEMFormat {
    CEF,         ///< Common Event Format (Arcsight)
    LEEF,        ///< Log Event Extended Format (QRadar)
    JSON,        ///< JSON (ELK, Splunk)
    SYSLOG_RFC5424 ///< RFC5424 Syslog
};

struct SIEMExportConfig {
    SIEMFormat  format;
    const char* hostname;
    uint16_t    port;
    bool        useTLS;
    const char* accessKey;  // API key or auth token
};

// ============================================================================
// Audit Trail Manager
// ============================================================================

class AuditTrailManager {
public:
    AuditTrailManager();
    ~AuditTrailManager();

    // Record an audit event
    void recordEvent(AuditEventType type, FeatureID feature, bool granted, const char* caller);

    // Get feature statistics in time window
    bool getFeatureStats(FeatureID feature, FeatureAuditStats& stats) const;

    // Get all feature statistics
    std::vector<FeatureAuditStats> getAllFeatureStats() const;

    // Get tier change history
    std::vector<TierChangeRecord> getTierChangeHistory(uint32_t maxRecords = 100) const;

    // Get anomalies detected in time window
    std::vector<AnomalyEvent> getAnomalies(uint32_t maxEvents = 50) const;

    // Clear audit trail (requires authentication)
    bool clearAuditTrail(const char* authToken);

    // Export audit trail to file
    bool exportToFile(const char* path, const char* format = "json");

    // Export to SIEM
    bool exportToSIEM(const SIEMExportConfig& config);

    // Get audit summary for compliance report
    std::string generateComplianceSummary() const;

    // Get JSON dump of recent events
    std::string toJSON(uint32_t maxEvents = 100) const;

    // Statistics
    uint32_t getTotalEvents() const;
    uint32_t getTotalDenials() const;
    uint32_t getTotalGrants() const;
    uint32_t getAnomalyCount() const;
    
    // Real-time monitoring
    float getDenialRate(uint32_t windowSeconds = 3600) const;
    bool isInAnomalousState() const;

private:
    // Analyze events for anomalies
    void analyzeAnomalities();

    // Check for excessive denials
    bool detectExcessiveDenials();

    // Check for rapid tier changes
    bool detectRapidTierChanges();

    // Check for suspicious feature patterns
    bool detectFeaturePatterns();

    // Update feature stats
    void updateFeatureStats(FeatureID feature, bool granted);

    // Persist audit log to disk
    bool persistToFile();

    // Load audit log from disk
    bool loadFromFile();

    // Members
    std::array<FeatureAuditStats, 61> m_featureStats;
    std::vector<TierChangeRecord>      m_tierHistory;
    std::vector<AnomalyEvent>          m_anomalies;
    
    uint32_t m_totalEvents;
    uint32_t m_totalDenials;
    uint32_t m_totalGrants;
    
    uint32_t m_lastAnomalyAnalysisTime;
    bool     m_isAnomalous;

    // Audit log file
    const char* m_auditLogPath;
};

// ============================================================================
// Anomaly Detector
// ============================================================================

class AnomalyDetector {
public:
    AnomalyDetector();
    ~AnomalyDetector();

    // Detect anomaly from event
    bool detectAnomaly(AuditEventType eventType, FeatureID feature, const char* caller);

    // Get anomaly severity (0.0 - 1.0)
    float getAnomalalySeverity() const;

    // Get anomaly description
    const char* getAnomalyDescription() const;

    // Reset detector state
    void reset();

    // Configure thresholds
    void setExcessiveDenialThreshold(uint32_t count);
    void setRapidTierChangeThreshold(uint32_t changesInTime);
    void setFeaturePatternThreshold(float abnormalityScore);

    // Get baseline metrics
    void getBaselineMetrics(uint32_t& avgGrants, uint32_t& avgDenials) const;

    // Check if current state is anomalous
    bool isAnomalous() const;

private:
    // Compute anomaly score
    float computeAnomalyScore();

    // Members
    uint32_t m_excessiveDenialThreshold;
    uint32_t m_rapidChangeThreshold;
    float    m_patternThreshold;

    float    m_currentSeverity;
    char     m_description[256];
    bool     m_isAnomalous;

    uint32_t m_baselineGrants;
    uint32_t m_baselineDenials;
};

// ============================================================================
// SIEM Exporter
// ============================================================================

class SIEMExporter {
public:
    SIEMExporter();
    ~SIEMExporter();

    // Export audit trail in CEF format
    std::string formatCEF(AuditEventType eventType, FeatureID feature, const char* caller);

    // Export audit trail in LEEF format
    std::string formatLEEF(AuditEventType eventType, FeatureID feature, const char* caller);

    // Export audit trail in JSON format
    std::string formatJSON(AuditEventType eventType, FeatureID feature, const char* caller);

    // Export audit trail in RFC5424 Syslog format
    std::string formatSyslog(AuditEventType eventType, FeatureID feature, const char* caller);

    // Send event to SIEM server
    bool sendToSIEM(const SIEMExportConfig& config, const std::string& event);

    // Batch export multiple events
    bool batchExport(const SIEMExportConfig& config, const std::vector<std::string>& events);

private:
    // Helper: Get event type string
    const char* getEventTypeString(AuditEventType type) const;

    // Helper: URL encode string
    std::string urlEncode(const char* str) const;

    // Helper: Get severity level
    int getSeverityLevel(AuditEventType type) const;
};

// ============================================================================
// Persistent Audit Log
// ============================================================================

class PersistentAuditLog {
public:
    PersistentAuditLog();
    ~PersistentAuditLog();

    // Append entry to persistent log
    bool append(AuditEventType type, FeatureID feature, bool granted, const char* caller);

    // Read entries from persistent log
    bool read(std::vector<LicenseAuditEntry>& entries, uint32_t maxCount = 1000);

    // Rotate log file (archive old log)
    bool rotate();

    // Clear log (requires auth)
    bool clear(const char* authToken);

    // Get current log size
    uint32_t getLogSize() const;

    // Get log entries since timestamp
    std::vector<LicenseAuditEntry> getEntriesSince(uint32_t timestamp) const;

private:
    // Get log file path
    const char* getLogPath() const;

    // Get archive log path
    const char* getArchivePath(uint32_t sequence) const;

    // Members
    uint32_t m_currentSize;
    uint32_t m_logRotationCount;
};

// ============================================================================
// Global Audit Trail Instances
// ============================================================================

extern AuditTrailManager g_auditTrailManager;
extern AnomalyDetector   g_anomalyDetector;
extern SIEMExporter      g_siemExporter;
extern PersistentAuditLog g_persistentAuditLog;

// ============================================================================
// Inline Helper Functions
// ============================================================================

inline const char* getEventTypeString(AuditEventType type) {
    switch (type) {
        case AuditEventType::FEATURE_GRANTED:         return "FEATURE_GRANTED";
        case AuditEventType::FEATURE_DENIED:          return "FEATURE_DENIED";
        case AuditEventType::LICENSE_ACTIVATED:       return "LICENSE_ACTIVATED";
        case AuditEventType::LICENSE_EXPIRED:         return "LICENSE_EXPIRED";
        case AuditEventType::LICENSE_REVOKED:         return "LICENSE_REVOKED";
        case AuditEventType::TAMPERING_DETECTED:      return "TAMPERING_DETECTED";
        case AuditEventType::OFFLINE_VALIDATION:      return "OFFLINE_VALIDATION";
        case AuditEventType::ONLINE_SYNC:             return "ONLINE_SYNC";
        case AuditEventType::GRACE_PERIOD_ENTERED:    return "GRACE_PERIOD_ENTERED";
        case AuditEventType::GRACE_PERIOD_EXCEEDED:   return "GRACE_PERIOD_EXCEEDED";
        case AuditEventType::TIER_DOWNGRADE:          return "TIER_DOWNGRADE";
        case AuditEventType::FEATURE_REMOVAL:         return "FEATURE_REMOVAL";
        case AuditEventType::CLOCK_SKEW_DETECTED:     return "CLOCK_SKEW_DETECTED";
        case AuditEventType::UNAUTHORIZED_ACCESS:     return "UNAUTHORIZED_ACCESS";
        case AuditEventType::AUDIT_LOG_ROTATION:      return "AUDIT_LOG_ROTATION";
        case AuditEventType::SYSTEM_EVENT:            return "SYSTEM_EVENT";
        default:                                      return "UNKNOWN";
    }
}

inline const char* getAnomalyTypeString(AnomalyType type) {
    switch (type) {
        case AnomalyType::EXCESSIVE_DENIALS:          return "EXCESSIVE_DENIALS";
        case AnomalyType::RAPID_TIER_CHANGES:         return "RAPID_TIER_CHANGES";
        case AnomalyType::SUSPICIOUS_FEATURE_PATTERN: return "SUSPICIOUS_FEATURE_PATTERN";
        case AnomalyType::HWID_MISMATCH:              return "HWID_MISMATCH";
        case AnomalyType::CERTIFICATE_TAMPERING:      return "CERTIFICATE_TAMPERING";
        case AnomalyType::GRACE_PERIOD_ABUSE:         return "GRACE_PERIOD_ABUSE";
        case AnomalyType::CLOCK_ROLLBACK:             return "CLOCK_ROLLBACK";
        case AnomalyType::CACHE_CORRUPTION:           return "CACHE_CORRUPTION";
        default:                                      return "UNKNOWN";
    }
}

}  // namespace RawrXD::License
