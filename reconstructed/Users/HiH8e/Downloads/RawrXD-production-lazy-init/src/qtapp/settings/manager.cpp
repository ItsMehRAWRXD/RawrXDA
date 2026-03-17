#include "settings_manager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QFile>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent),
      m_settings(nullptr),
      m_agentSettings(QMap<QString, QJsonObject>()),
      m_modelSettings(QMap<QString, QJsonObject>()),
      m_gpuBackends(QMap<QString, QJsonObject>())
{
    // Create QSettings with application organization and name
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_settings = new QSettings(configPath + "/RawrXD.ini", QSettings::IniFormat, this);
    
    // Load compression settings
    m_compressionSettings.preferred_type = m_settings->value("compression/preferred_type", 2).toInt();
    m_compressionSettings.enable_stats = m_settings->value("compression/enable_stats", true).toBool();
    m_compressionSettings.max_decomp_bytes = m_settings->value("compression/max_bytes", 10ULL*1024*1024*1024).toULongLong();
    
    qDebug() << "[SettingsManager] Initialized with config:" << m_settings->fileName();
}

SettingsManager& SettingsManager::instance()
{
    static SettingsManager* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new SettingsManager();
    }
    return *s_instance;
}

SettingsManager::~SettingsManager()
{
    if (m_settings) {
        m_settings->sync();
    }
    qDebug() << "[SettingsManager] Destroyed";
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    if (m_settings) {
        m_settings->setValue(key, value);
        
        // Update internal structs
        if (key == "compression/preferred_type") m_compressionSettings.preferred_type = value.toInt();
        else if (key == "compression/enable_stats") m_compressionSettings.enable_stats = value.toBool();
        else if (key == "compression/max_bytes") m_compressionSettings.max_decomp_bytes = value.toULongLong();
        
        m_settings->sync();
        emit settingChanged(key, value);
        qDebug() << "[SettingsManager] Set:" << key << "=" << value.toString();
    }
}

QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    if (m_settings) {
        return m_settings->value(key, defaultValue);
    }
    return defaultValue;
}

bool SettingsManager::contains(const QString& key) const
{
    if (m_settings) {
        return m_settings->contains(key);
    }
    return false;
}

void SettingsManager::remove(const QString& key)
{
    if (m_settings) {
        m_settings->remove(key);
        m_settings->sync();
        qDebug() << "[SettingsManager] Removed:" << key;
    }
}

void SettingsManager::sync()
{
    if (m_settings) {
        m_settings->sync();
        qDebug() << "[SettingsManager] Settings synced";
    }
}

void SettingsManager::setAgentSettings(const QString& agentId, const QJsonObject& settings)
{
    m_agentSettings[agentId] = settings;
    
    // Also save to QSettings
    if (m_settings) {
        QJsonDocument doc(settings);
        m_settings->setValue("agents/" + agentId, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    }
    
    emit agentSettingsChanged(agentId);
    qDebug() << "[SettingsManager] Agent settings updated:" << agentId;
}

QJsonObject SettingsManager::getAgentSettings(const QString& agentId) const
{
    auto it = m_agentSettings.find(agentId);
    if (it != m_agentSettings.end()) {
        return it.value();
    }
    return QJsonObject();
}

void SettingsManager::setModelSettings(const QString& modelPath, const QJsonObject& settings)
{
    m_modelSettings[modelPath] = settings;
    
    if (m_settings) {
        QJsonDocument doc(settings);
        QString modelKey = "models/" + modelPath;
        modelKey.replace('/', '_'); // Replace slashes with underscores
        m_settings->setValue(modelKey, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    }
    
    emit modelSettingsChanged(modelPath);
    qDebug() << "[SettingsManager] Model settings updated:" << modelPath;
}

QJsonObject SettingsManager::getModelSettings(const QString& modelPath) const
{
    auto it = m_modelSettings.find(modelPath);
    if (it != m_modelSettings.end()) {
        return it.value();
    }
    return QJsonObject();
}

void SettingsManager::setGPUBackend(const QString& backend, const QJsonObject& config)
{
    m_gpuBackends[backend] = config;
    
    if (m_settings) {
        QJsonDocument doc(config);
        m_settings->setValue("gpu/" + backend, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    }
    
    qDebug() << "[SettingsManager] GPU backend configured:" << backend;
}

QJsonObject SettingsManager::getGPUBackend(const QString& backend) const
{
    auto it = m_gpuBackends.find(backend);
    if (it != m_gpuBackends.end()) {
        return it.value();
    }
    return QJsonObject();
}

void SettingsManager::setSecuritySettings(const QJsonObject& settings)
{
    if (m_settings) {
        QJsonDocument doc(settings);
        m_settings->setValue("security", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    }
    
    emit securitySettingsChanged();
    qDebug() << "[SettingsManager] Security settings updated";
}

QJsonObject SettingsManager::getSecuritySettings() const
{
    if (m_settings && m_settings->contains("security")) {
        QString jsonStr = m_settings->value("security", "{}").toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        return doc.object();
    }
    return QJsonObject();
}

QJsonObject SettingsManager::exportAllSettings() const
{
    QJsonObject allSettings;
    
    // Export all QSettings
    if (m_settings) {
        allSettings["qsettings"] = QJsonObject();
        for (const QString& key : m_settings->allKeys()) {
            QJsonValue val(QJsonValue::String);
            QVariant variant = m_settings->value(key);
            if (variant.type() == QVariant::Int) {
                val = QJsonValue(variant.toInt());
            } else if (variant.type() == QVariant::Bool) {
                val = QJsonValue(variant.toBool());
            } else {
                val = QJsonValue(variant.toString());
            }
            allSettings["qsettings"].toObject()[key] = val;
        }
    }
    
    // Export agent settings
    QJsonObject agentSettings;
    for (auto it = m_agentSettings.constBegin(); it != m_agentSettings.constEnd(); ++it) {
        agentSettings[it.key()] = it.value();
    }
    allSettings["agents"] = agentSettings;
    
    // Export model settings
    QJsonObject modelSettings;
    for (auto it = m_modelSettings.constBegin(); it != m_modelSettings.constEnd(); ++it) {
        modelSettings[it.key()] = it.value();
    }
    allSettings["models"] = modelSettings;
    
    return allSettings;
}

bool SettingsManager::importSettings(const QJsonObject& settings)
{
    try {
        if (settings.contains("qsettings")) {
            QJsonObject qsettings = settings["qsettings"].toObject();
            for (auto it = qsettings.constBegin(); it != qsettings.constEnd(); ++it) {
                setValue(it.key(), it.value().toVariant());
            }
        }
        
        if (settings.contains("agents")) {
            QJsonObject agents = settings["agents"].toObject();
            for (auto it = agents.constBegin(); it != agents.constEnd(); ++it) {
                setAgentSettings(it.key(), it.value().toObject());
            }
        }
        
        if (settings.contains("models")) {
            QJsonObject models = settings["models"].toObject();
            for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
                setModelSettings(it.key(), it.value().toObject());
            }
        }
        
        qDebug() << "[SettingsManager] Settings imported successfully";
        return true;
    } catch (...) {
        qWarning() << "[SettingsManager] Error importing settings";
        return false;
    }
}
