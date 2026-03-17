#pragma once

#include <QString>
#include <QVariant>
#include <QSettings>

namespace rawrxd::config {

inline QString env(const char* key, const QString& def = QString()) {
    const QByteArray v = qgetenv(key);
    return v.isEmpty() ? def : QString::fromUtf8(v);
}

inline QVariant setting(const QString& key, const QVariant& def = QVariant()) {
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "RawrXD", "RawrXD");
    return s.value(key, def);
}

inline bool featureEnabled(const QString& featureKey, bool defaultOn = false) {
    const auto envKey = QString("RAWRXD_FEATURE_") + featureKey;
    const auto envVal = env(envKey.toUtf8().constData(), QString());
    if (!envVal.isEmpty()) {
        const auto lower = envVal.toLower();
        if (lower == "1" || lower == "true" || lower == "on") return true;
        if (lower == "0" || lower == "false" || lower == "off") return false;
    }

    return setting("features/" + featureKey, defaultOn).toBool();
}

} // namespace rawrxd::config
