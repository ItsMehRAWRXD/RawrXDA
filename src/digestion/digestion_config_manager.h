#pragma once

#include "digestion_reverse_engineering.h"

struct DigestionModuleConfig {
    DigestionConfig engineConfig;
    std::string databasePath;
    std::string schemaPath;
    std::string outputPath;
    std::stringList flags;
    bool enableDatabase = true;
    bool enableMetrics = true;
};

class DigestionConfigManager {
public:
    static DigestionModuleConfig loadFromFile(const std::string &path, std::string *error = nullptr);
    static DigestionModuleConfig loadFromJson(const void* &json, std::string *error = nullptr);
    static DigestionModuleConfig loadFromYaml(const std::string &yamlText, std::string *error = nullptr);

    static void* parseYamlToJson(const std::string &yamlText, std::string *error = nullptr);

private:
    static void* parseScalar(const std::string &value);
    static void assignValue(void* &root, const std::string &section, const std::string &key, const void* &value);
    static std::stringList parseInlineList(const std::string &value);
};


