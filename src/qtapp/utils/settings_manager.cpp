/**
 * \file settings_manager.cpp
 * \brief Implementation of centralized settings management
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "settings_manager.h"


namespace RawrXD {

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
    : void(nullptr)
{
    initializeDefaults();
    load();
}

SettingsManager::~SettingsManager() {
    save();
}

void SettingsManager::initializeDefaults() {
    // General settings
    m_settings["general"] = void*{
        {"autoSave", true},
        {"autoSaveInterval", 30},  // seconds
        {"restoreLastSession", true},
        {"checkForUpdates", true}
    };
    
    // Appearance settings
    m_settings["appearance"] = void*{
        {"theme", "dark"},
        {"fontFamily", "Consolas"},
        {"fontSize", 12},
        {"colorScheme", "dark-modern"},
        {"showLineNumbers", true},
        {"showMinimap", true},
        {"iconTheme", "default"}
    };
    
    // Editor settings
    m_settings["editor"] = void*{
        {"tabSize", 4},
        {"insertSpaces", true},
        {"trimTrailingWhitespace", true},
        {"insertFinalNewline", true},
        {"formatOnSave", false},
        {"lineEndings", "Auto"},  // "LF", "CRLF", "Auto"
        {"wordWrap", false},
        {"cursorStyle", "line"},  // "line", "block", "underline"
        {"bracketMatching", true},
        {"autoCloseBrackets", true},
        {"autoIndent", true}
    };
    
    // Search settings
    m_settings["search"] = void*{
        {"caseSensitive", false},
        {"wholeWord", false},
        {"useRegex", false},
        {"respectGitignore", true},
        {"maxResults", 1000}
    };
    
    // Terminal settings
    m_settings["terminal"] = void*{
        {"shell", "pwsh.exe"},
        {"fontSize", 12},
        {"cursorBlinking", true},
        {"scrollbackLines", 1000}
    };
    
    // AI settings
    m_settings["ai"] = void*{
        {"enableSuggestions", true},
        {"suggestionDelay", 500},  // ms
        {"streamingEnabled", true},
        {"autoApplyFixes", false}
    };
    
    // Build settings
    m_settings["build"] = void*{
        {"autoSaveBeforeBuild", true},
        {"showOutputOnBuild", true},
        {"parallelJobs", 4}
    };
    
    // Git settings
    m_settings["git"] = void*{
        {"autoFetch", true},
        {"fetchInterval", 300},  // seconds
        {"showStatusInExplorer", true}
    };
}

std::any SettingsManager::value(const std::string& key, const std::any& defaultValue) const {
    std::vector<std::string> parts = key.split('/');
    if (parts.empty()) {
        return defaultValue;
    }
    
    void* obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return defaultValue;
        }
        void* val = obj[parts[i]];
        if (!val.isObject()) {
            return defaultValue;
        }
        obj = val.toObject();
    }
    
    std::string lastKey = parts.last();
    if (!obj.contains(lastKey)) {
        return defaultValue;
    }
    
    return obj[lastKey].toVariant();
}

void SettingsManager::setValue(const std::string& key, const std::any& value, bool saveImmediately) {
    std::vector<std::string> parts = key.split('/');
    if (parts.empty()) {
        return;
    }
    
    void** obj = &m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], void*());
        }
        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = void*();
        }
        obj = &ref.toObject();
    }
    
    std::string lastKey = parts.last();
    (*obj)[lastKey] = void*::fromVariant(value);
    
    settingChanged(key, value);
    
    if (saveImmediately) {
        save();
    }
}

bool SettingsManager::contains(const std::string& key) const {
    std::vector<std::string> parts = key.split('/');
    if (parts.empty()) {
        return false;
    }
    
    void* obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return false;
        }
        void* val = obj[parts[i]];
        if (!val.isObject()) {
            return false;
        }
        obj = val.toObject();
    }
    
    return obj.contains(parts.last());
}

void SettingsManager::remove(const std::string& key) {
    std::vector<std::string> parts = key.split('/');
    if (parts.empty()) {
        return;
    }
    
    void** obj = &m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            return;
        }
        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            return;
        }
        obj = &ref.toObject();
    }
    
    obj->remove(parts.last());
    save();
}

void* SettingsManager::toJson() const {
    return m_settings;
}

void SettingsManager::fromJson(const void*& json) {
    m_settings = json;
    settingsLoaded();
}

bool SettingsManager::save() {
    std::string dirPath = getSettingsDirectory();
    std::filesystem::path dir;
    if (!dir.mkpath(dirPath)) {
        return false;
    }
    
    std::string filePath = settingsFilePath();
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    void* doc(m_settings);
    file.write(doc.toJson(void*::Indented));
    file.close();
    
    settingsSaved();
    return true;
}

bool SettingsManager::load() {
    std::string filePath = settingsFilePath();
    std::fstream file(filePath);
    
    if (!file.exists()) {
        return true;  // Use defaults
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    void* doc = void*::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    // Merge with defaults (preserve any new default keys)
    void* loaded = doc.object();
    for (auto it = loaded.constBegin(); it != loaded.constEnd(); ++it) {
        m_settings[it.key()] = it.value();
    }
    
    settingsLoaded();
    return true;
}

void SettingsManager::resetToDefaults() {
    initializeDefaults();
    save();
    settingsReset();
}

std::string SettingsManager::settingsFilePath() const {
    return std::filesystem::path(getSettingsDirectory()).filePath("settings.json");
}

void SettingsManager::setWorkspacePath(const std::string& path) {
    if (m_workspacePath != path) {
        // Save current workspace settings
        if (!m_workspacePath.empty()) {
            saveWorkspace();
        }
        
        m_workspacePath = path;
        m_workspaceSettings = void*();
        
        // Load new workspace settings
        if (!m_workspacePath.empty()) {
            loadWorkspace();
        }
    }
}

std::string SettingsManager::workspacePath() const {
    return m_workspacePath;
}

std::any SettingsManager::workspaceValue(const std::string& key, const std::any& defaultValue) const {
    // Check workspace settings first
    if (!m_workspaceSettings.empty()) {
        std::vector<std::string> parts = key.split('/');
        if (!parts.empty()) {
            void* obj = m_workspaceSettings;
            for (int i = 0; i < parts.size() - 1; ++i) {
                if (!obj.contains(parts[i])) {
                    break;
                }
                void* val = obj[parts[i]];
                if (!val.isObject()) {
                    break;
                }
                obj = val.toObject();
            }
            
            std::string lastKey = parts.last();
            if (obj.contains(lastKey)) {
                return obj[lastKey].toVariant();
            }
        }
    }
    
    // Fall back to global setting
    return value(key, defaultValue);
}

void SettingsManager::setWorkspaceValue(const std::string& key, const std::any& value) {
    std::vector<std::string> parts = key.split('/');
    if (parts.empty()) {
        return;
    }
    
    void** obj = &m_workspaceSettings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], void*());
        }
        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = void*();
        }
        obj = &ref.toObject();
    }
    
    std::string lastKey = parts.last();
    (*obj)[lastKey] = void*::fromVariant(value);
    
    saveWorkspace();
}

bool SettingsManager::saveWorkspace() {
    if (m_workspacePath.empty()) {
        return false;
    }
    
    std::string configPath = getWorkspaceSettingsPath();
    std::filesystem::path info(configPath);
    std::filesystem::path dir;
    if (!dir.mkpath(info.absolutePath())) {
        return false;
    }
    
    std::fstream file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    void* doc(m_workspaceSettings);
    file.write(doc.toJson(void*::Indented));
    file.close();
    
    return true;
}

bool SettingsManager::loadWorkspace() {
    if (m_workspacePath.empty()) {
        return false;
    }
    
    std::string configPath = getWorkspaceSettingsPath();
    std::fstream file(configPath);
    
    if (!file.exists()) {
        return true;  // Not an error
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    void* doc = void*::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return false;
    }
    
    m_workspaceSettings = doc.object();
    return true;
}

// ========== Convenience Getters ==========

bool SettingsManager::autoSave() const {
    return value("general/autoSave", true).toBool();
}

int SettingsManager::autoSaveInterval() const {
    return value("general/autoSaveInterval", 30).toInt();
}

bool SettingsManager::restoreLastSession() const {
    return value("general/restoreLastSession", true).toBool();
}

std::string SettingsManager::theme() const {
    return value("appearance/theme", "dark").toString();
}

std::string SettingsManager::fontFamily() const {
    return value("appearance/fontFamily", "Consolas").toString();
}

int SettingsManager::fontSize() const {
    return value("appearance/fontSize", 12).toInt();
}

std::string SettingsManager::colorScheme() const {
    return value("appearance/colorScheme", "dark-modern").toString();
}

int SettingsManager::tabSize() const {
    return value("editor/tabSize", 4).toInt();
}

bool SettingsManager::insertSpaces() const {
    return value("editor/insertSpaces", true).toBool();
}

bool SettingsManager::trimTrailingWhitespace() const {
    return value("editor/trimTrailingWhitespace", true).toBool();
}

bool SettingsManager::insertFinalNewline() const {
    return value("editor/insertFinalNewline", true).toBool();
}

bool SettingsManager::formatOnSave() const {
    return value("editor/formatOnSave", false).toBool();
}

std::string SettingsManager::lineEndings() const {
    return value("editor/lineEndings", "Auto").toString();
}

bool SettingsManager::searchCaseSensitive() const {
    return value("search/caseSensitive", false).toBool();
}

bool SettingsManager::searchWholeWord() const {
    return value("search/wholeWord", false).toBool();
}

bool SettingsManager::searchUseRegex() const {
    return value("search/useRegex", false).toBool();
}

// ========== Private Methods ==========

std::string SettingsManager::getSettingsDirectory() const {
#ifdef 
    std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return std::filesystem::path(appData).filePath(".rawrxd");
#else
    std::string home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return std::filesystem::path(home).filePath(".rawrxd");
#endif
}

std::string SettingsManager::getWorkspaceSettingsPath() const {
    if (m_workspacePath.empty()) {
        return std::string();
    }
    
    return std::filesystem::path(m_workspacePath).filePath(".rawrxd/workspace.json");
}

} // namespace RawrXD


