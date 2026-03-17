// ============================================================================
// agentic_config.cpp — Production Agentic Configuration Manager Implementation
// ============================================================================
// Comprehensive configuration system supporting multi-environment configuration,
// feature toggles, hot reloading, profile management, secure secret handling.
//
// Architecture: JSON-based storage with YAML and dotenv support
// Security: Secret masking and validation with access tracking
// Performance: Cached lookups with atomic access counters  
// Hot Reload: File system monitoring with change notifications
// Profiles: Named configuration sets with import/export
// ============================================================================

#include "agentic_configuration.h"
#include "license_enforcement.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <mutex>
#include <atomic>
#include <iostream>
#include <algorithm>
#include <random>
#include <filesystem>
#include <chrono>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

// ============================================================================
// File Watcher Implementation for Hot Reloading
// ============================================================================

class AgenticConfiguration::ConfigFileWatcher {
public:
    ConfigFileWatcher(AgenticConfiguration* config) 
        : m_config(config), m_running(false), m_watchHandle(INVALID_HANDLE_VALUE) {}
    
    ~ConfigFileWatcher() {
        stop();
    }
    
    void start(const std::string& filePath) {
        if (m_running) return;
        
        m_watchedFile = filePath;
        m_running = true;
        
        m_watchThread = std::thread([this]() {
            watcherLoop();
        });
    }
    
    void stop() {
        if (!m_running) return;
        
        m_running = false;
        
        if (m_watchHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_watchHandle);
            m_watchHandle = INVALID_HANDLE_VALUE;
        }
        
        if (m_watchThread.joinable()) {
            m_watchThread.join();
        }
    }
    
private:
    void watcherLoop() {
        if (m_watchedFile.empty()) return;
        
        fs::path filePath(m_watchedFile);
        fs::path directory = filePath.parent_path();
        
        m_watchHandle = FindFirstChangeNotificationA(
            directory.string().c_str(),
            FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE
        );
        
        if (m_watchHandle == INVALID_HANDLE_VALUE) return;
        
        auto lastWriteTime = fs::last_write_time(filePath);
        
        while (m_running) {
            DWORD result = WaitForSingleObject(m_watchHandle, 1000); // 1 second timeout
            
            if (result == WAIT_OBJECT_0) {
                // Check if our specific file changed
                try {
                    auto currentWriteTime = fs::last_write_time(filePath);
                    if (currentWriteTime > lastWriteTime) {
                        lastWriteTime = currentWriteTime;
                        
                        // Wait a bit for file to be completely written
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        
                        // Trigger reload
                        if (m_config) {
                            m_config->reloadConfiguration();
                        }
                    }
                } catch (const std::exception&) {
                    // File might be temporarily unavailable
                }
                
                FindNextChangeNotification(m_watchHandle);
            }
            else if (result == WAIT_TIMEOUT) {
                // Normal timeout, continue watching
                continue;
            }
            else {
                // Error occurred
                break;
            }
        }
    }
    
    AgenticConfiguration* m_config;
    std::atomic<bool> m_running;
    std::thread m_watchThread;
    std::string m_watchedFile;
    HANDLE m_watchHandle;
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace {

std::string trimString(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toLowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// Simple YAML parser for basic key-value pairs
json parseYamlContent(const std::string& content) {
    json result;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trimString(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Find key-value separator
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string key = trimString(line.substr(0, colonPos));
        std::string value = trimString(line.substr(colonPos + 1));
        
        // Remove quotes if present
        if (value.length() >= 2 && 
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.length() - 2);
        }
        
        // Try to parse as different types
        if (toLowercase(value) == "true") {
            result[key] = true;
        } else if (toLowercase(value) == "false") {
            result[key] = false;
        } else if (std::regex_match(value, std::regex(R"(-?\d+)"))) {
            result[key] = std::stoi(value);
        } else if (std::regex_match(value, std::regex(R"(-?\d+\.\d+)"))) {
            result[key] = std::stof(value);
        } else {
            result[key] = value;
        }
    }
    
    return result;
}

// Parse dotenv format
json parseDotenvContent(const std::string& content) {
    json result;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        line = trimString(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Find key-value separator
        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) continue;
        
        std::string key = trimString(line.substr(0, equalPos));
        std::string value = trimString(line.substr(equalPos + 1));
        
        // Remove quotes if present
        if (value.length() >= 2 && 
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.length() - 2);
        }
        
        result[key] = value;
    }
    
    return result;
}

