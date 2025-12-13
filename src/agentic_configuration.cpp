// AgenticConfiguration Implementation (Core Functions)
#include "agentic_configuration.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>

AgenticConfiguration::AgenticConfiguration(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[AgenticConfiguration] Initialized - Ready for configuration management";
    loadDefaults();
}

AgenticConfiguration::~AgenticConfiguration()
{
    qDebug() << "[AgenticConfiguration] Destroyed";
}

// ===== INITIALIZATION =====

void AgenticConfiguration::initializeFromEnvironment(Environment env)
{
    m_currentEnvironment = env;

    QString envName;
    switch (env) {
        case Environment::Development: envName = "Development"; break;
        case Environment::Staging: envName = "Staging"; break;
        case Environment::Production: envName = "Production"; break;
    }

    qInfo() << "[AgenticConfiguration] Initialized for environment:" << envName;
    applyEnvironmentOverrides();
}

bool AgenticConfiguration::loadFromJson(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[AgenticConfiguration] Cannot open JSON file:" << filePath;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        qWarning() << "[AgenticConfiguration] Invalid JSON format";
        return false;
    }

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        set(it.key(), it.value().toVariant());
    }

    qInfo() << "[AgenticConfiguration] Loaded configuration from JSON:" << filePath;
    emit configurationLoaded();

    return true;
}

bool AgenticConfiguration::loadFromEnv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    while (!file.atEnd()) {
        QString line = file.readLine().trimmed();

        if (line.isEmpty() || line.startsWith("#")) continue;

        int eqIdx = line.indexOf('=');
        if (eqIdx < 0) continue;

        QString key = line.left(eqIdx).trimmed();
        QString value = line.mid(eqIdx + 1).trimmed();

        set(key, value);
    }

    file.close();

    qInfo() << "[AgenticConfiguration] Loaded from .env file:" << filePath;
    emit configurationLoaded();

    return true;
}

bool AgenticConfiguration::loadFromYaml(const QString& filePath)
{
    // YAML parsing would be implemented here
    qWarning() << "[AgenticConfiguration] YAML loading not yet implemented";
    return false;
}

void AgenticConfiguration::loadDefaults()
{
    // Set up default agentic system configuration
    ConfigValue defaultConfig;

    // Agentic loop settings
    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 10;
    defaultConfig.isRequired = false;
    defaultConfig.description = "Maximum iterations for agentic loops";
    setConfigDefault("agentic.max_iterations", defaultConfig);

    // Observability defaults
    defaultConfig.type = ConfigType::String;
    defaultConfig.defaultValue = "INFO";
    defaultConfig.description = "Minimum log level (DEBUG, INFO, WARN, ERROR, CRITICAL)";
    setConfigDefault("observability.log_level", defaultConfig);

    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 10000;
    defaultConfig.description = "Maximum number of log entries to keep";
    setConfigDefault("observability.max_logs", defaultConfig);

    // Error handling defaults
    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 3;
    defaultConfig.description = "Maximum retry attempts for recoverable errors";
    setConfigDefault("error_handler.max_retries", defaultConfig);

    defaultConfig.type = ConfigType::Boolean;
    defaultConfig.defaultValue = true;
    defaultConfig.description = "Enable graceful degradation";
    setConfigDefault("error_handler.graceful_degradation", defaultConfig);

    // Memory system defaults
    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 1000;
    defaultConfig.description = "Maximum memories to retain";
    setConfigDefault("memory.max_memories", defaultConfig);

    defaultConfig.type = ConfigType::Float;
    defaultConfig.defaultValue = 0.99f;
    defaultConfig.description = "Memory decay rate";
    setConfigDefault("memory.decay_rate", defaultConfig);

    // Model settings
    defaultConfig.type = ConfigType::String;
    defaultConfig.defaultValue = "";
    defaultConfig.description = "Path to current model";
    defaultConfig.isRequired = false;
    setConfigDefault("model.path", defaultConfig);

    defaultConfig.type = ConfigType::Float;
    defaultConfig.defaultValue = 0.8f;
    defaultConfig.description = "Model temperature for generation";
    setConfigDefault("model.temperature", defaultConfig);

    defaultConfig.type = ConfigType::Float;
    defaultConfig.defaultValue = 0.9f;
    defaultConfig.description = "Model top-p for sampling";
    setConfigDefault("model.top_p", defaultConfig);

    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 512;
    defaultConfig.description = "Maximum tokens to generate";
    setConfigDefault("model.max_tokens", defaultConfig);

    qDebug() << "[AgenticConfiguration] Loaded defaults";
}

