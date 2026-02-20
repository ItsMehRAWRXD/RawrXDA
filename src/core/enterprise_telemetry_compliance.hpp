// enterprise_telemetry_compliance.hpp — Phase 17: Enterprise Telemetry & Compliance
// OpenTelemetry-compatible distributed tracing, tamper-evident audit trails,
// compliance policy engine, license metering, and GDPR/SOX export facilities.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
// Rule: No std::function — raw function pointers only.
#pragma once
#ifndef RAWRXD_ENTERPRISE_TELEMETRY_COMPLIANCE_HPP
#define RAWRXD_ENTERPRISE_TELEMETRY_COMPLIANCE_HPP

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <chrono>
#include <array>

// ============================================================================
// Enums
// ============================================================================

enum class TelemetryLevel : uint8_t {
    Off       = 0,
    Critical  = 1,
    Error     = 2,
    Warning   = 3,
    Info      = 4,
    Debug     = 5,
    Trace     = 6,
};

enum class SpanKind : uint8_t {
    Internal = 0,
    Server   = 1,
    Client   = 2,
    Producer = 3,
    Consumer = 4,
};

enum class SpanStatus : uint8_t {
    Unset = 0,
    Ok    = 1,
    Error = 2,
};

enum class ComplianceStandard : uint8_t {
    None      = 0,
    GDPR      = 1,
    SOX       = 2,
    HIPAA     = 3,
    PCI_DSS   = 4,
    ISO27001  = 5,
    FedRAMP   = 6,
};

enum class AuditEventType : uint8_t {
    SystemStart       =  0,
    SystemStop        =  1,
    UserLogin         =  2,
    UserLogout        =  3,
    ModelLoad         =  4,
    ModelUnload       =  5,
    InferenceRequest  =  6,
    InferenceComplete =  7,
    PatchApplied      =  8,
    PatchRolledBack   =  9,
    ConfigChange      = 10,
    AccessDenied      = 11,
    DataExport        = 12,
    DataDeletion      = 13,
    PolicyViolation   = 14,
    LicenseCheck      = 15,
    SecurityAlert     = 16,
    ComplianceReport  = 17,
};

enum class LicenseTier : uint8_t {
    Community   = 0,
    Professional = 1,
    Enterprise  = 2,
    OEM         = 3,
};

enum class MetricType : uint8_t {
    Counter   = 0,
    Gauge     = 1,
    Histogram = 2,
    Summary   = 3,
};

// ============================================================================
// Structs
// ============================================================================

struct TraceId {
    uint64_t high = 0;
    uint64_t low  = 0;
    bool isValid() const { return high != 0 || low != 0; }
};

struct SpanId {
    uint64_t value = 0;
    bool isValid() const { return value != 0; }
};

struct SpanAttribute {
    std::string key;
    std::string value;
};

struct TelemetrySpan {
    TraceId       traceId;
    SpanId        spanId;
    SpanId        parentSpanId;
    std::string   name;
    SpanKind      kind = SpanKind::Internal;
    SpanStatus    status = SpanStatus::Unset;
    std::string   statusMessage;
    uint64_t      startTimeUs = 0;   // microseconds since epoch
    uint64_t      endTimeUs   = 0;
    uint64_t      durationUs  = 0;
    std::vector<SpanAttribute>  attributes;
    std::vector<std::string>    events;

    static constexpr uint32_t MAX_ATTRIBUTES = 128;
    static constexpr uint32_t MAX_EVENTS     = 256;
};

struct AuditEntry {
    uint64_t         entryId   = 0;
    AuditEventType   eventType = AuditEventType::SystemStart;
    uint64_t         timestampUs = 0;
    std::string      actor;          // user / system / agent
    std::string      resource;       // what was accessed
    std::string      action;         // what was done
    std::string      detail;         // additional context
    std::string      ipAddress;
    uint64_t         sessionId = 0;
    uint8_t          severity  = 0;  // 0-10
    bool             tamperSealed = false;
    uint64_t         previousHash = 0;  // chained hash for tamper detection
    uint64_t         entryHash    = 0;  // hash of this entry
};

struct CompliancePolicy {
    uint32_t            policyId     = 0;
    std::string         name;
    ComplianceStandard  standard     = ComplianceStandard::None;
    std::string         description;
    bool                enforced     = true;
    bool                enabled      = true;

    // Validation: raw function pointer
    bool (*validator)(const AuditEntry* entry, void* userData) = nullptr;
    void* validatorData = nullptr;
};

struct ComplianceViolation {
    uint32_t  violationId = 0;
    uint32_t  policyId    = 0;
    uint64_t  auditEntryId = 0;
    std::string description;
    uint64_t  detectedAt = 0;
    bool      resolved   = false;
    std::string resolution;
};

struct LicenseInfo {
    std::string   licenseKey;
    LicenseTier   tier          = LicenseTier::Community;
    std::string   organizationId;
    std::string   organizationName;
    uint64_t      issuedAt      = 0;
    uint64_t      expiresAt     = 0;
    uint32_t      maxUsers      = 0;
    uint32_t      maxModels     = 0;
    uint64_t      maxInferences = 0;
    bool          valid         = false;
    std::vector<std::string> features;
};

