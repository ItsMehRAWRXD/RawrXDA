// ============================================================================
// feature_flags_runtime.cpp — 4-Layer Runtime Feature Flag Implementation
// ============================================================================
// 4-layer resolution order (highest priority wins):
//   Layer 1: Admin override (hardcoded, cannot be toggled by user)
//   Layer 2: Config toggle  (loaded from config file)
//   Layer 3: License gate   (checked against EnterpriseLicenseV2)
//   Layer 4: Compile-time   (default from feature manifest)
//
// PATTERN:   No exceptions. Returns bool.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "feature_flags_runtime.h"

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

namespace RawrXD::Flags {

// ============================================================================
// Singleton
// ============================================================================
FeatureFlagsRuntime& FeatureFlagsRuntime::Instance() {
    static FeatureFlagsRuntime s_instance;
    return s_instance;
}

// ============================================================================
// Constructor — initialize all features from manifest defaults
// ============================================================================
FeatureFlagsRuntime::FeatureFlagsRuntime() {
    // All states start zeroed (no overrides)
    std::memset(m_states, 0, sizeof(m_states));
    std::memset(m_events, 0, sizeof(m_events));
    std::memset(m_callbacks, 0, sizeof(m_callbacks));
    m_eventCount = 0;
    m_eventHead = 0;
    m_callbackCount = 0;
}

// ============================================================================
// Primary Query — 4-layer resolution
// ============================================================================
bool FeatureFlagsRuntime::isEnabled(License::FeatureID feature) const {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    const FeatureState& s = m_states[idx];

    // Layer 1: Admin override (highest priority)
    if (s.adminOverride) return s.adminValue;

    // Layer 2: Config override
    if (s.configOverride) return s.configValue;

    // Layer 3: License gate — check if current tier includes this feature
    {
        auto& lic = License::EnterpriseLicenseV2::Instance();
        License::FeatureMask mask = lic.currentMask();
        if (mask.test(idx)) return true;
    }

    // Layer 4: Compile-time default — enabled if implemented in manifest
    if (idx < License::TOTAL_FEATURES) {
        const License::FeatureDefV2& def = License::g_FeatureManifest[idx];
        // Community features default enabled; others depend on implementation
        if (def.minTier == License::LicenseTierV2::Community && def.implemented)
            return true;
    }

    return false;
}

// ============================================================================
// Decision Source — which layer provided the answer
// ============================================================================
ToggleSource FeatureFlagsRuntime::getDecisionSource(License::FeatureID feature) const {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return ToggleSource::CompileTime;

    std::lock_guard<std::mutex> lock(m_mutex);
    const FeatureState& s = m_states[idx];

    if (s.adminOverride)  return ToggleSource::Admin;
    if (s.configOverride) return ToggleSource::Config;

    // Check license
    auto& lic = License::EnterpriseLicenseV2::Instance();
    License::FeatureMask mask = lic.currentMask();
    if (mask.test(idx)) return ToggleSource::License;

    return ToggleSource::CompileTime;
}

// ============================================================================
// Admin Overrides (Layer 1)
// ============================================================================
void FeatureFlagsRuntime::setAdminOverride(License::FeatureID feature, bool enabled) {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[idx].adminOverride = true;
    m_states[idx].adminValue = enabled;

    recordEvent(feature, ToggleSource::Admin, enabled, "Admin override set");
}

void FeatureFlagsRuntime::clearAdminOverride(License::FeatureID feature) {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[idx].adminOverride = false;

    recordEvent(feature, ToggleSource::Admin, false, "Admin override cleared");
}

bool FeatureFlagsRuntime::hasAdminOverride(License::FeatureID feature) const {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_states[idx].adminOverride;
}

// ============================================================================
// Config Overrides (Layer 2)
// ============================================================================
void FeatureFlagsRuntime::setConfigOverride(License::FeatureID feature, bool enabled) {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[idx].configOverride = true;
    m_states[idx].configValue = enabled;

    recordEvent(feature, ToggleSource::Config, enabled, "Config override set");
}

void FeatureFlagsRuntime::clearConfigOverride(License::FeatureID feature) {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_states[idx].configOverride = false;

    recordEvent(feature, ToggleSource::Config, false, "Config override cleared");
}

bool FeatureFlagsRuntime::hasConfigOverride(License::FeatureID feature) const {
    uint32_t idx = static_cast<uint32_t>(feature);
    if (idx >= License::TOTAL_FEATURES) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    return m_states[idx].configOverride;
}

// ============================================================================
// License Refresh — update Layer 3 from current license state
// ============================================================================
void FeatureFlagsRuntime::refreshFromLicense() {
    std::lock_guard<std::mutex> lock(m_mutex);

    recordEvent(License::FeatureID::BasicGGUFLoading, ToggleSource::License,
               true, "License layer refreshed");
}

// ============================================================================
// Callbacks
// ============================================================================
void FeatureFlagsRuntime::onToggle(FeatureToggleCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb && m_callbackCount < MAX_CALLBACKS) {
        m_callbacks[m_callbackCount++] = cb;
    }
}

// ============================================================================
// Statistics
// ============================================================================
uint32_t FeatureFlagsRuntime::enabledCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (uint32_t i = 0; i < License::TOTAL_FEATURES; ++i) {
        License::FeatureID fid = static_cast<License::FeatureID>(i);
        // Temporarily unlock to avoid recursive lock — inline the logic
        const FeatureState& s = m_states[i];
        bool enabled = false;
        if (s.adminOverride) enabled = s.adminValue;
        else if (s.configOverride) enabled = s.configValue;
        else {
            auto& lic = License::EnterpriseLicenseV2::Instance();
            enabled = lic.currentMask().test(i);
        }
        if (enabled) count++;
        (void)fid;
    }
    return count;
}

uint32_t FeatureFlagsRuntime::overriddenCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (uint32_t i = 0; i < License::TOTAL_FEATURES; ++i) {
        if (m_states[i].adminOverride || m_states[i].configOverride) count++;
    }
    return count;
}

// ============================================================================
// Event Log
// ============================================================================
size_t FeatureFlagsRuntime::getEventCount() const {
    return m_eventCount;
}

const FeatureToggleEvent* FeatureFlagsRuntime::getEvents() const {
    return m_events;
}

void FeatureFlagsRuntime::recordEvent(License::FeatureID feature,
                                       ToggleSource source,
                                       bool enabled,
                                       const char* reason) {
    FeatureToggleEvent& ev = m_events[m_eventHead];
    ev.feature = feature;
    ev.source = source;
    ev.enabled = enabled;
    ev.reason = reason ? reason : "";

#ifdef _WIN32
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    ev.timestamp = static_cast<uint64_t>(ticks.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    ev.timestamp = static_cast<uint64_t>(tv.tv_sec) * 1000000ULL + tv.tv_usec;
#endif

    m_eventHead = (m_eventHead + 1) % MAX_EVENTS;
    if (m_eventCount < MAX_EVENTS) m_eventCount++;

    // Fire callbacks
    for (size_t i = 0; i < m_callbackCount; ++i) {
        if (m_callbacks[i]) m_callbacks[i](feature, enabled, source);
    }
}

} // namespace RawrXD::Flags
