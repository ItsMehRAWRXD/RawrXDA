// feature_flags_runtime.cpp - Runtime Feature Flag System Implementation
// Phase 2: Dynamic feature toggling with license integration

#include "feature_flags_runtime.h"
#include <sstream>
#include <iomanip>
#include <fstream>

// ============================================================================
// Singleton
// ============================================================================
FeatureFlagRuntime& FeatureFlagRuntime::getInstance() {
    static FeatureFlagRuntime instance;
    return instance;
}

FeatureFlagRuntime::FeatureFlagRuntime() = default;
FeatureFlagRuntime::~FeatureFlagRuntime() = default;

// ============================================================================
// Initialization
// ============================================================================
LicenseResult FeatureFlagRuntime::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load()) {
        return LicenseResult::ok("Feature flags already initialized");
    }

    initializeFlags();
    m_initialized.store(true);
    return LicenseResult::ok("Feature flags initialized");
}

void FeatureFlagRuntime::initializeFlags() {
    auto& mgr = EnterpriseLicenseManager::getInstance();
    auto manifest = mgr.getFullManifest();

    for (const auto& entry : manifest) {
        FeatureFlag flag;
        flag.feature = entry.id;
        flag.enabledByConfig = true;
        flag.enabledByAdmin = true;
        flag.enabledByLicense = mgr.isFeatureUnlocked(entry.id);
        flag.forceUnlock = false;
        m_flags[entry.id] = flag;
    }
}

// ============================================================================
// Query
// ============================================================================
bool FeatureFlagRuntime::isEnabled(EnterpriseFeature feature) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return false;
    return it->second.isEffectivelyEnabled();
}

bool FeatureFlagRuntime::isDisabledByConfig(EnterpriseFeature feature) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return true;
    return !it->second.enabledByConfig;
}

bool FeatureFlagRuntime::isDisabledByLicense(EnterpriseFeature feature) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return true;
    return !it->second.enabledByLicense;
}

FeatureFlag FeatureFlagRuntime::getFlag(EnterpriseFeature feature) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it != m_flags.end()) return it->second;
    FeatureFlag empty;
    empty.feature = feature;
    return empty;
}

std::vector<FeatureFlag> FeatureFlagRuntime::getAllFlags() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FeatureFlag> result;
    for (const auto& [k, v] : m_flags) {
        result.push_back(v);
    }
    return result;
}

// ============================================================================
// Admin Control
// ============================================================================
void FeatureFlagRuntime::setAdminOverride(EnterpriseFeature feature, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return;

    bool oldEnabled = it->second.isEffectivelyEnabled();
    it->second.enabledByAdmin = enabled;
    bool newEnabled = it->second.isEffectivelyEnabled();

    if (oldEnabled != newEnabled) {
        for (auto& cb : m_callbacks) {
            cb(feature, oldEnabled, newEnabled);
        }
    }
}

void FeatureFlagRuntime::setConfigOverride(EnterpriseFeature feature, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return;

    bool oldEnabled = it->second.isEffectivelyEnabled();
    it->second.enabledByConfig = enabled;
    bool newEnabled = it->second.isEffectivelyEnabled();

    if (oldEnabled != newEnabled) {
        for (auto& cb : m_callbacks) {
            cb(feature, oldEnabled, newEnabled);
        }
    }
}

void FeatureFlagRuntime::setForceUnlock(EnterpriseFeature feature, bool force) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_flags.find(feature);
    if (it == m_flags.end()) return;

    bool oldEnabled = it->second.isEffectivelyEnabled();
    it->second.forceUnlock = force;
    bool newEnabled = it->second.isEffectivelyEnabled();

    if (oldEnabled != newEnabled) {
        for (auto& cb : m_callbacks) {
            cb(feature, oldEnabled, newEnabled);
        }
    }
}

void FeatureFlagRuntime::enableAllForDev() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devMode.store(true);
    for (auto& [k, v] : m_flags) {
        bool oldEnabled = v.isEffectivelyEnabled();
        v.forceUnlock = true;
        bool newEnabled = v.isEffectivelyEnabled();
        if (oldEnabled != newEnabled) {
            for (auto& cb : m_callbacks) {
                cb(k, oldEnabled, newEnabled);
            }
        }
    }
}

