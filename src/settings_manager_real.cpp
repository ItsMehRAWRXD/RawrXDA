#include "settings_manager_real.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <mutex>

#include "logging/logger.h"
static Logger s_logger("settings_manager_real");

SettingsManager::SettingsManager(const std::string& configPath)
    : m_configPath(configPath) {
    loadDefaults();
}

SettingsManager::~SettingsManager() {
    if (m_dirty && m_autoSave) {
        save();
    }
}

bool SettingsManager::load() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return readFromDisk();
}

bool SettingsManager::save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeToDisk();
}

bool SettingsManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings = m_defaults;
    m_dirty = true;
    return writeToDisk();
}

bool SettingsManager::set(const std::string& key, const SettingValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!validateSetting(key, value)) {
        return false;
    }

    m_settings[key] = value;
    m_dirty = true;

    if (m_changeCallback) {
        m_changeCallback(key, value);
    }

    if (m_autoSave) {
        writeToDisk();
    }

    return true;
}

SettingsManager::SettingValue SettingsManager::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        return it.value();
    }

    auto defIt = m_defaults.find(key);
    if (defIt != m_defaults.end()) {
        return defIt.value();
    }

    return json();
}

SettingsManager::SettingValue SettingsManager::get(const std::string& key, const SettingValue& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        return it.value();
    }

    return defaultValue;
}

bool SettingsManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.contains(key) || m_defaults.contains(key);
}

bool SettingsManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains(key)) {
        m_settings.erase(key);
        m_dirty = true;
        
        if (m_autoSave) {
            writeToDisk();
        }
        return true;
    }

    return false;
}

void SettingsManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings = m_defaults;
    m_dirty = true;
}

bool SettingsManager::setTheme(const Theme& theme) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto themeJson = serializeTheme(theme);
    m_settings["themes"][theme.name] = themeJson;
    m_settings["currentTheme"] = theme.name;
    m_dirty = true;

    if (m_autoSave) {
        writeToDisk();
    }

    return true;
}

SettingsManager::Theme SettingsManager::getTheme(const std::string& themeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains("themes") && m_settings["themes"].contains(themeName)) {
        return deserializeTheme(m_settings["themes"][themeName]);
    }

    // Return default theme
    Theme defaultTheme;
    defaultTheme.name = "default";
    defaultTheme.darkMode = false;
    defaultTheme.fontSize = 12;
    defaultTheme.fontName = "Consolas";
    return defaultTheme;
}

std::vector<std::string> SettingsManager::listThemes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> themes;
    if (m_settings.contains("themes") && m_settings["themes"].is_object()) {
        for (auto& [name, _] : m_settings["themes"].items()) {
            themes.push_back(name);
        }
    }

    return themes;
}

std::string SettingsManager::getCurrentTheme() const {
    return get("currentTheme", "default").get<std::string>();
}

bool SettingsManager::switchTheme(const std::string& themeName) {
    return set("currentTheme", themeName);
}

void SettingsManager::createTheme(const std::string& baseName, const Theme& theme) {
    setTheme(theme);
}

bool SettingsManager::setKeybinding(const Keybinding& binding) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto kbJson = serializeKeybinding(binding);
    m_settings["keybindings"][binding.command] = kbJson;
    m_dirty = true;

    if (m_autoSave) {
        writeToDisk();
    }

    return true;
}

SettingsManager::Keybinding SettingsManager::getKeybinding(const std::string& command) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains("keybindings") && m_settings["keybindings"].contains(command)) {
        return deserializeKeybinding(m_settings["keybindings"][command]);
    }

    return Keybinding{command, "", "", true};
}

std::vector<SettingsManager::Keybinding> SettingsManager::listKeybindings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Keybinding> bindings;
    if (m_settings.contains("keybindings") && m_settings["keybindings"].is_object()) {
        for (auto& [cmd, data] : m_settings["keybindings"].items()) {
            bindings.push_back(deserializeKeybinding(data));
        }
    }

    return bindings;
}

std::vector<SettingsManager::Keybinding> SettingsManager::getKeybindingsByKeys(const std::string& keys) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<Keybinding> results;
    auto allBindings = listKeybindings();
    
    for (const auto& binding : allBindings) {
        if (binding.keys == keys) {
            results.push_back(binding);
        }
    }

    return results;
}

bool SettingsManager::removeKeybinding(const std::string& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains("keybindings") && m_settings["keybindings"].contains(command)) {
        m_settings["keybindings"].erase(command);
        m_dirty = true;

        if (m_autoSave) {
            writeToDisk();
        }

        return true;
    }

    return false;
}

void SettingsManager::resetKeybindingsToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_defaults.contains("keybindings")) {
        m_settings["keybindings"] = m_defaults["keybindings"];
        m_dirty = true;

        if (m_autoSave) {
            writeToDisk();
        }
    }
}

