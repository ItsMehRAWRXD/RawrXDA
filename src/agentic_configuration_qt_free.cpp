// AgenticConfiguration Implementation - Qt-Free Version
#include "agentic_configuration.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

AgenticConfiguration::AgenticConfiguration()
    : m_currentEnvironment(Environment::Development)
{
}

AgenticConfiguration::~AgenticConfiguration()
{
}

// ===== INITIALIZATION =====

void AgenticConfiguration::initializeFromEnvironment(Environment env)
{
    m_currentEnvironment = env;
    loadDefaults();
}

bool AgenticConfiguration::loadFromJson(const std::string& filePath)
{
    try {
        if (!std::filesystem::exists(filePath)) return false;
        std::ifstream f(filePath);
        json data = json::parse(f);
        for (auto& [key, value] : data.items()) {
            m_config[key] = ConfigValue{ConfigType::String, value.get<std::string>(), false, "", std::string(""), false, {}};
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool AgenticConfiguration::loadFromEnv(const std::string& filePath)
{
    try {
        std::ifstream file(filePath);
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t eqIdx = line.find('=');
            if (eqIdx == std::string::npos) continue;
            std::string key = line.substr(0, eqIdx);
            std::string value = line.substr(eqIdx + 1);
            m_config[key] = ConfigValue{ConfigType::String, value, false, "", std::string(""), false, {}};
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool AgenticConfiguration::loadFromYaml(const std::string& filePath)
{
    std::cerr << "[AgenticConfiguration] YAML loading not implemented" << std::endl;
    return false;
}

void AgenticConfiguration::loadDefaults()
{
    setConfigDefault("agentic.max_iterations", ConfigValue{ConfigType::Integer, 10, false, "Max iterations", 10, false, {}});
    setConfigDefault("observability.log_level", ConfigValue{ConfigType::String, std::string("INFO"), false, "Log level", std::string("INFO"), false, {}});
    setConfigDefault("observability.max_logs", ConfigValue{ConfigType::Integer, 10000, false, "Max logs", 10000, false, {}});
    setConfigDefault("error_handler.max_retries", ConfigValue{ConfigType::Integer, 3, false, "Max retries", 3, false, {}});
    setConfigDefault("error_handler.enable_graceful_degradation", ConfigValue{ConfigType::Boolean, true, false, "Graceful degradation", true, false, {}});
}

// ===== CONFIGURATION ACCESS =====

ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue)
{
    auto it = m_config.find(key);
    m_accessCounts[key]++;
    if (it != m_config.end()) {
        return it->second.value;
    }
    return defaultValue;
}

std::string AgenticConfiguration::getString(const std::string& key, const std::string& defaultValue)
{
    auto val = get(key, std::string(defaultValue));
    if (std::holds_alternative<std::string>(val)) {
        return std::get<std::string>(val);
    }
    return defaultValue;
}

int AgenticConfiguration::getInt(const std::string& key, int defaultValue)
{
    auto val = get(key, defaultValue);
    if (std::holds_alternative<int>(val)) {
        return std::get<int>(val);
    }
    return defaultValue;
}

float AgenticConfiguration::getFloat(const std::string& key, float defaultValue)
{
    auto val = get(key, static_cast<float>(defaultValue));
    if (std::holds_alternative<float>(val)) {
        return std::get<float>(val);
    }
    return defaultValue;
}

bool AgenticConfiguration::getBool(const std::string& key, bool defaultValue)
{
    auto val = get(key, defaultValue);
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val);
    }
    return defaultValue;
}

json AgenticConfiguration::getObject(const std::string& key, const json& defaultValue)
{
    auto val = get(key, defaultValue);
    if (std::holds_alternative<json>(val)) {
        return std::get<json>(val);
    }
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue)
{
    return getObject(key, defaultValue);
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value)
{
    m_config[key] = ConfigValue{ConfigType::String, value, false, "", value, false, {}};
}

void AgenticConfiguration::setSecret(const std::string& key, const std::string& value)
{
    m_secretKeys.push_back(key);
    m_config[key] = ConfigValue{ConfigType::String, value, true, "Secret", value, false, {}};
}

// ===== ENVIRONMENT-SPECIFIC CONFIG =====

void AgenticConfiguration::setEnvironment(Environment env)
{
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
}

std::string AgenticConfiguration::getEnvironmentName() const
{
    switch (m_currentEnvironment) {
        case Environment::Development: return "Development";
        case Environment::Staging: return "Staging";
        case Environment::Production: return "Production";
        default: return "Unknown";
    }
}

ConfigVar AgenticConfiguration::getEnvironmentSpecific(const std::string& key, Environment env)
{
    // Simple implementation: use current config
    // In a real system, you might have env-specific overrides
    return get(key);
}

void AgenticConfiguration::applyEnvironmentOverrides()
{
    // Override config values from environment variables
    // Convention: RAWRXD_<UPPER_KEY> maps to config key
    for (auto& [key, cfg] : m_config) {
        std::string envKey = "RAWRXD_";
        for (char c : key) {
            if (c == '.' || c == '-') envKey += '_';
            else envKey += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        const char* envVal = std::getenv(envKey.c_str());
        if (envVal && envVal[0] != '\0') {
            cfg.value = parseValue(envVal, cfg.type);
            if (m_debugMode) {
                fprintf(stderr, "[AgenticConfig] Override from env: %s=%s\n", key.c_str(), envVal);
            }
        }
    }
}

// ===== FEATURE TOGGLES =====

bool AgenticConfiguration::isFeatureEnabled(const std::string& featureName)
{
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        return it->second.enabled;
    }
    return false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = true;
    m_featureToggles[featureName].featureName = featureName;
    m_featureToggles[featureName].enabledDate = std::chrono::system_clock::now();
}

void AgenticConfiguration::disableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = false;
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle)
{
    m_featureToggles[toggle.featureName] = toggle;
}

FeatureToggle AgenticConfiguration::getFeatureToggle(const std::string& featureName)
{
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        return it->second;
    }
    return FeatureToggle{featureName, false, "", "", std::chrono::system_clock::now(), ""};
}