void FeatureFlagRuntime::resetToLicenseDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_devMode.store(false);
    auto& mgr = EnterpriseLicenseManager::getInstance();
    for (auto& [k, v] : m_flags) {
        bool oldEnabled = v.isEffectivelyEnabled();
        v.forceUnlock = false;
        v.enabledByConfig = true;
        v.enabledByAdmin = true;
        v.enabledByLicense = mgr.isFeatureUnlocked(k);
        bool newEnabled = v.isEffectivelyEnabled();
        if (oldEnabled != newEnabled) {
            for (auto& cb : m_callbacks) {
                cb(k, oldEnabled, newEnabled);
            }
        }
    }
}

void FeatureFlagRuntime::enableTier(LicenseTier tier) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& mgr = EnterpriseLicenseManager::getInstance();
    for (auto& [k, v] : m_flags) {
        if (mgr.getRequiredTier(k) == tier) {
            v.enabledByAdmin = true;
        }
    }
}

void FeatureFlagRuntime::disableTier(LicenseTier tier) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& mgr = EnterpriseLicenseManager::getInstance();
    for (auto& [k, v] : m_flags) {
        if (mgr.getRequiredTier(k) == tier) {
            v.enabledByAdmin = false;
        }
    }
}

// ============================================================================
// Refresh from License
// ============================================================================
void FeatureFlagRuntime::refreshFromLicense() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& mgr = EnterpriseLicenseManager::getInstance();
    for (auto& [k, v] : m_flags) {
        bool oldEnabled = v.isEffectivelyEnabled();
        v.enabledByLicense = mgr.isFeatureUnlocked(k);
        bool newEnabled = v.isEffectivelyEnabled();
        if (oldEnabled != newEnabled) {
            for (auto& cb : m_callbacks) {
                cb(k, oldEnabled, newEnabled);
            }
        }
    }
}

// ============================================================================
// Callbacks
// ============================================================================
void FeatureFlagRuntime::registerFlagChangeCallback(FlagChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks.push_back(std::move(cb));
}

// ============================================================================
// Config File I/O (simple key=value format)
// ============================================================================
LicenseResult FeatureFlagRuntime::loadFromConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return LicenseResult::error("Cannot open feature config file");
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Parse feature ID from hex
        uint32_t featureId = 0;
        try {
            featureId = static_cast<uint32_t>(std::stoul(key, nullptr, 16));
        } catch (...) {
            continue;
        }

        EnterpriseFeature feature = static_cast<EnterpriseFeature>(featureId);
        auto it = m_flags.find(feature);
        if (it != m_flags.end()) {
            it->second.enabledByConfig = (value == "1" || value == "true");
        }
    }

    return LicenseResult::ok("Feature config loaded");
}

LicenseResult FeatureFlagRuntime::saveToConfig(const std::string& configPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(configPath);
    if (!file.is_open()) {
        return LicenseResult::error("Cannot write feature config file");
    }

    file << "# RawrXD Feature Flags Configuration\n";
    file << "# Format: <hex_feature_id>=<enabled: 1|0>\n\n";

    for (const auto& [k, v] : m_flags) {
        file << std::hex << std::setw(4) << std::setfill('0')
             << static_cast<uint32_t>(k)
             << "=" << (v.enabledByConfig ? "1" : "0")
             << "  # " << EnterpriseFeatureToString(k) << "\n";
    }

    return LicenseResult::ok("Feature config saved");
}

// ============================================================================
// Display
// ============================================================================
std::string FeatureFlagRuntime::generateFlagReport() const {
    auto flags = getAllFlags();
    std::ostringstream rpt;

    rpt << "FEATURE FLAG RUNTIME STATUS\n";
    rpt << std::left
        << std::setw(40) << "Feature"
        << std::setw(10) << "Config"
        << std::setw(10) << "Admin"
        << std::setw(10) << "License"
        << std::setw(10) << "Force"
        << std::setw(10) << "Active"
        << "\n";
    rpt << std::string(90, '-') << "\n";

    for (const auto& f : flags) {
        rpt << std::left
            << std::setw(40) << EnterpriseFeatureToString(f.feature)
            << std::setw(10) << (f.enabledByConfig ? "ON" : "OFF")
            << std::setw(10) << (f.enabledByAdmin ? "ON" : "OFF")
            << std::setw(10) << (f.enabledByLicense ? "ON" : "OFF")
            << std::setw(10) << (f.forceUnlock ? "YES" : "---")
            << std::setw(10) << (f.isEffectivelyEnabled() ? "ACTIVE" : "BLOCKED")
            << "\n";
    }

    return rpt.str();
}