struct UsageMeter {
    std::atomic<uint64_t> inferenceCount{0};
    std::atomic<uint64_t> tokensProcessed{0};
    std::atomic<uint64_t> modelsLoaded{0};
    std::atomic<uint64_t> patchesApplied{0};
    std::atomic<uint64_t> apiCallCount{0};
    std::atomic<uint64_t> bytesTransferred{0};
    std::atomic<uint64_t> activeUsers{0};
    uint64_t              periodStartUs = 0;
    uint64_t              periodEndUs   = 0;

    UsageMeter() = default;
    UsageMeter(UsageMeter&& other) noexcept
        : inferenceCount(other.inferenceCount.load())
        , tokensProcessed(other.tokensProcessed.load())
        , modelsLoaded(other.modelsLoaded.load())
        , patchesApplied(other.patchesApplied.load())
        , apiCallCount(other.apiCallCount.load())
        , bytesTransferred(other.bytesTransferred.load())
        , activeUsers(other.activeUsers.load())
        , periodStartUs(other.periodStartUs)
        , periodEndUs(other.periodEndUs) {}
    UsageMeter& operator=(UsageMeter&& other) noexcept {
        inferenceCount.store(other.inferenceCount.load());
        tokensProcessed.store(other.tokensProcessed.load());
        modelsLoaded.store(other.modelsLoaded.load());
        patchesApplied.store(other.patchesApplied.load());
        apiCallCount.store(other.apiCallCount.load());
        bytesTransferred.store(other.bytesTransferred.load());
        activeUsers.store(other.activeUsers.load());
        periodStartUs = other.periodStartUs;
        periodEndUs   = other.periodEndUs;
        return *this;
    }
    UsageMeter(const UsageMeter&) = delete;
    UsageMeter& operator=(const UsageMeter&) = delete;
};

struct TelemetryMetric {
    std::string   name;
    std::string   description;
    std::string   unit;
    MetricType    type = MetricType::Counter;
    double        value = 0.0;
    uint64_t      timestampUs = 0;
    std::vector<SpanAttribute> labels;
};

struct TelemetryStats {
    std::atomic<uint64_t> totalSpans{0};
    std::atomic<uint64_t> activeSpans{0};
    std::atomic<uint64_t> completedSpans{0};
    std::atomic<uint64_t> droppedSpans{0};
    std::atomic<uint64_t> auditEntries{0};
    std::atomic<uint64_t> policyViolations{0};
    std::atomic<uint64_t> licenseChecks{0};
    std::atomic<uint64_t> metricsRecorded{0};
    std::atomic<uint64_t> exportsCompleted{0};
};

// ============================================================================
// Callback Types (raw function pointers — no std::function)
// ============================================================================

using SpanCompleteCallback    = void(*)(const TelemetrySpan* span, void* userData);
using AuditEventCallback      = void(*)(const AuditEntry* entry, void* userData);
using ViolationCallback       = void(*)(const ComplianceViolation* v, void* userData);
using MetricFlushCallback     = void(*)(const TelemetryMetric* metrics, uint32_t count,
                                        void* userData);
using LicenseExpiryCallback   = void(*)(const LicenseInfo* info, void* userData);

// ============================================================================
// EnterpriseTelemetryCompliance — Singleton
// ============================================================================

class EnterpriseTelemetryCompliance {
public:
    static EnterpriseTelemetryCompliance& instance();

    // ----- Tracing / Spans -----
    SpanId   startSpan(const std::string& name, SpanKind kind,
                       TraceId traceId = {}, SpanId parentSpan = {});
    PatchResult endSpan(SpanId spanId, SpanStatus status = SpanStatus::Ok,
                        const char* message = nullptr);
    PatchResult addSpanAttribute(SpanId spanId, const std::string& key,
                                 const std::string& value);
    PatchResult addSpanEvent(SpanId spanId, const std::string& eventName);
    const TelemetrySpan* getSpan(SpanId spanId) const;
    std::vector<TelemetrySpan> getTraceSpans(TraceId traceId) const;
    TraceId generateTraceId();
    SpanId  generateSpanId();

    // ----- Audit Trail -----
    uint64_t    recordAudit(AuditEventType type, const std::string& actor,
                            const std::string& resource, const std::string& action,
                            const std::string& detail = "");
    std::vector<AuditEntry> queryAudit(AuditEventType type, uint64_t sinceUs = 0,
                                       uint32_t maxResults = 100) const;
    std::vector<AuditEntry> queryAuditByActor(const std::string& actor,
                                              uint32_t maxResults = 100) const;
    PatchResult verifyAuditIntegrity() const;
    uint64_t    getAuditCount() const;

    // ----- Compliance Policies -----
    uint32_t    addPolicy(const CompliancePolicy& policy);
    PatchResult removePolicy(uint32_t policyId);
    PatchResult enablePolicy(uint32_t policyId, bool enable);
    PatchResult checkCompliance(const AuditEntry& entry);
    std::vector<ComplianceViolation> getViolations(uint32_t policyId = 0,
                                                    bool unresolvedOnly = true) const;
    PatchResult resolveViolation(uint32_t violationId, const std::string& resolution);
    PatchResult generateComplianceReport(const char* outputPath,
                                         ComplianceStandard standard) const;

