#include "production_config_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QDebug>

namespace RawrXD {

ProductionConfigManager& ProductionConfigManager::instance() {
    static ProductionConfigManager manager;
    return manager;
}

ProductionConfigManager::ProductionConfigManager() {
    applyDefaults();
}

bool ProductionConfigManager::loadConfig(const QString& path) {
    applyDefaults();

    QString configPath = path;
    if (configPath.isEmpty()) {
        QString baseDir = QCoreApplication::applicationDirPath();
        if (baseDir.isEmpty()) {
            baseDir = QDir::currentPath();
        }
        configPath = QDir(baseDir).filePath("config/production_config.json");
    }

    QFile file(configPath);
    if (!file.exists()) {
        qWarning() << "[ProductionConfigManager] Config not found:" << configPath;
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[ProductionConfigManager] Failed to open config:" << configPath;
        return false;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError error{};
    QJsonDocument doc = QJsonDocument::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[ProductionConfigManager] Invalid JSON:" << error.errorString();
        return false;
    }

    config_ = doc.object();
    if (config_.contains("environment")) {
        environment_ = config_.value("environment").toString(environment_);
    }

    applyFeatureList(config_);
    return true;
}

QString ProductionConfigManager::getEnvironment() const {
    return environment_;
}

bool ProductionConfigManager::isFeatureEnabled(const QString& feature) const {
    if (feature.isEmpty()) {
        return false;
    }
    return enabledFeatures_.contains(feature);
}

QVariant ProductionConfigManager::value(const QString& key, const QVariant& defaultValue) const {
    if (!config_.contains(key)) {
        return defaultValue;
    }
    return config_.value(key).toVariant();
}

void ProductionConfigManager::applyDefaults() {
    config_ = QJsonObject{};
    environment_ = qEnvironmentVariable("RAWRXD_ENV", "development");
    enabledFeatures_.clear();

    const QStringList defaults = {
        "tier2_integration",
        "telemetry",
        "ollama",
        "gguf_local",
        "structured_logging",
        "exception_logging"
    };
    for (const QString& feature : defaults) {
        enabledFeatures_.insert(feature);
    }
}

void ProductionConfigManager::applyFeatureList(const QJsonObject& root) {
    if (!root.contains("features")) {
        return;
    }
    const QJsonValue value = root.value("features");
    if (!value.isArray()) {
        return;
    }

    const QJsonArray array = value.toArray();
    enabledFeatures_.clear();
    for (const QJsonValue& entry : array) {
        if (entry.isString()) {
            enabledFeatures_.insert(entry.toString());
        }
    }
}

} // namespace RawrXD
