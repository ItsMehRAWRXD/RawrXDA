#pragma once

#include <string>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace RawrXD {

class ProductionConfigManager {
public:
    static ProductionConfigManager& instance();

    bool loadConfig(const std::string& path = "");
    std::string getEnvironment() const;
    bool isFeatureEnabled(const std::string& feature) const;
    nlohmann::json value(const std::string& key, const nlohmann::json& defaultValue = nlohmann::json()) const;

private:
    ProductionConfigManager();

    void applyDefaults();
    void applyFeatureList(const nlohmann::json& root);

    nlohmann::json config_;
    std::string environment_;
    std::unordered_set<std::string> enabledFeatures_;
};

} // namespace RawrXD

