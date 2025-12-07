/**
 * \file settings_manager.cpp
 * \brief Implementation of centralized settings management
 * \author RawrXD Team
 * \date 2025-12-05
 */

#include "settings_manager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace RawrXD {

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager()
    : QObject(nullptr)
{
    initializeDefaults();
    load();
}

SettingsManager::~SettingsManager() {
    save();
}

void SettingsManager::initializeDefaults() {
    // General settings
    m_settings["general"] = QJsonObject{
        {"autoSave", true},
        {"autoSaveInterval", 30},  // seconds
        {"restoreLastSession", true},
        {"checkForUpdates", true}
    };
    
    // Appearance settings
    m_settings["appearance"] = QJsonObject{
        {"theme", "dark"},
        {"fontFamily", "Consolas"},
        {"fontSize", 12},
        {"colorScheme", "dark-modern"},
        {"showLineNumbers", true},
        {"showMinimap", true},
        {"iconTheme", "default"}
    };
    
    // Editor settings
    m_settings["editor"] = QJsonObject{
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
    m_settings["search"] = QJsonObject{
        {"caseSensitive", false},
        {"wholeWord", false},
        {"useRegex", false},
        {"respectGitignore", true},
        {"maxResults", 1000}
    };
    
    // Terminal settings
    m_settings["terminal"] = QJsonObject{
        {"shell", "pwsh.exe"},
        {"fontSize", 12},
        {"cursorBlinking", true},
        {"scrollbackLines", 1000}
    };
    
    // AI settings
    m_settings["ai"] = QJsonObject{
        {"enableSuggestions", true},
        {"suggestionDelay", 500},  // ms
        {"streamingEnabled", true},
        {"autoApplyFixes", false}
    };
    
    // Build settings
    m_settings["build"] = QJsonObject{
        {"autoSaveBeforeBuild", true},
        {"showOutputOnBuild", true},
        {"parallelJobs", 4}
    };
    
    // Git settings
    m_settings["git"] = QJsonObject{
        {"autoFetch", true},
        {"fetchInterval", 300},  // seconds
        {"showStatusInExplorer", true}
    };
}

QVariant SettingsManager::value(const QString& key, const QVariant& defaultValue) const {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return defaultValue;
    }
    
    QJsonObject obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return defaultValue;
        }
        QJsonValue val = obj[parts[i]];
        if (!val.isObject()) {
            return defaultValue;
        }
        obj = val.toObject();
    }
    
    QString lastKey = parts.last();
    if (!obj.contains(lastKey)) {
        return defaultValue;
    }
    
    return obj[lastKey].toVariant();
}

void SettingsManager::setValue(const QString& key, const QVariant& value, bool saveImmediately) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    }
    
    QJsonObject* obj = &m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], QJsonObject());
        }
        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = QJsonObject();
        }
        obj = &ref.toObject();
    }
    
    QString lastKey = parts.last();
    (*obj)[lastKey] = QJsonValue::fromVariant(value);
    
    emit settingChanged(key, value);
    
    if (saveImmediately) {
        save();
    }
}

bool SettingsManager::contains(const QString& key) const {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return false;
    }
    
    QJsonObject obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return false;
        }
        QJsonValue val = obj[parts[i]];
        if (!val.isObject()) {
            return false;
        }
        obj = val.toObject();
    }
    
    return obj.contains(parts.last());
}

void SettingsManager::remove(const QString& key) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    }
    
    QJsonObject* obj = &m_settings;
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

QJsonObject SettingsManager::toJson() const {
    return m_settings;
}

void SettingsManager::fromJson(const QJsonObject& json) {
    m_settings = json;
    emit settingsLoaded();
}

bool SettingsManager::save() {
    QString dirPath = getSettingsDirectory();
    QDir dir;
    if (!dir.mkpath(dirPath)) {
        qWarning() << "Failed to create settings directory:" << dirPath;
        return false;
    }
    
    QString filePath = settingsFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open settings file for writing:" << filePath;
        return false;
    }
    
    QJsonDocument doc(m_settings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    emit settingsSaved();
    qDebug() << "Settings saved to:" << filePath;
    return true;
}

