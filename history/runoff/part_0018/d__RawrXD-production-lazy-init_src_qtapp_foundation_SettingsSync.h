#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMutex>
#include <QJsonObject>

// SettingsSync manages IDE settings persistence with schema validation and env overrides.
class SettingsSync : public QObject {
    Q_OBJECT
public:
    explicit SettingsSync(QObject* parent = nullptr);
    ~SettingsSync();

    bool load(const QString& filePath);
    bool save(const QString& filePath) const;

    QVariant value(const QString& key, const QVariant& def = QVariant()) const;
    void setValue(const QString& key, const QVariant& v);

    void applyEnvOverrides(const QString& prefix = "IDE_");
    void setDefaults(const QVariantMap& defaults);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private:
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

    QVariantMap m_settings;
    QVariantMap m_defaults;
    mutable QMutex m_mutex;
};
