#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class Environment {
    Development,
    Staging,
    Production
};

class AgenticConfiguration {
public:
    static AgenticConfiguration& getInstance();

    explicit AgenticConfiguration(void* parent = nullptr);
    ~AgenticConfiguration();

    void initializeFromEnvironment(Environment env);
    bool loadFromJson(const std::string& filePath);
    
    void set(const std::string& key, const std::string& value);
    std::string get(const std::string& key, const std::string& defaultValue = "") const;

    std::string getModelPath() const { return get("model_path", "models/default.gguf"); }

private:
    void loadDefaults();
    void applyEnvironmentOverrides();
    void configurationLoaded();

    Environment m_currentEnvironment;
    std::unordered_map<std::string, std::string> m_settings;
};
