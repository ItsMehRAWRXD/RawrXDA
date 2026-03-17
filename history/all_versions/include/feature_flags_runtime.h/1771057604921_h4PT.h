// ============================================================================
// feature_flags_runtime.h — Phase 2: Runtime Feature Flag System
// ============================================================================
// 4-layer feature toggle system for enterprise license enforcement:
//
//   Layer 1: Compile-Time  — constexpr flags in enterprise_license.h manifest
//   Layer 2: License-Time  — FeatureMask from loaded license key
//   Layer 3: Config-Time   — JSON config overrides (feature toggles)
//   Layer 4: Runtime       — Dynamic enable/disable via API or REPL
//
// Each feature must pass ALL layers to be considered "enabled".
// Audit trail records every toggle event.
//
// PATTERN:   No exceptions. Returns bool/status codes.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <cstdint>
#include <mutex>
#include "enterprise_license.h"

namespace RawrXD::Flags {

// ============================================================================
// Toggle Source — which layer caused the toggle
// ============================================================================
enum class ToggleSource : uint32_t {
    CompileTime = 0,    // Manifest says not implemented
    License     = 1,    // License key doesn't include feature
    Config      = 2,    // Config file disabled feature
    Runtime     = 3,    // API call disabled feature
    COUNT
};

// ============================================================================
// Toggle Event — audit record for feature state changes
// ============================================================================
struct ToggleEvent {
    uint64_t                    timestamp;
    License::FeatureID          feature;
    ToggleSource                source;
    bool                        newState;  // true = enabled
    const char*                 reason;
};

// ============================================================================
// Feature Flag State — per-feature 4-layer evaluation
// ============================================================================
struct FeatureFlagState {
    License::FeatureID  id;
    bool    compileEnabled;     // Layer 1: manifest says implemented
    bool    licenseEnabled;     // Layer 2: license key grants feature
    bool    configEnabled;      // Layer 3: config file allows feature
    bool    runtimeEnabled;     // Layer 4: runtime toggle is on

    /// Feature is enabled only if ALL layers agree
    bool isEffectivelyEnabled() const {
        return compileEnabled && licenseEnabled && configEnabled && runtimeEnabled;
    }
};

// ============================================================================
// Feature Flags Runtime — Singleton 4-layer toggle system
// ============================================================================
class FeatureFlagsRuntime {
public:
    static FeatureFlagsRuntime& Instance();

    // Non-copyable
    FeatureFlagsRuntime(const FeatureFlagsRuntime&) = delete;
    FeatureFlagsRuntime& operator=(const FeatureFlagsRuntime&) = delete;

    // ── Lifecycle ──
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── Feature Queries ──

    /// Check if feature passes all 4 layers
    bool isEnabled(License::FeatureID id) const;

    /// Get detailed state of a feature across all layers
    FeatureFlagState getState(License::FeatureID id) const;

    /// Get all feature states
    void getAllStates(FeatureFlagState* outStates, uint32_t maxCount,
                     uint32_t* outCount) const;

    // ── Layer 3: Config Overrides ──

    /// Load feature toggles from a JSON config file
    bool loadConfigOverrides(const char* jsonPath);

    /// Set config override for a specific feature
    void setConfigOverride(License::FeatureID id, bool enabled);

    /// Reset all config overrides to default (all enabled)
    void resetConfigOverrides();

    // ── Layer 4: Runtime Toggles ──

    /// Enable or disable a feature at runtime
    void setRuntimeToggle(License::FeatureID id, bool enabled,
                          const char* reason = nullptr);

    /// Enable all features at runtime (runtime layer only)
    void enableAllRuntime();

    /// Disable all features at runtime (runtime layer only)
    void disableAllRuntime();

    // ── Audit Trail ──
    static constexpr size_t MAX_TOGGLE_EVENTS = 1024;
    size_t getToggleEventCount() const;
    const ToggleEvent* getToggleEvents() const;
    void clearToggleEvents();

    // ── Refresh ──

    /// Re-evaluate all features against current license state
    void refreshFromLicense();

    // ── Callbacks ──
    using FeatureToggleCallback = void(*)(License::FeatureID id, bool newState,
                                          ToggleSource source);
    void onToggle(FeatureToggleCallback cb);

private:
    FeatureFlagsRuntime() = default;
    ~FeatureFlagsRuntime() = default;

    void recordEvent(License::FeatureID id, ToggleSource source,
                     bool newState, const char* reason);
    void evaluateFeature(uint32_t idx);

    mutable std::mutex m_mutex;
    bool m_initialized = false;

    // Per-feature layer states
    bool m_configOverrides[License::TOTAL_FEATURES]{};
    bool m_runtimeToggles[License::TOTAL_FEATURES]{};

    // Toggle event ring buffer
    ToggleEvent m_events[MAX_TOGGLE_EVENTS]{};
    size_t m_eventCount = 0;
    size_t m_eventHead = 0;

    // Callbacks
    static constexpr size_t MAX_CALLBACKS = 8;
    FeatureToggleCallback m_callbacks[MAX_CALLBACKS]{};
    size_t m_callbackCount = 0;
};

// ============================================================================
// Convenience macros
// ============================================================================
#define FEATURE_FLAG_CHECK(feature) \
    RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled( \
        RawrXD::License::FeatureID::feature)

#define FEATURE_FLAG_GATE(feature) \
    if (!FEATURE_FLAG_CHECK(feature)) return

#define FEATURE_FLAG_GATE_BOOL(feature) \
    if (!FEATURE_FLAG_CHECK(feature)) return false

} // namespace RawrXD::Flags