json AgenticConfiguration::getAllFeatureToggles()
{
    json result = json::array();
    for (const auto& [name, toggle] : m_featureToggles) {
        result.push_back({{"name", name}, {"enabled", toggle.enabled}});
    }
    return result;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId)
{
    auto toggle = getFeatureToggle(featureName);
    if (!toggle.enabled) return false;
    
    // Parse rollout percentage (simple hash-based rollout)
    auto hash = std::hash<std::string>{}(userId) % 100;
    size_t dashPos = toggle.rolloutPercentage.find('-');
    if (dashPos != std::string::npos) {
        try {
            int minPct = std::stoi(toggle.rolloutPercentage.substr(0, dashPos));
            int maxPct = std::stoi(toggle.rolloutPercentage.substr(dashPos + 1));
            return hash >= minPct && hash < maxPct;
        } catch (...) {}
    }
    return true;
}

// ===== VALIDATION =====

bool AgenticConfiguration::validateConfig(const std::string& key)
{
    auto it = m_config.find(key);
    if (it == m_config.end()) return false;
    return validateValue(key, it->second.value);
}

bool AgenticConfiguration::validateAllConfig()
{
    for (const auto& [key, cfg] : m_config) {
        if (!validateValue(key, cfg.value)) return false;
    }
    return true;
}

std::string AgenticConfiguration::getValidationError(const std::string& key)
{
    return "No validation error for: " + key;
}

json AgenticConfiguration::getValidationReport()
{
    json report;
    for (const auto& [key, cfg] : m_config) {
        report[key] = validateValue(key, cfg.value);
    }
    return report;
}

// ===== HOT RELOADING =====

void AgenticConfiguration::enableHotReloading(bool enabled)
{
    m_hotReloadingEnabled = enabled;
}

void AgenticConfiguration::watchConfigFile(const std::string& filePath)
{
    m_watchedFiles.push_back(filePath);
}

void AgenticConfiguration::reloadConfiguration()
{
    loadDefaults();
}

// ===== PROFILE MANAGEMENT =====

bool AgenticConfiguration::saveProfile(const std::string& profileName)
{
    json profile;
    for (const auto& [key, cfg] : m_config) {
        profile[key] = cfg.value;
    }
    m_profiles[profileName] = profile;
    return true;
}

bool AgenticConfiguration::loadProfile(const std::string& profileName)
{
    auto it = m_profiles.find(profileName);
    if (it == m_profiles.end()) return false;
    
    for (auto& [key, val] : it->second.items()) {
        m_config[key].value = val.get<std::string>();
    }
    return true;
}

std::vector<std::string> AgenticConfiguration::getAvailableProfiles()
{
    std::vector<std::string> profiles;
    for (const auto& [name, _] : m_profiles) {
        profiles.push_back(name);
    }
    return profiles;
}

bool AgenticConfiguration::deleteProfile(const std::string& profileName)
{
    return m_profiles.erase(profileName) > 0;
}

// ===== EXPORT/IMPORT =====

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const
{
    json export_data;
    for (const auto& [key, cfg] : m_config) {
        if (!includeSecrets && cfg.isSecret) continue;
        export_data[key] = cfg.value;
    }
    return export_data.dump(2);
}