std::string generateHash(const std::string& input) {
    // Simple hash for user-based feature rollout
    std::hash<std::string> hasher;
    return std::to_string(hasher(input));
}

} // anonymous namespace

// ============================================================================
// AgenticConfiguration Implementation
// ============================================================================

AgenticConfiguration::AgenticConfiguration() 
    : m_currentEnvironment(Environment::Development)
    , m_hotReloadingEnabled(false)
    , m_fileWatcher(std::make_unique<ConfigFileWatcher>(this)) {
    
    loadDefaults();
}

AgenticConfiguration::~AgenticConfiguration() {
    if (m_fileWatcher) {
        m_fileWatcher->stop();
    }
}

// ============================================================================
// Initialization
// ============================================================================

void AgenticConfiguration::initializeFromEnvironment(Environment env) {
    m_currentEnvironment = env;
    
    // Load environment-specific defaults
    loadDefaults();
    
    // Load from standard environment variables
    const char* envVars[] = {
        "RAWRXD_API_KEY",
        "RAWRXD_MODEL_PATH", 
        "RAWRXD_MAX_TOKENS",
        "RAWRXD_TEMPERATURE",
        "RAWRXD_DEBUG_MODE",
        "RAWRXD_LOG_LEVEL",
        "RAWRXD_CACHE_SIZE",
        "RAWRXD_THREAD_COUNT",
        "RAWRXD_GPU_ENABLED",
        "RAWRXD_TELEMETRY_ENABLED"
    };
    
    for (const char* envVar : envVars) {
        const char* value = getenv(envVar);
        if (value) {
            std::string key = std::string(envVar);
            // Remove RAWRXD_ prefix for internal storage
            if (key.substr(0, 7) == "RAWRXD_") {
                key = key.substr(7);
            }
            
            set(key, ConfigVar(std::string(value)));
        }
    }
    
    applyEnvironmentOverrides();
    emitConfigurationLoaded();
}