bool SettingsManager::addModel(const ModelConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto modelJson = serializeModel(config);
    m_settings["models"][config.name] = modelJson;
    m_dirty = true;

    if (m_autoSave) {
        writeToDisk();
    }

    return true;
}

SettingsManager::ModelConfig SettingsManager::getModel(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains("models") && m_settings["models"].contains(modelName)) {
        return deserializeModel(m_settings["models"][modelName]);
    }

    return ModelConfig{modelName, "", "", 2048, 0, false};
}

std::vector<SettingsManager::ModelConfig> SettingsManager::listModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<ModelConfig> models;
    if (m_settings.contains("models") && m_settings["models"].is_object()) {
        for (auto& [name, data] : m_settings["models"].items()) {
            models.push_back(deserializeModel(data));
        }
    }

    return models;
}

bool SettingsManager::removeModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_settings.contains("models") && m_settings["models"].contains(modelName)) {
        m_settings["models"].erase(modelName);
        m_dirty = true;

        if (m_autoSave) {
            writeToDisk();
        }

        return true;
    }

    return false;
}

std::string SettingsManager::getDefaultModel() const {
    return get("defaultModel", "default").get<std::string>();
}

bool SettingsManager::setDefaultModel(const std::string& modelName) {
    return set("defaultModel", modelName);
}

SettingsManager::EditorSettings SettingsManager::getEditorSettings() const {
    auto settings = get("editor");
    
    EditorSettings result;
    result.tabSize = settings.value("tabSize", 4);
    result.useSpaces = settings.value("useSpaces", true);
    result.autoFormat = settings.value("autoFormat", true);
    result.wordWrap = settings.value("wordWrap", false);
    result.minimap = settings.value("minimap", true);
    result.lineNumbers = settings.value("lineNumbers", true);
    result.fontFamily = settings.value("fontFamily", "Consolas");
    result.fontSize = settings.value("fontSize", 12);
    result.autoSave = settings.value("autoSave", true);
    result.autoSaveDelay = settings.value("autoSaveDelay", 5000);

    return result;
}

void SettingsManager::setEditorSettings(const EditorSettings& settings) {
    json editorJson;
    editorJson["tabSize"] = settings.tabSize;
    editorJson["useSpaces"] = settings.useSpaces;
    editorJson["autoFormat"] = settings.autoFormat;
    editorJson["wordWrap"] = settings.wordWrap;
    editorJson["minimap"] = settings.minimap;
    editorJson["lineNumbers"] = settings.lineNumbers;
    editorJson["fontFamily"] = settings.fontFamily;
    editorJson["fontSize"] = settings.fontSize;
    editorJson["autoSave"] = settings.autoSave;
    editorJson["autoSaveDelay"] = settings.autoSaveDelay;

    set("editor", editorJson);
}

SettingsManager::TerminalSettings SettingsManager::getTerminalSettings() const {
    auto settings = get("terminal");
    
    TerminalSettings result;
    result.defaultShell = settings.value("defaultShell", "powershell");
    result.inheritEnv = settings.value("inheritEnv", true);
    result.bufferSize = settings.value("bufferSize", 10000);
    result.enableLogging = settings.value("enableLogging", false);
    result.logPath = settings.value("logPath", "");

    return result;
}

void SettingsManager::setTerminalSettings(const TerminalSettings& settings) {
    json termJson;
    termJson["defaultShell"] = settings.defaultShell;
    termJson["inheritEnv"] = settings.inheritEnv;
    termJson["bufferSize"] = settings.bufferSize;
    termJson["enableLogging"] = settings.enableLogging;
    termJson["logPath"] = settings.logPath;

    set("terminal", termJson);
}

SettingsManager::AISettings SettingsManager::getAISettings() const {
    auto settings = get("ai");
    
    AISettings result;
    result.defaultModel = settings.value("defaultModel", "local");
    result.temperature = settings.value("temperature", 0.7);
    result.topP = settings.value("topP", 0.95);
    result.maxTokens = settings.value("maxTokens", 512);
    result.contextWindow = settings.value("contextWindow", 4096);
    result.enableCompletion = settings.value("enableCompletion", true);
    result.completionDelay = settings.value("completionDelay", 300);
    result.confidenceThreshold = settings.value("confidenceThreshold", 0.4);
    result.enableDeepThinking = settings.value("enableDeepThinking", true);
    result.enableAutoCorrection = settings.value("enableAutoCorrection", true);

    return result;
}