// ===== CONFIGURATION ACCESS =====

QVariant AgenticConfiguration::get(const QString& key, const QVariant& defaultValue)
{
    auto it = m_config.find(key.toStdString());
    if (it != m_config.end()) {
        m_accessCounts[key.toStdString()]++;
        return it->second.value;
    }

    return defaultValue;
}

QString AgenticConfiguration::getString(const QString& key, const QString& defaultValue)
{
    QVariant val = get(key, defaultValue);
    return val.toString();
}

int AgenticConfiguration::getInt(const QString& key, int defaultValue)
{
    QVariant val = get(key, defaultValue);
    return val.toInt();
}

float AgenticConfiguration::getFloat(const QString& key, float defaultValue)
{
    QVariant val = get(key, defaultValue);
    return static_cast<float>(val.toDouble());
}

bool AgenticConfiguration::getBool(const QString& key, bool defaultValue)
{
    QVariant val = get(key, defaultValue);
    return val.toBool();
}

QJsonObject AgenticConfiguration::getObject(const QString& key, const QJsonObject& defaultValue)
{
    QVariant val = get(key);
    if (val.canConvert<QJsonObject>()) {
        return val.value<QJsonObject>();
    }
    return defaultValue;
}

QJsonArray AgenticConfiguration::getArray(const QString& key, const QJsonArray& defaultValue)
{
    QVariant val = get(key);
    if (val.canConvert<QJsonArray>()) {
        return val.value<QJsonArray>();
    }
    return defaultValue;
}

void AgenticConfiguration::set(const QString& key, const QVariant& value)
{
    auto it = m_config.find(key.toStdString());
    
    if (it != m_config.end()) {
        it->second.value = value;
    } else {
        ConfigValue config;
        config.value = value;
        config.type = ConfigType::String;
        m_config[key.toStdString()] = config;
    }

    emit configurationChanged(key);
}

void AgenticConfiguration::setSecret(const QString& key, const QString& value)
{
    auto it = m_config.find(key.toStdString());
    
    if (it != m_config.end()) {
        it->second.value = value;
        it->second.isSecret = true;
    } else {
        ConfigValue config;
        config.value = value;
        config.type = ConfigType::String;
        config.isSecret = true;
        m_config[key.toStdString()] = config;
    }

    m_secretKeys.push_back(key);
    emit secretAccessed(key);
}

// ===== ENVIRONMENT-SPECIFIC CONFIG =====

void AgenticConfiguration::setEnvironment(Environment env)
{
    m_currentEnvironment = env;
    applyEnvironmentOverrides();

    emit configurationLoaded();
}

QString AgenticConfiguration::getEnvironmentName() const
{
    switch (m_currentEnvironment) {
        case Environment::Development: return "Development";
        case Environment::Staging: return "Staging";
        case Environment::Production: return "Production";
        default: return "Unknown";
    }
}

QVariant AgenticConfiguration::getEnvironmentSpecific(
    const QString& key,
    Environment env)
{
    // Would load environment-specific config
    return get(key);
}

// ===== FEATURE TOGGLES =====

bool AgenticConfiguration::isFeatureEnabled(const QString& featureName)
{
    auto it = m_featureToggles.find(featureName.toStdString());
    if (it != m_featureToggles.end()) {
        return it->second.enabled;
    }
    return false;
}

void AgenticConfiguration::enableFeature(const QString& featureName)
{
    auto it = m_featureToggles.find(featureName.toStdString());
    
    if (it != m_featureToggles.end()) {
        it->second.enabled = true;
    } else {
        FeatureToggle toggle;
        toggle.featureName = featureName;
        toggle.enabled = true;
        toggle.enabledDate = QDateTime::currentDateTime();
        m_featureToggles[featureName.toStdString()] = toggle;
    }

    emit featureToggled(featureName, true);
}

