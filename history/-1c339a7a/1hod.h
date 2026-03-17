// feature_flags_runtime.h - Runtime Feature Flag System
// Phase 2: Dynamic feature toggling, hot-reload, config-driven
// Extends enterprise_license.h with runtime override capability

#pragma once

#include "enterprise_license.h"
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>

// ============================================================================
// Runtime Feature Flag — extends license gating with per-feature toggles
// ============================================================================
struct FeatureFlag {
    EnterpriseFeature feature;
    bool enabledByConfig = true;     // Config file toggle
    bool enabledByAdmin = true;      // Admin override
    bool enabledByLicense = false;   // Determined by license tier
    bool forceUnlock = false;        // Debug/dev mode override

    bool isEffectivelyEnabled() const {
        if (forceUnlock) return true;
        return enabledByConfig && enabledByAdmin && enabledByLicense;
    }
};

// ============================================================================
// Feature Flag Runtime Manager
// ============================================================================
class FeatureFlagRuntime {
public:
    static FeatureFlagRuntime& getInstance();

    // Initialize from license manager + config
    LicenseResult initialize();
    LicenseResult loadFromConfig(const std::string& configPath);
    LicenseResult saveToConfig(const std::string& configPath) const;

    // Query
    bool isEnabled(EnterpriseFeature feature) const;
    bool isDisabledByConfig(EnterpriseFeature feature) const;
    bool isDisabledByLicense(EnterpriseFeature feature) const;
    FeatureFlag getFlag(EnterpriseFeature feature) const;
    std::vector<FeatureFlag> getAllFlags() const;

    // Admin control
    void setAdminOverride(EnterpriseFeature feature, bool enabled);
    void setConfigOverride(EnterpriseFeature feature, bool enabled);
    void setForceUnlock(EnterpriseFeature feature, bool force);
    void enableAllForDev();   // Dev mode: unlock everything
    void resetToLicenseDefaults();

    // Batch operations
    void enableTier(LicenseTier tier);
    void disableTier(LicenseTier tier);

    // Refresh from license manager (call after license change)
    void refreshFromLicense();

    // Callbacks
    using FlagChangeCallback = std::function<void(EnterpriseFeature, bool oldEnabled, bool newEnabled)>;
    void registerFlagChangeCallback(FlagChangeCallback cb);

    // Display
    std::string generateFlagReport() const;

    ~FeatureFlagRuntime();
    FeatureFlagRuntime(const FeatureFlagRuntime&) = delete;
    FeatureFlagRuntime& operator=(const FeatureFlagRuntime&) = delete;

private:
    FeatureFlagRuntime();
    void initializeFlags();

    std::map<EnterpriseFeature, FeatureFlag> m_flags;
    std::vector<FlagChangeCallback> m_callbacks;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_devMode{false};
};
