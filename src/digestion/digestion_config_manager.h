#pragma once

#include "digestion_reverse_engineering.h"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

struct DigestionModuleConfig {
    DigestionConfig engineConfig;
    std::string databasePath;
    std::string schemaPath;
    std::string outputPath;
    std::vector<std::string> flags;
    bool enableDatabase = true;
    bool enableMetrics = true;
};

class DigestionConfigManager {
public:
    static DigestionModuleConfig loadFromFile(const std::string& path, std::string* error = nullptr);
    static DigestionModuleConfig loadFromJson(const nlohmann::json& json, std::string* error = nullptr);
    static DigestionModuleConfig loadFromYaml(const std::string& yamlText, std::string* error = nullptr);

    static nlohmann::json parseYamlToJson(const std::string& yamlText, std::string* error = nullptr);

private:
    static nlohmann::json parseScalar(const std::string& value);
    static void assignValue(nlohmann::json& root, const std::string& section, const std::string& key, const nlohmann::json& value);
    static std::vector<std::string> parseInlineList(const std::string& value);
};

