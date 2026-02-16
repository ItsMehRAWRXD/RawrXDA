// ============================================================================
// license_enforcement.cpp — Phase 3: Subsystem Enforcement Gates + Audit Trail
// ============================================================================
// Implements the LicenseEnforcer singleton with 10 subsystem gates,
// 4-layer feature flag integration, enforcement policy modes, and
// comprehensive audit trail logging.
//
// PATTERN:   No exceptions. Returns EnforcementResult/bool.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "license_enforcement.h"
#include "feature_flags_runtime.h"
#include "rawrxd_telemetry_exports.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace RawrXD::Enforce {

// ============================================================================
// HasBackingImplementation — 21 features are gated but have no backing (see docs/ENTERPRISE_FEATURES_WITHOUT_BACKING.md)
// ============================================================================
bool HasBackingImplementation(License::FeatureID id) {
    using F = License::FeatureID;
    switch (id) {
        case F::AdvancedSettingsPanel:   // 13
        case F::ModelComparison:         // 18
        case F::BatchProcessing:        // 19
        case F::PromptLibrary:          // 24
        case F::ExportImportSessions:   // 25
        case F::SchematicStudioIDE:     // 33
        case F::WiringOracleDebug:      // 34
        case F::ModelSharding:          // 37
        case F::TensorParallel:         // 38
        case F::PipelineParallel:       // 39
        case F::ContinuousBatching:     // 40
        case F::GPTQQuantization:       // 41
        case F::AWQQuantization:        // 42
        case F::CustomQuantSchemes:     // 43
        case F::DynamicBatchSizing:     // 45
        case F::PriorityQueuing:        // 46
        case F::RateLimitingEngine:     // 47
        case F::APIKeyManagement:       // 49
        case F::ModelSigningVerify:     // 50
        case F::RawrTunerIDE:           // 54
        case F::CustomSecurityPolicies: // 58
            return false;
        default:
            return true;
    }
}

// ============================================================================
// Singleton
// ============================================================================
LicenseEnforcer& LicenseEnforcer::Instance() {
    static LicenseEnforcer s_instance;
    return s_instance;
}

// ============================================================================
// Initialize
// ============================================================================
bool LicenseEnforcer::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;

    initGates();
    m_policy = EnforcementPolicy::Strict;
    m_eventCount = 0;
    m_eventHead = 0;
    m_denialCb = nullptr;
    m_initialized = true;

    recordEvent(SubsystemID::Inference, License::FeatureID::BasicGGUFLoading,
               true, "initialize", "Enforcement system initialized");

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void LicenseEnforcer::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
}

// ============================================================================
// Init Gates — configure subsystem→feature mappings
// ============================================================================
void LicenseEnforcer::initGates() {
    auto& g = m_gates;

    g[0] = { SubsystemID::Inference,       "Inference Engine",    true, 0, 0, License::FeatureID::CPUInference };
    g[1] = { SubsystemID::Hotpatch,        "Hotpatch System",     true, 0, 0, License::FeatureID::MemoryHotpatching };
    g[2] = { SubsystemID::Agentic,         "Agentic Framework",   true, 0, 0, License::FeatureID::AgenticFailureDetect };
    g[3] = { SubsystemID::DualEngine,      "800B Dual-Engine",    true, 0, 0, License::FeatureID::DualEngine800B };
    g[4] = { SubsystemID::Quantization,    "Quantization Suite",  true, 0, 0, License::FeatureID::Q5Q8F16Quantization };
    g[5] = { SubsystemID::ModelManagement, "Model Management",    true, 0, 0, License::FeatureID::MultiModelLoading };
    g[6] = { SubsystemID::Security,        "Security & Auth",     true, 0, 0, License::FeatureID::AuditLogging };
    g[7] = { SubsystemID::Networking,      "Network & Server",    true, 0, 0, License::FeatureID::ServerHotpatching };
    g[8] = { SubsystemID::Observability,   "Observability",       true, 0, 0, License::FeatureID::ObservabilityDashboard };
    g[9] = { SubsystemID::Sovereign,       "Sovereign",           true, 0, 0, License::FeatureID::AirGappedDeploy };
}

// ============================================================================
// Policy
// ============================================================================
void LicenseEnforcer::setPolicy(EnforcementPolicy policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
}

EnforcementPolicy LicenseEnforcer::getPolicy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policy;
}