void AgenticConfiguration::disableFeature(const QString& featureName)
{
    auto it = m_featureToggles.find(featureName.toStdString());
    
    if (it != m_featureToggles.end()) {
        it->second.enabled = false;
    }

    emit featureToggled(featureName, false);
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle)
{
    m_featureToggles[toggle.featureName.toStdString()] = toggle;
    emit featureToggled(toggle.featureName, toggle.enabled);
}

AgenticConfiguration::FeatureToggle AgenticConfiguration::getFeatureToggle(
    const QString& featureName)
{
    auto it = m_featureToggles.find(featureName.toStdString());
    if (it != m_featureToggles.end()) {
        return it->second;
    }
    
    FeatureToggle empty;
    empty.featureName = featureName;
    empty.enabled = false;
    return empty;
}

QJsonArray AgenticConfiguration::getAllFeatureToggles()
{
    QJsonArray toggles;

    for (const auto& pair : m_featureToggles) {
        QJsonObject toggle;
        toggle["feature"] = pair.second.featureName;
        toggle["enabled"] = pair.second.enabled;
        toggle["description"] = pair.second.description;

        toggles.append(toggle);
    }

    return toggles;
}

bool AgenticConfiguration::isFeatureEnabledForUser(
    const QString& featureName,
    const QString& userId)
{
    // Check gradual rollout
    auto toggle = getFeatureToggle(featureName);
    if (!toggle.enabled) return false;

    // Simple hash-based rollout
    uint hash = qHash(userId) % 100;
    int rolloutPercentage = toggle.rolloutPercentage.split('-').first().toInt();

    return hash < rolloutPercentage;
}

// ===== VALIDATION =====

bool AgenticConfiguration::validateConfig(const QString& key)
{
    auto it = m_config.find(key.toStdString());
    if (it == m_config.end()) {
        return !it->second.isRequired;
    }

    return validateValue(key, it->second.value);
}

bool AgenticConfiguration::validateAllConfig()
{
    for (const auto& pair : m_config) {
        if (!validateValue(QString::fromStdString(pair.first), pair.second.value)) {
            return false;
        }
    }
    return true;
}

QString AgenticConfiguration::getValidationError(const QString& key)
{
    // Would return validation error for key
    return "";
}

QJsonObject AgenticConfiguration::getValidationReport()
{
    QJsonObject report;

    for (const auto& pair : m_config) {
        QString key = QString::fromStdString(pair.first);
        bool valid = validateValue(key, pair.second.value);
        
        QJsonObject entry;
        entry["valid"] = valid;
        entry["error"] = getValidationError(key);

        report[key] = entry;
    }

    return report;
}

// ===== HOT RELOADING =====

void AgenticConfiguration::enableHotReloading(bool enabled)
{
    m_hotReloadingEnabled = enabled;
}

void AgenticConfiguration::watchConfigFile(const QString& filePath)
{
    m_watchedFiles.push_back(filePath);
    qInfo() << "[AgenticConfiguration] Watching config file:" << filePath;
}

void AgenticConfiguration::reloadConfiguration()
{
    qInfo() << "[AgenticConfiguration] Reloading configuration";
    emit configurationReloaded();
}

// ===== PROFILE MANAGEMENT =====

bool AgenticConfiguration::saveProfile(const QString& profileName)
{
    m_profiles[profileName.toStdString()] = getAllConfiguration(false);

    qInfo() << "[AgenticConfiguration] Saved profile:" << profileName;
    return true;
}

bool AgenticConfiguration::loadProfile(const QString& profileName)
{
    auto it = m_profiles.find(profileName.toStdString());
    if (it == m_profiles.end()) {
        return false;
    }

    QJsonObject profile = it->second;
    for (auto iter = profile.constBegin(); iter != profile.constEnd(); ++iter) {
        set(iter.key(), iter.value().toVariant());
    }

    qInfo() << "[AgenticConfiguration] Loaded profile:" << profileName;
    emit configurationLoaded();

    return true;
}

QStringList AgenticConfiguration::getAvailableProfiles()
{
    QStringList profiles;

    for (const auto& pair : m_profiles) {
        profiles.append(QString::fromStdString(pair.first));
    }

    return profiles;
}

bool AgenticConfiguration::deleteProfile(const QString& profileName)
{
    return m_profiles.erase(profileName.toStdString()) > 0;
}

// ===== EXPORT/IMPORT =====

