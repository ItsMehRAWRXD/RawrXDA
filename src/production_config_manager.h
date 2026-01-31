#pragma once

namespace RawrXD {

class ProductionConfigManager {
public:
    static ProductionConfigManager& instance();

    bool loadConfig(const std::string& path = std::string());
    std::string getEnvironment() const;
    bool isFeatureEnabled(const std::string& feature) const;
    std::any value(const std::string& key, const std::any& defaultValue = std::any()) const;

private:
    ProductionConfigManager();

    void applyDefaults();
    void applyFeatureList(const void*& root);

    void* config_;
    std::string environment_;
    QSet<std::string> enabledFeatures_;
};

} // namespace RawrXD



