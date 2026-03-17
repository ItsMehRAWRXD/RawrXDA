// license_enforcement.cpp - Phase 3: Enterprise License Enforcement Implementation
// Audit-logged enforcement gates for all major subsystems

#include "license_enforcement.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <ctime>

// ============================================================================
// Singleton
// ============================================================================
LicenseEnforcementGate& LicenseEnforcementGate::getInstance() {
    static LicenseEnforcementGate instance;
    return instance;
}

LicenseEnforcementGate::LicenseEnforcementGate() = default;
LicenseEnforcementGate::~LicenseEnforcementGate() = default;

LicenseResult LicenseEnforcementGate::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load()) {
        return LicenseResult::ok("Enforcement gate already initialized");
    }
    m_initialized.store(true);
    return LicenseResult::ok("Enforcement gate initialized");
}

uint64_t LicenseEnforcementGate::currentTimeMs() const {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count()
    );
}

// ============================================================================
// Primary Gate
// ============================================================================
bool LicenseEnforcementGate::checkAccess(EnterpriseFeature feature,
                                          const std::string& callerContext) {
    m_totalChecks++;

    // Check runtime feature flags first (includes license + config + admin)
    auto& flags = FeatureFlagRuntime::getInstance();
    bool allowed = flags.isEnabled(feature);

    if (!allowed && !m_strictMode) {
        // Warn-only mode: allow but log as violation
        logEvent(feature, true, callerContext, "WARN: Feature blocked but enforcement is non-strict");
        m_totalAllowed++;
        return true;
    }

    if (allowed) {
        m_totalAllowed++;
        logEvent(feature, true, callerContext, "Access granted");
    } else {
        m_totalBlocked++;
        auto& mgr = EnterpriseLicenseManager::getInstance();
        LicenseTier required = mgr.getRequiredTier(feature);
        const char* reason = "License tier insufficient";
        if (flags.isDisabledByConfig(feature)) {
            reason = "Disabled by configuration";
        } else if (!flags.getFlag(feature).enabledByAdmin) {
            reason = "Disabled by administrator";
        }
        logEvent(feature, false, callerContext, reason);
    }

    return allowed;
}

LicenseResult LicenseEnforcementGate::gateFeature(EnterpriseFeature feature,
                                                    const std::string& callerContext) {
    if (checkAccess(feature, callerContext)) {
        return LicenseResult::ok("Access granted");
    }

    auto& mgr = EnterpriseLicenseManager::getInstance();
    std::string msg = std::string("Feature locked: ") + EnterpriseFeatureToString(feature) +
                      " (requires " + LicenseTierToString(mgr.getRequiredTier(feature)) +
                      ", current: " + LicenseTierToString(mgr.getCurrentTier()) + ")";
    return LicenseResult::error(msg.c_str(), static_cast<int>(feature));
}

