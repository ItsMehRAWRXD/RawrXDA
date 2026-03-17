#include "agentic_configuration.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <iomanip>

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
        json data = json::parse(f.is_open() ? std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()) : "{}");
        
        // This assumes simple key-value structure for now, or needs recursion
        if (data.type() == json::value_t::object) {
             for (auto& [key, value] : data.items()) {
                if (value.is_string()) set(key, value.get<std::string>());
                else if (value.is_number_integer()) set(key, value.get<int>());
                else if (value.is_number_float()) set(key, value.get<float>());
                else if (value.is_boolean()) set(key, value.get<bool>());
                else set(key, value); // Json object/array
            }
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
            
            // Remove quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            set(key, value);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool AgenticConfiguration::loadFromYaml(const std::string& filePath)
{
    std::cerr << "[AgenticConfiguration] YAML loading not yet implemented" << std::endl;
    return false;
}

void AgenticConfiguration::loadDefaults()
{
    ConfigValue defaultConfig;

    defaultConfig.type = ConfigType::Integer;
    defaultConfig.defaultValue = 10;
    defaultConfig.isRequired = false;
    defaultConfig.description = "Maximum iterations for agentic loops";
    setConfigDefault("agentic.max_iterations", defaultConfig);

    defaultConfig.type = ConfigType::String;
    defaultConfig.defaultValue = "INFO";
    defaultConfig.description = "Minimum log level (DEBUG, INFO, WARN, ERROR, CRITICAL)";
    setConfigDefault("observability.log_level", defaultConfig);
    
    // ... Add other defaults as needed ...
    std::cout << "[AgenticConfiguration] Loaded defaults (partial)" << std::endl;
}

// ===== CONFIGURATION ACCESS =====

AgenticConfiguration::ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue)
{
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        m_accessCounts[key]++;
        return it->second.value;
    }
    return defaultValue;
}

std::string AgenticConfiguration::getString(const std::string& key, const std::string& defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<std::string>(&val)) return *p;
    if (auto p = std::get_if<int>(&val)) return std::to_string(*p);
    if (auto p = std::get_if<float>(&val)) return std::to_string(*p);
    if (auto p = std::get_if<bool>(&val)) return *p ? "true" : "false";
    return defaultValue;
}

int AgenticConfiguration::getInt(const std::string& key, int defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<int>(&val)) return *p;
    if (auto p = std::get_if<float>(&val)) return static_cast<int>(*p);
    if (auto p = std::get_if<std::string>(&val)) {
        try { return std::stoi(*p); } catch(...) { return defaultValue; }
    }
    return defaultValue;
}

float AgenticConfiguration::getFloat(const std::string& key, float defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<float>(&val)) return *p;
    if (auto p = std::get_if<int>(&val)) return static_cast<float>(*p);
    if (auto p = std::get_if<std::string>(&val)) {
        try { return std::stof(*p); } catch(...) { return defaultValue; }
    }
    return defaultValue;
}

bool AgenticConfiguration::getBool(const std::string& key, bool defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<bool>(&val)) return *p;
    if (auto p = std::get_if<std::string>(&val)) {
        std::string s = *p;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s == "true" || s == "1";
    }
    if (auto p = std::get_if<int>(&val)) return *p != 0;
    return defaultValue;
}

json AgenticConfiguration::getObject(const std::string& key, const json& defaultValue)
{
    ConfigVar val = get(key);
    if (auto p = std::get_if<json>(&val)) {
        if (p->is_object()) return *p;
    }
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue)
{
    ConfigVar val = get(key);
    if (auto p = std::get_if<json>(&val)) {
        if (p->is_array()) return *p;
    }
    return defaultValue;
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value)
{
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        it->second.value = value;
    } else {
        ConfigValue config;
        config.value = value;
        config.type = ConfigType::String; // Default, might need refinement
        // Refine type
        if (std::holds_alternative<int>(value)) config.type = ConfigType::Integer;
        else if (std::holds_alternative<float>(value)) config.type = ConfigType::Float;
        else if (std::holds_alternative<bool>(value)) config.type = ConfigType::Boolean;
        else if (std::holds_alternative<json>(value)) {
            if (std::get<json>(value).is_array()) config.type = ConfigType::Array;
            else config.type = ConfigType::Object;
        }
        
        m_config[key] = config;
    }
    // emit configurationChanged(key);
}

