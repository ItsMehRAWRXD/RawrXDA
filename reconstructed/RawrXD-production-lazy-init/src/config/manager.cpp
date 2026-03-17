#include "config_manager.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QStringList>

namespace RawrXD {

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load(const QString& path) {
    QMutexLocker lock(&mutex_);
    if (loaded_) {
        return true;
    }

    QString configPath = path;
    if (configPath.isEmpty()) {
        // Default: alongside binary in ../config/production-api-config.json
        const QString base = QCoreApplication::applicationDirPath();
        configPath = base + "/../config/production-api-config.json";
    }

    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return false;
    }

    root_ = doc.object();

    // Environment substitution for any string containing ${VAR}
    auto substitute = [this](QJsonObject& obj, const QString& prefix, auto&& substituteRef) -> void {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it->isString()) {
                const QString val = it->toString();
                const QString resolved = resolveEnvironment(val);
                if (resolved != val) {
                    it.value() = resolved;
                }
            } else if (it->isObject()) {
                QJsonObject nested = it->toObject();
                substituteRef(nested, prefix + it.key() + ".", substituteRef);
                it.value() = nested;
            }
        }
    };

    substitute(root_, QString(), substitute);
    loaded_ = true;
    return true;
}

QJsonObject ConfigManager::root() const {
    QMutexLocker lock(&mutex_);
    return root_;
}

QJsonObject ConfigManager::section(const QString& name) const {
    QMutexLocker lock(&mutex_);
    return root_.value(name).toObject();
}

QString ConfigManager::getString(const QString& dottedKey, const QString& defaultValue) const {
    const QJsonValue v = lookup(dottedKey);
    return v.isString() ? v.toString() : defaultValue;
}

bool ConfigManager::getBool(const QString& dottedKey, bool defaultValue) const {
    const QJsonValue v = lookup(dottedKey);
    return v.isBool() ? v.toBool() : defaultValue;
}

int ConfigManager::getInt(const QString& dottedKey, int defaultValue) const {
    const QJsonValue v = lookup(dottedKey);
    return v.isDouble() ? static_cast<int>(v.toDouble()) : defaultValue;
}

QString ConfigManager::resolveEnvironment(const QString& value) const {
    // Substitute ${VAR} with environment variable if present
    QString out = value;
    int start = out.indexOf("${");
    while (start != -1) {
        int end = out.indexOf('}', start + 2);
        if (end == -1) break;
        const QString key = out.mid(start + 2, end - start - 2);
        const QByteArray envVal = qgetenv(key.toUtf8().constData());
        if (!envVal.isEmpty()) {
            out.replace(start, end - start + 1, QString::fromUtf8(envVal));
        }
        start = out.indexOf("${", start + 1);
    }
    return out;
}

QJsonValue ConfigManager::lookup(const QString& dottedKey) const {
    QMutexLocker lock(&mutex_);
    const QStringList parts = dottedKey.split('.', Qt::SkipEmptyParts);
    QJsonValue current = root_;
    for (const QString& part : parts) {
        if (!current.isObject()) {
            return QJsonValue();
        }
        current = current.toObject().value(part);
    }
    return current;
}

} // namespace RawrXD