// ============================================================================
// Audit Logging
// ============================================================================
void LicenseEnforcementGate::logEvent(EnterpriseFeature feature, bool allowed,
                                       const std::string& callerContext,
                                       const char* reason) {
    if (!m_auditEnabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto& mgr = EnterpriseLicenseManager::getInstance();

    EnforcementEvent event;
    event.feature = feature;
    event.allowed = allowed;
    event.requiredTier = mgr.getRequiredTier(feature);
    event.currentTier = mgr.getCurrentTier();
    event.callerContext = callerContext;
    event.timestampMs = currentTimeMs();
    event.reason = reason;

    m_auditLog.push_back(event);

    // Trim to max size
    if (m_auditLog.size() > m_maxAuditEntries) {
        m_auditLog.erase(m_auditLog.begin(),
                         m_auditLog.begin() + (m_auditLog.size() - m_maxAuditEntries));
    }
}

// ============================================================================
// Subsystem-Specific Gates
// ============================================================================

// ── Hotpatch Subsystem ──
LicenseResult LicenseEnforcementGate::gateMemoryHotpatch(const std::string& patchName) {
    return gateFeature(EnterpriseFeature::MemoryHotpatching,
                       "MemoryHotpatch:" + patchName);
}

LicenseResult LicenseEnforcementGate::gateByteLevelHotpatch(const std::string& patchName) {
    return gateFeature(EnterpriseFeature::ByteLevelHotpatching,
                       "ByteLevelHotpatch:" + patchName);
}

LicenseResult LicenseEnforcementGate::gateServerHotpatch(const std::string& patchName) {
    return gateFeature(EnterpriseFeature::ServerHotpatching,
                       "ServerHotpatch:" + patchName);
}

LicenseResult LicenseEnforcementGate::gateUnifiedHotpatch(const std::string& operation) {
    return gateFeature(EnterpriseFeature::UnifiedHotpatchManager,
                       "UnifiedHotpatch:" + operation);
}

// ── Agent Subsystem ──
LicenseResult LicenseEnforcementGate::gateAgenticKernel(const std::string& operation) {
    return gateFeature(EnterpriseFeature::AgenticKernel,
                       "AgenticKernel:" + operation);
}

LicenseResult LicenseEnforcementGate::gateAgentOrchestrator(const std::string& taskType) {
    return gateFeature(EnterpriseFeature::AgentOrchestrator,
                       "AgentOrchestrator:" + taskType);
}

LicenseResult LicenseEnforcementGate::gateMCPServer(const std::string& endpoint) {
    return gateFeature(EnterpriseFeature::MCPServerHost,
                       "MCPServer:" + endpoint);
}

LicenseResult LicenseEnforcementGate::gateToolExecution(const std::string& toolName) {
    return gateFeature(EnterpriseFeature::ToolExecutionEngine,
                       "ToolExecution:" + toolName);
}

LicenseResult LicenseEnforcementGate::gateSelfCorrection(const std::string& errorType) {
    return gateFeature(EnterpriseFeature::AgenticSelfCorrector,
                       "SelfCorrection:" + errorType);
}

// ── Inference Subsystem ──
LicenseResult LicenseEnforcementGate::gateDualEngine(const std::string& operation) {
    return gateFeature(EnterpriseFeature::DualEngineInference,
                       "DualEngine:" + operation);
}

LicenseResult LicenseEnforcementGate::gate800BModel(const std::string& modelPath) {
    return gateFeature(EnterpriseFeature::Engine800B,
                       "800BModel:" + modelPath);
}

LicenseResult LicenseEnforcementGate::gateStreamingInference(const std::string& modelName) {
    return gateFeature(EnterpriseFeature::StreamingInference,
                       "StreamingInference:" + modelName);
}

// ── IDE Subsystem ──
LicenseResult LicenseEnforcementGate::gateSchematicStudio() {
    return gateFeature(EnterpriseFeature::SchematicStudio, "SchematicStudio");
}

LicenseResult LicenseEnforcementGate::gateWiringOracle() {
    return gateFeature(EnterpriseFeature::WiringOracle, "WiringOracle");
}

LicenseResult LicenseEnforcementGate::gateObservability() {
    return gateFeature(EnterpriseFeature::ObservabilityDashboard, "Observability");
}

LicenseResult LicenseEnforcementGate::gateCodeSigning(const std::string& binaryPath) {
    return gateFeature(EnterpriseFeature::CodeSigner,
                       "CodeSigner:" + binaryPath);
}

// ============================================================================
// Audit Queries
// ============================================================================
std::vector<EnforcementEvent> LicenseEnforcementGate::getRecentEvents(size_t maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_auditLog.size() <= maxCount) {
        return m_auditLog;
    }
    return std::vector<EnforcementEvent>(
        m_auditLog.end() - static_cast<ptrdiff_t>(maxCount),
        m_auditLog.end()
    );
}

void LicenseEnforcementGate::clearAuditLog() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_auditLog.clear();
    m_totalChecks.store(0);
    m_totalBlocked.store(0);
    m_totalAllowed.store(0);
}

void LicenseEnforcementGate::setEnforcementMode(bool strict) {
    m_strictMode = strict;
}

// ============================================================================
// Enforcement Report
// ============================================================================
std::string LicenseEnforcementGate::generateEnforcementReport() const {
    std::ostringstream rpt;

    rpt << "================================================================\n";
    rpt << "  License Enforcement Audit Report\n";
    rpt << "================================================================\n\n";

    rpt << "ENFORCEMENT STATISTICS\n";
    rpt << "  Total Checks:  " << m_totalChecks.load() << "\n";
    rpt << "  Allowed:       " << m_totalAllowed.load() << "\n";
    rpt << "  Blocked:       " << m_totalBlocked.load() << "\n";
    rpt << "  Mode:          " << (m_strictMode ? "STRICT" : "WARN-ONLY") << "\n";
    rpt << "  Audit Logging: " << (m_auditEnabled ? "ON" : "OFF") << "\n\n";

    auto events = getRecentEvents(50);
    if (!events.empty()) {
        rpt << "RECENT ENFORCEMENT EVENTS (last " << events.size() << "):\n";
        rpt << std::left
            << std::setw(35) << "Feature"
            << std::setw(10) << "Result"
            << std::setw(14) << "Required"
            << std::setw(14) << "Current"
            << "Context\n";
        rpt << std::string(90, '-') << "\n";

        for (const auto& e : events) {
            rpt << std::left
                << std::setw(35) << EnterpriseFeatureToString(e.feature)
                << std::setw(10) << (e.allowed ? "ALLOW" : "BLOCK")
                << std::setw(14) << LicenseTierToString(e.requiredTier)
                << std::setw(14) << LicenseTierToString(e.currentTier)
                << e.callerContext << "\n";
        }
    }

    return rpt.str();
}
