// ============================================================================
// license_enforcement.h — Phase 3: Subsystem Enforcement Gates + Audit Trail
// ============================================================================
// Defines the LicenseEnforcer singleton for runtime feature enforcement.
// 10 subsystem gates map enterprise features to enforcement checkpoints.
//
// Architecture:
//   LicenseEnforcer::Instance()
//    ├─ SubsystemGate[10]     (Inference, Hotpatch, Agentic, DualEngine, ...)
//    ├─ EnforcementPolicy     (Strict / Warn / Permissive)
//    ├─ EnforcementEvent[]    (4096-entry ring buffer audit trail)
//    └─ DenialCallback        (function pointer for denial notifications)
//
// Convenience Macros:
//   ENFORCE_FEATURE(feat)      — void return on denial
//   ENFORCE_FEATURE_BOOL(feat) — return false on denial
//   ENFORCE_FEATURE_NULL(feat) — return nullptr on denial
//
// PATTERN:   No exceptions. Returns bool/EnforcementResult.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "enterprise_license.h"

#include <cstdint>
#include <mutex>

namespace RawrXD::Enforce {

// ============================================================================
// Subsystem ID — maps to one of 10 enforcement gates
// ============================================================================
enum class SubsystemID : uint32_t {
    Inference       = 0,    // CPU/GPU inference engine
    Hotpatch        = 1,    // Memory/Byte/Server hotpatching
    Agentic         = 2,    // Agentic framework (puppeteer, self-correction)
    DualEngine      = 3,    // 800B Dual-Engine
    Quantization    = 4,    // Q5/Q8/GPTQ/AWQ
    ModelManagement = 5,    // Multi-model, sessions, prompt library
    Security        = 6,    // Audit, RBAC, API keys
    Networking      = 7,    // Server hotpatch, proxy
    Observability   = 8,    // Dashboard, telemetry
    Sovereign       = 9,    // Air-gap, HSM, FIPS
    COUNT           = 10
};

// ============================================================================
// Enforcement Policy — how to handle denied features
// ============================================================================
enum class EnforcementPolicy : uint32_t {
    Strict     = 0,     // Block access — return denial
    Warn       = 1,     // Allow access but log warning
    Permissive = 2      // Allow access silently
};

// ============================================================================
// Subsystem Gate — one per subsystem
// ============================================================================
struct SubsystemGate {
    SubsystemID         id{};
    const char*         name = "";
    bool                enabled = true;
    uint64_t            allowCount = 0;
    uint64_t            denyCount = 0;
    License::FeatureID  primaryFeature = License::FeatureID::BasicGGUFLoading;
};

// ============================================================================
// Enforcement Result — detailed gate check result
// ============================================================================
struct EnforcementResult {
    bool                    allowed;
    SubsystemID             subsystem;
    License::FeatureID      feature;
    License::LicenseTierV2  requiredTier;
    License::LicenseTierV2  currentTier;
    const char*             detail;

    static EnforcementResult allow(SubsystemID sub, License::FeatureID feat) {
        return { true, sub, feat,
                 License::LicenseTierV2::Community,
                 License::LicenseTierV2::Community,
                 "Access granted" };
    }

    static EnforcementResult deny(SubsystemID sub, License::FeatureID feat,
                                   License::LicenseTierV2 required,
                                   License::LicenseTierV2 current,
                                   const char* reason = "Access denied") {
        return { false, sub, feat, required, current, reason };
    }
};

// ============================================================================
// Enforcement Event — audit trail entry
// ============================================================================
struct EnforcementEvent {
    uint64_t                timestamp;
    SubsystemID             subsystem;
    License::FeatureID      feature;
    bool                    allowed;
    EnforcementPolicy       policyApplied;
    const char*             caller;
    const char*             detail;
};

// ============================================================================
// Denial Callback
// ============================================================================
using DenialCallback = void(*)(const EnforcementResult& result);

// ============================================================================
// LicenseEnforcer — singleton enforcement engine
// ============================================================================
class LicenseEnforcer {
public:
    static LicenseEnforcer& Instance();

