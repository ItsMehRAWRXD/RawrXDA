// digestion_config_manager.cpp — C++20, Win32. No Qt. Uses nlohmann::json.

#include "digestion_config_manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace {

std::string trimCopy(std::string s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}

bool endsWithIgnoreCase(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin(),
        [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
}

std::vector<std::string> split(const std::string& s, char sep, bool skipEmpty = true) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, sep)) {
        std::string t = trimCopy(part);
        if (!skipEmpty || !t.empty()) out.push_back(std::move(t));
    }
    return out;
}

int indexOf(const std::string& s, char c, size_t from = 0) {
    auto pos = s.find(c, from);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
}

std::string left(const std::string& s, size_t n) {
    return s.substr(0, std::min(n, s.size()));
}

std::string mid(const std::string& s, size_t pos, size_t len = std::string::npos) {
    if (pos >= s.size()) return {};
    return s.substr(pos, len);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

int compareIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    for (size_t i = 0; i < a.size(); ++i) {
        int u = std::tolower(static_cast<unsigned char>(a[i]));
        int v = std::tolower(static_cast<unsigned char>(b[i]));
        if (u != v) return u < v ? -1 : 1;
    }
    return 0;
}

} // namespace

DigestionModuleConfig DigestionConfigManager::loadFromFile(const std::string& path, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "Failed to open config: " + path;
        return DigestionModuleConfig();
    }
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (endsWithIgnoreCase(path, ".yaml") || endsWithIgnoreCase(path, ".yml")) {
        return loadFromYaml(data, error);
    }

    try {
        nlohmann::json doc = nlohmann::json::parse(data);
        if (!doc.is_object()) {
            if (error) *error = "JSON root is not an object";
            return DigestionModuleConfig();
        }
        return loadFromJson(doc, error);
    } catch (const nlohmann::json::exception& e) {
        if (error) *error = std::string("JSON parse error: ") + e.what();
        return DigestionModuleConfig();
    }
}

DigestionModuleConfig DigestionConfigManager::loadFromJson(const nlohmann::json& json, std::string* error) {
    DigestionModuleConfig moduleConfig;

    auto digestion = json.contains("digestion") && json["digestion"].is_object() ? json["digestion"] : nlohmann::json::object();
    auto database = json.contains("database") && json["database"].is_object() ? json["database"] : nlohmann::json::object();

    moduleConfig.engineConfig.chunkSize = digestion.value("chunk_size", moduleConfig.engineConfig.chunkSize);
    moduleConfig.engineConfig.threadCount = digestion.value("threads", moduleConfig.engineConfig.threadCount);
    moduleConfig.engineConfig.maxTasksPerFile = digestion.value("max_tasks_per_file", moduleConfig.engineConfig.maxTasksPerFile);
    moduleConfig.engineConfig.maxFiles = digestion.value("max_files", moduleConfig.engineConfig.maxFiles);
    moduleConfig.engineConfig.applyExtensions = digestion.value("apply_fixes", moduleConfig.engineConfig.applyExtensions);
    moduleConfig.engineConfig.createBackups = digestion.value("create_backups", moduleConfig.engineConfig.createBackups);
    moduleConfig.engineConfig.incremental = digestion.value("incremental", moduleConfig.engineConfig.incremental);
    moduleConfig.engineConfig.useGitMode = digestion.value("git_mode", moduleConfig.engineConfig.useGitMode);
    if (digestion.contains("backup_dir") && digestion["backup_dir"].is_string())
        moduleConfig.engineConfig.backupDir = digestion["backup_dir"].get<std::string>();

    if (digestion.contains("flags")) {
        const auto& f = digestion["flags"];
        if (f.is_array()) {
            for (const auto& el : f)
                if (el.is_string()) moduleConfig.flags.push_back(el.get<std::string>());
        } else if (f.is_string()) {
            moduleConfig.flags = split(f.get<std::string>(), ',', true);
        }
    }

    if (database.contains("path") && database["path"].is_string())
        moduleConfig.databasePath = database["path"].get<std::string>();
    if (moduleConfig.databasePath.empty())
        moduleConfig.databasePath = "digestion_results.db";
    if (database.contains("schema") && database["schema"].is_string())
        moduleConfig.schemaPath = database["schema"].get<std::string>();
    moduleConfig.enableDatabase = database.value("enabled", true);
    if (digestion.contains("output_path") && digestion["output_path"].is_string())
        moduleConfig.outputPath = digestion["output_path"].get<std::string>();

    if (moduleConfig.engineConfig.chunkSize <= 0)
        moduleConfig.engineConfig.chunkSize = 50;
    if (moduleConfig.engineConfig.threadCount < 0)
        moduleConfig.engineConfig.threadCount = 0;
    if (moduleConfig.databasePath.empty() && moduleConfig.enableDatabase) {
        if (error) *error = "Database path is empty";
        moduleConfig.enableDatabase = false;
    }

    return moduleConfig;
}

