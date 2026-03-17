// AgenticConfiguration Implementation (Pure C++20)
#include "agentic_configuration.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>

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
    applyEnvironmentOverrides();
}

bool AgenticConfiguration::loadFromJson(const std::string& filePath)
{
    try {
        if (!std::filesystem::exists(filePath)) return false;
        std::ifstream f(filePath);
        if (!f.is_open()) return false;
        
        json data = json::parse(f);
        
        for (auto& [key, value] : data.items()) {
            if (value.is_string()) set(key, value.get<std::string>());
            else if (value.is_boolean()) set(key, value.get<bool>());
            else if (value.is_number_integer()) set(key, value.get<int>());
            else if (value.is_number_float()) set(key, value.get<float>());
            // Objects/Arrays in Variant?
            else if (value.is_object() || value.is_array()) set(key, value);
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
    // YAML not implemented
    return false;
}

void AgenticConfiguration::loadDefaults()
{
    ConfigValue conf;
    conf.isSecret = false;
    conf.isRequired = false;
    
    // Agentic
    conf.type = ConfigType::Integer;
    conf.value = 10;
    conf.description = "Maximum iterations for agentic loops";
    setConfigDefault("agentic.max_iterations", conf);

    // Logs
    conf.type = ConfigType::String;
    conf.value = std::string("INFO");
    conf.description = "Minimum log level";
    setConfigDefault("observability.log_level", conf);
    
    // API
    conf.type = ConfigType::String;
    conf.value = std::string("");
    conf.description = "API Key";
    conf.isSecret = true;
    setConfigDefault("api.key", conf);
}

// ===== CONFIGURATION ACCESS =====

AgenticConfiguration::ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue)
{
    m_accessCounts[key]++;
    
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        if (it->second.isSecret) {
            emitSecretAccessed(key);
        }
        return it->second.value;
    }
    return defaultValue;
}

std::string AgenticConfiguration::getString(const std::string& key, const std::string& defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<std::string>(&val)) return *p;
    return defaultValue;
}

int AgenticConfiguration::getInt(const std::string& key, int defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<int>(&val)) return *p;
    return defaultValue;
}

float AgenticConfiguration::getFloat(const std::string& key, float defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<float>(&val)) return *p;
    if (auto p = std::get_if<int>(&val)) return static_cast<float>(*p);
    return defaultValue;
}

bool AgenticConfiguration::getBool(const std::string& key, bool defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<bool>(&val)) return *p;
    return defaultValue;
}

json AgenticConfiguration::getObject(const std::string& key, const json& defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<json>(&val)) {
        if (p->is_object()) return *p;
    }
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<json>(&val)) {
        if (p->is_array()) return *p;
    }
    return defaultValue;
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value)
{
    ConfigValue conf;
    // Keep existing metadata if present
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        conf = it->second;
    }
    conf.value = value;
    m_config[key] = conf;
    
    emitConfigurationChanged(key);
}

void AgenticConfiguration::setSecret(const std::string& key, const std::string& value)
{
    ConfigValue conf;
    auto it = m_config.find(key);
    if (it != m_config.end()) conf = it->second;
    
    conf.value = value;
    conf.isSecret = true;
    m_config[key] = conf;
    
    emitSecretAccessed(key);
}

// ===== ENVIRONMENT =====

void AgenticConfiguration::setEnvironment(Environment env)
{
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
    emitConfigurationLoaded();
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
    if (it != m_featureToggles.end()) return it->second.enabled;
    return false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = true;
    m_featureToggles[featureName].featureName = featureName;
    m_featureToggles[featureName].enabledDate = std::chrono::system_clock::now();
    emitFeatureToggled(featureName, true);
}

void AgenticConfiguration::disableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = false;
    m_featureToggles[featureName].featureName = featureName;
    emitFeatureToggled(featureName, false);
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle)
{
    m_featureToggles[toggle.featureName] = toggle;
    emitFeatureToggled(toggle.featureName, toggle.enabled);
}

AgenticConfiguration::FeatureToggle AgenticConfiguration::getFeatureToggle(const std::string& featureName)
{
    return m_featureToggles[featureName];
}

json AgenticConfiguration::getAllFeatureToggles()
{
    json arr = json::array();
    for (const auto& [name, toggle] : m_featureToggles) {
        json obj;
        obj["name"] = name;
        obj["enabled"] = toggle.enabled;
        obj["description"] = toggle.description;
        arr.push_back(obj);
    }
    return arr;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId)
{
    if (!isFeatureEnabled(featureName)) return false;
    FeatureToggle t = getFeatureToggle(featureName);
    if (t.rolloutPercentage.empty()) return true;
    
    try {
        int pct = std::stoi(t.rolloutPercentage);
        if (pct >= 100) return true;
        if (pct <= 0) return false;
        size_t h = std::hash<std::string>{}(userId);
        return (h % 100) < (size_t)pct;
    } catch(...) { return false; }
}