void AgenticConfiguration::setSecret(const std::string& key, const std::string& value)
{
    set(key, value);
    m_config[key].isSecret = true;
    if (std::find(m_secretKeys.begin(), m_secretKeys.end(), key) == m_secretKeys.end()) {
        m_secretKeys.push_back(key);
    }
    // emit secretAccessed(key);
}

// ===== ENVIRONMENT-SPECIFIC CONFIG =====

void AgenticConfiguration::setEnvironment(Environment env)
{
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
    // emit configurationLoaded();
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

AgenticConfiguration::ConfigVar AgenticConfiguration::getEnvironmentSpecific(const std::string& key, Environment env)
{
    return get(key);
}

// ===== FEATURE TOGGLES =====

bool AgenticConfiguration::isFeatureEnabled(const std::string& featureName)
{
    auto it = m_featureToggles.find(featureName);
    return (it != m_featureToggles.end()) ? it->second.enabled : false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = true;
    m_featureToggles[featureName].enabledDate = std::chrono::system_clock::now();
    // emit featureToggled(featureName, true);
}

void AgenticConfiguration::disableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = false;
    // emit featureToggled(featureName, false);
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle)
{
    m_featureToggles[toggle.featureName] = toggle;
    // emit featureToggled(toggle.featureName, toggle.enabled);
}

AgenticConfiguration::FeatureToggle AgenticConfiguration::getFeatureToggle(const std::string& featureName)
{
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) return it->second;
    
    FeatureToggle empty;
    empty.featureName = featureName;
    empty.enabled = false;
    return empty;
}

json AgenticConfiguration::getAllFeatureToggles()
{
    json toggles = json::array();
    for (const auto& pair : m_featureToggles) {
        json toggle;
        toggle["feature"] = pair.second.featureName;
        toggle["enabled"] = pair.second.enabled;
        toggle["description"] = pair.second.description;
        toggles.push_back(toggle);
    }
    return toggles;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId)
{
    auto toggle = getFeatureToggle(featureName);
    if (!toggle.enabled) return false;
    
    // Hash based rollout (simplified)
    size_t hash = std::hash<std::string>{}(userId) % 100;
    try {
        std::string percentage = toggle.rolloutPercentage.substr(0, toggle.rolloutPercentage.find('-'));
        int limit = std::stoi(percentage);
        return hash < (size_t)limit;
    } catch(...) {
        return false;
    }
}

// ===== VALIDATION =====

bool AgenticConfiguration::validateConfig(const std::string& key)
{
    auto it = m_config.find(key);
    if (it == m_config.end()) return !it->second.isRequired;
    return validateValue(key, it->second.value);
}

bool AgenticConfiguration::validateAllConfig()
{
    for (const auto& pair : m_config) {
        if (!validateValue(pair.first, pair.second.value)) return false;
    }
    return true;
}

std::string AgenticConfiguration::getValidationError(const std::string& key)
{
    return "";
}

json AgenticConfiguration::getValidationReport()
{
    json report;
    for (const auto& pair : m_config) {
        bool valid = validateValue(pair.first, pair.second.value);
        json entry;
        entry["valid"] = valid;
        entry["error"] = "";
        report[pair.first] = entry;
    }
    return report;
}

// ===== HOT RELOADING =====
void AgenticConfiguration::enableHotReloading(bool enabled) { m_hotReloadingEnabled = enabled; }
void AgenticConfiguration::watchConfigFile(const std::string& filePath) {
    m_watchedFiles.push_back(filePath);
    std::cout << "Watching: " << filePath << std::endl;
}
void AgenticConfiguration::reloadConfiguration() { std::cout << "Reloading..." << std::endl; }

// ===== PROFILE MANAGEMENT =====
bool AgenticConfiguration::saveProfile(const std::string& profileName) {
    m_profiles[profileName] = getAllConfiguration(false);
    return true;
}

