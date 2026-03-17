// ============================================================================
// license_enforcement.h — Phase 3: License Enforcement Gate System
// ============================================================================
// Subsystem-level enforcement gates that wrap enterprise feature access.
// Each IDE subsystem passes through an enforcement checkpoint before
// executing gated functionality. Produces a complete audit trail.
//
// Architecture:
//   LicenseEnforcer::Instance()
//    ├─ SubsystemGate[]     (per-subsystem enforcement checkpoint)
//    ├─ EnforcementPolicy   (strict, warn, permissive modes)
//    ├─ DenialHandler       (callback for denied access)
//    └─ AuditStream         (ring buffer of enforcement events)
//
// Integrates with:
//   - EnterpriseLicenseV2   (tier + feature mask)
//   - FeatureFlagsRuntime   (4-layer toggle evaluation)
//   - FeatureRegistry       (Phase 31 audit system)
//
// PATTERN:   No exceptions. Returns bool/EnforcementResult.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <mutex>
#include "enterprise_license.h"

namespace RawrXD::Enforce {

// ============================================================================
// Enforcement Policy — how to handle unlicensed access
// ============================================================================
enum class EnforcementPolicy : uint32_t {
    Strict      = 0,    // Block unlicensed access completely
    Warn        = 1,    // Allow but log a warning
    Permissive  = 2,    // Allow silently (dev mode)
    COUNT
};

// ============================================================================
// Subsystem ID — major IDE subsystems with enforcement gates
// ============================================================================
enum class SubsystemID : uint32_t {
    Inference       = 0,    // CPU/GPU inference engine
    Hotpatch        = 1,    // Three-layer hotpatch system
    Agentic         = 2,    // Agentic failure detection / puppeteer
    DualEngine      = 3,    // 800B dual-engine inference
    Quantization    = 4,    // Quantization formats beyond Q4
    ModelManagement = 5,    // Multi-model, sharding, comparison
    Security        = 6,    // RBAC, model signing, audit logging
    Networking      = 7,    // Server hotpatch, rate limiting, API keys
    Observability   = 8,    // Telemetry, dashboard, metrics
    Sovereign       = 9,    // Air-gap, HSM, FIPS, tamper detection
    COUNT
};

// ============================================================================
// Enforcement Result — detailed gate check outcome
// ============================================================================
struct EnforcementResult {
    bool        allowed;
    const char* detail;
    SubsystemID subsystem;
    License::FeatureID feature;
    License::LicenseTierV2 requiredTier;
    License::LicenseTierV2 currentTier;

    static EnforcementResult allow(SubsystemID sub, License::FeatureID feat) {
        return { true, "Access granted", sub, feat,
                 License::LicenseTierV2::Community,
                 License::LicenseTierV2::Community };
    }
    static EnforcementResult deny(SubsystemID sub, License::FeatureID feat,
                                   License::LicenseTierV2 required,
                                   License::LicenseTierV2 current,
                                   const char* msg) {
        return { false, msg, sub, feat, required, current };
    }
};

// ============================================================================
// Enforcement Event — audit trail record
// ============================================================================
struct EnforcementEvent {
    uint64_t            timestamp;
    SubsystemID         subsystem;
    License::FeatureID  feature;
    bool                allowed;
    EnforcementPolicy   policyApplied;
    const char*         caller;
    const char*         detail;
};

// ============================================================================
// Subsystem Gate — tracks enforcement state per subsystem
// ============================================================================
struct SubsystemGate {
    SubsystemID     id;
    const char*     name;
    bool            enabled;            // Gate is active
    uint64_t        allowCount;         // Cumulative grants
    uint64_t        denyCount;          // Cumulative denials
    License::FeatureID primaryFeature;  // Main feature this subsystem maps to
};

// ============================================================================
// Denial Handler — function pointer callback for denied access
// ============================================================================
using DenialCallback = void(*)(const EnforcementResult& result);

// ============================================================================
// License Enforcer — Singleton
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
    bool isInitialized() const { return m_initialized; }

    // ── Policy ──
    void setPolicy(EnforcementPolicy policy);
    EnforcementPolicy getPolicy() const;

    // ── Gate Checks ──

    /// Check if a specific feature is allowed through enforcement
    EnforcementResult checkGate(License::FeatureID feature,
                                 const char* caller = nullptr);

    /// Check if a subsystem is allowed to operate
    EnforcementResult checkSubsystem(SubsystemID subsystem,
                                      const char* caller = nullptr);

    /// Convenience: returns true if allowed, false if denied
    bool allow(License::FeatureID feature, const char* caller = nullptr);

    // ── Subsystem Gates ──
    const SubsystemGate& getGate(SubsystemID id) const;
    void enableGate(SubsystemID id, bool enabled);

    // ── Denial Handler ──
    void setDenialHandler(DenialCallback cb);

    // ── Audit Trail ──
    static constexpr size_t MAX_EVENTS = 2048;
    size_t getEventCount() const;
    const EnforcementEvent* getEvents() const;
    void clearEvents();

    // ── Statistics ──
    uint64_t getTotalAllowCount() const;
    uint64_t getTotalDenyCount() const;
    void resetStatistics();

    // ── Report ──
    /// Generate enforcement summary report
    /// Returns chars written to buf
    int generateReport(char* buf, size_t bufLen) const;

private:
    LicenseEnforcer() = default;
    ~LicenseEnforcer() = default;

    void recordEvent(SubsystemID sub, License::FeatureID feat,
                     bool allowed, const char* caller, const char* detail);
    SubsystemID featureToSubsystem(License::FeatureID id) const;
    void initGates();

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    EnforcementPolicy m_policy = EnforcementPolicy::Strict;

    SubsystemGate m_gates[static_cast<size_t>(SubsystemID::COUNT)]{};

    // Audit ring buffer
    EnforcementEvent m_events[MAX_EVENTS]{};
    size_t m_eventCount = 0;
    size_t m_eventHead = 0;

    // Denial callback
    DenialCallback m_denialCb = nullptr;
};

// ============================================================================
// Convenience Macros — subsystem-level enforcement
// ============================================================================
#define ENFORCE_FEATURE(feature) \
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return

#define ENFORCE_FEATURE_BOOL(feature) \
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return false

#define ENFORCE_FEATURE_NULL(feature) \
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow( \
        RawrXD::License::FeatureID::feature, __FUNCTION__)) return nullptr

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