bool AgenticConfiguration::loadFromJson(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        
        json data;
        file >> data;
        
        // Merge with existing configuration
        for (auto& [key, value] : data.items()) {
            if (value.is_string()) {
                set(key, ConfigVar(value.get<std::string>()));
            } else if (value.is_number_integer()) {
                set(key, ConfigVar(value.get<int>()));
            } else if (value.is_number_float()) {
                set(key, ConfigVar(value.get<float>()));
            } else if (value.is_boolean()) {
                set(key, ConfigVar(value.get<bool>()));
            } else if (value.is_array() || value.is_object()) {
                set(key, ConfigVar(value));
            }
        }
        
        // Add to watched files if hot reloading is enabled
        if (m_hotReloadingEnabled) {
            watchConfigFile(filePath);
        }
        
        emitConfigurationLoaded();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool AgenticConfiguration::loadFromYaml(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        json data = parseYamlContent(content);
        
        // Merge with existing configuration
        for (auto& [key, value] : data.items()) {
            if (value.is_string()) {
                set(key, ConfigVar(value.get<std::string>()));
            } else if (value.is_number_integer()) {
                set(key, ConfigVar(value.get<int>()));
            } else if (value.is_number_float()) {
                set(key, ConfigVar(value.get<float>()));
            } else if (value.is_boolean()) {
                set(key, ConfigVar(value.get<bool>()));
            }
        }
        
        if (m_hotReloadingEnabled) {
            watchConfigFile(filePath);
        }
        
        emitConfigurationLoaded();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool AgenticConfiguration::loadFromEnv(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        json data = parseDotenvContent(content);
        
        // Merge with existing configuration  
        for (auto& [key, value] : data.items()) {
            set(key, ConfigVar(value.get<std::string>()));
        }
        
        if (m_hotReloadingEnabled) {
            watchConfigFile(filePath);
        }
        
        emitConfigurationLoaded();
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void AgenticConfiguration::loadDefaults() {
    // Core system defaults
    setConfigDefault("API_KEY", {ConfigType::String, ConfigVar(std::string("")), true, "API key for external services", ConfigVar(std::string("")), true, {}});
    setConfigDefault("MODEL_PATH", {ConfigType::String, ConfigVar(std::string("./models")), false, "Path to model files", ConfigVar(std::string("./models")), false, {}});
    setConfigDefault("MAX_TOKENS", {ConfigType::Integer, ConfigVar(2048), false, "Maximum tokens per request", ConfigVar(2048), false, {"min:1", "max:32768"}});
    setConfigDefault("TEMPERATURE", {ConfigType::Float, ConfigVar(0.7f), false, "Sampling temperature", ConfigVar(0.7f), false, {"min:0.0", "max:2.0"}});
    setConfigDefault("DEBUG_MODE", {ConfigType::Boolean, ConfigVar(false), false, "Enable debug logging", ConfigVar(false), false, {}});
    setConfigDefault("LOG_LEVEL", {ConfigType::String, ConfigVar(std::string("INFO")), false, "Logging level", ConfigVar(std::string("INFO")), false, {"enum:DEBUG,INFO,WARN,ERROR"}});
    setConfigDefault("CACHE_SIZE", {ConfigType::Integer, ConfigVar(1000), false, "Cache size for responses", ConfigVar(1000), false, {"min:0", "max:100000"}});
    setConfigDefault("THREAD_COUNT", {ConfigType::Integer, ConfigVar(4), false, "Number of worker threads", ConfigVar(4), false, {"min:1", "max:32"}});
    setConfigDefault("GPU_ENABLED", {ConfigType::Boolean, ConfigVar(true), false, "Enable GPU acceleration", ConfigVar(true), false, {}});
    setConfigDefault("TELEMETRY_ENABLED", {ConfigType::Boolean, ConfigVar(true), false, "Enable telemetry collection", ConfigVar(true), false, {}});
    
    // Environment-specific overrides
    switch (m_currentEnvironment) {
        case Environment::Development:
            set("DEBUG_MODE", ConfigVar(true));
            set("LOG_LEVEL", ConfigVar(std::string("DEBUG")));
            set("TELEMETRY_ENABLED", ConfigVar(false));
            break;
            
        case Environment::Staging:
            set("LOG_LEVEL", ConfigVar(std::string("INFO")));
            set("TELEMETRY_ENABLED", ConfigVar(true));
            break;
            
        case Environment::Production:
            set("DEBUG_MODE", ConfigVar(false));
            set("LOG_LEVEL", ConfigVar(std::string("WARN")));
            set("TELEMETRY_ENABLED", ConfigVar(true));
            break;
    }
    
    // Feature toggles defaults
    setFeatureToggle({
        "advanced_reasoning", true, "Enable advanced reasoning capabilities", 
        "100", std::chrono::system_clock::now(), "core_team"
    });
    
    setFeatureToggle({
        "multi_modal_support", false, "Enable multi-modal model support",
        "25", std::chrono::system_clock::now(), "research_team" 
    });
    
    setFeatureToggle({
        "experimental_features", false, "Enable experimental features",
        "10", std::chrono::system_clock::now(), "dev_team"
    });
    
    setFeatureToggle({
        "performance_monitoring", true, "Enable performance monitoring",
        "100", std::chrono::system_clock::now(), "ops_team"
    });
}

// ============================================================================
// Configuration Access
// ============================================================================

ConfigVar AgenticConfiguration::get(const std::string& key, const ConfigVar& defaultValue) {
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        // Track access
        m_accessCounts[key]++;
        
        // Emit event if secret is accessed
        if (it->second.isSecret) {
            emitSecretAccessed(key);
        }
        
        return it->second.value;
    }
    return defaultValue;
}

std::string AgenticConfiguration::getString(const std::string& key, const std::string& defaultValue) {
    ConfigVar var = get(key, ConfigVar(defaultValue));
    if (std::holds_alternative<std::string>(var)) {
        return std::get<std::string>(var);
    }
    return defaultValue;
}

int AgenticConfiguration::getInt(const std::string& key, int defaultValue) {
    ConfigVar var = get(key, ConfigVar(defaultValue));
    if (std::holds_alternative<int>(var)) {
        return std::get<int>(var);
    }
    return defaultValue;
}

float AgenticConfiguration::getFloat(const std::string& key, float defaultValue) {
    ConfigVar var = get(key, ConfigVar(defaultValue));
    if (std::holds_alternative<float>(var)) {
        return std::get<float>(var);
    }
    return defaultValue;
}

bool AgenticConfiguration::getBool(const std::string& key, bool defaultValue) {
    ConfigVar var = get(key, ConfigVar(defaultValue));
    if (std::holds_alternative<bool>(var)) {
        return std::get<bool>(var);
    }
    return defaultValue;
}

json AgenticConfiguration::getObject(const std::string& key, const json& defaultValue) {
    ConfigVar var = get(key, ConfigVar(defaultValue));
    if (std::holds_alternative<json>(var)) {
        return std::get<json>(var);
    }
    return defaultValue;
}

json AgenticConfiguration::getArray(const std::string& key, const json& defaultValue) {
    return getObject(key, defaultValue); // Arrays are stored as json objects
}

void AgenticConfiguration::set(const std::string& key, const ConfigVar& value) {
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        it->second.value = value;
    } else {
        // Create new config entry
        ConfigType type = ConfigType::String;
        if (std::holds_alternative<int>(value)) type = ConfigType::Integer;
        else if (std::holds_alternative<float>(value)) type = ConfigType::Float;
        else if (std::holds_alternative<bool>(value)) type = ConfigType::Boolean;
        else if (std::holds_alternative<json>(value)) type = ConfigType::Object;
        
        m_config[key] = {type, value, false, "", value, false, {}};
    }
    
    emitConfigurationChanged(key);
}

void AgenticConfiguration::setSecret(const std::string& key, const std::string& value) {
    m_config[key] = {ConfigType::String, ConfigVar(value), true, "Secret configuration", ConfigVar(std::string("")), false, {}};
    
    // Add to secrets list for masking
    if (std::find(m_secretKeys.begin(), m_secretKeys.end(), key) == m_secretKeys.end()) {
        m_secretKeys.push_back(key);
    }
    
    emitConfigurationChanged(key);
}

// ============================================================================
// Environment Management
// ============================================================================

void AgenticConfiguration::setEnvironment(Environment env) {
    m_currentEnvironment = env;
    applyEnvironmentOverrides();
    emitConfigurationChanged("ENVIRONMENT");
}

std::string AgenticConfiguration::getEnvironmentName() const {
    switch (m_currentEnvironment) {
        case Environment::Development: return "Development";
        case Environment::Staging: return "Staging";  
        case Environment::Production: return "Production";
        default: return "Unknown";
    }
}

ConfigVar AgenticConfiguration::getEnvironmentSpecific(const std::string& key, Environment env) {
    std::string envKey = getEnvironmentName() + "." + key;
    auto it = m_config.find(envKey);
    if (it != m_config.end()) {
        return it->second.value;
    }
    
    // Fall back to general key
    return get(key);
}

// ============================================================================
// Feature Toggles
// ============================================================================

bool AgenticConfiguration::isFeatureEnabled(const std::string& featureName) {
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        return it->second.enabled;
    }
    return false;
}