void SettingsManager::setAISettings(const AISettings& settings) {
    json aiJson;
    aiJson["defaultModel"] = settings.defaultModel;
    aiJson["temperature"] = settings.temperature;
    aiJson["topP"] = settings.topP;
    aiJson["maxTokens"] = settings.maxTokens;
    aiJson["contextWindow"] = settings.contextWindow;
    aiJson["enableCompletion"] = settings.enableCompletion;
    aiJson["completionDelay"] = settings.completionDelay;
    aiJson["confidenceThreshold"] = settings.confidenceThreshold;
    aiJson["enableDeepThinking"] = settings.enableDeepThinking;
    aiJson["enableAutoCorrection"] = settings.enableAutoCorrection;

    set("ai", aiJson);
}

SettingsManager::CloudSettings SettingsManager::getCloudSettings() const {
    auto settings = get("cloud");
    
    CloudSettings result;
    result.enableCloudModels = settings.value("enableCloudModels", false);
    result.openaiApiKey = settings.value("openaiApiKey", "");
    result.azureApiKey = settings.value("azureApiKey", "");
    result.azureEndpoint = settings.value("azureEndpoint", "");
    result.anthropicApiKey = settings.value("anthropicApiKey", "");
    result.preferLocal = settings.value("preferLocal", true);

    return result;
}

void SettingsManager::setCloudSettings(const CloudSettings& settings) {
    json cloudJson;
    cloudJson["enableCloudModels"] = settings.enableCloudModels;
    cloudJson["openaiApiKey"] = settings.openaiApiKey;
    cloudJson["azureApiKey"] = settings.azureApiKey;
    cloudJson["azureEndpoint"] = settings.azureEndpoint;
    cloudJson["anthropicApiKey"] = settings.anthropicApiKey;
    cloudJson["preferLocal"] = settings.preferLocal;

    set("cloud", cloudJson);
}

std::string SettingsManager::getWorkspaceDirectory() const {
    return get("workspace.directory", ".").get<std::string>();
}

void SettingsManager::setWorkspaceDirectory(const std::string& path) {
    set("workspace.directory", path);
}

std::vector<std::string> SettingsManager::getRecentProjects(int maxCount) const {
    std::vector<std::string> recent;
    auto projects = get("workspace.recentProjects");
    
    if (projects.is_array()) {
        for (size_t i = 0; i < projects.size() && static_cast<int>(i) < maxCount; ++i) {
            recent.push_back(projects[i].get<std::string>());
        }
    }

    return recent;
}

void SettingsManager::addRecentProject(const std::string& path) {
    auto projects = get("workspace.recentProjects");
    
    if (!projects.is_array()) {
        projects = json::array();
    }

    // Remove duplicates
    auto it = std::find_if(projects.begin(), projects.end(),
        [&path](const json& j) { return j.get<std::string>() == path; }
    );
    if (it != projects.end()) {
        projects.erase(it);
    }

    // Add to front
    projects.insert(projects.begin(), path);

    // Keep only 10 recent
    if (projects.size() > 10) {
        projects.erase(projects.begin() + 10, projects.end());
    }

    set("workspace.recentProjects", projects);
}

bool SettingsManager::validateSetting(const std::string& key, const SettingValue& value) {
    m_validationErrors.clear();
    
    // Validate known settings with type and range constraints
    if (key == "editor.fontSize") {
        if (auto* v = std::get_if<int>(&value)) {
            if (*v < 6 || *v > 72) {
                m_validationErrors.push_back("Font size must be between 6 and 72");
                return false;
            }
        } else {
            m_validationErrors.push_back("Font size must be an integer");
            return false;
        }
    } else if (key == "editor.tabSize") {
        if (auto* v = std::get_if<int>(&value)) {
            if (*v < 1 || *v > 16) {
                m_validationErrors.push_back("Tab size must be between 1 and 16");
                return false;
            }
        }
    } else if (key == "inference.temperature") {
        if (auto* v = std::get_if<double>(&value)) {
            if (*v < 0.0 || *v > 2.0) {
                m_validationErrors.push_back("Temperature must be between 0.0 and 2.0");
                return false;
            }
        }
    } else if (key == "inference.maxTokens") {
        if (auto* v = std::get_if<int>(&value)) {
            if (*v < 1 || *v > 32768) {
                m_validationErrors.push_back("Max tokens must be between 1 and 32768");
                return false;
            }
        }
    }
    
    return true;
}

std::vector<std::string> SettingsManager::getValidationErrors() const {
    return m_validationErrors;
}

bool SettingsManager::exportToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        file << m_settings.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsManager::importFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        file >> m_settings;
        m_dirty = true;
        return true;
    } catch (...) {
        return false;
    }
}

