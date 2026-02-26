#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <atomic>

namespace RawrXD::Agentic::Wiring {

/// Runtime feature flags with hot-reload support
class FeatureFlags {
public:
    static FeatureFlags& instance();
    
    /// Set flag value (thread-safe)
    void set(const std::string& name, bool value);
    
    /// Get flag value (thread-safe, fast path)
    bool get(const std::string& name, bool defaultValue = false) const;
    
    /// Get string flag
    std::string getString(const std::string& name, const std::string& defaultValue = "") const;
    
    /// Set string flag
    void setString(const std::string& name, const std::string& value);
    
    /// Get integer flag
    int getInt(const std::string& name, int defaultValue = 0) const;
    
    /// Set integer flag
    void setInt(const std::string& name, int value);
    
    /// Get float flag
    float getFloat(const std::string& name, float defaultValue = 0.0f) const;
    
    /// Set float flag
    void setFloat(const std::string& name, float value);
    
    /// Register callback for flag changes
    using FlagCallback = std::function<void(const std::string& name, bool oldValue, bool newValue)>;
    void onFlagChanged(const std::string& name, FlagCallback callback);
    
    /// Load from config file (TOML/JSON)
    bool loadFromFile(const std::string& filePath);
    
    /// Save to config file
    bool saveToFile(const std::string& filePath) const;
    
    /// Get all flags as JSON
    std::string toJson() const;
    
    /// Clear all flags
    void clear();
    
    /// List all flag names
    std::vector<std::string> listFlags() const;
    
private:
    FeatureFlags() = default;
    ~FeatureFlags() = default;
    
    FeatureFlags(const FeatureFlags&) = delete;
    FeatureFlags& operator=(const FeatureFlags&) = delete;
    
    mutable std::mutex m_mutex;
    
    // Optimized storage: separate maps for each type to avoid variant overhead
    std::unordered_map<std::string, std::atomic<bool>> m_boolFlags;
    std::unordered_map<std::string, std::atomic<int>> m_intFlags;
    std::unordered_map<std::string, std::atomic<float>> m_floatFlags;
    std::unordered_map<std::string, std::string> m_stringFlags;
    
    // Callbacks
    std::unordered_map<std::string, std::vector<FlagCallback>> m_callbacks;
    
    void notifyCallbacks(const std::string& name, bool oldValue, bool newValue);
};

/// RAII helper for feature flag scope
class FeatureFlagScope {
public:
    FeatureFlagScope(const std::string& name, bool value)
        : m_name(name), m_oldValue(FeatureFlags::instance().get(name)) {
        FeatureFlags::instance().set(name, value);
    }
    
    ~FeatureFlagScope() {
        FeatureFlags::instance().set(m_name, m_oldValue);
    }
    
private:
    std::string m_name;
    bool m_oldValue;
};

} // namespace RawrXD::Agentic::Wiring