    // Non-copyable
    LicenseEnforcer(const LicenseEnforcer&) = delete;
    LicenseEnforcer& operator=(const LicenseEnforcer&) = delete;

    // ── Lifecycle ──
    bool initialize();
    void shutdown();

    // ── Gate Checks ──
    EnforcementResult checkGate(License::FeatureID feature,
                                 const char* caller = nullptr);
    EnforcementResult checkSubsystem(SubsystemID subsystem,
                                      const char* caller = nullptr);
    bool allow(License::FeatureID feature, const char* caller = nullptr);

    /// 3-arg overload: allow(subsystem, feature, caller) — subsystem is informational
    bool allow(SubsystemID subsystem, License::FeatureID feature, const char* caller);

    // ── Policy ──
    void setPolicy(EnforcementPolicy policy);
    EnforcementPolicy getPolicy() const;

    // ── Gate Management ──
    const SubsystemGate& getGate(SubsystemID id) const;
    void enableGate(SubsystemID id, bool enabled);

    // ── Denial Handler ──
    void setDenialHandler(DenialCallback cb);

    // ── Audit Trail ──
    static constexpr size_t MAX_EVENTS = 4096;
    size_t getEventCount() const;
    const EnforcementEvent* getEvents() const;
    void clearEvents();

    // ── Statistics ──
    uint64_t getTotalAllowCount() const;
    uint64_t getTotalDenyCount() const;
    void resetStatistics();

    // ── Report ──
    int generateReport(char* buf, size_t bufLen) const;

private:
    LicenseEnforcer() = default;
    ~LicenseEnforcer() = default;

    void initGates();
    SubsystemID featureToSubsystem(License::FeatureID id) const;
    void recordEvent(SubsystemID sub, License::FeatureID feat,
                     bool allowed, const char* caller, const char* detail);

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    EnforcementPolicy m_policy = EnforcementPolicy::Strict;

    // Gates
    SubsystemGate m_gates[static_cast<size_t>(SubsystemID::COUNT)]{};

    // Audit ring buffer
    EnforcementEvent m_events[MAX_EVENTS]{};
    size_t m_eventCount = 0;
    size_t m_eventHead = 0;

    // Denial callback
    DenialCallback m_denialCb = nullptr;
};

// ============================================================================
// Inline enforcement helpers — type-safe replacements for macro wrappers
// ============================================================================
inline bool enforceFeatureCheck(RawrXD::License::FeatureID id, const char* caller) {
    return RawrXD::Enforce::LicenseEnforcer::Instance().allow(id, caller);
}

// Use enforceFeatureCheck() directly instead of macro wrappers:
//   if (!enforceFeatureCheck(RawrXD::License::FeatureID::MyFeature, __FUNCTION__)) return;
//   if (!enforceFeatureCheck(RawrXD::License::FeatureID::MyFeature, __FUNCTION__)) return false;
//   if (!enforceFeatureCheck(RawrXD::License::FeatureID::MyFeature, __FUNCTION__)) return nullptr;

// ============================================================================
// Backing implementation check — 21 enterprise features are license-gated but
// have no backing implementation yet. See docs/ENTERPRISE_FEATURES_WITHOUT_BACKING.md
// ============================================================================
bool HasBackingImplementation(License::FeatureID id);

// ============================================================================
// Subsystem name lookup
// ============================================================================
inline const char* subsystemName(SubsystemID id) {
    switch (id) {
        case SubsystemID::Inference:       return "Inference";
        case SubsystemID::Hotpatch:        return "Hotpatch";
        case SubsystemID::Agentic:         return "Agentic";
        case SubsystemID::DualEngine:      return "DualEngine";
        case SubsystemID::Quantization:    return "Quantization";
        case SubsystemID::ModelManagement: return "ModelManagement";
        case SubsystemID::Security:        return "Security";
        case SubsystemID::Networking:      return "Networking";
        case SubsystemID::Observability:   return "Observability";
        case SubsystemID::Sovereign:       return "Sovereign";
        default:                           return "Unknown";
    }
}

} // namespace RawrXD::Enforce
