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

bool AgenticConfiguration::loadFromYaml(const std::string& filePath) { return false; }

void AgenticConfiguration::loadDefaults()
{
    ConfigValue conf;
    conf.isSecret = false;
    conf.isRequired = false;
    conf.type = ConfigType::Integer;
    conf.value = 10;
    conf.description = "Max iterations";
    setConfigDefault("agentic.max_iterations", conf);
    
    conf.type = ConfigType::String;
    conf.value = std::string("INFO");
    conf.description = "Log level";
    setConfigDefault("observability.log_level", conf);
}

AgenticConfiguration::ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue)
{
    m_accessCounts[key]++;
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        if (it->second.isSecret) emitSecretAccessed(key);
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
    if (auto p = std::get_if<json>(&val)) if (p->is_object()) return *p;
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue)
{
    ConfigVar val = get(key, defaultValue);
    if (auto p = std::get_if<json>(&val)) if (p->is_array()) return *p;
    return defaultValue;
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value)
{
    ConfigValue conf;
    auto it = m_config.find(key);
    if (it != m_config.end()) conf = it->second;
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

void AgenticConfiguration::setEnvironment(Environment env) { m_currentEnvironment = env; applyEnvironmentOverrides(); emitConfigurationLoaded(); }
std::string AgenticConfiguration::getEnvironmentName() const { return "Production"; }
AgenticConfiguration::ConfigVar AgenticConfiguration::getEnvironmentSpecific(const std::string& key, Environment env) { return get(key); }

bool AgenticConfiguration::isFeatureEnabled(const std::string& featureName)
{
    auto it = m_featureToggles.find(featureName);
    return (it != m_featureToggles.end()) ? it->second.enabled : false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = true;
    m_featureToggles[featureName].featureName = featureName;
    emitFeatureToggled(featureName, true);
}

void AgenticConfiguration::disableFeature(const std::string& featureName)
{
    m_featureToggles[featureName].enabled = false;
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
    for (const auto& [n, t] : m_featureToggles) {
        json obj; obj["name"] = n; obj["enabled"] = t.enabled;
        arr.push_back(obj);
    }
    return arr;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId)
{
    return isFeatureEnabled(featureName);
}

bool AgenticConfiguration::validateConfig(const std::string& key) { return true; }
bool AgenticConfiguration::validateAllConfig() { return true; }
std::string AgenticConfiguration::getValidationError(const std::string& key) { return ""; }
json AgenticConfiguration::getValidationReport() { return json::object(); }

void AgenticConfiguration::enableHotReloading(bool enabled) { m_hotReloadingEnabled = enabled; }
void AgenticConfiguration::watchConfigFile(const std::string& filePath) { m_watchedFiles.push_back(filePath); }
void AgenticConfiguration::reloadConfiguration() { emitConfigurationReloaded(); }

bool AgenticConfiguration::saveProfile(const std::string& profileName) { return false; }
bool AgenticConfiguration::loadProfile(const std::string& profileName) { return false; }
std::vector<std::string> AgenticConfiguration::getAvailableProfiles() { return {}; }
bool AgenticConfiguration::deleteProfile(const std::string& profileName) { return false; }

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const { return getAllConfiguration(includeSecrets).dump(); }
bool AgenticConfiguration::importConfiguration(const std::string& jsonData) { return false; }
std::string AgenticConfiguration::generateConfigurationDocumentation() const { return ""; }

std::string AgenticConfiguration::maskSecrets(const std::string& text) const { return text; }
bool AgenticConfiguration::validateNoSecretsInLogs() const { return true; }

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const
{
    json j = json::object();
    for (const auto& [key, val] : m_config) {
        if (!includeSecrets && val.isSecret) continue;
        if (auto p = std::get_if<std::string>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<int>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<float>(&val.value)) j[key] = *p;
        else if (auto p = std::get_if<bool>(&val.value)) j[key] = *p;
    }
    return j;
}

json AgenticConfiguration::getConfigurationSchema() const { return json::object(); }
std::string AgenticConfiguration::getConfigurationHelp(const std::string& key) { return ""; }
int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const { return 0; }
json AgenticConfiguration::getConfigurationUsageStats() { return json::object(); }

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config) { m_config[key] = config; }
AgenticConfiguration::ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type) { return valueStr; }
bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value) { return true; }
void AgenticConfiguration::applyEnvironmentOverrides() {}
json AgenticConfiguration::parseYamlFile(const std::string& filePath) { return json::object(); }
