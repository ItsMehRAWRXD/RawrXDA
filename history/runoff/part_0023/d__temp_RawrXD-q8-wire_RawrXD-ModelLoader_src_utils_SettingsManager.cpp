/**
 * SettingsManager implementation - Cross-platform preferences management
 */

#include "SettingsManager.h"
#include "PathResolver.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// Static instance
static SettingsManager* g_instance = nullptr;
static std::mutex g_instanceMutex;

SettingsManager& SettingsManager::getInstance()
{
    if (!g_instance) {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        if (!g_instance) {
            g_instance = new SettingsManager();
        }
    }
    return *g_instance;
}

SettingsManager::SettingsManager()
    : m_initialized(false), m_dirty(false)
{
}

SettingsManager::~SettingsManager()
{
    if (m_dirty) {
        save();
    }
}

void SettingsManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return;
    }

    // Ensure settings directory exists
    PathResolver::ensurePathExists(PathResolver::getConfigPath().toStdString());

    // Load existing settings
    load();

    m_initialized = true;
}

std::string SettingsManager::getSettingsFilePath()
{
    return PathResolver::getConfigPath().toStdString() + "\\ide_settings.ini";
}

int SettingsManager::getInt(const std::string& key, int defaultValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        try {
            return std::get<int>(it->second);
        } catch (...) {
            // Type mismatch, try to convert from string
            try {
                return std::stoi(std::get<std::string>(it->second));
            } catch (...) {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

double SettingsManager::getDouble(const std::string& key, double defaultValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        try {
            return std::get<double>(it->second);
        } catch (...) {
            try {
                return std::stod(std::get<std::string>(it->second));
            } catch (...) {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

bool SettingsManager::getBool(const std::string& key, bool defaultValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        try {
            return std::get<bool>(it->second);
        } catch (...) {
            try {
                std::string val = std::get<std::string>(it->second);
                return val == "1" || val == "true" || val == "True" || val == "TRUE";
            } catch (...) {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

std::string SettingsManager::getString(const std::string& key, const std::string& defaultValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        try {
            return std::get<std::string>(it->second);
        } catch (...) {
            // Try to convert to string
            try {
                return variantToString(it->second);
            } catch (...) {
                return defaultValue;
            }
        }
    }
    return defaultValue;
}

void SettingsManager::setInt(const std::string& key, int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
    m_dirty = true;
}

void SettingsManager::setDouble(const std::string& key, double value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
    m_dirty = true;
}

void SettingsManager::setBool(const std::string& key, bool value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
    m_dirty = true;
}

void SettingsManager::setString(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
    m_dirty = true;
}

void SettingsManager::setValue(const std::string& key, const SettingValue& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
    m_dirty = true;
}

SettingsManager::SettingValue SettingsManager::getValue(const std::string& key, const SettingValue& defaultValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        return it->second;
    }
    return defaultValue;
}

bool SettingsManager::contains(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_settings.find(key) != m_settings.end();
}

void SettingsManager::remove(const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings.erase(key);
    m_dirty = true;
}

void SettingsManager::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings.clear();
    m_dirty = true;
}

const std::map<std::string, SettingsManager::SettingValue>& SettingsManager::getAll() const
{
    return m_settings;
}

void SettingsManager::save()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = getSettingsFilePath();
    saveToINI(filePath);
    m_dirty = false;
}

void SettingsManager::load()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = getSettingsFilePath();
    loadFromINI(filePath);
}

void SettingsManager::loadFromINI(const std::string& filePath)
{
    m_settings.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;  // File doesn't exist yet, start with empty settings
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Skip sections [name]
        if (line[0] == '[') {
            continue;
        }

        // Parse key=value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            // Try to parse as appropriate type
            // Integer
            try {
                m_settings[key] = std::stoi(value);
                continue;
            } catch (...) {
            }

            // Boolean
            if (value == "true" || value == "True" || value == "TRUE" || value == "1") {
                m_settings[key] = true;
                continue;
            }
            if (value == "false" || value == "False" || value == "FALSE" || value == "0") {
                m_settings[key] = false;
                continue;
            }

            // Double
            try {
                m_settings[key] = std::stod(value);
                continue;
            } catch (...) {
            }

            // String (default)
            m_settings[key] = value;
        }
    }

    file.close();
}

void SettingsManager::saveToINI(const std::string& filePath)
{
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return;  // Failed to open file
    }

    file << "; RawrXD IDE Settings\n";
    file << "; Auto-generated - do not edit manually\n\n";

    for (const auto& pair : m_settings) {
        std::string value = variantToString(pair.second);
        file << pair.first << "=" << value << "\n";
    }

    file.close();
}

std::string SettingsManager::variantToString(const SettingValue& value)
{
    try {
        return std::to_string(std::get<int>(value));
    } catch (...) {
    }

    try {
        return std::to_string(std::get<double>(value));
    } catch (...) {
    }

    try {
        return std::get<bool>(value) ? "true" : "false";
    } catch (...) {
    }

    try {
        return std::get<std::string>(value);
    } catch (...) {
    }

    return "";
}

SettingsManager::SettingValue SettingsManager::stringToVariant(const std::string& value)
{
    // Try integer
    try {
        return std::stoi(value);
    } catch (...) {
    }

    // Try double
    try {
        return std::stod(value);
    } catch (...) {
    }

    // Try boolean
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }

    // Default to string
    return value;
}

