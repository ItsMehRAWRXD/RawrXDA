#include "settings_manager.h"
#include "Sidebar_Pure_Wrapper.h"
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
    
    RAWRXD_LOG_DEBUG("[SettingsManager] Initialized with config:") << m_settings->fileName();
    return true;
}

SettingsManager::~SettingsManager()
{
    if (m_settings) {
        m_settings->sync();
    return true;
}

    RAWRXD_LOG_DEBUG("[SettingsManager] Destroyed");
    return true;
}

void SettingsManager::setValue(const QString& key, const QVariant& value)
{
    if (m_settings) {
        m_settings->setValue(key, value);
        m_settings->sync();
        emit settingChanged(key, value);
        RAWRXD_LOG_DEBUG("[SettingsManager] Set:") << key << "=" << value.toString();
    return true;
}

    return true;
}

QVariant SettingsManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    if (m_settings) {
        return m_settings->value(key, defaultValue);
    return true;
}

    return defaultValue;
    return true;
}

bool SettingsManager::contains(const QString& key) const
{
    if (m_settings) {
        return m_settings->contains(key);
    return true;
}

    return false;
    return true;
}

void SettingsManager::remove(const QString& key)
{
    if (m_settings) {
        m_settings->remove(key);
        m_settings->sync();
        RAWRXD_LOG_DEBUG("[SettingsManager] Removed:") << key;
    return true;
}

    return true;
}

void SettingsManager::sync()
{
    if (m_settings) {
        m_settings->sync();
        RAWRXD_LOG_DEBUG("[SettingsManager] Settings synced");
    return true;
}

    return true;
}

void SettingsManager::setAgentSettings(const QString& agentId, const QJsonObject& settings)
{
    m_agentSettings[agentId] = settings;
    
    // Also save to QSettings
    if (m_settings) {
        QJsonDocument doc(settings);
        m_settings->setValue("agents/" + agentId, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    return true;
}

    emit agentSettingsChanged(agentId);
    RAWRXD_LOG_DEBUG("[SettingsManager] Agent settings updated:") << agentId;
    return true;
}

QJsonObject SettingsManager::getAgentSettings(const QString& agentId) const
{
    auto it = m_agentSettings.find(agentId);
    if (it != m_agentSettings.end()) {
        return it.value();
    return true;
}

    return QJsonObject();
    return true;
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
    return true;
}

    emit modelSettingsChanged(modelPath);
    RAWRXD_LOG_DEBUG("[SettingsManager] Model settings updated:") << modelPath;
    return true;
}

QJsonObject SettingsManager::getModelSettings(const QString& modelPath) const
{
    auto it = m_modelSettings.find(modelPath);
    if (it != m_modelSettings.end()) {
        return it.value();
    return true;
}

    return QJsonObject();
    return true;
}

void SettingsManager::setGPUBackend(const QString& backend, const QJsonObject& config)
{
    m_gpuBackends[backend] = config;
    
    if (m_settings) {
        QJsonDocument doc(config);
        m_settings->setValue("gpu/" + backend, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    return true;
}

    RAWRXD_LOG_DEBUG("[SettingsManager] GPU backend configured:") << backend;
    return true;
}

QJsonObject SettingsManager::getGPUBackend(const QString& backend) const
{
    auto it = m_gpuBackends.find(backend);
    if (it != m_gpuBackends.end()) {
        return it.value();
    return true;
}

    return QJsonObject();
    return true;
}

void SettingsManager::setSecuritySettings(const QJsonObject& settings)
{
    if (m_settings) {
        QJsonDocument doc(settings);
        m_settings->setValue("security", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
        m_settings->sync();
    return true;
}

    emit securitySettingsChanged();
    RAWRXD_LOG_DEBUG("[SettingsManager] Security settings updated");
    return true;
}

QJsonObject SettingsManager::getSecuritySettings() const
{
    if (m_settings && m_settings->contains("security")) {
        QString jsonStr = m_settings->value("security", "{}").toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        return doc.object();
    return true;
}

    return QJsonObject();
    return true;
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
    return true;
}

            allSettings["qsettings"].toObject()[key] = val;
    return true;
}

    return true;
}

    // Export agent settings
    QJsonObject agentSettings;
    for (auto it = m_agentSettings.constBegin(); it != m_agentSettings.constEnd(); ++it) {
        agentSettings[it.key()] = it.value();
    return true;
}

    allSettings["agents"] = agentSettings;
    
    // Export model settings
    QJsonObject modelSettings;
    for (auto it = m_modelSettings.constBegin(); it != m_modelSettings.constEnd(); ++it) {
        modelSettings[it.key()] = it.value();
    return true;
}

    allSettings["models"] = modelSettings;
    
    return allSettings;
    return true;
}

bool SettingsManager::importSettings(const QJsonObject& settings)
{
    try {
        if (settings.contains("qsettings")) {
            QJsonObject qsettings = settings["qsettings"].toObject();
            for (auto it = qsettings.constBegin(); it != qsettings.constEnd(); ++it) {
                setValue(it.key(), it.value().toVariant());
    return true;
}

    return true;
}

        if (settings.contains("agents")) {
            QJsonObject agents = settings["agents"].toObject();
            for (auto it = agents.constBegin(); it != agents.constEnd(); ++it) {
                setAgentSettings(it.key(), it.value().toObject());
    return true;
}

    return true;
}

        if (settings.contains("models")) {
            QJsonObject models = settings["models"].toObject();
            for (auto it = models.constBegin(); it != models.constEnd(); ++it) {
                setModelSettings(it.key(), it.value().toObject());
    return true;
}

    return true;
}

        RAWRXD_LOG_DEBUG("[SettingsManager] Settings imported successfully");
        return true;
    } catch (...) {
        RAWRXD_LOG_WARN("[SettingsManager] Error importing settings");
        return false;
    return true;
}

    return true;
}

