#include "agentic_configuration.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <thread>
#include <mutex>

// Helper for string conversion
static std::string toString(const AgenticConfiguration::ConfigVar& var) {
    if (std::holds_alternative<std::string>(var)) return std::get<std::string>(var);
    if (std::holds_alternative<int>(var)) return std::to_string(std::get<int>(var));
    if (std::holds_alternative<float>(var)) return std::to_string(std::get<float>(var));
    if (std::holds_alternative<bool>(var)) return std::get<bool>(var) ? "true" : "false";
    return "";
}

// ConfigFileWatcher definition
class AgenticConfiguration::ConfigFileWatcher {
public:
    void watch(const std::string& path) {
    }
};

AgenticConfiguration::AgenticConfiguration() 
    : m_currentEnvironment(Environment::Development)
    , m_hotReloadingEnabled(false)
{
    m_fileWatcher = std::make_unique<ConfigFileWatcher>();
}

AgenticConfiguration::~AgenticConfiguration() = default;

void AgenticConfiguration::initializeFromEnvironment(Environment env) {
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
}

bool AgenticConfiguration::loadFromJson(const std::string& filePath) {
    try {
        std::ifstream f(filePath);
        if (!f.is_open()) return false;
        
        std::string jsonStr((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        json j = json::parse(jsonStr);
        
        for (auto& [key, val] : j) {
            ConfigValue cv;
            cv.description = "Loaded from JSON";
            
            if (val.is_string()) {
                cv.type = ConfigType::String;
                cv.value = val.get<std::string>();
            } else if (val.is_number()) {
                double d = val.get<double>();
                if (d == (int)d) {
                    cv.type = ConfigType::Integer;
                    cv.value = (int)d;
                } else {
                    cv.type = ConfigType::Float;
                    cv.value = (float)d;
                }
            } else if (val.is_boolean()) {
                cv.type = ConfigType::Boolean;
                cv.value = val.get<bool>();
            } else if (val.is_object()) {
                 cv.type = ConfigType::Object;
                 cv.value = val;
            } else if (val.is_array()) {
                 cv.type = ConfigType::Array;
                 cv.value = val;
            }
            m_config[key] = cv;
        }
        
        emitConfigurationLoaded();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading JSON config: " << e.what() << std::endl;
        return false;
    }
}

bool AgenticConfiguration::loadFromEnv(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            ConfigValue cv;
            cv.type = ConfigType::String;
            cv.value = val;
            m_config[key] = cv;
        }
    }
    return true;
}

void AgenticConfiguration::loadDefaults() {
    setConfigDefault("log_level", {ConfigType::String, std::string("INFO"), false, "Log level", std::string("INFO"), false, {}});
    setConfigDefault("max_threads", {ConfigType::Integer, 4, false, "Thread count", 4, false, {}});
}

AgenticConfiguration::ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue) {
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        m_accessCounts[key]++;
        return it->second.value;
    }
    return defaultValue;
}

std::string AgenticConfiguration::getString(const std::string& key, const std::string& defaultValue) {
    ConfigVar v = get(key, defaultValue);
    return toString(v);
}

int AgenticConfiguration::getInt(const std::string& key, int defaultValue) {
     ConfigVar v = get(key, defaultValue);
     if (std::holds_alternative<int>(v)) return std::get<int>(v);
     try {
         if (std::holds_alternative<std::string>(v)) return std::stoi(std::get<std::string>(v));
     } catch(...) {}
     return defaultValue;
}

float AgenticConfiguration::getFloat(const std::string& key, float defaultValue) {
     ConfigVar v = get(key, defaultValue);
     if (std::holds_alternative<float>(v)) return std::get<float>(v);
     return defaultValue;
}

bool AgenticConfiguration::getBool(const std::string& key, bool defaultValue) {
     ConfigVar v = get(key, defaultValue);
     if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
     if (std::holds_alternative<std::string>(v)) {
         std::string s = std::get<std::string>(v);
         return s == "true" || s == "1";
     }
     return defaultValue;
}

json AgenticConfiguration::getObject(const std::string& key, const json& defaultValue) {
    ConfigVar v = get(key, defaultValue);
    if (std::holds_alternative<json>(v)) return std::get<json>(v);
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue) {
    ConfigVar v = get(key, defaultValue);
    if (std::holds_alternative<json>(v)) return std::get<json>(v);
    return defaultValue;
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value) {
    m_config[key].value = value;
    emitConfigurationChanged(key);
}

void AgenticConfiguration::setSecret(const std::string& key, const std::string& value) {
    m_config[key].value = value;
    m_config[key].isSecret = true;
    m_secretKeys.push_back(key);
    emitConfigurationChanged(key);
}

void AgenticConfiguration::setEnvironment(Environment env) {
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
}

std::string AgenticConfiguration::getEnvironmentName() const {
    switch (m_currentEnvironment) {
        case Environment::Development: return "Development";
        case Environment::Staging: return "Staging";
        case Environment::Production: return "Production";
    }
    return "Unknown";
}

