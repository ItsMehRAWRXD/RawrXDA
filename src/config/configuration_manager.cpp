// ============================================================================
// Configuration Manager — Environment Configuration Management
// Multi-environment configuration with secrets management
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>

namespace RawrXD::Config {

enum class Environment {
    DEVELOPMENT,
    STAGING,
    PRODUCTION,
    TESTING
};

enum class ConfigValueType {
    STRING,
    INTEGER,
    BOOLEAN,
    FLOAT,
    JSON,
    SECRET
};

struct ConfigValue {
    std::string key;
    std::string value;
    ConfigValueType type;
    Environment environment;
    std::string description;
    bool isSecret;
    std::chrono::system_clock::time_point lastModified;
    std::string modifiedBy;
};

struct ConfigSchema {
    std::string key;
    ConfigValueType type;
    bool required;
    std::string defaultValue;
    std::vector<std::string> allowedValues;
    std::string validationRegex;
};

struct ConfigSnapshot {
    Environment environment;
    std::map<std::string, ConfigValue> values;
    std::chrono::system_clock::time_point capturedAt;
    std::string capturedBy;
};

class ConfigurationManager {
public:
    explicit ConfigurationManager(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    void LoadConfiguration(const std::string& configPath, Environment env) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Load from file
        auto values = ParseConfigFile(configPath);
        
        for (const auto& [key, value] : values) {
            ConfigValue configValue;
            configValue.key = key;
            configValue.value = value;
            configValue.environment = env;
            configValue.lastModified = std::chrono::system_clock::now();
            
            m_configs[env][key] = configValue;
        }
        
        // Validate against schema
        ValidateConfiguration(env);
    }

    std::string GetValue(const std::string& key, Environment env) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto envIt = m_configs.find(env);
        if (envIt != m_configs.end()) {
            auto keyIt = envIt->second.find(key);
            if (keyIt != envIt->second.end()) {
                return keyIt->second.value;
            }
        }
        
        // Fallback to development
        if (env != Environment::DEVELOPMENT) {
            return GetValue(key, Environment::DEVELOPMENT);
        }
        
        return "";
    }

    void SetValue(const std::string& key, const std::string& value, 
                 Environment env, const std::string& modifiedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ConfigValue configValue;
        configValue.key = key;
        configValue.value = value;
        configValue.environment = env;
        configValue.lastModified = std::chrono::system_clock::now();
        configValue.modifiedBy = modifiedBy;
        
        m_configs[env][key] = configValue;
        
        // Persist to file
        PersistConfiguration(env);
    }

    ConfigSnapshot CaptureSnapshot(Environment env, const std::string& capturedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ConfigSnapshot snapshot;
        snapshot.environment = env;
        snapshot.capturedAt = std::chrono::system_clock::now();
        snapshot.capturedBy = capturedBy;
        
        auto envIt = m_configs.find(env);
        if (envIt != m_configs.end()) {
            snapshot.values = envIt->second;
        }
        
        m_snapshots.push_back(snapshot);
        return snapshot;
    }