    // ----- License / Metering -----
    PatchResult setLicense(const LicenseInfo& license);
    PatchResult validateLicense() const;
    LicenseTier getCurrentTier() const;
    bool        isFeatureAllowed(const std::string& feature) const;
    PatchResult checkUsageLimit(const std::string& resource) const;
    void        recordUsage(const std::string& metric, uint64_t amount = 1);
    UsageMeter  getUsageMeter() const;
    PatchResult resetUsagePeriod();

    // ----- Metrics -----
    PatchResult recordMetric(const TelemetryMetric& metric);
    PatchResult incrementCounter(const std::string& name, double amount = 1.0);
    PatchResult setGauge(const std::string& name, double value);
    PatchResult recordHistogram(const std::string& name, double value);
    std::vector<TelemetryMetric> getMetrics(const std::string& nameFilter = "") const;
    PatchResult flushMetrics();

    // ----- Export / GDPR -----
    PatchResult exportAuditLog(const char* outputPath, uint64_t sinceUs = 0) const;
    PatchResult exportUserData(const char* outputPath, const std::string& userId) const;
    PatchResult deleteUserData(const std::string& userId);
    PatchResult exportTelemetryOTLP(const char* outputPath) const;  // OpenTelemetry format

    // ----- Configuration -----
    void setTelemetryLevel(TelemetryLevel level);
    TelemetryLevel getTelemetryLevel() const;
    void setMaxAuditEntries(uint32_t max);
    void setMaxSpans(uint32_t max);

    // ----- Callbacks -----
    void setSpanCompleteCallback(SpanCompleteCallback cb, void* userData);
    void setAuditEventCallback(AuditEventCallback cb, void* userData);
    void setViolationCallback(ViolationCallback cb, void* userData);
    void setMetricFlushCallback(MetricFlushCallback cb, void* userData);
    void setLicenseExpiryCallback(LicenseExpiryCallback cb, void* userData);

    // ----- Stats -----
    const TelemetryStats& stats() const { return m_stats; }
    void resetStats();

private:
    EnterpriseTelemetryCompliance();
    ~EnterpriseTelemetryCompliance();
    EnterpriseTelemetryCompliance(const EnterpriseTelemetryCompliance&)            = delete;
    EnterpriseTelemetryCompliance& operator=(const EnterpriseTelemetryCompliance&) = delete;

    // Tamper-evident chain
    uint64_t computeAuditHash(const AuditEntry& entry) const;
    uint64_t lastAuditHash() const;

    // Internal
    void pruneSpans();
    void pruneAudit();
    uint64_t nowUs() const;

    mutable std::mutex m_mutex;

    // Tracing
    std::unordered_map<uint64_t, TelemetrySpan>      m_spans;
    std::unordered_map<uint64_t, std::vector<SpanId>> m_traceIndex;  // traceId.low -> spans
    std::atomic<uint64_t> m_nextSpanId{1};
    std::atomic<uint64_t> m_nextTraceHigh{1};
    uint32_t m_maxSpans = 50000;

    // Audit
    std::deque<AuditEntry>  m_auditLog;
    std::atomic<uint64_t>   m_nextAuditId{1};
    uint32_t                m_maxAuditEntries = 100000;

    // Compliance
    std::unordered_map<uint32_t, CompliancePolicy>    m_policies;
    std::vector<ComplianceViolation>                  m_violations;
    std::atomic<uint32_t>   m_nextPolicyId{1};
    std::atomic<uint32_t>   m_nextViolationId{1};

    // License
    LicenseInfo m_license;
    mutable UsageMeter  m_usage;

    // Metrics
    std::deque<TelemetryMetric>     m_metrics;
    std::unordered_map<std::string, double> m_counters;
    std::unordered_map<std::string, double> m_gauges;
    static constexpr size_t MAX_METRICS = 100000;

    // Config
    std::atomic<TelemetryLevel> m_level{TelemetryLevel::Info};

    // Callbacks
    SpanCompleteCallback  m_spanCb     = nullptr;
    void*                 m_spanCbData = nullptr;
    AuditEventCallback    m_auditCb     = nullptr;
    void*                 m_auditCbData = nullptr;
    ViolationCallback     m_violationCb     = nullptr;
    void*                 m_violationCbData = nullptr;
    MetricFlushCallback   m_metricFlushCb     = nullptr;
    void*                 m_metricFlushCbData = nullptr;
    LicenseExpiryCallback m_licenseExpiryCb     = nullptr;
    void*                 m_licenseExpiryCbData = nullptr;

    // Stats
    mutable TelemetryStats m_stats;

    static constexpr size_t MAX_AUDIT_DEFAULT = 100000;
    static constexpr size_t MAX_SPANS_DEFAULT = 50000;
};

#endif // RAWRXD_ENTERPRISE_TELEMETRY_COMPLIANCE_HPP
