// ============================================================================
// feature_flags_runtime.h — 4-Layer Runtime Feature Flag System
// ============================================================================
// Provides a layered feature toggle system with 4 priority levels:
//   Layer 1 (highest): Admin override — hardcoded, cannot be toggled
//   Layer 2:           Config toggle  — config-file driven
//   Layer 3:           License gate   — checked against EnterpriseLicenseV2
//   Layer 4 (lowest):  Compile-time   — default based on feature manifest
//
// Resolution: highest-priority layer with an opinion wins.
//
// PATTERN:   No exceptions. Returns bool.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "enterprise_license.h"

#include <cstdint>
#include <mutex>
#include <atomic>

namespace RawrXD::Flags {

// ============================================================================
// Toggle Source — which layer provided the decision
// ============================================================================
enum class ToggleSource : uint32_t {
    CompileTime  = 0,   // Default from feature manifest
    License      = 1,   // Checked against current license tier
    Config       = 2,   // Config file override
    Admin        = 3,   // Admin hardcoded override
    COUNT        = 4
};

// ============================================================================
// Feature Toggle Event — for audit/debugging
// ============================================================================
struct FeatureToggleEvent {
    License::FeatureID  feature;
    ToggleSource        source;
    bool                enabled;
    const char*         reason;
    uint64_t            timestamp;
};

// ============================================================================
// Feature Toggle Callback
// ============================================================================
using FeatureToggleCallback = void(*)(License::FeatureID feature, bool enabled,
                                       ToggleSource source);

// ============================================================================
// FeatureFlagsRuntime — singleton 4-layer feature flag resolver
// ============================================================================
class FeatureFlagsRuntime {
public:
    static FeatureFlagsRuntime& Instance();

    // Non-copyable
    FeatureFlagsRuntime(const FeatureFlagsRuntime&) = delete;
    FeatureFlagsRuntime& operator=(const FeatureFlagsRuntime&) = delete;

    // ── Primary Query ──
    bool isEnabled(License::FeatureID feature) const;
    ToggleSource getDecisionSource(License::FeatureID feature) const;

    // ── Layer Overrides ──
    void setAdminOverride(License::FeatureID feature, bool enabled);
    void clearAdminOverride(License::FeatureID feature);
    bool hasAdminOverride(License::FeatureID feature) const;

    void setConfigOverride(License::FeatureID feature, bool enabled);
    void clearConfigOverride(License::FeatureID feature);
    bool hasConfigOverride(License::FeatureID feature) const;

    // ── License Refresh ──
    void refreshFromLicense();

    // ── Callbacks ──
    void onToggle(FeatureToggleCallback cb);

    // ── Statistics ──
    uint32_t enabledCount() const;
    uint32_t overriddenCount() const;

    // ── Event Log ──
    static constexpr size_t MAX_EVENTS = 256;
    size_t getEventCount() const;
    const FeatureToggleEvent* getEvents() const;

private:
    FeatureFlagsRuntime();
    ~FeatureFlagsRuntime() = default;

    void recordEvent(License::FeatureID feature, ToggleSource source,
                     bool enabled, const char* reason);

    mutable std::mutex m_mutex;

    // Per-feature state
    struct FeatureState {
        bool adminOverride = false;     // Layer 1
        bool adminValue = false;
        bool configOverride = false;    // Layer 2
        bool configValue = false;
    };

    FeatureState m_states[License::TOTAL_FEATURES]{};

    // Event log ring buffer
    FeatureToggleEvent m_events[MAX_EVENTS]{};
    size_t m_eventCount = 0;
    size_t m_eventHead = 0;

    // Callbacks
    static constexpr size_t MAX_CALLBACKS = 8;
    FeatureToggleCallback m_callbacks[MAX_CALLBACKS]{};
    size_t m_callbackCount = 0;
};

} // namespace RawrXD::Flags