void SettingsManager::printAllSettings() const {
    s_logger.info( m_settings.dump(2) << std::endl;
}

void SettingsManager::loadDefaults() {
    m_defaults = {
        {"editor", {
            {"tabSize", 4},
            {"useSpaces", true},
            {"autoFormat", true},
            {"wordWrap", false},
            {"minimap", true},
            {"lineNumbers", true},
            {"fontFamily", "Consolas"},
            {"fontSize", 12},
            {"autoSave", true},
            {"autoSaveDelay", 5000}
        }},
        {"terminal", {
            {"defaultShell", "powershell"},
            {"inheritEnv", true},
            {"bufferSize", 10000},
            {"enableLogging", false},
            {"logPath", ""}
        }},
        {"ai", {
            {"defaultModel", "local"},
            {"temperature", 0.7},
            {"topP", 0.95},
            {"maxTokens", 512},
            {"contextWindow", 4096},
            {"enableCompletion", true},
            {"completionDelay", 300},
            {"confidenceThreshold", 0.4},
            {"enableDeepThinking", true},
            {"enableAutoCorrection", true}
        }},
        {"cloud", {
            {"enableCloudModels", false},
            {"openaiApiKey", ""},
            {"azureApiKey", ""},
            {"azureEndpoint", ""},
            {"anthropicApiKey", ""},
            {"preferLocal", true}
        }},
        {"currentTheme", "default"},
        {"defaultModel", "default"},
        {"workspace", {
            {"directory", "."},
            {"recentProjects", json::array()}
        }},
        {"themes", {}},
        {"keybindings", {}},
        {"models", {}}
    };

    m_settings = m_defaults;
}

void SettingsManager::mergeSettings(const json& defaults, json& current) {
    for (auto& [key, value] : defaults.items()) {
        if (!current.contains(key)) {
            current[key] = value;
        } else if (value.is_object() && current[key].is_object()) {
            mergeSettings(value, current[key]);
        }
    }
}

json SettingsManager::serializeTheme(const Theme& theme) const {
    json j;
    j["name"] = theme.name;
    j["fontName"] = theme.fontName;
    j["fontSize"] = theme.fontSize;
    j["darkMode"] = theme.darkMode;
    
    for (const auto& [key, color] : theme.colors) {
        j["colors"][key] = color;
    }

    return j;
}

SettingsManager::Theme SettingsManager::deserializeTheme(const json& data) const {
    Theme theme;
    theme.name = data.value("name", "");
    theme.fontName = data.value("fontName", "Consolas");
    theme.fontSize = data.value("fontSize", 12);
    theme.darkMode = data.value("darkMode", false);
    
    if (data.contains("colors") && data["colors"].is_object()) {
        for (auto& [key, color] : data["colors"].items()) {
            theme.colors[key] = color.get<std::string>();
        }
    }

    return theme;
}

json SettingsManager::serializeKeybinding(const Keybinding& keybinding) const {
    json j;
    j["command"] = keybinding.command;
    j["keys"] = keybinding.keys;
    j["description"] = keybinding.description;
    j["enabled"] = keybinding.enabled;
    return j;
}

SettingsManager::Keybinding SettingsManager::deserializeKeybinding(const json& data) const {
    Keybinding keybinding;
    keybinding.command = data.value("command", "");
    keybinding.keys = data.value("keys", "");
    keybinding.description = data.value("description", "");
    keybinding.enabled = data.value("enabled", true);
    return keybinding;
}

json SettingsManager::serializeModel(const ModelConfig& model) const {
    json j;
    j["name"] = model.name;
    j["path"] = model.path;
    j["type"] = model.type;
    j["contextWindow"] = model.contextWindow;
    j["gpuLayers"] = model.gpuLayers;
    j["loaded"] = model.loaded;
    return j;
}

SettingsManager::ModelConfig SettingsManager::deserializeModel(const json& data) const {
    ModelConfig model;
    model.name = data.value("name", "");
    model.path = data.value("path", "");
    model.type = data.value("type", "GGUF");
    model.contextWindow = data.value("contextWindow", 2048);
    model.gpuLayers = data.value("gpuLayers", 0);
    model.loaded = data.value("loaded", false);
    return model;
}

bool SettingsManager::readFromDisk() {
    try {
        if (std::filesystem::exists(m_configPath)) {
            std::ifstream file(m_configPath);
            json loaded;
            file >> loaded;
            mergeSettings(m_defaults, loaded);
            m_settings = loaded;
            return true;
        }
    } catch (const std::exception& e) {
        s_logger.error( "[SettingsManager] Error reading config: " << e.what() << std::endl;
    }

    return false;
}

bool SettingsManager::writeToDisk() {
    try {
        // Ensure directory exists
        auto path = std::filesystem::path(m_configPath);
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(m_configPath);
        file << m_settings.dump(2);
        m_dirty = false;
        return true;
    } catch (const std::exception& e) {
        s_logger.error( "[SettingsManager] Error writing config: " << e.what() << std::endl;
        return false;
    }
}
