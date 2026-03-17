#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <variant>
#include <chrono>
#include <functional>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

/**
 * @class AgenticConfiguration
 * @brief External configuration management for environment-specific values
 * 
 * Features:
 * - Environment variable support (Development, Staging, Production)
 * - Configuration file loading (YAML, JSON, dotenv)
 * - Feature toggles for experimental features
 * - Validation and type checking
 * - Hot reloading with change notifications
 * - Hierarchical configuration with defaults
 * - Secure handling of sensitive values (API keys, tokens)
 */
class AgenticConfiguration
{
public:
    enum class Environment {
        Development,
        Staging,
        Production
    };

    enum class ConfigType {
        String,
        Integer,
        Float,
        Boolean,
        Array,
        Object
    };

    typedef std::variant<std::string, int, float, bool, json> ConfigVar;

    struct ConfigValue {
        ConfigType type;
        ConfigVar value;
        bool isSecret;
        std::string description;
        ConfigVar defaultValue;
        bool isRequired;
        std::vector<std::string> validationRules;
    };

    struct FeatureToggle {
        std::string featureName;
        bool enabled;
        std::string description;
        std::string rolloutPercentage; // "0-100" for gradual rollout
        std::chrono::system_clock::time_point enabledDate;
        std::string owner;
    };

public:
    explicit AgenticConfiguration();
    ~AgenticConfiguration();

    // ===== INITIALIZATION =====
    
    // Initialize from environment
    void initializeFromEnvironment(Environment env);
    
    // Load from file
    bool loadFromJson(const std::string& filePath);
    bool loadFromYaml(const std::string& filePath);
    bool loadFromEnv(const std::string& filePath); // dotenv format
    
    // Load defaults
    void loadDefaults();

    // ===== CONFIGURATION ACCESS =====
    
    // Get configuration values
    ConfigVar get(const std::string& key, const ConfigVar& defaultValue = ConfigVar());
    std::string getString(const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& key, int defaultValue = 0);
    float getFloat(const std::string& key, float defaultValue = 0.0f);
    bool getBool(const std::string& key, bool defaultValue = false);
    json getObject(const std::string& key, const json& defaultValue = json());
    json getArray(const std::string& key, const json& defaultValue = json());
    
    // Set configuration values at runtime
    void set(const std::string& key, const ConfigVar& value);
    void setSecret(const std::string& key, const std::string& value);

    // ===== ENVIRONMENT-SPECIFIC CONFIG =====
    
    Environment getCurrentEnvironment() const { return m_currentEnvironment; }
    void setEnvironment(Environment env);
    std::string getEnvironmentName() const;
    
    // Get environment-specific config
    ConfigVar getEnvironmentSpecific(
        const std::string& key,
        Environment env = Environment::Development
    );

    // ===== FEATURE TOGGLES =====
    
    bool isFeatureEnabled(const std::string& featureName);
    void enableFeature(const std::string& featureName);
    void disableFeature(const std::string& featureName);
    void setFeatureToggle(const FeatureToggle& toggle);
    FeatureToggle getFeatureToggle(const std::string& featureName);
    json getAllFeatureToggles();
    
    // Gradual rollout
    bool isFeatureEnabledForUser(
        const std::string& featureName,
        const std::string& userId
    );

    // ===== VALIDATION =====
    
    bool validateConfig(const std::string& key);
    bool validateAllConfig();
    std::string getValidationError(const std::string& key);
    json getValidationReport();

    // ===== HOT RELOADING =====
    
    void enableHotReloading(bool enabled);
    void watchConfigFile(const std::string& filePath);
    void reloadConfiguration();

    // ===== PROFILE MANAGEMENT =====
    
    // Save current configuration as profile
    bool saveProfile(const std::string& profileName);
    
    // Load saved profile
    bool loadProfile(const std::string& profileName);
    
    // List available profiles
    std::vector<std::string> getAvailableProfiles();
    
    // Delete profile
    bool deleteProfile(const std::string& profileName);

    // ===== EXPORT/IMPORT =====
    
    std::string exportConfiguration(bool includeSecrets = false) const;
    bool importConfiguration(const std::string& jsonData);
    
    // Export for documentation
    std::string generateConfigurationDocumentation() const;

    // ===== SENSITIVE DATA HANDLING =====
    
    // Mask secrets in output
    std::string maskSecrets(const std::string& text) const;
    
    // Validate secrets are not logged
    bool validateNoSecretsInLogs() const;

    // ===== DEFAULTS =====
    
    // Get all keys with current values
    json getAllConfiguration(bool includeSecrets = false) const;
    
    // Get schema
    json getConfigurationSchema() const;
    
    // Get help text for key
    std::string getConfigurationHelp(const std::string& key);

    // ===== METRICS AND MONITORING =====
    
    int getConfigurationAccessCount(const std::string& key) const;
    json getConfigurationUsageStats();
    
    // Callbacks or Listeners (Replaces Signals)
    // For now, we stub these out or could add std::function callbacks if needed.
    // void configurationLoaded();
    // void configurationReloaded();
    // void configurationChanged(const std::string& key);
    // void featureToggled(const std::string& featureName, bool enabled);
    // void validationError(const std::string& key, const std::string& error);
    // void secretAccessed(const std::string& secretName);

private:
    // Internal helpers
    void setConfigDefault(const std::string& key, const ConfigValue& config);
    ConfigVar parseValue(const std::string& valueStr, ConfigType type);
    bool validateValue(const std::string& key, const ConfigVar& value);
    void applyEnvironmentOverrides();

    // YAML parsing helper (if YAML support needed)
    json parseYamlFile(const std::string& filePath);

    // Configuration store
    std::unordered_map<std::string, ConfigValue> m_config;
    std::unordered_map<std::string, FeatureToggle> m_featureToggles;
    
    // Profile storage
    std::unordered_map<std::string, json> m_profiles;
    
    // Current state
    Environment m_currentEnvironment = Environment::Development;
    bool m_hotReloadingEnabled = false;
    
    // Watched files
    std::vector<std::string> m_watchedFiles;
    
    // Access tracking
    std::unordered_map<std::string, int> m_accessCounts;
    
    // Secrets list (for masking)
    std::vector<std::string> m_secretKeys;

    // File watchers (for hot reload)
    class ConfigFileWatcher;
    std::unique_ptr<ConfigFileWatcher> m_fileWatcher;
};
