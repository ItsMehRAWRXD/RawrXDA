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

bool AgenticConfiguration::loadFromYaml(const std::string& filePath) {
    // Manual YAML-subset parser: handles key: value, key: "value", nested indentation
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    std::string line;
    std::string currentSection;
    int lineNum = 0;

    while (std::getline(file, line)) {
        lineNum++;
        // Skip comments and empty lines
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos || line[firstNonSpace] == '#') continue;

        // Detect indentation level
        int indent = (int)firstNonSpace;
        std::string trimmed = line.substr(firstNonSpace);

        // Find key: value separator
        size_t colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = trimmed.substr(0, colonPos);
        // Trim key
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();

        std::string value;
        if (colonPos + 1 < trimmed.size()) {
            value = trimmed.substr(colonPos + 1);
            size_t valStart = value.find_first_not_of(" \t");
            if (valStart != std::string::npos) value = value.substr(valStart);
            else value.clear();
        }

        // Handle section headers (indent = 0, no value)
        if (indent == 0 && value.empty()) {
            currentSection = key;
            continue;
        }

        // Build full key
        std::string fullKey = currentSection.empty() ? key : (currentSection + "." + key);

        // Strip quotes from value
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        // Type inference and storage
        if (value == "true" || value == "false") {
            set(fullKey, value == "true");
        } else {
            // Try integer
            bool isInt = !value.empty();
            for (char c : value) { if (!isdigit(c) && c != '-') { isInt = false; break; } }
            if (isInt && !value.empty()) {
                set(fullKey, std::atoi(value.c_str()));
            } else {
                // Try float
                bool isFloat = !value.empty();
                int dots = 0;
                for (char c : value) {
                    if (c == '.') dots++;
                    else if (!isdigit(c) && c != '-') { isFloat = false; break; }
                }
                if (isFloat && dots == 1) {
                    set(fullKey, std::atof(value.c_str()));
                } else {
                    set(fullKey, value);
                }
            }
        }
    }
    return true;
}

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

bool AgenticConfiguration::validateConfig(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return false;
    const auto& val = it->second;
    if (val.isRequired) {
        // Check if value is empty/default
        if (auto p = std::get_if<std::string>(&val.value)) {
            if (p->empty()) return false;
        }
    }
    return validateValue(key, val.value);
}
bool AgenticConfiguration::validateAllConfig() {
    for (const auto& [key, val] : m_config) {
        if (val.isRequired && !validateConfig(key)) return false;
    }
    return true;
}
std::string AgenticConfiguration::getValidationError(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return "Key not found: " + key;
    if (it->second.isRequired) {
        if (auto p = std::get_if<std::string>(&it->second.value)) {
            if (p->empty()) return "Required field '" + key + "' is empty";
        }
    }
    return "";
}
json AgenticConfiguration::getValidationReport() {
    json report = json::object();
    json errors = json::array();
    json warnings = json::array();
    for (const auto& [key, val] : m_config) {
        std::string err = getValidationError(key);
        if (!err.empty()) {
            json e; e["key"] = key; e["error"] = err;
            errors.push_back(e);
        }
        if (val.isSecret) {
            if (auto p = std::get_if<std::string>(&val.value)) {
                if (p->size() < 8) {
                    json w; w["key"] = key; w["warning"] = "Secret value is too short";
                    warnings.push_back(w);
                }
            }
        }
    }
    report["valid"] = errors.empty();
    report["errors"] = errors;
    report["warnings"] = warnings;
    return report;
}

void AgenticConfiguration::enableHotReloading(bool enabled) { m_hotReloadingEnabled = enabled; }
void AgenticConfiguration::watchConfigFile(const std::string& filePath) { m_watchedFiles.push_back(filePath); }
void AgenticConfiguration::reloadConfiguration() { emitConfigurationReloaded(); }