DigestionModuleConfig DigestionConfigManager::loadFromYaml(const std::string& yamlText, std::string* error) {
    nlohmann::json j = parseYamlToJson(yamlText, error);
    if (j.is_null() && error && !error->empty())
        return DigestionModuleConfig();
    return loadFromJson(j, error);
}

nlohmann::json DigestionConfigManager::parseYamlToJson(const std::string& yamlText, std::string* error) {
    nlohmann::json root = nlohmann::json::object();
    std::string currentSection;
    std::string currentListKey;
    nlohmann::json currentList = nlohmann::json::array();

    std::regex lineSplit(R"(\r?\n)");
    std::sregex_token_iterator it(yamlText.begin(), yamlText.end(), lineSplit, -1);
    std::sregex_token_iterator end;

    auto flushList = [&]() {
        if (currentListKey.empty()) return;
        if (currentSection.empty()) {
            root[currentListKey] = currentList;
        } else {
            if (!root.contains(currentSection)) root[currentSection] = nlohmann::json::object();
            nlohmann::json& sectionObj = root[currentSection];
            if (!sectionObj.is_object()) sectionObj = nlohmann::json::object();
            sectionObj[currentListKey] = currentList;
        }
        currentListKey.clear();
        currentList = nlohmann::json::array();
    };

    for (; it != end; ++it) {
        std::string rawLine = *it;
        int commentIndex = indexOf(rawLine, '#');
        if (commentIndex >= 0) rawLine = rawLine.substr(0, static_cast<size_t>(commentIndex));
        std::string line = trimCopy(rawLine);
        if (line.empty()) continue;

        if (startsWith(line, "- ")) {
            if (currentListKey.empty()) {
                if (error) *error = "YAML list item without list key";
                continue;
            }
            currentList.push_back(parseScalar(trimCopy(mid(line, 2))));
            continue;
        }

        int colonIndex = indexOf(line, ':');
        if (colonIndex < 0) {
            if (error) *error = "Invalid YAML line: " + line;
            continue;
        }

        std::string key = trimCopy(left(line, static_cast<size_t>(colonIndex)));
        std::string value = trimCopy(mid(line, static_cast<size_t>(colonIndex) + 1));

        if (value.empty()) {
            flushList();
            currentSection = key;
            if (!root.contains(currentSection)) root[currentSection] = nlohmann::json::object();
            continue;
        }

        if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
            flushList();
            nlohmann::json list = nlohmann::json::array();
            std::vector<std::string> parts = parseInlineList(value);
            for (const auto& part : parts) list.push_back(parseScalar(part));
            assignValue(root, currentSection, key, list);
            continue;
        }

        if (value == "|") {
            if (error) *error = "Multiline YAML values are not supported";
            continue;
        }

        if (value == "-") {
            currentListKey = key;
            currentList = nlohmann::json::array();
            continue;
        }

        flushList();
        assignValue(root, currentSection, key, parseScalar(value));
    }

    flushList();
    return root;
}

nlohmann::json DigestionConfigManager::parseScalar(const std::string& value) {
    std::string trimmed = trimCopy(value);
    if (compareIgnoreCase(trimmed, "true") == 0) return true;
    if (compareIgnoreCase(trimmed, "false") == 0) return false;

    try {
        size_t pos = 0;
        int v = std::stoi(trimmed, &pos);
        if (pos == trimmed.size()) return v;
    } catch (...) {}
    try {
        size_t pos = 0;
        double v = std::stod(trimmed, &pos);
        if (pos == trimmed.size()) return v;
    } catch (...) {}

    std::string cleaned = trimmed;
    if (cleaned.size() >= 2 && ((cleaned.front() == '"' && cleaned.back() == '"') || (cleaned.front() == '\'' && cleaned.back() == '\'')))
        cleaned = cleaned.substr(1, cleaned.size() - 2);
    return cleaned;
}

void DigestionConfigManager::assignValue(nlohmann::json& root, const std::string& section, const std::string& key, const nlohmann::json& value) {
    if (section.empty()) {
        root[key] = value;
        return;
    }
    if (!root.contains(section)) root[section] = nlohmann::json::object();
    nlohmann::json& sectionObj = root[section];
    if (!sectionObj.is_object()) sectionObj = nlohmann::json::object();
    sectionObj[key] = value;
}

std::vector<std::string> DigestionConfigManager::parseInlineList(const std::string& value) {
    if (value.size() < 2) return {};
    std::string inner = trimCopy(value.substr(1, value.size() - 2));
    std::vector<std::string> parts = split(inner, ',', true);
    std::vector<std::string> out;
    for (auto& p : parts) out.push_back(trimCopy(p));
    return out;
}
