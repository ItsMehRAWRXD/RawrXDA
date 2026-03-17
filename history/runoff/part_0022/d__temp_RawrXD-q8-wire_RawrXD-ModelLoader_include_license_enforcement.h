// license_enforcement.h - Phase 3: Enterprise License Enforcement Gate
// Enforcement points for all major subsystems
// Provides decorator/guard patterns for feature-gated code paths
// No exceptions. Thread-safe. Audit logging.

#pragma once

#include "enterprise_license.h"
#include "feature_flags_runtime.h"
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <chrono>
#include <cstdint>

// ============================================================================
// Enforcement Event — audit trail for license checks
// ============================================================================
struct EnforcementEvent {
    EnterpriseFeature feature;
    bool allowed;
    LicenseTier requiredTier;
    LicenseTier currentTier;
    std::string callerContext;     // Function/module that triggered the check
    uint64_t timestampMs;          // Milliseconds since epoch
    const char* reason;            // Why it was allowed/denied
};

// ============================================================================
// License Enforcement Gate
// ============================================================================
class LicenseEnforcementGate {
public:
    static LicenseEnforcementGate& getInstance();

    // Initialize enforcement system
    LicenseResult initialize();

    // ── Primary Gate Function ──
    // Returns true if feature is allowed, false if blocked
    // Logs the access attempt for audit
    bool checkAccess(EnterpriseFeature feature, const std::string& callerContext = "");

    // Gate with automatic LicenseResult return
    LicenseResult gateFeature(EnterpriseFeature feature, const std::string& callerContext = "");

    // ── Subsystem-Specific Gates ──
    // Each returns LicenseResult for consistent error handling

    // Hotpatch subsystem
    LicenseResult gateMemoryHotpatch(const std::string& patchName);
    LicenseResult gateByteLevelHotpatch(const std::string& patchName);
    LicenseResult gateServerHotpatch(const std::string& patchName);
    LicenseResult gateUnifiedHotpatch(const std::string& operation);

    // Agent subsystem
    LicenseResult gateAgenticKernel(const std::string& operation);
    LicenseResult gateAgentOrchestrator(const std::string& taskType);
    LicenseResult gateMCPServer(const std::string& endpoint);
    LicenseResult gateToolExecution(const std::string& toolName);
    LicenseResult gateSelfCorrection(const std::string& errorType);

    // Inference subsystem
    LicenseResult gateDualEngine(const std::string& operation);
    LicenseResult gate800BModel(const std::string& modelPath);
    LicenseResult gateStreamingInference(const std::string& modelName);

    // IDE subsystem
    LicenseResult gateSchematicStudio();
    LicenseResult gateWiringOracle();
    LicenseResult gateObservability();
    LicenseResult gateCodeSigning(const std::string& binaryPath);

    // ── Audit ──
    std::vector<EnforcementEvent> getRecentEvents(size_t maxCount = 100) const;
    std::string generateEnforcementReport() const;
    uint64_t getTotalChecks() const { return m_totalChecks; }
    uint64_t getTotalBlocked() const { return m_totalBlocked; }
    uint64_t getTotalAllowed() const { return m_totalAllowed; }
    void clearAuditLog();

    // ── Configuration ──
    void setAuditLogging(bool enabled) { m_auditEnabled = enabled; }
    void setMaxAuditEntries(size_t max) { m_maxAuditEntries = max; }
    void setEnforcementMode(bool strict); // strict=block, !strict=warn-only

    ~LicenseEnforcementGate();
    LicenseEnforcementGate(const LicenseEnforcementGate&) = delete;
    LicenseEnforcementGate& operator=(const LicenseEnforcementGate&) = delete;

private:
    LicenseEnforcementGate();
    void logEvent(EnterpriseFeature feature, bool allowed,
                  const std::string& callerContext, const char* reason);
    uint64_t currentTimeMs() const;

    mutable std::mutex m_mutex;
    std::vector<EnforcementEvent> m_auditLog;
    std::atomic<uint64_t> m_totalChecks{0};
    std::atomic<uint64_t> m_totalBlocked{0};
    std::atomic<uint64_t> m_totalAllowed{0};
    std::atomic<bool> m_initialized{false};
    bool m_auditEnabled = true;
    bool m_strictMode = true;
    size_t m_maxAuditEntries = 10000;
};

// ============================================================================
// Convenience Macros for Enforcement
// ============================================================================

// Check and return LicenseResult if blocked
#define ENFORCE_FEATURE(feature) \
    do { \
        auto __r = LicenseEnforcementGate::getInstance().gateFeature( \
            EnterpriseFeature::feature, __FUNCTION__); \
        if (!__r.success) return __r; \
    } while(0)

// Check and return custom value if blocked
#define ENFORCE_FEATURE_OR(feature, retval) \
    do { \
        if (!LicenseEnforcementGate::getInstance().checkAccess( \
            EnterpriseFeature::feature, __FUNCTION__)) { \
            return retval; \
        } \
    } while(0)

// Check and execute block if blocked
#define ENFORCE_FEATURE_ELSE(feature, block) \
    do { \
        if (!LicenseEnforcementGate::getInstance().checkAccess( \
            EnterpriseFeature::feature, __FUNCTION__)) { \
            block; \
        } \
    } while(0)