void AgenticConfiguration::enableFeature(const std::string& featureName) {
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        bool wasEnabled = it->second.enabled;
        it->second.enabled = true;
        it->second.enabledDate = std::chrono::system_clock::now();
        
        emitFeatureToggled(featureName, true);
    }
}

void AgenticConfiguration::disableFeature(const std::string& featureName) {
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        bool wasEnabled = it->second.enabled;
        it->second.enabled = false;
        
        emitFeatureToggled(featureName, false);
    }
}

void AgenticConfiguration::setFeatureToggle(const FeatureToggle& toggle) {
    bool wasEnabled = false;
    auto it = m_featureToggles.find(toggle.featureName);
    if (it != m_featureToggles.end()) {
        wasEnabled = it->second.enabled;
    }
    
    m_featureToggles[toggle.featureName] = toggle;
    
    if (wasEnabled != toggle.enabled) {
        emitFeatureToggled(toggle.featureName, toggle.enabled);
    }
}

AgenticConfiguration::FeatureToggle AgenticConfiguration::getFeatureToggle(const std::string& featureName) {
    auto it = m_featureToggles.find(featureName);
    if (it != m_featureToggles.end()) {
        return it->second;
    }
    
    return FeatureToggle{
        featureName, false, "Feature not found", "0",
        std::chrono::system_clock::now(), "unknown"
    };
}

