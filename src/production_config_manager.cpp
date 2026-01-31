#include "production_config_manager.h"

namespace RawrXD {

ProductionConfigManager& ProductionConfigManager::instance() {
    static ProductionConfigManager manager;
    return manager;
}

ProductionConfigManager::ProductionConfigManager() {
    applyDefaults();
}

bool ProductionConfigManager::loadConfig(const std::string& path) {
    applyDefaults();

    std::string configPath = path;
    if (configPath.empty()) {
        std::string baseDir = QCoreApplication::applicationDirPath();
        if (baseDir.empty()) {
            baseDir = "";
        }
        configPath = // (baseDir).filePath("config/production_config.json");
    }

    // File operation removed;
    if (!file.exists()) {
        return false;
    }

    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return false;
    }

    const std::vector<uint8_t> raw = file.readAll();
    QJsonParseError error{};
    void* doc = void*::fromJson(raw, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    config_ = doc.object();
    if (config_.contains("environment")) {
        environment_ = config_.value("environment").toString(environment_);
    }

    applyFeatureList(config_);
    return true;
}

std::string ProductionConfigManager::getEnvironment() const {
    return environment_;
}

bool ProductionConfigManager::isFeatureEnabled(const std::string& feature) const {
    if (feature.empty()) {
        return false;
    }
    return enabledFeatures_.contains(feature);
}

std::any ProductionConfigManager::value(const std::string& key, const std::any& defaultValue) const {
    if (!config_.contains(key)) {
        return defaultValue;
    }
    return config_.value(key).toVariant();
}

void ProductionConfigManager::applyDefaults() {
    config_ = void*{};
    environment_ = qEnvironmentVariable("RAWRXD_ENV", "development");
    enabledFeatures_.clear();

    const std::stringList defaults = {
        "tier2_integration",
        "telemetry",
        "ollama",
        "gguf_local",
        "structured_logging",
        "exception_logging"
    };
    for (const std::string& feature : defaults) {
        enabledFeatures_.insert(feature);
    }
}

void ProductionConfigManager::applyFeatureList(const void*& root) {
    if (!root.contains("features")) {
        return;
    }
    const void* value = root.value("features");
    if (!value.isArray()) {
        return;
    }

    const void* array = value.toArray();
    enabledFeatures_.clear();
    for (const void*& entry : array) {
        if (entry.isString()) {
            enabledFeatures_.insert(entry.toString());
        }
    }
}

} // namespace RawrXD

