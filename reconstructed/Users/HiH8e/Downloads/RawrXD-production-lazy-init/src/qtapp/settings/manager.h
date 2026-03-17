#pragma once

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QMap>

struct CompressionSettings {
    int preferred_type = 2; // BRUTAL_GZIP
    bool enable_stats = true;
    uint64_t max_decomp_bytes = 10ULL * 1024 * 1024 * 1024; // 10GB
};

class SettingsManager : public QObject {
    Q_OBJECT

public:
    explicit SettingsManager(QObject* parent = nullptr);
    ~SettingsManager();

    static SettingsManager& instance();

    // Core settings management
    void setValue(const QString& key, const QVariant& value);
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    bool contains(const QString& key) const;
    void remove(const QString& key);
    void sync();

    // Compression settings
    CompressionSettings& compressionSettings() { return m_compressionSettings; }
    const CompressionSettings& compressionSettings() const { return m_compressionSettings; }

    // Agent-specific settings
    void setAgentSettings(const QString& agentId, const QJsonObject& settings);
    QJsonObject getAgentSettings(const QString& agentId) const;

    // Model settings
    void setModelSettings(const QString& modelPath, const QJsonObject& settings);
    QJsonObject getModelSettings(const QString& modelPath) const;

    // GPU backend settings
    void setGPUBackend(const QString& backend, const QJsonObject& config);
    QJsonObject getGPUBackend(const QString& backend) const;

    // Security settings
    void setSecuritySettings(const QJsonObject& settings);
    QJsonObject getSecuritySettings() const;

    // Export/Import
    QJsonObject exportAllSettings() const;
    bool importSettings(const QJsonObject& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);
    void agentSettingsChanged(const QString& agentId);
    void modelSettingsChanged(const QString& modelPath);
    void securitySettingsChanged();

private:
    QSettings* m_settings;
    CompressionSettings m_compressionSettings;
    QMap<QString, QJsonObject> m_agentSettings;
    QMap<QString, QJsonObject> m_modelSettings;
    QMap<QString, QJsonObject> m_gpuBackends;
};
