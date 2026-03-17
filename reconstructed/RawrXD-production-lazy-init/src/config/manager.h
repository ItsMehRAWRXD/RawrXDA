#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <memory>
#include <mutex>

namespace RawrXD {

class ConfigManager : public QObject {
    Q_OBJECT

public:
    static ConfigManager& instance();
    
    // Configuration loading
    bool loadConfig(const QString& configPath = QString());
    bool loadEnvironmentConfig(const QString& environment);
    bool reloadConfig();
    
    // Feature flags
    bool isFeatureEnabled(const QString& featureName) const;
    void setFeatureEnabled(const QString& featureName, bool enabled);
    
    // Configuration access
    QJsonObject section(const QString& sectionName) const;
    QJsonObject getLoggingConfig() const;
    QJsonObject getMetricsConfig() const;
    QJsonObject getTracingConfig() const;
    QJsonObject getHotpatchingConfig() const;
    QJsonObject getErrorDetectionConfig() const;
    QJsonObject getResourceLimitsConfig() const;
    QJsonObject getHealthChecksConfig() const;
    
    // Generic config access
    QJsonValue getValue(const QString& path) const;
    QString getString(const QString& path, const QString& defaultValue = QString()) const;
    int getInt(const QString& path, int defaultValue = 0) const;
    double getDouble(const QString& path, double defaultValue = 0.0) const;
    bool getBool(const QString& path, bool defaultValue = false) const;
    
    // Current environment
    QString getCurrentEnvironment() const { return m_currentEnvironment; }
    QString getConfigVersion() const;
    
    // Hot reload support
    void enableHotReload(bool enable);
    bool isHotReloadEnabled() const { return m_hotReloadEnabled; }
    
signals:
    void configLoaded(const QString& environment);
    void configReloaded();
    void configError(const QString& error);
    void featureToggled(const QString& feature, bool enabled);
    
private:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    bool loadConfigFromFile(const QString& filePath);
    QString resolveConfigPath(const QString& environment) const;
    QJsonValue getNestedValue(const QJsonObject& obj, const QStringList& path) const;
    
private slots:
    void onConfigFileChanged(const QString& path);
    
private:
    mutable std::mutex m_mutex;
    QJsonObject m_config;
    QString m_currentConfigPath;
    QString m_currentEnvironment;
    bool m_hotReloadEnabled = false;
    class QFileSystemWatcher* m_watcher = nullptr;
};

} // namespace RawrXD