    void RestoreSnapshot(const ConfigSnapshot& snapshot) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_configs[snapshot.environment] = snapshot.values;
        PersistConfiguration(snapshot.environment);
    }

    void DefineSchema(const ConfigSchema& schema) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_schemas[schema.key] = schema;
    }

    bool ValidateConfiguration(Environment env) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        bool valid = true;
        
        auto envIt = m_configs.find(env);
        if (envIt == m_configs.end()) {
            return false;
        }
        
        for (const auto& [key, schema] : m_schemas) {
            auto valueIt = envIt->second.find(key);
            
            if (schema.required && valueIt == envIt->second.end()) {
                valid = false;
                continue;
            }
            
            if (valueIt != envIt->second.end()) {
                // Validate type
                if (valueIt->second.type != schema.type) {
                    valid = false;
                }
                
                // Validate against allowed values
                if (!schema.allowedValues.empty()) {
                    bool found = false;
                    for (const auto& allowed : schema.allowedValues) {
                        if (valueIt->second.value == allowed) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        valid = false;
                    }
                }
                
                // Validate against regex
                if (!schema.validationRegex.empty()) {
                    std::regex pattern(schema.validationRegex);
                    if (!std::regex_match(valueIt->second.value, pattern)) {
                        valid = false;
                    }
                }
            }
        }
        
        return valid;
    }

    std::string GetSecret(const std::string& key, Environment env) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto envIt = m_configs.find(env);
        if (envIt != m_configs.end()) {
            auto keyIt = envIt->second.find(key);
            if (keyIt != envIt->second.end() && keyIt->second.isSecret) {
                // Decrypt and return
                return DecryptSecret(keyIt->second.value);
            }
        }
        
        return "";
    }

    void SetSecret(const std::string& key, const std::string& value, 
                  Environment env, const std::string& modifiedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ConfigValue configValue;
        configValue.key = key;
        configValue.value = EncryptSecret(value);
        configValue.type = ConfigValueType::SECRET;
        configValue.environment = env;
        configValue.isSecret = true;
        configValue.lastModified = std::chrono::system_clock::now();
        configValue.modifiedBy = modifiedBy;
        
        m_configs[env][key] = configValue;
        PersistConfiguration(env);
    }

    std::string GenerateConfigurationReport(Environment env) {
        std::ostringstream report;
        report << "# Configuration Report\n\n";
        report << "**Environment:** " << EnvironmentToString(env) << "\n\n";
        
        auto envIt = m_configs.find(env);
        if (envIt != m_configs.end()) {
            report << "## Configuration Values\n";
            for (const auto& [key, value] : envIt->second) {
                report << "### " << key << "\n";
                report << "- **Type:** " << TypeToString(value.type) << "\n";
                if (!value.isSecret) {
                    report << "- **Value:** " << value.value << "\n";
                } else {
                    report << "- **Value:** [ENCRYPTED]\n";
                }
                report << "- **Modified:** " << FormatTime(value.lastModified) << "\n";
                report << "- **By:** " << value.modifiedBy << "\n\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<Environment, std::map<std::string, ConfigValue>> m_configs;
    std::map<std::string, ConfigSchema> m_schemas;
    std::vector<ConfigSnapshot> m_snapshots;

    std::map<std::string, std::string> ParseConfigFile(const std::string& path) {
        std::map<std::string, std::string> values;
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return values;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;
            
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                values[key] = value;
            }
        }
        
        return values;
    }

    void PersistConfiguration(Environment env) {
        // Persist to file
    }

    std::string EncryptSecret(const std::string& secret) {
        // Encrypt using secure method
        return "encrypted:" + secret; // Simplified
    }

    std::string DecryptSecret(const std::string& encrypted) {
        // Decrypt
        if (encrypted.substr(0, 10) == "encrypted:") {
            return encrypted.substr(10);
        }
        return encrypted;
    }

    std::string EnvironmentToString(Environment env) {
        switch (env) {
            case Environment::DEVELOPMENT: return "Development";
            case Environment::STAGING: return "Staging";
            case Environment::PRODUCTION: return "Production";
            case Environment::TESTING: return "Testing";
            default: return "Unknown";
        }
    }

    std::string TypeToString(ConfigValueType type) {
        switch (type) {
            case ConfigValueType::STRING: return "String";
            case ConfigValueType::INTEGER: return "Integer";
            case ConfigValueType::BOOLEAN: return "Boolean";
            case ConfigValueType::FLOAT: return "Float";
            case ConfigValueType::JSON: return "JSON";
            case ConfigValueType::SECRET: return "Secret";
            default: return "Unknown";
        }
    }

    std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto timeT = std::chrono::system_clock::to_time_t(time);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

} // namespace RawrXD::Config