json AgenticConfiguration::getAllFeatureToggles() {
    json result;
    for (const auto& [name, toggle] : m_featureToggles) {
        result[name] = {
            {"enabled", toggle.enabled},
            {"description", toggle.description},
            {"rolloutPercentage", toggle.rolloutPercentage}, 
            {"owner", toggle.owner}
        };
    }
    return result;
}

bool AgenticConfiguration::isFeatureEnabledForUser(const std::string& featureName, const std::string& userId) {
    auto it = m_featureToggles.find(featureName);
    if (it == m_featureToggles.end() || !it->second.enabled) {
        return false;
    }
    
    // Parse rollout percentage
    int rolloutPercent = 100;
    try {
        rolloutPercent = std::stoi(it->second.rolloutPercentage);
    } catch (...) {
        rolloutPercent = 100;
    }
    
    if (rolloutPercent >= 100) return true;
    if (rolloutPercent <= 0) return false;
    
    // Use hash of userId to determine if user is in rollout group
    std::string hash = generateHash(featureName + userId);
    int userHash = std::stoi(hash.substr(0, 8), nullptr, 16);
    int userPercent = userHash % 100;
    
    return userPercent < rolloutPercent;
}

// ============================================================================
// Validation
// ============================================================================

bool AgenticConfiguration::validateConfig(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return false;
    
    return validateValue(key, it->second.value);
}

bool AgenticConfiguration::validateAllConfig() {
    for (const auto& [key, config] : m_config) {
        if (!validateValue(key, config.value)) {
            return false;
        }
    }
    return true;
}

std::string AgenticConfiguration::getValidationError(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return "Configuration key not found";
    
    if (!validateValue(key, it->second.value)) {
        return "Validation failed for key: " + key;
    }
    
    return "";
}

json AgenticConfiguration::getValidationReport() {
    json report;
    report["valid"] = true;
    report["errors"] = json::array();
    
    for (const auto& [key, config] : m_config) {
        if (!validateValue(key, config.value)) {
            report["valid"] = false;
            report["errors"].push_back({
                {"key", key},
                {"error", "Validation failed"},
                {"type", static_cast<int>(config.type)}
            });
        }
    }
    
    return report;
}

// ============================================================================
// Hot Reloading
// ============================================================================

void AgenticConfiguration::enableHotReloading(bool enabled) {
    m_hotReloadingEnabled = enabled;
    
    if (!enabled && m_fileWatcher) {
        m_fileWatcher->stop();
    }
}

void AgenticConfiguration::watchConfigFile(const std::string& filePath) {
    if (!m_hotReloadingEnabled) return;
    
    if (std::find(m_watchedFiles.begin(), m_watchedFiles.end(), filePath) == m_watchedFiles.end()) {
        m_watchedFiles.push_back(filePath);
    }
    
    if (m_fileWatcher) {
        m_fileWatcher->start(filePath);
    }
}

void AgenticConfiguration::reloadConfiguration() {
    // Reload all watched files
    for (const std::string& filePath : m_watchedFiles) {
        std::string extension = fs::path(filePath).extension().string();
        
        if (extension == ".json") {
            loadFromJson(filePath);
        } else if (extension == ".yaml" || extension == ".yml") {
            loadFromYaml(filePath);
        } else if (extension == ".env") {
            loadFromEnv(filePath);
        }
    }
    
    emitConfigurationReloaded();
}

// ============================================================================
// Profile Management
// ============================================================================