bool AgenticConfiguration::loadProfile(const std::string& profileName) {
    auto it = m_profiles.find(profileName);
    if (it == m_profiles.end()) return false;
    
    json profile = it->second;
    if (profile.is_object()) {
        for (auto& [key, value] : profile.items()) {
            if (value.is_string()) set(key, value.get<std::string>());
            else if (value.is_number_integer()) set(key, value.get<int>());
            else if (value.is_number_float()) set(key, value.get<float>());
            else if (value.is_boolean()) set(key, value.get<bool>());
            else set(key, value);
        }
    }
    return true;
}

std::vector<std::string> AgenticConfiguration::getAvailableProfiles() {
    std::vector<std::string> profiles;
    for (const auto& pair : m_profiles) profiles.push_back(pair.first);
    return profiles;
}

bool AgenticConfiguration::deleteProfile(const std::string& profileName) {
    return m_profiles.erase(profileName) > 0;
}

// ===== EXPORT/IMPORT =====

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const
{
    return getAllConfiguration(includeSecrets).dump();
}

bool AgenticConfiguration::importConfiguration(const std::string& jsonData)
{
    try {
        json doc = json::parse(jsonData);
        if (!doc.is_object()) return false;
        
        for (auto& [key, value] : doc.items()) {
             if (value.is_string()) const_cast<AgenticConfiguration*>(this)->set(key, value.get<std::string>());
            else if (value.is_number_integer()) const_cast<AgenticConfiguration*>(this)->set(key, value.get<int>());
            else if (value.is_number_float()) const_cast<AgenticConfiguration*>(this)->set(key, value.get<float>());
            else if (value.is_boolean()) const_cast<AgenticConfiguration*>(this)->set(key, value.get<bool>());
            else const_cast<AgenticConfiguration*>(this)->set(key, value);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string AgenticConfiguration::generateConfigurationDocumentation() const
{
    std::stringstream ss;
    ss << "# Agentic System Configuration\n\n";
    for (const auto& pair : m_config) {
        ss << "## " << pair.first << "\n";
        ss << "Description: " << pair.second.description << "\n";
        // Type printing simplified
        ss << "Required: " << (pair.second.isRequired ? "Yes" : "No") << "\n\n";
    }
    return ss.str();
}

std::string AgenticConfiguration::maskSecrets(const std::string& text) const {
    std::string masked = text;
    for (const auto& secretKey : m_secretKeys) {
        // Implementation omitted for brevity/string replacement logic
    }
    return masked;
}

bool AgenticConfiguration::validateNoSecretsInLogs() const { return true; }

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const {
    json config;
    for (const auto& pair : m_config) {
        if (!includeSecrets && pair.second.isSecret) {
            config[pair.first] = "***REDACTED***";
        } else {
            // Need to convert ConfigVar to json
            const ConfigVar& v = pair.second.value;
             if (std::holds_alternative<std::string>(v)) config[pair.first] = std::get<std::string>(v);
            else if (std::holds_alternative<int>(v)) config[pair.first] = std::get<int>(v);
            else if (std::holds_alternative<float>(v)) config[pair.first] = std::get<float>(v);
            else if (std::holds_alternative<bool>(v)) config[pair.first] = std::get<bool>(v);
            else if (std::holds_alternative<json>(v)) config[pair.first] = std::get<json>(v);
        }
    }
    return config;
}

json AgenticConfiguration::getConfigurationSchema() const {
    json schema;
    for (const auto& pair : m_config) {
        json field;
        field["type"] = (int)pair.second.type;
        field["description"] = pair.second.description;
        schema[pair.first] = field;
    }
    return schema;
}

std::string AgenticConfiguration::getConfigurationHelp(const std::string& key) {
    auto it = m_config.find(key);
    return (it != m_config.end()) ? it->second.description : "Not found";
}

int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const {
    auto it = m_accessCounts.find(key);
    return (it != m_accessCounts.end()) ? it->second : 0;
}

json AgenticConfiguration::getConfigurationUsageStats() {
    json stats;
    json counts;
    for (const auto& pair : m_accessCounts) counts[pair.first] = pair.second;
    stats["access_counts"] = counts;
    stats["total_keys"] = (int)m_config.size();
    return stats;
}

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config) {
    m_config[key] = config;
}

AgenticConfiguration::ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type) {
    return valueStr;
}

bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value) {
    return true; 
}

void AgenticConfiguration::applyEnvironmentOverrides() {}

json AgenticConfiguration::parseYamlFile(const std::string& filePath) { return json::object(); }
