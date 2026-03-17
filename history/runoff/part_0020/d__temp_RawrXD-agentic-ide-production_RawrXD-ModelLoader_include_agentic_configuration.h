#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <unordered_map>

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
class AgenticConfiguration : public QObject
{
    Q_OBJECT

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

    struct ConfigValue {
        ConfigType type;
        QVariant value;
        bool isSecret;
        QString description;
        QVariant defaultValue;
        bool isRequired;
        QStringList validationRules;
    };

    struct FeatureToggle {
        QString featureName;
        bool enabled;
        QString description;
        QString rolloutPercentage; // "0-100" for gradual rollout
        QDateTime enabledDate;
        QString owner;
    };

public:
    explicit AgenticConfiguration(QObject* parent = nullptr);
    ~AgenticConfiguration();

    // ===== INITIALIZATION =====
    
    // Initialize from environment
    void initializeFromEnvironment(Environment env);
    
    // Load from file
    bool loadFromJson(const QString& filePath);
    bool loadFromEnv(const QString& filePath); // dotenv format
    bool loadFromYaml(const QString& filePath);
    
    // Load defaults
    void loadDefaults();

    // ===== CONFIGURATION ACCESS =====
    
    // Get configuration values
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant());
    QString getString(const QString& key, const QString& defaultValue = "");
    int getInt(const QString& key, int defaultValue = 0);
    float getFloat(const QString& key, float defaultValue = 0.0f);
    bool getBool(const QString& key, bool defaultValue = false);
    QJsonObject getObject(const QString& key, const QJsonObject& defaultValue = QJsonObject());
    QJsonArray getArray(const QString& key, const QJsonArray& defaultValue = QJsonArray());
    
    // Set configuration values at runtime
    void set(const QString& key, const QVariant& value);
    void setSecret(const QString& key, const QString& value);

    // ===== ENVIRONMENT-SPECIFIC CONFIG =====
    
    Environment getCurrentEnvironment() const { return m_currentEnvironment; }
    void setEnvironment(Environment env);
    QString getEnvironmentName() const;
    
    // Get environment-specific config
    QVariant getEnvironmentSpecific(
        const QString& key,
        Environment env = Environment::Development
    );

    // ===== FEATURE TOGGLES =====
    
    bool isFeatureEnabled(const QString& featureName);
    void enableFeature(const QString& featureName);
    void disableFeature(const QString& featureName);
    void setFeatureToggle(const FeatureToggle& toggle);
    FeatureToggle getFeatureToggle(const QString& featureName);
    QJsonArray getAllFeatureToggles();
    
    // Gradual rollout
    bool isFeatureEnabledForUser(
        const QString& featureName,
        const QString& userId
    );

    // ===== VALIDATION =====
    
    bool validateConfig(const QString& key);
    bool validateAllConfig();
    QString getValidationError(const QString& key);
    QJsonObject getValidationReport();

    // ===== HOT RELOADING =====
    
    void enableHotReloading(bool enabled);
    void watchConfigFile(const QString& filePath);
    void reloadConfiguration();

    // ===== PROFILE MANAGEMENT =====
    
    // Save current configuration as profile
    bool saveProfile(const QString& profileName);
    
    // Load saved profile
    bool loadProfile(const QString& profileName);
    
    // List available profiles
    QStringList getAvailableProfiles();
    
    // Delete profile
    bool deleteProfile(const QString& profileName);

    // ===== EXPORT/IMPORT =====
    
    QString exportConfiguration(bool includeSecrets = false) const;
    bool importConfiguration(const QString& jsonData);
    
    // Export for documentation
    QString generateConfigurationDocumentation() const;

    // ===== SENSITIVE DATA HANDLING =====
    
    // Mask secrets in output
    QString maskSecrets(const QString& text) const;
    
    // Validate secrets are not logged
    bool validateNoSecretsInLogs() const;

    // ===== DEFAULTS =====
    
    // Get all keys with current values
    QJsonObject getAllConfiguration(bool includeSecrets = false) const;
    
    // Get schema
    QJsonObject getConfigurationSchema() const;
    
    // Get help text for key
    QString getConfigurationHelp(const QString& key);

    // ===== METRICS AND MONITORING =====
    
    int getConfigurationAccessCount(const QString& key) const;
    QJsonObject getConfigurationUsageStats();

signals:
    void configurationLoaded();
    void configurationReloaded();
    void configurationChanged(const QString& key);
    void featureToggled(const QString& featureName, bool enabled);
    void validationError(const QString& key, const QString& error);
    void secretAccessed(const QString& secretName);

private:
    // Internal helpers
    void setConfigDefault(const QString& key, const ConfigValue& config);
    QVariant parseValue(const QString& valueStr, ConfigType type);
    bool validateValue(const QString& key, const QVariant& value);
    void applyEnvironmentOverrides();

    // YAML parsing helper (if YAML support needed)
    QJsonObject parseYamlFile(const QString& filePath);

    // Configuration store
    std::unordered_map<std::string, ConfigValue> m_config;
    std::unordered_map<std::string, FeatureToggle> m_featureToggles;
    
    // Profile storage
    std::unordered_map<std::string, QJsonObject> m_profiles;
    
    // Current state
    Environment m_currentEnvironment = Environment::Development;
    bool m_hotReloadingEnabled = false;
    
    // Watched files
    std::vector<QString> m_watchedFiles;
    
    // Access tracking
    std::unordered_map<std::string, int> m_accessCounts;
    
    // Secrets list (for masking)
    std::vector<QString> m_secretKeys;

    // File watchers (for hot reload)
    class ConfigFileWatcher;
    std::unique_ptr<ConfigFileWatcher> m_fileWatcher;
};