void SettingsManager::exportToJSON(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    file << "{\n";
    size_t count = 0;
    for (const auto& pair : m_settings) {
        file << "  \"" << pair.first << "\": ";

        // Format based on type
        try {
            file << std::get<int>(pair.second);
        } catch (...) {
            try {
                file << std::get<double>(pair.second);
            } catch (...) {
                try {
                    file << (std::get<bool>(pair.second) ? "true" : "false");
                } catch (...) {
                    file << "\"" << std::get<std::string>(pair.second) << "\"";
                }
            }
        }

        if (++count < m_settings.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "}\n";

    file.close();
}

void SettingsManager::importFromJSON(const std::string& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return;
    }

    // Simple JSON parser (production code might use a proper JSON library)
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines, braces
        if (line.empty() || line.find_first_not_of(" \t{}[]") == std::string::npos) {
            continue;
        }

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            // Extract key
            size_t keyStart = line.find('"');
            size_t keyEnd = line.find('"', keyStart + 1);
            if (keyStart != std::string::npos && keyEnd != std::string::npos) {
                std::string key = line.substr(keyStart + 1, keyEnd - keyStart - 1);

                // Extract value
                std::string value = line.substr(colonPos + 1);
                value.erase(0, value.find_first_not_of(" \t\""));
                value.erase(value.find_last_not_of(" \t\",") + 1);

                m_settings[key] = stringToVariant(value);
            }
        }
    }

    file.close();
    m_dirty = true;
}

// ========== Convenience Methods ==========

void SettingsManager::saveUILayout(int outputHeight, int selectedTab, int terminalHeight, bool outputVisible)
{
    setInt("ui.outputHeight", outputHeight);
    setInt("ui.selectedTab", selectedTab);
    setInt("ui.terminalHeight", terminalHeight);
    setBool("ui.outputVisible", outputVisible);
}

void SettingsManager::loadUILayout(int& outputHeight, int& selectedTab, int& terminalHeight, bool& outputVisible)
{
    outputHeight = getInt("ui.outputHeight", 150);
    selectedTab = getInt("ui.selectedTab", 0);
    terminalHeight = getInt("ui.terminalHeight", 200);
    outputVisible = getBool("ui.outputVisible", true);
}

void SettingsManager::saveInferenceSettings(const std::string& ollamaUrl, const std::string& modelTag,
                                           bool useStreaming, const std::string& tokenLimit)
{
    setString("inference.ollamaUrl", ollamaUrl);
    setString("inference.modelTag", modelTag);
    setBool("inference.useStreaming", useStreaming);
    setString("inference.tokenLimit", tokenLimit);
}

void SettingsManager::loadInferenceSettings(std::string& ollamaUrl, std::string& modelTag,
                                           bool& useStreaming, std::string& tokenLimit)
{
    ollamaUrl = getString("inference.ollamaUrl", "http://localhost:11434");
    modelTag = getString("inference.modelTag", "");
    useStreaming = getBool("inference.useStreaming", false);
    tokenLimit = getString("inference.tokenLimit", "512");
}

void SettingsManager::saveEditorPreferences(const std::string& fontName, int fontSize,
                                           int tabWidth, bool autoFormat, bool wordWrap)
{
    setString("editor.fontName", fontName);
    setInt("editor.fontSize", fontSize);
    setInt("editor.tabWidth", tabWidth);
    setBool("editor.autoFormat", autoFormat);
    setBool("editor.wordWrap", wordWrap);
}

void SettingsManager::loadEditorPreferences(std::string& fontName, int& fontSize,
                                           int& tabWidth, bool& autoFormat, bool& wordWrap)
{
    fontName = getString("editor.fontName", "Courier New");
    fontSize = getInt("editor.fontSize", 11);
    tabWidth = getInt("editor.tabWidth", 4);
    autoFormat = getBool("editor.autoFormat", false);
    wordWrap = getBool("editor.wordWrap", true);
}

void SettingsManager::saveSeverityFilter(int filterLevel)
{
    setInt("filter.severityLevel", filterLevel);
}

int SettingsManager::loadSeverityFilter()
{
    return getInt("filter.severityLevel", 0);  // 0 = All, 1 = Info+, 2 = Warning+, 3 = Error+
}

void SettingsManager::saveLastWorkspace(const std::string& workspacePath)
{
    setString("workspace.lastPath", workspacePath);
}

std::string SettingsManager::loadLastWorkspace()
{
    return getString("workspace.lastPath", "");
}

void SettingsManager::saveRendererPreference(bool useVulkan)
{
    setBool("renderer.useVulkan", useVulkan);
}

bool SettingsManager::loadRendererPreference()
{
    return getBool("renderer.useVulkan", false);
}