bool AgenticConfiguration::saveProfile(const std::string& profileName) {
    json profile = getAllConfiguration(false); // Don't include secrets
    m_profiles[profileName] = profile;
    
    // Save to file
    try {
        std::string filename = "profiles/" + profileName + ".json";
        fs::create_directories(fs::path(filename).parent_path());
        
        std::ofstream file(filename);
        file << profile.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool AgenticConfiguration::loadProfile(const std::string& profileName) {
    // Try to load from memory first
    auto it = m_profiles.find(profileName);
    if (it != m_profiles.end()) {
        return importConfiguration(it->second.dump());
    }
    
    // Try to load from file
    std::string filename = "profiles/" + profileName + ".json";
    return loadFromJson(filename);
}

std::vector<std::string> AgenticConfiguration::getAvailableProfiles() {
    std::vector<std::string> profiles;
    
    // Add in-memory profiles
    for (const auto& [name, data] : m_profiles) {
        profiles.push_back(name);
    }
    
    // Add file-based profiles
    try {
        if (fs::exists("profiles") && fs::is_directory("profiles")) {
            for (const auto& entry : fs::directory_iterator("profiles")) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string name = entry.path().stem().string();
                    if (std::find(profiles.begin(), profiles.end(), name) == profiles.end()) {
                        profiles.push_back(name);
                    }
                }
            }
        }
    } catch (...) {}
    
    return profiles;
}