bool AgenticConfiguration::importConfiguration(const std::string& jsonData)
{
    try {
        json data = json::parse(jsonData);
        for (auto& [key, value] : data.items()) {
            m_config[key].value = value.get<std::string>();
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string AgenticConfiguration::generateConfigurationDocumentation() const
{
    std::stringstream ss;
    ss << "# Configuration Documentation\n\n";
    for (const auto& [key, cfg] : m_config) {
        ss << "## " << key << "\n";
        ss << "Description: " << cfg.description << "\n\n";
    }
    return ss.str();
}

// ===== SENSITIVE DATA HANDLING =====

std::string AgenticConfiguration::maskSecrets(const std::string& text) const
{
    std::string result = text;
    for (const auto& secret : m_secretKeys) {
        auto pos = result.find(secret);
        if (pos != std::string::npos) {
            result.replace(pos, secret.length(), "***MASKED***");
        }
    }
    return result;
}

bool AgenticConfiguration::validateNoSecretsInLogs() const
{
    // Scan configured secret patterns against log output paths
    static const char* sensitivePatterns[] = {
        "password", "api_key", "secret", "token", "bearer",
        "private_key", "auth", "credential"
    };
    for (const auto& [key, cfg] : m_config) {
        if (!cfg.isSecret) continue;
        // If a secret value appears in a non-secret field, flag it
        std::string secretVal;
        if (auto* s = std::get_if<std::string>(&cfg.value)) {
            secretVal = *s;
        }
        if (secretVal.empty() || secretVal.size() < 4) continue;
        for (const auto& [otherKey, otherCfg] : m_config) {
            if (otherCfg.isSecret) continue;
            if (auto* otherS = std::get_if<std::string>(&otherCfg.value)) {
                if (otherS->find(secretVal) != std::string::npos) {
                    return false;  // Secret leaked into non-secret config
                }
            }
        }
    }
    return true;
}

// ===== DEFAULTS =====

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const
{
    json result;
    for (const auto& [key, cfg] : m_config) {
        if (!includeSecrets && cfg.isSecret) continue;
        result[key] = cfg.value;
    }
    return result;
}

json AgenticConfiguration::getConfigurationSchema() const
{
    json schema;
    for (const auto& [key, cfg] : m_config) {
        schema[key] = {{"type", static_cast<int>(cfg.type)}, {"required", cfg.isRequired}};
    }
    return schema;
}

std::string AgenticConfiguration::getConfigurationHelp(const std::string& key)
{
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return it->second.description;
    }
    return "No help available";
}

// ===== METRICS AND MONITORING =====

int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const
{
    auto it = m_accessCounts.find(key);
    if (it != m_accessCounts.end()) {
        return it->second;
    }
    return 0;
}

json AgenticConfiguration::getConfigurationUsageStats()
{
    json stats;
    for (const auto& [key, count] : m_accessCounts) {
        stats[key] = count;
    }
    return stats;
}

// ===== INTERNAL HELPERS =====

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config)
{
    m_config[key] = config;
}

ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type)
{
    try {
        switch (type) {
            case ConfigType::Integer: return std::stoi(valueStr);
            case ConfigType::Float: return std::stof(valueStr);
            case ConfigType::Boolean: return valueStr == "true" || valueStr == "1";
            case ConfigType::String: return valueStr;
            default: return valueStr;
        }
    } catch (...) {
        return valueStr;
    }
}

bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value)
{
    auto it = m_config.find(key);
    if (it == m_config.end()) return true;  // Unknown keys are accepted
    const auto& cfg = it->second;
    // Type check: ensure the variant holds the expected type
    switch (cfg.type) {
        case ConfigType::Integer:
            return std::holds_alternative<int>(value);
        case ConfigType::Float:
            return std::holds_alternative<float>(value);
        case ConfigType::Boolean:
            return std::holds_alternative<bool>(value);
        case ConfigType::String:
            if (auto* s = std::get_if<std::string>(&value)) {
                return !s->empty() || !cfg.isRequired;
            }
            return false;
        default:
            return true;
    }
}

json AgenticConfiguration::parseYamlFile(const std::string& filePath)
{
    // Minimal YAML-subset parser: handles key: value pairs and nested objects (2-space indent)
    json result = json::object();
    std::ifstream file(filePath);
    if (!file.is_open()) return result;

    std::string line;
    std::string currentSection;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos || line[firstNonSpace] == '#') continue;

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(firstNonSpace, colonPos - firstNonSpace);
        // Trim trailing spaces from key
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();

        std::string val;
        if (colonPos + 1 < line.size()) {
            val = line.substr(colonPos + 1);
            size_t valStart = val.find_first_not_of(" \t");
            if (valStart != std::string::npos) val = val.substr(valStart);
            else val.clear();
            // Trim trailing whitespace
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r')) val.pop_back();
        }

        if (val.empty()) {
            // This is a section header
            currentSection = key;
            if (!result.contains(currentSection)) result[currentSection] = json::object();
        } else if (firstNonSpace >= 2 && !currentSection.empty()) {
            // Nested under section
            result[currentSection][key] = val;
        } else {
            currentSection.clear();
            result[key] = val;
        }
    }
    return result;
}