// ============================================================================
// Feature-to-Subsystem Mapping
// ============================================================================
SubsystemID LicenseEnforcer::featureToSubsystem(License::FeatureID id) const {
    uint32_t idx = static_cast<uint32_t>(id);

    // Community features → Inference
    if (idx <= 5) return SubsystemID::Inference;

    // Pro: Hotpatching (6–9)
    if (idx >= 6 && idx <= 9) return SubsystemID::Hotpatch;

    // Pro: Quantization (10)
    if (idx == 10) return SubsystemID::Quantization;

    // Pro: GPU backends (12, 26)
    if (idx == 12 || idx == 26) return SubsystemID::Inference;

    // Pro: Misc features (11, 13–25)
    if (idx >= 11 && idx <= 25) return SubsystemID::ModelManagement;

    // Enterprise: Dual Engine (27)
    if (idx == 27) return SubsystemID::DualEngine;

    // Enterprise: Agentic (28–30)
    if (idx >= 28 && idx <= 30) return SubsystemID::Agentic;

    // Enterprise: Proxy/Server Patching (31–32)
    if (idx >= 31 && idx <= 32) return SubsystemID::Networking;

    // Enterprise: IDE Features (33–34)
    if (idx >= 33 && idx <= 34) return SubsystemID::Observability;

    // Enterprise: GPU/Performance (35–47. 44 is Multi-GPU)
    if (idx >= 35 && idx <= 47) return SubsystemID::Inference;

    // Enterprise: Security/Audit (48–52)
    if (idx >= 48 && idx <= 52) return SubsystemID::Security;

    // Enterprise: Acceleration/Tukner (53–54)
    if (idx == 53) return SubsystemID::Inference;
    if (idx == 54) return SubsystemID::ModelManagement;

    // Sovereign (55–65)
    if (idx >= 55) return SubsystemID::Sovereign;

    return SubsystemID::Inference;
}

// ============================================================================
// Gate Checks
// ============================================================================
EnforcementResult LicenseEnforcer::checkGate(License::FeatureID feature,
                                              const char* caller) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return EnforcementResult::deny(
            SubsystemID::Inference, feature,
            License::LicenseTierV2::Community,
            License::LicenseTierV2::Community,
            "Enforcement system not initialized");
    }

    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) {
        return EnforcementResult::deny(
            SubsystemID::Inference, feature,
            License::LicenseTierV2::Sovereign,
            License::LicenseTierV2::Community,
            "Invalid feature ID");
    }

    SubsystemID sub = featureToSubsystem(feature);
    uint32_t gateIdx = static_cast<uint32_t>(sub);

    // Check if gate is enabled
    if (gateIdx < static_cast<uint32_t>(SubsystemID::COUNT)) {
        if (!m_gates[gateIdx].enabled) {
            // Gate disabled: always allow
            m_gates[gateIdx].allowCount++;
            recordEvent(sub, feature, true, caller, "Gate disabled — allowed");
            return EnforcementResult::allow(sub, feature);
        }
    }

    // Check feature through 4-layer flag system
    auto& flags = Flags::FeatureFlagsRuntime::Instance();
    bool featureEnabled = flags.isEnabled(feature);

    // Apply enforcement policy
    bool allowed = false;
    const char* detail = nullptr;

    if (featureEnabled) {
        allowed = true;
        detail = "Feature enabled — access granted";
    } else {
        switch (m_policy) {
            case EnforcementPolicy::Strict:
                allowed = false;
                detail = "Feature disabled — access denied (strict policy)";
                break;
            case EnforcementPolicy::Warn:
                allowed = true;
                detail = "Feature disabled — access allowed with warning";
                break;
            case EnforcementPolicy::Permissive:
                allowed = true;
                detail = "Feature disabled — access allowed (permissive policy)";
                break;
            default:
                allowed = false;
                detail = "Unknown policy — denied";
                break;
        }
    }

    // Update gate statistics
    if (gateIdx < static_cast<uint32_t>(SubsystemID::COUNT)) {
        if (allowed) m_gates[gateIdx].allowCount++;
        else         m_gates[gateIdx].denyCount++;
    }

    // Record audit event
    recordEvent(sub, feature, allowed, caller, detail);

    // Fire denial callback
    if (!allowed && m_denialCb) {
        License::LicenseTierV2 required = License::g_FeatureManifest[idx].minTier;
        auto& lic = License::EnterpriseLicenseV2::Instance();
        License::LicenseTierV2 current = lic.currentTier();

        EnforcementResult result = EnforcementResult::deny(
            sub, feature, required, current, detail);
        m_denialCb(result);
    }

    if (allowed) {
        return EnforcementResult::allow(sub, feature);
    } else {
        License::LicenseTierV2 required = License::g_FeatureManifest[idx].minTier;
        auto& lic = License::EnterpriseLicenseV2::Instance();
        return EnforcementResult::deny(
            sub, feature, required, lic.currentTier(), detail);
    }
}

EnforcementResult LicenseEnforcer::checkSubsystem(SubsystemID subsystem,
                                                    const char* caller) {
    uint32_t gateIdx = static_cast<uint32_t>(subsystem);
    if (gateIdx >= static_cast<uint32_t>(SubsystemID::COUNT)) {
        return EnforcementResult::deny(
            subsystem, License::FeatureID::BasicGGUFLoading,
            License::LicenseTierV2::Sovereign,
            License::LicenseTierV2::Community,
            "Invalid subsystem ID");
    }

    // Check the primary feature for this subsystem
    return checkGate(m_gates[gateIdx].primaryFeature, caller);
}

bool LicenseEnforcer::allow(License::FeatureID feature, const char* caller) {
    EnforcementResult result = checkGate(feature, caller);
    return result.allowed;
}