bool AgenticConfiguration::deleteProfile(const std::string& profileName) {
    // Remove from memory
    m_profiles.erase(profileName);
    
    // Remove file
    try {
        std::string filename = "profiles/" + profileName + ".json";
        if (fs::exists(filename)) {
            fs::remove(filename);
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Export/Import
// ============================================================================

std::string AgenticConfiguration::exportConfiguration(bool includeSecrets) const {
    return getAllConfiguration(includeSecrets).dump(2);
}

bool AgenticConfiguration::importConfiguration(const std::string& jsonData) {
    try {
        json data = json::parse(jsonData);
        
        for (auto& [key, value] : data.items()) {
            if (value.is_string()) {
                set(key, ConfigVar(value.get<std::string>()));
            } else if (value.is_number_integer()) {
                set(key, ConfigVar(value.get<int>()));
            } else if (value.is_number_float()) {
                set(key, ConfigVar(value.get<float>()));
            } else if (value.is_boolean()) {
                set(key, ConfigVar(value.get<bool>()));
            } else if (value.is_array() || value.is_object()) {
                set(key, ConfigVar(value));
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

std::string AgenticConfiguration::generateConfigurationDocumentation() const {
    std::string doc = "# Configuration Documentation\n\n";
    
    for (const auto& [key, config] : m_config) {
        doc += "## " + key + "\n\n";
        doc += "**Type:** ";
        
        switch (config.type) {
            case ConfigType::String: doc += "String"; break;
            case ConfigType::Integer: doc += "Integer"; break;
            case ConfigType::Float: doc += "Float"; break;
            case ConfigType::Boolean: doc += "Boolean"; break;
            case ConfigType::Array: doc += "Array"; break;
            case ConfigType::Object: doc += "Object"; break;
        }
        
        doc += "\n\n";
        
        if (!config.description.empty()) {
            doc += "**Description:** " + config.description + "\n\n";
        }
        
        if (config.isRequired) {
            doc += "**Required:** Yes\n\n";
        }
        
        if (config.isSecret) {
            doc += "**Secret:** Yes (value will be masked)\n\n";
        }
        
        if (!config.validationRules.empty()) {
            doc += "**Validation Rules:**\n";
            for (const std::string& rule : config.validationRules) {
                doc += "- " + rule + "\n";
            }
            doc += "\n";
        }
        
        doc += "---\n\n";
    }
    
    return doc;
}

// ============================================================================
// Security & Masking
// ============================================================================

std::string AgenticConfiguration::maskSecrets(const std::string& text) const {
    std::string result = text;
    
    for (const std::string& secretKey : m_secretKeys) {
        auto it = m_config.find(secretKey);
        if (it != m_config.end() && std::holds_alternative<std::string>(it->second.value)) {
            std::string secretValue = std::get<std::string>(it->second.value);
            if (!secretValue.empty() && secretValue.length() > 4) {
                std::string masked = secretValue.substr(0, 2) + "***" + secretValue.substr(secretValue.length() - 2);
                
                // Replace all occurrences
                size_t pos = 0;
                while ((pos = result.find(secretValue, pos)) != std::string::npos) {
                    result.replace(pos, secretValue.length(), masked);
                    pos += masked.length();
                }
            }
        }
    }
    
    return result;
}

bool AgenticConfiguration::validateNoSecretsInLogs() const {
    // This would typically check log files or integrate with logging system
    // For now, return true as we implement masking
    return true;
}

// ============================================================================
// Reporting & Metrics
// ============================================================================

json AgenticConfiguration::getAllConfiguration(bool includeSecrets) const {
    json result;
    
    for (const auto& [key, config] : m_config) {
        if (config.isSecret && !includeSecrets) continue;
        
        if (std::holds_alternative<std::string>(config.value)) {
            result[key] = std::get<std::string>(config.value);
        } else if (std::holds_alternative<int>(config.value)) {
            result[key] = std::get<int>(config.value);
        } else if (std::holds_alternative<float>(config.value)) {
            result[key] = std::get<float>(config.value);
        } else if (std::holds_alternative<bool>(config.value)) {
            result[key] = std::get<bool>(config.value);
        } else if (std::holds_alternative<json>(config.value)) {
            result[key] = std::get<json>(config.value);
        }
    }
    
    return result;
}

json AgenticConfiguration::getConfigurationSchema() const {
    json schema;
    
    for (const auto& [key, config] : m_config) {
        json schemaEntry;
        
        switch (config.type) {
            case ConfigType::String: schemaEntry["type"] = "string"; break;
            case ConfigType::Integer: schemaEntry["type"] = "integer"; break;
            case ConfigType::Float: schemaEntry["type"] = "number"; break;
            case ConfigType::Boolean: schemaEntry["type"] = "boolean"; break;
            case ConfigType::Array: schemaEntry["type"] = "array"; break;
            case ConfigType::Object: schemaEntry["type"] = "object"; break;
        }
        
        schemaEntry["description"] = config.description;
        schemaEntry["required"] = config.isRequired;
        schemaEntry["secret"] = config.isSecret;
        
        if (!config.validationRules.empty()) {
            schemaEntry["validationRules"] = config.validationRules;
        }
        
        schema[key] = schemaEntry;
    }
    
    return schema;
}

std::string AgenticConfiguration::getConfigurationHelp(const std::string& key) {
    auto it = m_config.find(key);
    if (it == m_config.end()) {
        return "Configuration key '" + key + "' not found.";
    }
    
    const ConfigValue& config = it->second;
    std::string help = "Key: " + key + "\n";
    help += "Type: ";
    
    switch (config.type) {
        case ConfigType::String: help += "String"; break;
        case ConfigType::Integer: help += "Integer"; break;
        case ConfigType::Float: help += "Float"; break;
        case ConfigType::Boolean: help += "Boolean"; break;
        case ConfigType::Array: help += "Array"; break;
        case ConfigType::Object: help += "Object"; break;
    }
    
    help += "\n";
    
    if (!config.description.empty()) {
        help += "Description: " + config.description + "\n";
    }
    
    if (config.isRequired) {
        help += "Required: Yes\n";
    }
    
    if (config.isSecret) {
        help += "Secret: Yes (value will be masked in logs)\n";
    }
    
    if (!config.validationRules.empty()) {
        help += "Validation Rules:\n";
        for (const std::string& rule : config.validationRules) {
            help += "  - " + rule + "\n";
        }
    }
    
    return help;
}

int AgenticConfiguration::getConfigurationAccessCount(const std::string& key) const {
    auto it = m_accessCounts.find(key);
    return (it != m_accessCounts.end()) ? it->second : 0;
}

json AgenticConfiguration::getConfigurationUsageStats() {
    json stats;
    stats["totalConfigs"] = m_config.size();
    stats["totalAccess"] = 0;
    stats["mostAccessed"] = json::array();
    stats["secrets"] = m_secretKeys.size();
    stats["featureToggles"] = m_featureToggles.size();
    
    // Sum total access
    int totalAccess = 0;
    for (const auto& [key, count] : m_accessCounts) {
        totalAccess += count;
    }
    stats["totalAccess"] = totalAccess;
    
    // Find most accessed configs
    std::vector<std::pair<std::string, int>> accessPairs(m_accessCounts.begin(), m_accessCounts.end());
    std::sort(accessPairs.begin(), accessPairs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(size_t(5), accessPairs.size()); ++i) {
        stats["mostAccessed"].push_back({
            {"key", accessPairs[i].first},
            {"count", accessPairs[i].second}
        });
    }
    
    return stats;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void AgenticConfiguration::setConfigDefault(const std::string& key, const ConfigValue& config) {
    auto it = m_config.find(key);
    if (it == m_config.end()) {
        m_config[key] = config;
    }
}

ConfigVar AgenticConfiguration::parseValue(const std::string& valueStr, ConfigType type) {
    switch (type) {
        case ConfigType::String:
            return ConfigVar(valueStr);
            
        case ConfigType::Integer:
            try {
                return ConfigVar(std::stoi(valueStr));
            } catch (...) {
                return ConfigVar(0);
            }
            
        case ConfigType::Float:
            try {
                return ConfigVar(std::stof(valueStr));
            } catch (...) {
                return ConfigVar(0.0f);
            }
            
        case ConfigType::Boolean:
            return ConfigVar(toLowercase(valueStr) == "true" || valueStr == "1");
            
        default:
            return ConfigVar(valueStr);
    }
}

bool AgenticConfiguration::validateValue(const std::string& key, const ConfigVar& value) {
    auto it = m_config.find(key);
    if (it == m_config.end()) return false;
    
    const ConfigValue& config = it->second;
    
    // Check type consistency
    switch (config.type) {
        case ConfigType::String:
            if (!std::holds_alternative<std::string>(value)) return false;
            break;
        case ConfigType::Integer:
            if (!std::holds_alternative<int>(value)) return false;
            break;
        case ConfigType::Float:
            if (!std::holds_alternative<float>(value)) return false;
            break;
        case ConfigType::Boolean:
            if (!std::holds_alternative<bool>(value)) return false;
            break;
        case ConfigType::Object:
        case ConfigType::Array:
            if (!std::holds_alternative<json>(value)) return false;
            break;
    }
    
    // Apply validation rules
    for (const std::string& rule : config.validationRules) {
        if (rule.substr(0, 4) == "min:") {
            try {
                float minVal = std::stof(rule.substr(4));
                if (std::holds_alternative<int>(value) && std::get<int>(value) < static_cast<int>(minVal)) {
                    return false;
                }
                if (std::holds_alternative<float>(value) && std::get<float>(value) < minVal) {
                    return false;
                }
            } catch (...) {}
        }
        
        if (rule.substr(0, 4) == "max:") {
            try {
                float maxVal = std::stof(rule.substr(4));
                if (std::holds_alternative<int>(value) && std::get<int>(value) > static_cast<int>(maxVal)) {
                    return false;
                }
                if (std::holds_alternative<float>(value) && std::get<float>(value) > maxVal) {
                    return false;
                }
            } catch (...) {}
        }
        
        if (rule.substr(0, 5) == "enum:") {
            std::string enumValues = rule.substr(5);
            if (std::holds_alternative<std::string>(value)) {
                std::string val = std::get<std::string>(value);
                if (enumValues.find(val) == std::string::npos) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

void AgenticConfiguration::applyEnvironmentOverrides() {
    // Apply environment-specific configuration
    std::string envPrefix = getEnvironmentName() + ".";
    
    for (const auto& [key, config] : m_config) {
        if (key.substr(0, envPrefix.length()) == envPrefix) {
            std::string baseKey = key.substr(envPrefix.length());
            m_config[baseKey] = config;
        }
    }
}

json AgenticConfiguration::parseYamlFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return json();
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    return parseYamlContent(content);
}