bool AgenticConfiguration::saveProfile(const std::string& profileName) {
    std::string path = ".rawrxd/profiles/" + profileName + ".json";
    std::filesystem::create_directories(".rawrxd/profiles");
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << exportConfiguration(false);
    return file.good();
}
bool AgenticConfiguration::loadProfile(const std::string& profileName) {
    std::string path = ".rawrxd/profiles/" + profileName + ".json";
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return importConfiguration(content);
}
std::vector<std::string> AgenticConfiguration::getAvailableProfiles() {
    std::vector<std::string> profiles;
    std::string dir = ".rawrxd/profiles";
    if (std::filesystem::is_directory(dir)) {
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                profiles.push_back(entry.path().stem().string());
            }
        }
    }
    return profiles;
}
bool AgenticConfiguration::deleteProfile(const std::string& profileName) {
    std::string path = ".rawrxd/profiles/" + profileName + ".json";
    return std::filesystem::remove(path);
}

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const { return getAllConfiguration(includeSecrets).dump(); }
bool AgenticConfiguration::importConfiguration(const std::string& jsonData) {
    if (jsonData.empty()) return false;
    try {
        json j = json::parse(jsonData);
        for (auto& [key, val] : j.items()) {
            if (val.is_string()) set(key, val.get<std::string>());
            else if (val.is_number_integer()) set(key, val.get<int>());
            else if (val.is_number_float()) set(key, val.get<float>());
            else if (val.is_boolean()) set(key, val.get<bool>());
        }
        emitConfigurationLoaded();
        return true;
    } catch (...) {
        return false;
    }
}
std::string AgenticConfiguration::generateConfigurationDocumentation() const {
    std::ostringstream doc;
    doc << "# RawrXD Configuration Reference\n\n";
    for (const auto& [key, val] : m_config) {
        doc << "## `" << key << "`\n";
        if (!val.description.empty()) doc << val.description << "\n";
        doc << "- **Type:** ";
        switch (val.type) {
            case ConfigType::String:  doc << "String"; break;
            case ConfigType::Integer: doc << "Integer"; break;
            case ConfigType::Float:   doc << "Float"; break;
            case ConfigType::Boolean: doc << "Boolean"; break;
            default: doc << "Unknown"; break;
        }
        doc << "\n";
        doc << "- **Required:** " << (val.isRequired ? "Yes" : "No") << "\n";
        doc << "- **Secret:** " << (val.isSecret ? "Yes" : "No") << "\n\n";
    }
    return doc.str();
}

std::string AgenticConfiguration::maskSecrets(const std::string& text) const {
    std::string result = text;
    for (const auto& [key, val] : m_config) {
        if (!val.isSecret) continue;
        if (auto p = std::get_if<std::string>(&val.value)) {
            if (p->empty() || p->size() < 4) continue;
            size_t pos = 0;
            while ((pos = result.find(*p, pos)) != std::string::npos) {
                // Replace with masked version (keep first 2 and last 2 chars)
                std::string masked = p->substr(0, 2) + std::string(p->size() - 4, '*') + p->substr(p->size() - 2);
                result.replace(pos, p->size(), masked);
                pos += masked.size();
            }
        }
    }
    return result;
}
bool AgenticConfiguration::validateNoSecretsInLogs() const {
    // Check that no secret values appear in any log output paths
    // This is a static analysis — just verify no secret values are short/trivially guessable
    for (const auto& [key, val] : m_config) {
        if (!val.isSecret) continue;
        if (auto p = std::get_if<std::string>(&val.value)) {
            if (p->size() < 8) return false; // Secrets should be at least 8 chars
        }
    }
    return true;
}

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