bool LicenseEnforcer::allow(SubsystemID /*subsystem*/, License::FeatureID feature, const char* caller) {
    // Subsystem is informational — gate check is on the feature
    return allow(feature, caller);
}

// ============================================================================
// Gate Management
// ============================================================================
const SubsystemGate& LicenseEnforcer::getGate(SubsystemID id) const {
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx >= static_cast<uint32_t>(SubsystemID::COUNT)) {
        static const SubsystemGate s_empty{};
        return s_empty;
    }
    return m_gates[idx];
}

void LicenseEnforcer::enableGate(SubsystemID id, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t idx = static_cast<uint32_t>(id);
    if (idx < static_cast<uint32_t>(SubsystemID::COUNT)) {
        m_gates[idx].enabled = enabled;
    }
}

// ============================================================================
// Denial Handler
// ============================================================================
void LicenseEnforcer::setDenialHandler(DenialCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_denialCb = cb;
}

// ============================================================================
// Audit Trail
// ============================================================================
size_t LicenseEnforcer::getEventCount() const {
    return m_eventCount;
}

const EnforcementEvent* LicenseEnforcer::getEvents() const {
    return m_events;
}

void LicenseEnforcer::clearEvents() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCount = 0;
    m_eventHead = 0;
}

void LicenseEnforcer::recordEvent(SubsystemID sub, License::FeatureID feat,
                                   bool allowed, const char* caller,
                                   const char* detail) {
    auto& lic = License::EnterpriseLicenseV2::Instance();
    bool auditEnabled = lic.isFeatureEnabled(License::FeatureID::AuditLogging);

    EnforcementEvent& ev = m_events[m_eventHead];

#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    ev.timestamp = static_cast<uint64_t>(ticks.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    ev.timestamp = static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
#endif

    ev.subsystem     = sub;
    ev.feature       = feat;
    ev.allowed       = allowed;
    ev.policyApplied = m_policy;
    ev.caller        = caller;
    ev.detail        = detail;

    m_eventHead = (m_eventHead + 1) % MAX_EVENTS;
    if (m_eventCount < MAX_EVENTS) m_eventCount++;

    // --- Phase 3: Telemetry Integration ---
    if (auditEnabled) {
        // Emit formal telemetry for enterprise audit compliance
        // (Assuming UTC is already linked)
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
        if (!allowed) {
            UTC_LogEvent("license_denied");
        }
#endif
    }
}

// ============================================================================
// Statistics
// ============================================================================
uint64_t LicenseEnforcer::getTotalAllowCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t total = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(SubsystemID::COUNT); ++i) {
        total += m_gates[i].allowCount;
    }
    return total;
}

uint64_t LicenseEnforcer::getTotalDenyCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint64_t total = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(SubsystemID::COUNT); ++i) {
        total += m_gates[i].denyCount;
    }
    return total;
}

void LicenseEnforcer::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t i = 0; i < static_cast<uint32_t>(SubsystemID::COUNT); ++i) {
        m_gates[i].allowCount = 0;
        m_gates[i].denyCount = 0;
    }
}

// ============================================================================
// Report Generation
// ============================================================================
int LicenseEnforcer::generateReport(char* buf, size_t bufLen) const {
    if (!buf || bufLen == 0) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);
    int w = 0;

    const char* policyStr = "Strict";
    if (m_policy == EnforcementPolicy::Warn) policyStr = "Warn";
    if (m_policy == EnforcementPolicy::Permissive) policyStr = "Permissive";

    auto& lic = License::EnterpriseLicenseV2::Instance();

    w += snprintf(buf + w, bufLen - w,
        "=== License Enforcement Report ===\n"
        "Policy:    %s\n"
        "Tier:      %s\n"
        "Events:    %zu\n\n",
        policyStr, License::tierName(lic.currentTier()), m_eventCount);

    w += snprintf(buf + w, bufLen - w,
        "%-20s %-8s %-8s %-8s\n",
        "Subsystem", "Enabled", "Allowed", "Denied");
    w += snprintf(buf + w, bufLen - w,
        "%-20s %-8s %-8s %-8s\n",
        "--------------------", "--------", "--------", "--------");

    for (uint32_t i = 0; i < static_cast<uint32_t>(SubsystemID::COUNT); ++i) {
        if (static_cast<size_t>(w) >= bufLen - 100) break;

        const SubsystemGate& g = m_gates[i];
        w += snprintf(buf + w, bufLen - w,
            "%-20s %-8s %-8llu %-8llu\n",
            g.name,
            g.enabled ? "Yes" : "No",
            (unsigned long long)g.allowCount,
            (unsigned long long)g.denyCount);
    }

    // Total
    uint64_t totalAllow = 0, totalDeny = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(SubsystemID::COUNT); ++i) {
        totalAllow += m_gates[i].allowCount;
        totalDeny += m_gates[i].denyCount;
    }

    w += snprintf(buf + w, bufLen - w,
        "\nTotal: %llu allowed, %llu denied\n",
        (unsigned long long)totalAllow, (unsigned long long)totalDeny);

    return w;
}

} // namespace RawrXD::Enforce