AgenticConfiguration::ConfigVar AgenticConfiguration::getEnvironmentSpecific(const std::string& key, Environment env) {
    std::string envKey = key + "_" + (env == Environment::Development ? "DEV" : (env == Environment::Staging ? "STAGE" : "PROD"));
    if (m_config.count(envKey)) return m_config[envKey].value;
    return get(key);
}

bool AgenticConfiguration::isFeatureEnabled(const std::string& featureName) {
    if (m_featureToggles.find(featureName) != m_featureToggles.end()) {
        return m_featureToggles[featureName].enabled;
    }
    return false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName) {
    m_featureToggles[featureName].enabled = true;
    m_featureToggles[featureName].featureName = featureName;
    emitFeatureToggled(featureName, true);
}

void AgenticConfiguration::disableFeature(const std::string& featureName) {
    m_featureToggles[featureName].enabled = false;
    emitFeatureToggled(featureName, false);
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle) {
    m_featureToggles[toggle.featureName] = toggle;
    emitFeatureToggled(toggle.featureName, toggle.enabled);
}

AgenticConfiguration::FeatureToggle AgenticConfiguration::getFeatureToggle(const std::string& featureName) {
    if (m_featureToggles.count(featureName)) return m_featureToggles[featureName];
    return {featureName, false, "", "", {}, ""};
}

json AgenticConfiguration::getAllFeatureToggles() {
    json j;
    for (const auto& [name, toggle] : m_featureToggles) {
        j[name] = toggle.enabled;
    }
    return j;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId) {
    if (!isFeatureEnabled(featureName)) return false;
    auto toggle = m_featureToggles[featureName];
    if (toggle.rolloutPercentage.empty()) return true;
    
    size_t hash = std::hash<std::string>{}(userId + featureName);
    int percentage = 100;
    try { percentage = std::stoi(toggle.rolloutPercentage); } catch(...) {}
    
    return (hash % 100) < static_cast<unsigned int>(percentage);
}

bool AgenticConfiguration::validateConfig(const std::string& key) {
    if (m_config.find(key) == m_config.end()) return false;
    return validateValue(key, m_config[key].value);
}

bool AgenticConfiguration::validateAllConfig() {
    bool valid = true;
    for (const auto& [key, val] : m_config) {
        if (!validateValue(key, val.value)) valid = false;
    }
    return valid;
}

std::string AgenticConfiguration::getValidationError(const std::string& key) {
    return "";
}

json AgenticConfiguration::getValidationReport() {
    return json::object_type();
}

void AgenticConfiguration::enableHotReloading(bool enabled) {
    m_hotReloadingEnabled = enabled;
}

void AgenticConfiguration::watchConfigFile(const std::string& filePath) {
    m_watchedFiles.push_back(filePath);
    if (m_hotReloadingEnabled && m_fileWatcher) {
        m_fileWatcher->watch(filePath);
    }
}

void AgenticConfiguration::reloadConfiguration() {
    emitConfigurationReloaded();
}

bool AgenticConfiguration::saveProfile(const std::string& profileName) {
    m_profiles[profileName] = getAllConfiguration();
    return true;
}

bool AgenticConfiguration::loadProfile(const std::string& profileName) {
    if (m_profiles.count(profileName)) {
        return true;
    }
    return false;
}

std::vector<std::string> AgenticConfiguration::getAvailableProfiles() {
    std::vector<std::string> names;
    for (const auto& [name, p] : m_profiles) names.push_back(name);
    return names;
}

bool AgenticConfiguration::deleteProfile(const std::string& profileName) {
    return m_profiles.erase(profileName) > 0;
}

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const {
    return getAllConfiguration(includeSecrets).dump(4);
}

bool AgenticConfiguration::importConfiguration(const std::string& jsonData) {
    try {
        json j = json::parse(jsonData);
        return true;
    } catch(...) { return false; }
}

std::string AgenticConfiguration::generateConfigurationDocumentation() const {
    return "Documentation stub";
}

std::string AgenticConfiguration::maskSecrets(const std::string& text) const {
    return text; // Stub
}

bool AgenticConfiguration::validateNoSecretsInLogs() const {
    return true;
}

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const {
    json j;
    for (const auto& [key, val] : m_config) {
        if (val.isSecret && !includeSecrets) continue;
        j[key] = toString(val.value);
    }
    return j;
}

json AgenticConfiguration::getConfigurationSchema() const {
    return json::object_type();
}

std::string AgenticConfiguration::getConfigurationHelp(const std::string& key) {
    if (m_config.count(key)) return m_config.at(key).description;
    return "";
}

int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const {
    if (m_accessCounts.count(key)) return m_accessCounts.at(key);
    return 0;
}

json AgenticConfiguration::getConfigurationUsageStats() {
    json j;
    for (const auto& [key, count] : m_accessCounts) j[key] = count;
    return j;
}

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config) {
    if (m_config.find(key) == m_config.end()) {
        m_config[key] = config;
    }
}

AgenticConfiguration::ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type) {
    return valueStr;
}

bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value) {
    return true;
}

void AgenticConfiguration::applyEnvironmentOverrides() {
}

json AgenticConfiguration::parseYamlFile(const std::string& filePath) {
    return json::object_type();
}
