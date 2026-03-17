#pragma once
/**
 * @file config_manager.h
 * @brief Configuration management system for RawrXD IDE
 * 
 * Provides environment-based configuration, feature flags, and runtime
 * reconfiguration support. Replaces hardcoded values with configurable settings.
 */

#include <string>
#include <unordered_map>
#include <variant>
#include <optional>
#include <mutex>
#include <functional>
#include <vector>

namespace RawrXD {

// Configuration value types
using ConfigValue = std::variant<std::string, int, double, bool>;

/**
 * @class ConfigManager
 * @brief Centralized configuration management with environment support
 */
class ConfigManager {
public:
    static ConfigManager& instance();
    
    // Environment detection
    enum class Environment { Development, Staging, Production };
    Environment getEnvironment() const;
    void setEnvironment(Environment env);
    
    // Configuration loading
    bool loadFromFile(const std::string& filepath);
    bool loadFromEnvironment();  // Load from environment variables
    bool saveToFile(const std::string& filepath) const;
    
    // Value access with type safety
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;
    
    std::string getString(const std::string& key, const std::string& def = "") const;
    int getInt(const std::string& key, int def = 0) const;
    double getDouble(const std::string& key, double def = 0.0) const;
    bool getBool(const std::string& key, bool def = false) const;
    
    // Value setting
    void set(const std::string& key, const ConfigValue& value);
    
    // Feature flags
    bool isFeatureEnabled(const std::string& featureName) const;
    void enableFeature(const std::string& featureName);
    void disableFeature(const std::string& featureName);
    std::vector<std::string> getEnabledFeatures() const;
    
    // Change callbacks
    using ChangeCallback = std::function<void(const std::string& key, const ConfigValue& newValue)>;
    void addChangeCallback(ChangeCallback callback);
    
    // Default configurations
    void setDefaults();
    
    // Model-specific settings
    struct ModelConfig {
        std::string path;
        int contextLength = 2048;
        int batchSize = 512;
        int gpuLayers = 0;
        double temperature = 0.7;
        double topP = 0.9;
        bool useFlashAttention = true;
    };
    
    ModelConfig getModelConfig(const std::string& modelName) const;
    void setModelConfig(const std::string& modelName, const ModelConfig& config);
    
    // Memory settings
    struct MemoryConfig {
        size_t maxZoneMemoryMB = 512;
        size_t blockSizeMB = 128;
        size_t maxCachedTensors = 100;
        bool enableNUMA = true;
        bool aggressiveEviction = false;
    };
    
    MemoryConfig getMemoryConfig() const;
    void setMemoryConfig(const MemoryConfig& config);
    
    // Server settings
    struct ServerConfig {
        int httpPort = 8080;
        int metricsPort = 9090;
        int maxConnections = 100;
        int requestTimeoutMs = 30000;
        bool enableTLS = false;
        std::string certPath;
        std::string keyPath;
    };
    
    ServerConfig getServerConfig() const;
    void setServerConfig(const ServerConfig& config);

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    void notifyChange(const std::string& key, const ConfigValue& value);
    std::string getEnvPrefix() const;
    
    mutable std::mutex m_mutex;
    Environment m_environment = Environment::Development;
    std::unordered_map<std::string, ConfigValue> m_config;
    std::unordered_map<std::string, bool> m_featureFlags;
    std::unordered_map<std::string, ModelConfig> m_modelConfigs;
    MemoryConfig m_memoryConfig;
    ServerConfig m_serverConfig;
    std::vector<ChangeCallback> m_changeCallbacks;
};

// Template implementations
template<typename T>
T ConfigManager::get(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    if (it == m_config.end()) return defaultValue;
    
    try {
        return std::get<T>(it->second);
    } catch (...) {
        return defaultValue;
    }
}

} // namespace RawrXD