// ===== VALIDATION =====

bool AgenticConfiguration::validateConfig(const std::string& key)
{
    ConfigVar v = get(key);
    return validateValue(key, v);
}

bool AgenticConfiguration::validateAllConfig()
{
    bool valid = true;
    for (const auto& [key, val] : m_config) {
        if (!validateValue(key, val.value)) valid = false;
    }
    return valid;
}

std::string AgenticConfiguration::getValidationError(const std::string& key)
{
    return ""; // Stub
}

json AgenticConfiguration::getValidationReport()
{
    return json::object();
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
    for (const auto& f : m_watchedFiles) {
        if (f.ends_with(".json")) loadFromJson(f);
       // else if (f.ends_with(".env")) loadFromEnv(f);
    }
    emitConfigurationReloaded();
}

// ===== PROFILE =====

bool AgenticConfiguration::saveProfile(const std::string& profileName)
{
    m_profiles[profileName] = getAllConfiguration(true);
    return true;
}

bool AgenticConfiguration::loadProfile(const std::string& profileName)
{
    auto it = m_profiles.find(profileName);
    if (it == m_profiles.end()) return false;
    
     json p = it->second;
     for (auto& [key, val] : p.items()) {
         if (val.is_string()) set(key, val.get<std::string>());
         else if (val.is_boolean()) set(key, val.get<bool>());
         else if (val.is_number_integer()) set(key, val.get<int>());
         else if (val.is_number_float()) set(key, val.get<float>());
     }
     emitConfigurationLoaded();
     return true;
}

std::vector<std::string> AgenticConfiguration::getAvailableProfiles()
{
    std::vector<std::string> profs;
    for (const auto& [k,v] : m_profiles) profs.push_back(k);
    return profs;
}

bool AgenticConfiguration::deleteProfile(const std::string& profileName)
{
    return m_profiles.erase(profileName) > 0;
}

// ===== EXPORT =====

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const
{
    return getAllConfiguration(includeSecrets).dump(4);
}

bool AgenticConfiguration::importConfiguration(const std::string& jsonData)
{
    try {
        json j = json::parse(jsonData);
        for (auto& [key, val] : j.items()) {
             if (val.is_string()) const_cast<AgenticConfiguration*>(this)->set(key, val.get<std::string>());
             // Simplified
        }
        return true;
    } catch (...) { return false; }
}

std::string AgenticConfiguration::generateConfigurationDocumentation() const
{
    return "# Configuration";
}

// ===== SECRETS =====

std::string AgenticConfiguration::maskSecrets(const std::string& text) const
{
    std::string out = text;
    for (const auto& [k,v] : m_config) {
        if (v.isSecret) {
            std::string s;
            if (auto p = std::get_if<std::string>(&v.value)) s = *p;
            if (!s.empty()) {
                size_t pos = 0;
                while((pos = out.find(s, pos)) != std::string::npos) {
                    out.replace(pos, s.length(), "*****");
                    pos += 5;
                }
            }
        }
    }
    return out;
}

bool AgenticConfiguration::validateNoSecretsInLogs() const
{
    return true;
}

// ===== DEFAULTS / METRICS =====

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const
{
    json j = json::object();
    for (const auto& [key, val] : m_config) {
        if (val.isSecret && !includeSecrets) continue;
        
        if (auto p = std::get_if<std::string>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<int>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<bool>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<float>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<json>(&val.value)) j[key] = *p;
    }
    return j;
}

json AgenticConfiguration::getConfigurationSchema() const
{
    return json::object();
}

std::string AgenticConfiguration::getConfigurationHelp(const std::string& key)
{
    auto it = m_config.find(key);
    if (it != m_config.end()) return it->second.description;
    return "";
}

int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const
{
    auto it = m_accessCounts.find(key);
    if (it != m_accessCounts.end()) return it->second;
    return 0;
}

json AgenticConfiguration::getConfigurationUsageStats()
{
    json j = json::object();
    for (const auto& [key, val] : m_accessCounts) j[key] = val;
    return j;
}

// ===== PRIVATE =====

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config)
{
    m_config[key] = config;
}

AgenticConfiguration::ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type)
{
    if (type == ConfigType::Integer) return std::stoi(valueStr);
    if (type == ConfigType::Float) return std::stof(valueStr);
    if (type == ConfigType::Boolean) return (valueStr == "true" || valueStr == "1");
    // if (type == ConfigType::Array || type == ConfigType::Object) return json::parse(valueStr);
    return valueStr;
}

bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value)
{
    return true; 
}

void AgenticConfiguration::applyEnvironmentOverrides()
{
}

json AgenticConfiguration::parseYamlFile(const std::string& filePath)
{
    return json::object();
}
