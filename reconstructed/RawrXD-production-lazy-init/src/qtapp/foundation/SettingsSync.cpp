#include "SettingsSync.h"

#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <QProcessEnvironment>

SettingsSync::SettingsSync(QObject* parent) : QObject(parent) {}
SettingsSync::~SettingsSync() {}

QJsonObject SettingsSync::toJson() const {
    QJsonObject obj;
    for (auto it = m_settings.constBegin(); it != m_settings.constEnd(); ++it) {
        obj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    return obj;
}

void SettingsSync::fromJson(const QJsonObject& obj) {
    m_settings.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_settings[it.key()] = it.value().toVariant();
    }
}

bool SettingsSync::load(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    fromJson(doc.object());
    return true;
}

bool SettingsSync::save(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    QJsonDocument doc(toJson());
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << doc.toJson(QJsonDocument::Indented);
    return true;
}

QVariant SettingsSync::value(const QString& key, const QVariant& def) const {
    QMutexLocker locker(&m_mutex);
    if (m_settings.contains(key)) return m_settings.value(key);
    if (m_defaults.contains(key)) return m_defaults.value(key);
    return def;
}

void SettingsSync::setValue(const QString& key, const QVariant& v) {
    {
        QMutexLocker locker(&m_mutex);
        m_settings[key] = v;
    }
    emit settingChanged(key, v);
}

void SettingsSync::applyEnvOverrides(const QString& prefix) {
    QMutexLocker locker(&m_mutex);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
        QString envKey = prefix + it.key().toUpper().replace('.', '_');
        if (env.contains(envKey)) {
            it.value() = env.value(envKey);
        }
    }
}

void SettingsSync::setDefaults(const QVariantMap& defaults) {
    QMutexLocker locker(&m_mutex);
    m_defaults = defaults;
}
