#include "production_config_manager.h"
#include <fstream>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

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
        char buffer[MAX_PATH];
#ifdef _WIN32
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::filesystem::path appPath(buffer);
        configPath = (appPath.parent_path() / "config/production_config.json").string();
#else
        configPath = "config/production_config.json";
#endif
    }

    if (!std::filesystem::exists(configPath)) {
        return false;
    }

    try {
        std::ifstream file(configPath);
        if (!file.is_open()) return false;
        
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        config_ = nlohmann::json::parse(content);
        
        if (config_.contains("environment") && config_["environment"].is_string()) {
            environment_ = config_["environment"].get<std::string>();
        }

        applyFeatureList(config_);
        return true;
    } catch (...) {
        return false;
    }
}

void ProductionConfigManager::applyDefaults() {
    environment_ = "production";
    enabledFeatures_.clear();
}

void ProductionConfigManager::applyFeatureList(const nlohmann::json& root) {
    if (root.contains("features") && root["features"].is_array()) {
        for (auto& feature : root["features"]) {
            if (feature.is_string()) {
                enabledFeatures_.insert(feature.get<std::string>());
            }
        }
    }
}

std::string ProductionConfigManager::getEnvironment() const {
    return environment_;
}

bool ProductionConfigManager::isFeatureEnabled(const std::string& feature) const {
    return enabledFeatures_.find(feature) != enabledFeatures_.end();
}

nlohmann::json ProductionConfigManager::value(const std::string& key, const nlohmann::json& defaultValue) const {
    if (config_.contains(key)) {
        return config_[key];
    }
    return defaultValue;
}

} // namespace RawrXD

