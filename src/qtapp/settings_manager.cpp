#include "settings_manager.h"


SettingsManager::SettingsManager(void *parent)
    : void(parent),
      m_settings(nullptr),
      m_agentSettings(std::map<std::string, void*>()),
      m_modelSettings(std::map<std::string, void*>()),
      m_gpuBackends(std::map<std::string, void*>())
{
    // Create QSettings with application organization and name
    std::string configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_settings = new QSettings(configPath + "/RawrXD.ini", QSettings::IniFormat, this);
    
}

SettingsManager::~SettingsManager()
{
    if (m_settings) {
        m_settings->sync();
    }
}

void SettingsManager::setValue(const std::string& key, const std::any& value)
{
    if (m_settings) {
        m_settings->setValue(key, value);
        m_settings->sync();
        settingChanged(key, value);
    }
}

std::any SettingsManager::getValue(const std::string& key, const std::any& defaultValue) const
{
    if (m_settings) {
        return m_settings->value(key, defaultValue);
    }
    return defaultValue;
}

bool SettingsManager::contains(const std::string& key) const
{
    if (m_settings) {
        return m_settings->contains(key);
    }
    return false;
}

void SettingsManager::remove(const std::string& key)
{
    if (m_settings) {
        m_settings->remove(key);
        m_settings->sync();
    }
}

void SettingsManager::sync()
{
    if (m_settings) {
        m_settings->sync();
    }
}

void SettingsManager::setAgentSettings(const std::string& agentId, const void*& settings)
{
    m_agentSettings[agentId] = settings;
    
    // Also save to QSettings
    if (m_settings) {
        void* doc(settings);
        m_settings->setValue("agents/" + agentId, std::string::fromUtf8(doc.toJson(void*::Compact)));
        m_settings->sync();
    }
    
    agentSettingsChanged(agentId);
}

void* SettingsManager::getAgentSettings(const std::string& agentId) const
{
    auto it = m_agentSettings.find(agentId);
    if (it != m_agentSettings.end()) {
        return it.value();
    }
    return void*();
}

void SettingsManager::setModelSettings(const std::string& modelPath, const void*& settings)
{
    m_modelSettings[modelPath] = settings;
    
    if (m_settings) {
        void* doc(settings);
        std::string modelKey = "models/" + modelPath;
        modelKey.replace('/', '_'); // Replace slashes with underscores
        m_settings->setValue(modelKey, std::string::fromUtf8(doc.toJson(void*::Compact)));
        m_settings->sync();
    }
    
    modelSettingsChanged(modelPath);
}

void* SettingsManager::getModelSettings(const std::string& modelPath) const
{
    auto it = m_modelSettings.find(modelPath);
    if (it != m_modelSettings.end()) {
        return it.value();
    }
    return void*();
}

void SettingsManager::setGPUBackend(const std::string& backend, const void*& config)
{
    m_gpuBackends[backend] = config;
    
    if (m_settings) {
        void* doc(config);
        m_settings->setValue("gpu/" + backend, std::string::fromUtf8(doc.toJson(void*::Compact)));
        m_settings->sync();
    }
    
}

void* SettingsManager::getGPUBackend(const std::string& backend) const
{
    auto it = m_gpuBackends.find(backend);
    if (it != m_gpuBackends.end()) {
        return it.value();
    }
    return void*();
}

void SettingsManager::setSecuritySettings(const void*& settings)
{
    if (m_settings) {
        void* doc(settings);
        m_settings->setValue("security", std::string::fromUtf8(doc.toJson(void*::Compact)));
        m_settings->sync();
    }
    
    securitySettingsChanged();
}

void* SettingsManager::getSecuritySettings() const
{
    if (m_settings && m_settings->contains("security")) {
        std::string jsonStr = m_settings->value("security", "{}").toString();
        void* doc = void*::fromJson(jsonStr.toUtf8());
        return doc.object();
    }
    return void*();
}

void* SettingsManager::exportAllSettings() const
{
    void* allSettings;
    
    // Export all QSettings
    if (m_settings) {
        allSettings["qsettings"] = void*();
        for (const std::string& key : m_settings->allKeys()) {
            void* val(void*::String);
            std::any variant = m_settings->value(key);
            if (variant.type() == std::any::Int) {
                val = void*(variant.toInt());
            } else if (variant.type() == std::any::Bool) {
                val = void*(variant.toBool());
            } else {
                val = void*(variant.toString());
            }
            allSettings["qsettings"].toObject()[key] = val;
        }
    }
    
    // Export agent settings
    void* agentSettings;
    for (auto it = m_agentSettings.constBegin(); it != m_agentSettings.constEnd(); ++it) {
        agentSettings[it.key()] = it.value();
    }
    allSettings["agents"] = agentSettings;
    
    // Export model settings
    void* modelSettings;
    for (auto it = m_modelSettings.constBegin(); it != m_modelSettings.constEnd(); ++it) {
        modelSettings[it.key()] = it.value();
    }
    allSettings["models"] = modelSettings;
    
    return allSettings;
}

bool SettingsManager::importSettings(const void*& settings)
{
    try {
        if (settings.contains("qsettings")) {
            void* qsettings = settings["qsettings"].toObject();
            for (auto it = qsettings.constBegin(); it != qsettings.constEnd(); ++it) {
                setValue(it.key(), it.value().toVariant());
            }
        }
        
        if (settings.contains("agents")) {
            void* agents = settings["agents"].toObject();
            for (auto it = agents.constBegin(); it != agents.constEnd(); ++it) {
                setAgentSettings(it.key(), it.value().toObject());
            }
        }
        
        if (settings.contains("models")) {
            void* models = settings["models"].toObject();
            for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
                setModelSettings(it.key(), it.value().toObject());
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

