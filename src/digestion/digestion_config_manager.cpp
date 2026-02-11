#include "digestion_config_manager.h"
DigestionModuleConfig DigestionConfigManager::loadFromFile(const std::string &path, std::string *error) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        if (error) *error = std::string("Failed to open config: %1"));
        return DigestionModuleConfig();
    }

    const std::vector<uint8_t> data = file.readAll();
    const std::string text = std::string::fromUtf8(data);

    if (path.endsWith(".yaml", CaseInsensitive) || path.endsWith(".yml", CaseInsensitive)) {
        return loadFromYaml(text, error);
    }

    QJsonParseError parseError;
    const void* doc = void*::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = std::string("JSON parse error: %1"));
        return DigestionModuleConfig();
    }
    return loadFromJson(doc.object(), error);
}

DigestionModuleConfig DigestionConfigManager::loadFromJson(const void* &json, std::string *error) {
    DigestionModuleConfig moduleConfig;

    const void* digestion = json.value("digestion").toObject();
    const void* database = json.value("database").toObject();

    moduleConfig.engineConfig.chunkSize = digestion.value("chunk_size").toInt(moduleConfig.engineConfig.chunkSize);
    moduleConfig.engineConfig.threadCount = digestion.value("threads").toInt(moduleConfig.engineConfig.threadCount);
    moduleConfig.engineConfig.maxTasksPerFile = digestion.value("max_tasks_per_file").toInt(moduleConfig.engineConfig.maxTasksPerFile);
    moduleConfig.engineConfig.maxFiles = digestion.value("max_files").toInt(moduleConfig.engineConfig.maxFiles);
    moduleConfig.engineConfig.applyExtensions = digestion.value("apply_fixes").toBool(moduleConfig.engineConfig.applyExtensions);
    moduleConfig.engineConfig.createBackups = digestion.value("create_backups").toBool(moduleConfig.engineConfig.createBackups);
    moduleConfig.engineConfig.incremental = digestion.value("incremental").toBool(moduleConfig.engineConfig.incremental);
    moduleConfig.engineConfig.useGitMode = digestion.value("git_mode").toBool(moduleConfig.engineConfig.useGitMode);
    moduleConfig.engineConfig.backupDir = digestion.value("backup_dir").toString(moduleConfig.engineConfig.backupDir);

    const void* flagsValue = digestion.value("flags");
    if (flagsValue.isArray()) {
        for (const void* &flag : flagsValue.toArray()) {
            moduleConfig.flags.append(flag.toString());
        }
    } else if (flagsValue.isString()) {
        moduleConfig.flags = flagsValue.toString().split(',', SkipEmptyParts);
    }

    moduleConfig.databasePath = database.value("path").toString("digestion_results.db");
    moduleConfig.schemaPath = database.value("schema").toString();
    moduleConfig.enableDatabase = database.value("enabled").toBool(true);
    moduleConfig.outputPath = digestion.value("output_path").toString();

    if (moduleConfig.engineConfig.chunkSize <= 0) {
        moduleConfig.engineConfig.chunkSize = 50;
    }

    if (moduleConfig.engineConfig.threadCount < 0) {
        moduleConfig.engineConfig.threadCount = 0;
    }

    if (moduleConfig.databasePath.empty() && moduleConfig.enableDatabase) {
        if (error) *error = "Database path is empty";
        moduleConfig.enableDatabase = false;
    }

    return moduleConfig;
}

DigestionModuleConfig DigestionConfigManager::loadFromYaml(const std::string &yamlText, std::string *error) {
    void* json = parseYamlToJson(yamlText, error);
    return loadFromJson(json, error);
}

void* DigestionConfigManager::parseYamlToJson(const std::string &yamlText, std::string *error) {
    void* root;
    std::string currentSection;
    std::string currentListKey;
    void* currentList;

    const std::stringList lines = yamlText.split(std::regex("\r?\n"));
    auto flushList = [&]() {
        if (!currentListKey.empty()) {
            void* sectionObj = root.value(currentSection).toObject();
            sectionObj.insert(currentListKey, currentList);
            root.insert(currentSection, sectionObj);
            currentListKey.clear();
            currentList = void*();
        }
    };

    for (const std::string &rawLine : lines) {
        std::string line = rawLine;
        const int commentIndex = line.indexOf('#');
        if (commentIndex >= 0) line = line.left(commentIndex);
        line = line.trimmed();
        if (line.empty()) continue;

        if (line.startsWith("- ")) {
            if (currentListKey.empty()) {
                if (error) *error = "YAML list item without list key";
                continue;
            }
            currentList.append(parseScalar(line.mid(2).trimmed()));
            continue;
        }

        const int colonIndex = line.indexOf(':');
        if (colonIndex < 0) {
            if (error) *error = std::string("Invalid YAML line: %1");
            continue;
        }

        const std::string key = line.left(colonIndex).trimmed();
        std::string value = line.mid(colonIndex + 1).trimmed();

        if (value.empty()) {
            flushList();
            currentSection = key;
            if (!root.contains(currentSection)) root.insert(currentSection, void*());
            continue;
        }

        if (value.startsWith('[') && value.endsWith(']')) {
            flushList();
            void* list;
            const std::stringList parts = parseInlineList(value);
            for (const std::string &part : parts) list.append(parseScalar(part));
            assignValue(root, currentSection, key, list);
            continue;
        }

        if (value == "|") {
            if (error) *error = "Multiline YAML values are not supported";
            continue;
        }

        if (value == "-") {
            currentListKey = key;
            currentList = void*();
            continue;
        }

        flushList();
        assignValue(root, currentSection, key, parseScalar(value));
    }

    flushList();
    return root;
}

void* DigestionConfigManager::parseScalar(const std::string &value) {
    const std::string trimmed = value.trimmed();
    if (trimmed.compare("true", CaseInsensitive) == 0) return true;
    if (trimmed.compare("false", CaseInsensitive) == 0) return false;

    bool ok = false;
    const int intValue = trimmed.toInt(&ok);
    if (ok) return intValue;

    const double doubleValue = trimmed.toDouble(&ok);
    if (ok) return doubleValue;

    std::string cleaned = trimmed;
    if ((cleaned.startsWith('"') && cleaned.endsWith('"')) || (cleaned.startsWith('\\'') && cleaned.endsWith('\\''))) {
        cleaned = cleaned.mid(1, cleaned.size() - 2);
    }
    return cleaned;
}

void DigestionConfigManager::assignValue(void* &root, const std::string &section, const std::string &key, const void* &value) {
    if (section.empty()) {
        root.insert(key, value);
        return;
    }
    void* sectionObj = root.value(section).toObject();
    sectionObj.insert(key, value);
    root.insert(section, sectionObj);
}

std::stringList DigestionConfigManager::parseInlineList(const std::string &value) {
    std::string inner = value.mid(1, value.length() - 2).trimmed();
    std::stringList parts;
    for (const std::string &part : inner.split(',', SkipEmptyParts)) {
        parts.append(part.trimmed());
    }
    return parts;
}

