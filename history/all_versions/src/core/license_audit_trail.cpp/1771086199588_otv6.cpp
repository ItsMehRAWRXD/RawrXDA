// ============================================================================
// license_audit_trail.cpp — Enterprise License Audit Trail System
// ============================================================================

#include "../include/license_audit_trail.h"
#include "../include/enterprise_license.h"
#include <cstring>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <windows.h>

namespace RawrXD::License {

// ============================================================================
// Global Audit Trail Instances
// ============================================================================

AuditTrailManager g_auditTrailManager;
AnomalyDetector   g_anomalyDetector;
SIEMExporter      g_siemExporter;
PersistentAuditLog g_persistentAuditLog;

// ============================================================================
// Audit Trail Manager Implementation
// ============================================================================

AuditTrailManager::AuditTrailManager()
    : m_totalEvents(0),
      m_totalDenials(0),
      m_totalGrants(0),
      m_lastAnomalyAnalysisTime(0),
      m_isAnomalous(false),
      m_auditLogPath("C:\\ProgramData\\RawrXD\\audit.log") {
    
    std::memset(m_featureStats.data(), 0, sizeof(m_featureStats));
    
    // Initialize feature stats
    for (uint32_t i = 0; i < 61; ++i) {
        m_featureStats[i].featureID = i;
    }

    loadFromFile();
}

AuditTrailManager::~AuditTrailManager() {
    persistToFile();
}

void AuditTrailManager::recordEvent(AuditEventType type, FeatureID feature, bool granted, const char* caller) {
    m_totalEvents++;

    if (granted) {
        m_totalGrants++;
    } else {
        m_totalDenials++;
    }

    // Update feature stats
    if (feature < 61) {
        updateFeatureStats(feature, granted);
    }

    // Record in persistent log
    g_persistentAuditLog.append(type, feature, granted, caller);

    // Analyze for anomalies periodically
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (now - m_lastAnomalyAnalysisTime > 60) {  // Analyze every 60 seconds
        analyzeAnomalities();
        m_lastAnomalyAnalysisTime = now;
    }
}

bool AuditTrailManager::getFeatureStats(FeatureID feature, FeatureAuditStats& stats) const {
    if (feature >= 61) return false;

    stats = m_featureStats[feature];
    stats.denialRate = (stats.grantCount + stats.denyCount > 0)
        ? (float)stats.denyCount / (stats.grantCount + stats.denyCount)
        : 0.0f;

    return true;
}

std::vector<FeatureAuditStats> AuditTrailManager::getAllFeatureStats() const {
    std::vector<FeatureAuditStats> stats;
    for (const auto& s : m_featureStats) {
        FeatureAuditStats copy = s;
        copy.denialRate = (s.grantCount + s.denyCount > 0)
            ? (float)s.denyCount / (s.grantCount + s.denyCount)
            : 0.0f;
        stats.push_back(copy);
    }
    return stats;
}

std::vector<TierChangeRecord> AuditTrailManager::getTierChangeHistory(uint32_t maxRecords) const {
    if (m_tierHistory.size() <= maxRecords) {
        return m_tierHistory;
    }
    return std::vector<TierChangeRecord>(
        m_tierHistory.end() - maxRecords,
        m_tierHistory.end()
    );
}

std::vector<AnomalyEvent> AuditTrailManager::getAnomalies(uint32_t maxEvents) const {
    if (m_anomalies.size() <= maxEvents) {
        return m_anomalies;
    }
    return std::vector<AnomalyEvent>(
        m_anomalies.end() - maxEvents,
        m_anomalies.end()
    );
}

bool AuditTrailManager::clearAuditTrail(const char* authToken) {
    // Verify auth token (simplified)
    if (!authToken || std::strlen(authToken) == 0) {
        return false;
    }

    m_featureStats.fill({});
    m_tierHistory.clear();
    m_anomalies.clear();
    m_totalEvents = 0;
    m_totalDenials = 0;
    m_totalGrants = 0;

    return persistToFile();
}

bool AuditTrailManager::exportToFile(const char* path, const char* format) {
    if (!path) return false;

    std::string content;
    if (std::strcmp(format, "json") == 0) {
        content = toJSON();
    } else {
        // Default to JSON
        content = toJSON();
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << content;
    return file.good();
}

bool AuditTrailManager::exportToSIEM(const SIEMExportConfig& config) {
    auto stats = getAllFeatureStats();
    std::vector<std::string> events;

    for (const auto& stat : stats) {
        std::string event = g_siemExporter.formatJSON(
            AuditEventType::SYSTEM_EVENT,
            static_cast<FeatureID>(stat.featureID),
            "audit_export"
        );
        events.push_back(event);
    }

    return g_siemExporter.batchExport(config, events);
}

std::string AuditTrailManager::generateComplianceSummary() const {
    std::ostringstream oss;

    oss << "=== License Audit Compliance Summary ===\n\n";
    oss << "Total Events: " << m_totalEvents << "\n";
    oss << "Total Grants: " << m_totalGrants << "\n";
    oss << "Total Denials: " << m_totalDenials << "\n";
    oss << "Denial Rate: " << std::fixed << std::setprecision(2) 
        << getDenialRate() * 100.0f << "%\n\n";

    oss << "Anomalies Detected: " << m_anomalies.size() << "\n";
    if (m_isAnomalous) {
        oss << "STATUS: ANOMALOUS STATE DETECTED\n";
    } else {
        oss << "STATUS: NORMAL\n";
    }

    oss << "\nFeature Statistics:\n";
    oss << std::string(50, '-') << "\n";

    for (const auto& stat : m_featureStats) {
        if (stat.grantCount + stat.denyCount > 0) {
            oss << "Feature " << stat.featureID << ": ";
            oss << stat.grantCount << " grants, ";
            oss << stat.denyCount << " denials\n";
        }
    }

    return oss.str();
}

std::string AuditTrailManager::toJSON(uint32_t maxEvents) const {
    std::ostringstream json;

    json << "{\n";
    json << "  \"timestamp\": " << std::time(nullptr) << ",\n";
    json << "  \"totalEvents\": " << m_totalEvents << ",\n";
    json << "  \"totalGrants\": " << m_totalGrants << ",\n";
    json << "  \"totalDenials\": " << m_totalDenials << ",\n";
    json << "  \"denialRate\": " << std::fixed << std::setprecision(4) << getDenialRate() << ",\n";
    json << "  \"isAnomalous\": " << (m_isAnomalous ? "true" : "false") << ",\n";
    json << "  \"anomalyCount\": " << m_anomalies.size() << ",\n";

    json << "  \"features\": [\n";
    bool first = true;
    for (const auto& stat : m_featureStats) {
        if (stat.grantCount + stat.denyCount > 0) {
            if (!first) json << ",\n";
            json << "    {\n";
            json << "      \"id\": " << stat.featureID << ",\n";
            json << "      \"grants\": " << stat.grantCount << ",\n";
            json << "      \"denials\": " << stat.denyCount << ",\n";
            json << "      \"denialRate\": " << std::fixed << std::setprecision(4) 
                 << ((stat.grantCount + stat.denyCount > 0) 
                     ? (float)stat.denyCount / (stat.grantCount + stat.denyCount)
                     : 0.0f) << "\n";
            json << "    }";
            first = false;
        }
    }
    json << "\n  ]\n";
    json << "}\n";

    return json.str();
}

uint32_t AuditTrailManager::getTotalEvents() const {
    return m_totalEvents;
}

uint32_t AuditTrailManager::getTotalDenials() const {
    return m_totalDenials;
}

uint32_t AuditTrailManager::getTotalGrants() const {
    return m_totalGrants;
}

uint32_t AuditTrailManager::getAnomalyCount() const {
    return m_anomalies.size();
}

float AuditTrailManager::getDenialRate(uint32_t windowSeconds) const {
    if (m_totalGrants + m_totalDenials == 0) return 0.0f;
    return (float)m_totalDenials / (m_totalGrants + m_totalDenials);
}

bool AuditTrailManager::isInAnomalousState() const {
    return m_isAnomalous;
}

void AuditTrailManager::analyzeAnomalities() {
    m_isAnomalous = false;

    if (detectExcessiveDenials()) {
        m_isAnomalous = true;
    }

    if (detectRapidTierChanges()) {
        m_isAnomalous = true;
    }

    if (detectFeaturePatterns()) {
        m_isAnomalous = true;
    }
}

bool AuditTrailManager::detectExcessiveDenials() {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    uint32_t denialCount = 0;

    // Count denials in last hour
    for (const auto& event : getAnomalies()) {
        if (event.timestamp > now - AUDIT_STATS_WINDOW_SECONDS) {
            denialCount++;
        }
    }

    return denialCount > AUDIT_ANOMALY_THRESHOLD;
}

bool AuditTrailManager::detectRapidTierChanges() {
    uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    uint32_t changeCount = 0;

    for (const auto& change : m_tierHistory) {
        if (change.timestamp > now - AUDIT_STATS_WINDOW_SECONDS) {
            changeCount++;
        }
    }

    return changeCount > 5;  // More than 5 tier changes in 1 hour
}

bool AuditTrailManager::detectFeaturePatterns() {
    // Check if any feature has abnormally high denial rate
    for (const auto& stat : m_featureStats) {
        float denialRate = (stat.grantCount + stat.denyCount > 0)
            ? (float)stat.denyCount / (stat.grantCount + stat.denyCount)
            : 0.0f;

        if (denialRate > 0.8f && stat.grantCount + stat.denyCount > 10) {
            return true;  // 80% denial rate on feature with 10+ accesses
        }
    }

    return false;
}

void AuditTrailManager::updateFeatureStats(FeatureID feature, bool granted) {
    if (feature >= 61) return;

    if (granted) {
        m_featureStats[feature].grantCount++;
    } else {
        m_featureStats[feature].denyCount++;
    }

    m_featureStats[feature].lastAccessTime = static_cast<uint32_t>(std::time(nullptr));
}

bool AuditTrailManager::persistToFile() {
    std::ostringstream oss;
    oss << toJSON();

    std::ofstream file(m_auditLogPath);
    if (!file.is_open()) return false;

    file << oss.str();
    return file.good();
}

bool AuditTrailManager::loadFromFile() {
    // TODO: Parse JSON from file and restore state
    return true;
}

// ============================================================================
// Anomaly Detector Implementation
// ============================================================================

AnomalyDetector::AnomalyDetector()
    : m_excessiveDenialThreshold(AUDIT_ANOMALY_THRESHOLD),
      m_rapidChangeThreshold(5),
      m_patternThreshold(0.8f),
      m_currentSeverity(0.0f),
      m_isAnomalous(false),
      m_baselineGrants(100),
      m_baselineDenials(5) {
    std::memset(m_description, 0, sizeof(m_description));
}

AnomalyDetector::~AnomalyDetector() {
}

bool AnomalyDetector::detectAnomaly(AuditEventType eventType, FeatureID feature, const char* caller) {
    m_currentSeverity = 0.0f;
    m_isAnomalous = false;
    std::memset(m_description, 0, sizeof(m_description));

    // Check for tampering events
    if (eventType == AuditEventType::TAMPERING_DETECTED) {
        m_isAnomalous = true;
        m_currentSeverity = 0.9f;
        std::strcpy_s(m_description, sizeof(m_description), "License tampering detected");
        return true;
    }

    // Check for excessive denials
    if (eventType == AuditEventType::FEATURE_DENIED) {
        m_currentSeverity = computeAnomalyScore();
        if (m_currentSeverity > 0.7f) {
            m_isAnomalous = true;
            std::strcpy_s(m_description, sizeof(m_description), "Excessive feature denials detected");
            return true;
        }
    }

    // Check for clock skew
    if (eventType == AuditEventType::CLOCK_SKEW_DETECTED) {
        m_isAnomalous = true;
        m_currentSeverity = 0.85f;
        std::strcpy_s(m_description, sizeof(m_description), "System clock tampering detected");
        return true;
    }

    return false;
}

float AnomalyDetector::getAnomalalySeverity() const {
    return m_currentSeverity;
}

const char* AnomalyDetector::getAnomalyDescription() const {
    return m_description;
}

void AnomalyDetector::reset() {
    m_currentSeverity = 0.0f;
    m_isAnomalous = false;
    std::memset(m_description, 0, sizeof(m_description));
}

void AnomalyDetector::setExcessiveDenialThreshold(uint32_t count) {
    m_excessiveDenialThreshold = count;
}

void AnomalyDetector::setRapidTierChangeThreshold(uint32_t changesInTime) {
    m_rapidChangeThreshold = changesInTime;
}

void AnomalyDetector::setFeaturePatternThreshold(float abnormalityScore) {
    m_patternThreshold = abnormalityScore;
}

void AnomalyDetector::getBaselineMetrics(uint32_t& avgGrants, uint32_t& avgDenials) const {
    avgGrants = m_baselineGrants;
    avgDenials = m_baselineDenials;
}

bool AnomalyDetector::isAnomalous() const {
    return m_isAnomalous;
}

float AnomalyDetector::computeAnomalyScore() {
    auto& lic = EnterpriseLicenseV2::Instance();
    
    float grantRate = (float)g_auditTrailManager.getTotalGrants() / 
                      std::max(1u, g_auditTrailManager.getTotalEvents());
    float denialRate = 1.0f - grantRate;

    // Score increases with higher denial rate
    return denialRate;
}

// ============================================================================
// SIEM Exporter Implementation
// ============================================================================

SIEMExporter::SIEMExporter() {
}

SIEMExporter::~SIEMExporter() {
}

std::string SIEMExporter::formatCEF(AuditEventType eventType, FeatureID feature, const char* caller) {
    std::ostringstream cef;

    cef << "CEF:0|RawrXD|LicenseAudit|1.0|";
    cef << (uint32_t)eventType << "|";
    cef << getEventTypeString(eventType) << "|";
    cef << getSeverityLevel(eventType) << "|";
    cef << "act=" << getEventTypeString(eventType) << " ";
    cef << "feature=" << (uint32_t)feature << " ";
    cef << "caller=" << (caller ? caller : "unknown");

    return cef.str();
}

std::string SIEMExporter::formatLEEF(AuditEventType eventType, FeatureID feature, const char* caller) {
    std::ostringstream leef;

    leef << "LEEF:2.0|RawrXD|LicenseAudit|1.0|";
    leef << (uint32_t)eventType << "|";
    leef << "evt=" << getEventTypeString(eventType) << "\t";
    leef << "feature=" << (uint32_t)feature << "\t";
    leef << "caller=" << (caller ? caller : "unknown") << "\t";
    leef << "sev=" << getSeverityLevel(eventType);

    return leef.str();
}

std::string SIEMExporter::formatJSON(AuditEventType eventType, FeatureID feature, const char* caller) {
    std::ostringstream json;

    json << "{\n";
    json << "  \"timestamp\": " << std::time(nullptr) << ",\n";
    json << "  \"eventType\": \"" << getEventTypeString(eventType) << "\",\n";
    json << "  \"feature\": " << (uint32_t)feature << ",\n";
    json << "  \"caller\": \"" << (caller ? caller : "unknown") << "\",\n";
    json << "  \"severity\": " << getSeverityLevel(eventType) << "\n";
    json << "}\n";

    return json.str();
}

std::string SIEMExporter::formatSyslog(AuditEventType eventType, FeatureID feature, const char* caller) {
    std::ostringstream syslog;

    time_t now = std::time(nullptr);
    struct tm* tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S", tm_info);

    syslog << timestamp << " RawrXD[" << GetCurrentProcessId() << "]: ";
    syslog << getEventTypeString(eventType) << " feature=" << (uint32_t)feature;
    syslog << " caller=" << (caller ? caller : "unknown");

    return syslog.str();
}

bool SIEMExporter::sendToSIEM(const SIEMExportConfig& config, const std::string& event) {
    // TODO: Implement actual SIEM transmission
    return true;
}

bool SIEMExporter::batchExport(const SIEMExportConfig& config, const std::vector<std::string>& events) {
    for (const auto& event : events) {
        if (!sendToSIEM(config, event)) {
            return false;
        }
    }
    return true;
}

const char* SIEMExporter::getEventTypeString(AuditEventType type) const {
    return RawrXD::License::getEventTypeString(type);
}

std::string SIEMExporter::urlEncode(const char* str) const {
    std::ostringstream encoded;
    while (str && *str) {
        if (isalnum(*str) || *str == '-' || *str == '_' || *str == '.') {
            encoded << *str;
        } else {
            encoded << '%' << std::hex << std::uppercase << (int)(unsigned char)*str;
        }
        ++str;
    }
    return encoded.str();
}

int SIEMExporter::getSeverityLevel(AuditEventType type) const {
    switch (type) {
        case AuditEventType::TAMPERING_DETECTED:
        case AuditEventType::UNAUTHORIZED_ACCESS:
            return 10;  // Critical
        case AuditEventType::LICENSE_REVOKED:
        case AuditEventType::LICENSE_EXPIRED:
        case AuditEventType::CLOCK_SKEW_DETECTED:
            return 8;   // High
        case AuditEventType::FEATURE_DENIED:
        case AuditEventType::GRACE_PERIOD_EXCEEDED:
            return 6;   // Medium
        default:
            return 4;   // Low
    }
}

// ============================================================================
// Persistent Audit Log Implementation
// ============================================================================

PersistentAuditLog::PersistentAuditLog()
    : m_currentSize(0),
      m_logRotationCount(0) {
}

PersistentAuditLog::~PersistentAuditLog() {
}

bool PersistentAuditLog::append(AuditEventType type, FeatureID feature, bool granted, const char* caller) {
    // Create directory if needed
    CreateDirectory("C:\\ProgramData\\RawrXD", nullptr);

    std::ofstream file(getLogPath(), std::ios::binary | std::ios::app);
    if (!file.is_open()) return false;

    // Write simple binary entry
    uint32_t timestamp = static_cast<uint32_t>(std::time(nullptr));
    uint8_t eventType = (uint8_t)type;
    uint8_t featureID = (uint8_t)feature;
    uint8_t grantedFlag = granted ? 1 : 0;

    file.write(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    file.write(reinterpret_cast<char*>(&eventType), sizeof(eventType));
    file.write(reinterpret_cast<char*>(&featureID), sizeof(featureID));
    file.write(reinterpret_cast<char*>(&grantedFlag), sizeof(grantedFlag));

    // Write caller name (null-terminated string, truncated to 64 bytes)
    char callerBuf[64] = {};
    if (caller) {
        std::strncpy_s(callerBuf, sizeof(callerBuf), caller, 63);
    }
    file.write(callerBuf, sizeof(callerBuf));

    m_currentSize += sizeof(timestamp) + sizeof(eventType) + sizeof(featureID) + 
                     sizeof(grantedFlag) + sizeof(callerBuf);

    // Rotate if needed
    if (m_currentSize > AUDIT_LOG_ROTATION_SIZE) {
        rotate();
    }

    return file.good();
}

bool PersistentAuditLog::read(std::vector<LicenseAuditEntry>& entries, uint32_t maxCount) {
    // TODO: Implement binary log reading
    return true;
}

bool PersistentAuditLog::rotate() {
    std::string archivePath = getArchivePath(m_logRotationCount++);
    return MoveFile(getLogPath(), archivePath.c_str()) != FALSE;
}

bool PersistentAuditLog::clear(const char* authToken) {
    if (!authToken || std::strlen(authToken) == 0) {
        return false;
    }

    m_currentSize = 0;
    return DeleteFile(getLogPath()) != FALSE;
}

uint32_t PersistentAuditLog::getLogSize() const {
    return m_currentSize;
}

std::vector<LicenseAuditEntry> PersistentAuditLog::getEntriesSince(uint32_t timestamp) const {
    std::vector<LicenseAuditEntry> entries;
    // TODO: Read from persistent log and filter by timestamp
    return entries;
}

const char* PersistentAuditLog::getLogPath() const {
    return "C:\\ProgramData\\RawrXD\\audit.log";
}

const char* PersistentAuditLog::getArchivePath(uint32_t sequence) const {
    static char buf[256];
    std::snprintf(buf, sizeof(buf), "C:\\ProgramData\\RawrXD\\audit.%u.log.gz", sequence);
    return buf;
}

}  // namespace RawrXD::License
