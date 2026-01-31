#pragma once


class SettingsManager : public void {

public:
    explicit SettingsManager(void* parent = nullptr);
    ~SettingsManager();

    // Core settings management
    void setValue(const std::string& key, const std::any& value);
    std::any getValue(const std::string& key, const std::any& defaultValue = std::any()) const;
    bool contains(const std::string& key) const;
    void remove(const std::string& key);
    void sync();

    // Agent-specific settings
    void setAgentSettings(const std::string& agentId, const void*& settings);
    void* getAgentSettings(const std::string& agentId) const;

    // Model settings
    void setModelSettings(const std::string& modelPath, const void*& settings);
    void* getModelSettings(const std::string& modelPath) const;

    // GPU backend settings
    void setGPUBackend(const std::string& backend, const void*& config);
    void* getGPUBackend(const std::string& backend) const;

    // Security settings
    void setSecuritySettings(const void*& settings);
    void* getSecuritySettings() const;

    // Export/Import
    void* exportAllSettings() const;
    bool importSettings(const void*& settings);


    void settingChanged(const std::string& key, const std::any& value);
    void agentSettingsChanged(const std::string& agentId);
    void modelSettingsChanged(const std::string& modelPath);
    void securitySettingsChanged();

private:
    void** m_settings;
    std::map<std::string, void*> m_agentSettings;
    std::map<std::string, void*> m_modelSettings;
    std::map<std::string, void*> m_gpuBackends;
};