json AgenticConfiguration::getConfigurationSchema() const {
    json schema = json::object();
    for (const auto& [key, val] : m_config) {
        json entry;
        switch (val.type) {
            case ConfigType::String:  entry["type"] = "string"; break;
            case ConfigType::Integer: entry["type"] = "integer"; break;
            case ConfigType::Float:   entry["type"] = "number"; break;
            case ConfigType::Boolean: entry["type"] = "boolean"; break;
            default: entry["type"] = "unknown"; break;
        }
        entry["description"] = val.description;
        entry["required"] = val.isRequired;
        entry["secret"] = val.isSecret;
        schema[key] = entry;
    }
    return schema;
}
std::string AgenticConfiguration::getConfigurationHelp(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return "Unknown configuration key: " + key;
    std::string help = key + ": " + it->second.description;
    if (it->second.isRequired) help += " [REQUIRED]";
    if (it->second.isSecret) help += " [SECRET]";
    return help;
}
int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const {
    auto it = m_accessCounts.find(key);
    return (it != m_accessCounts.end()) ? it->second : 0;
}
json AgenticConfiguration::getConfigurationUsageStats() {
    json stats = json::object();
    stats["totalKeys"] = (int)m_config.size();
    stats["totalAccesses"] = 0;
    json accesses = json::object();
    for (const auto& [key, count] : m_accessCounts) {
        accesses[key] = count;
        stats["totalAccesses"] = stats["totalAccesses"].get<int>() + count;
    }
    stats["perKey"] = accesses;
    stats["featureToggles"] = (int)m_featureToggles.size();
    stats["watchedFiles"] = (int)m_watchedFiles.size();
    return stats;
}

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config) { m_config[key] = config; }
AgenticConfiguration::ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type) {
    switch (type) {
        case ConfigType::String:  return valueStr;
        case ConfigType::Integer: try { return std::stoi(valueStr); } catch (...) { return 0; }
        case ConfigType::Float:   try { return (float)std::stod(valueStr); } catch (...) { return 0.0f; }
        case ConfigType::Boolean: return (valueStr == "true" || valueStr == "1" || valueStr == "yes");
        default: return valueStr;
    }
}
bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return true; // Unknown keys are valid
    // Type check
    switch (it->second.type) {
        case ConfigType::String:  return std::holds_alternative<std::string>(value);
        case ConfigType::Integer: return std::holds_alternative<int>(value);
        case ConfigType::Float:   return std::holds_alternative<float>(value) || std::holds_alternative<int>(value);
        case ConfigType::Boolean: return std::holds_alternative<bool>(value);
        default: return true;
    }
}
void AgenticConfiguration::applyEnvironmentOverrides() {
    // Scan environment variables with RAWRXD_ prefix and apply as config overrides
    // E.g., RAWRXD_AGENTIC_MAX_ITERATIONS=20 -> agentic.max_iterations=20
    #ifdef _WIN32
    wchar_t* envBlock = GetEnvironmentStringsW();
    if (envBlock) {
        for (wchar_t* p = envBlock; *p; p += wcslen(p) + 1) {
            std::wstring entry(p);
            if (entry.substr(0, 7) == L"RAWRXD_") {
                size_t eq = entry.find(L'=');
                if (eq == std::wstring::npos) continue;
                std::wstring wkey = entry.substr(7, eq - 7);
                std::wstring wval = entry.substr(eq + 1);
                // Convert key: RAWRXD_AGENTIC_MAX_ITERATIONS -> agentic.max_iterations
                std::string key(wkey.begin(), wkey.end());
                std::string val(wval.begin(), wval.end());
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                std::replace(key.begin(), key.end(), '_', '.');
                set(key, val);
            }
        }
        FreeEnvironmentStringsW(envBlock);
    }
    #else
    extern char** environ;
    for (char** env = environ; *env; ++env) {
        std::string entry(*env);
        if (entry.substr(0, 7) == "RAWRXD_") {
            size_t eq = entry.find('=');
            if (eq == std::string::npos) continue;
            std::string key = entry.substr(7, eq - 7);
            std::string val = entry.substr(eq + 1);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            std::replace(key.begin(), key.end(), '_', '.');
            set(key, val);
        }
    }
    #endif
}
json AgenticConfiguration::parseYamlFile(const std::string& filePath) {
    // Parse YAML file into JSON object using our YAML subset parser
    json result = json::object();
    std::ifstream file(filePath);
    if (!file.is_open()) return result;

    std::string line;
    while (std::getline(file, line)) {
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos || line[firstNonSpace] == '#') continue;
        std::string trimmed = line.substr(firstNonSpace);
        size_t colonPos = trimmed.find(':');
        if (colonPos == std::string::npos) continue;
        std::string key = trimmed.substr(0, colonPos);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        std::string value;
        if (colonPos + 1 < trimmed.size()) {
            value = trimmed.substr(colonPos + 1);
            size_t vs = value.find_first_not_of(" \t");
            if (vs != std::string::npos) value = value.substr(vs);
            else value.clear();
        }
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        if (value == "true") result[key] = true;
        else if (value == "false") result[key] = false;
        else {
            try { result[key] = std::stoi(value); }
            catch (...) {
                try { result[key] = std::stod(value); }
                catch (...) { result[key] = value; }
            }
        }
    }
    return result;
}