bool SettingsManager::load() {
    QString filePath = settingsFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        qDebug() << "Settings file does not exist, using defaults:" << filePath;
        return true;  // Use defaults
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open settings file for reading:" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        qWarning() << "Invalid settings JSON";
        return false;
    }
    
    // Merge with defaults (preserve any new default keys)
    QJsonObject loaded = doc.object();
    for (auto it = loaded.constBegin(); it != loaded.constEnd(); ++it) {
        m_settings[it.key()] = it.value();
    }
    
    emit settingsLoaded();
    qDebug() << "Settings loaded from:" << filePath;
    return true;
}

void SettingsManager::resetToDefaults() {
    initializeDefaults();
    save();
    emit settingsReset();
    qDebug() << "Settings reset to defaults";
}

QString SettingsManager::settingsFilePath() const {
    return QDir(getSettingsDirectory()).filePath("settings.json");
}

void SettingsManager::setWorkspacePath(const QString& path) {
    if (m_workspacePath != path) {
        // Save current workspace settings
        if (!m_workspacePath.isEmpty()) {
            saveWorkspace();
        }
        
        m_workspacePath = path;
        m_workspaceSettings = QJsonObject();
        
        // Load new workspace settings
        if (!m_workspacePath.isEmpty()) {
            loadWorkspace();
        }
    }
}

QString SettingsManager::workspacePath() const {
    return m_workspacePath;
}

QVariant SettingsManager::workspaceValue(const QString& key, const QVariant& defaultValue) const {
    // Check workspace settings first
    if (!m_workspaceSettings.isEmpty()) {
        QStringList parts = key.split('/');
        if (!parts.isEmpty()) {
            QJsonObject obj = m_workspaceSettings;
            for (int i = 0; i < parts.size() - 1; ++i) {
                if (!obj.contains(parts[i])) {
                    break;
                }
                QJsonValue val = obj[parts[i]];
                if (!val.isObject()) {
                    break;
                }
                obj = val.toObject();
            }
            
            QString lastKey = parts.last();
            if (obj.contains(lastKey)) {
                return obj[lastKey].toVariant();
            }
        }
    }
    
    // Fall back to global setting
    return value(key, defaultValue);
}

void SettingsManager::setWorkspaceValue(const QString& key, const QVariant& value) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    }
    
    QJsonObject* obj = &m_workspaceSettings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], QJsonObject());
        }
        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = QJsonObject();
        }
        obj = &ref.toObject();
    }
    
    QString lastKey = parts.last();
    (*obj)[lastKey] = QJsonValue::fromVariant(value);
    
    saveWorkspace();
}

bool SettingsManager::saveWorkspace() {
    if (m_workspacePath.isEmpty()) {
        return false;
    }
    
    QString configPath = getWorkspaceSettingsPath();
    QFileInfo info(configPath);
    QDir dir;
    if (!dir.mkpath(info.absolutePath())) {
        qWarning() << "Failed to create workspace config directory:" << info.absolutePath();
        return false;
    }
    
    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open workspace settings for writing:" << configPath;
        return false;
    }
    
    QJsonDocument doc(m_workspaceSettings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qDebug() << "Workspace settings saved to:" << configPath;
    return true;
}

bool SettingsManager::loadWorkspace() {
    if (m_workspacePath.isEmpty()) {
        return false;
    }
    
    QString configPath = getWorkspaceSettingsPath();
    QFile file(configPath);
    
    if (!file.exists()) {
        qDebug() << "Workspace settings file does not exist:" << configPath;
        return true;  // Not an error
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open workspace settings for reading:" << configPath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        qWarning() << "Invalid workspace settings JSON";
        return false;
    }
    
    m_workspaceSettings = doc.object();
    qDebug() << "Workspace settings loaded from:" << configPath;
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

QString SettingsManager::theme() const {
    return value("appearance/theme", "dark").toString();
}

QString SettingsManager::fontFamily() const {
    return value("appearance/fontFamily", "Consolas").toString();
}

int SettingsManager::fontSize() const {
    return value("appearance/fontSize", 12).toInt();
}

QString SettingsManager::colorScheme() const {
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

QString SettingsManager::lineEndings() const {
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

QString SettingsManager::getSettingsDirectory() const {
#ifdef Q_OS_WIN
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(".rawrxd");
#else
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir(home).filePath(".rawrxd");
#endif
}

QString SettingsManager::getWorkspaceSettingsPath() const {
    if (m_workspacePath.isEmpty()) {
        return QString();
    }
    
    return QDir(m_workspacePath).filePath(".rawrxd/workspace.json");
}

} // namespace RawrXD