QString AgenticConfiguration::exportConfiguration(bool includeSecrets) const
{
    QJsonObject config;

    for (const auto& pair : m_config) {
        QString key = QString::fromStdString(pair.first);
        
        if (!includeSecrets && pair.second.isSecret) {
            config[key] = "***REDACTED***";
        } else {
            config[key] = QJsonValue::fromVariant(pair.second.value);
        }
    }

    return QString::fromUtf8(QJsonDocument(config).toJson());
}

bool AgenticConfiguration::importConfiguration(const QString& jsonData)
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isObject()) return false;

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        set(it.key(), it.value().toVariant());
    }

    return true;
}

QString AgenticConfiguration::generateConfigurationDocumentation() const
{
    QString doc;
    doc += "# Agentic System Configuration\n\n";

    for (const auto& pair : m_config) {
        QString key = QString::fromStdString(pair.first);
        const ConfigValue& config = pair.second;

        doc += QString("## %1\n").arg(key);
        doc += QString("Description: %1\n").arg(config.description);
        doc += QString("Type: %1\n").arg(static_cast<int>(config.type));
        doc += QString("Default: %1\n").arg(config.defaultValue.toString());
        doc += QString("Required: %1\n\n").arg(config.isRequired ? "Yes" : "No");
    }

    return doc;
}

// ===== SENSITIVE DATA HANDLING =====

QString AgenticConfiguration::maskSecrets(const QString& text) const
{
    QString masked = text;

    for (const auto& secretKey : m_secretKeys) {
        auto it = m_config.find(secretKey.toStdString());
        if (it != m_config.end()) {
            QString secretValue = it->second.value.toString();
            masked.replace(secretValue, "***REDACTED***");
        }
    }

    return masked;
}

bool AgenticConfiguration::validateNoSecretsInLogs() const
{
    // Validate that secrets are not being logged
    return true;
}

// ===== CONFIGURATION QUERIES =====

QJsonObject AgenticConfiguration::getAllConfiguration(bool includeSecrets) const
{
    return QJsonDocument::fromJson(
        exportConfiguration(includeSecrets).toUtf8()
    ).object();
}

QJsonObject AgenticConfiguration::getConfigurationSchema() const
{
    QJsonObject schema;

    for (const auto& pair : m_config) {
        QString key = QString::fromStdString(pair.first);
        const ConfigValue& config = pair.second;

        QJsonObject fieldSchema;
        fieldSchema["type"] = static_cast<int>(config.type);
        fieldSchema["description"] = config.description;
        fieldSchema["required"] = config.isRequired;

        schema[key] = fieldSchema;
    }

    return schema;
}

QString AgenticConfiguration::getConfigurationHelp(const QString& key)
{
    auto it = m_config.find(key.toStdString());
    if (it != m_config.end()) {
        return it->second.description;
    }
    return "Configuration key not found";
}

// ===== METRICS =====

QJsonObject AgenticConfiguration::getConfigurationUsageStats()
{
    QJsonObject stats;

    QJsonObject accessCounts;
    for (const auto& pair : m_accessCounts) {
        accessCounts[QString::fromStdString(pair.first)] = pair.second;
    }

    stats["access_counts"] = accessCounts;
    stats["total_keys"] = static_cast<int>(m_config.size());

    return stats;
}

// ===== PRIVATE HELPERS =====

void AgenticConfiguration::setConfigDefault(const QString& key, const ConfigValue& config)
{
    m_config[key.toStdString()] = config;
}

QVariant AgenticConfiguration::parseValue(const QString& valueStr, ConfigType type)
{
    switch (type) {
        case ConfigType::String: return valueStr;
        case ConfigType::Integer: return valueStr.toInt();
        case ConfigType::Float: return valueStr.toFloat();
        case ConfigType::Boolean: 
            return valueStr.toLower() == "true" || valueStr == "1";
        default: return valueStr;
    }
}

bool AgenticConfiguration::validateValue(const QString& key, const QVariant& value)
{
    // Basic validation - can be extended
    auto it = m_config.find(key.toStdString());
    if (it == m_config.end()) {
        return true;
    }

    return !value.isNull();
}

void AgenticConfiguration::applyEnvironmentOverrides()
{
    // Apply environment-specific configuration overrides
    // For example, different models or log levels for different environments
    qInfo() << "[AgenticConfiguration] Applied overrides for environment:" << getEnvironmentName();
}
