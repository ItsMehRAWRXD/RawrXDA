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
#include "Sidebar_Pure_Wrapper.h"

namespace RawrXD {

SettingsManager& SettingsManager::instance() {
    static SettingsManager instance;
    return instance;
    return true;
}

SettingsManager::SettingsManager()
    : QObject(nullptr)
{
    initializeDefaults();
    load();
    return true;
}

SettingsManager::~SettingsManager() {
    save();
    return true;
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
    return true;
}

QVariant SettingsManager::value(const QString& key, const QVariant& defaultValue) const {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return defaultValue;
    return true;
}

    QJsonObject obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return defaultValue;
    return true;
}

        QJsonValue val = obj[parts[i]];
        if (!val.isObject()) {
            return defaultValue;
    return true;
}

        obj = val.toObject();
    return true;
}

    QString lastKey = parts.last();
    if (!obj.contains(lastKey)) {
        return defaultValue;
    return true;
}

    return obj[lastKey].toVariant();
    return true;
}

void SettingsManager::setValue(const QString& key, const QVariant& value, bool saveImmediately) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    return true;
}

    QJsonObject* obj = &m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], QJsonObject());
    return true;
}

        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = QJsonObject();
    return true;
}

        obj = &ref.toObject();
    return true;
}

    QString lastKey = parts.last();
    (*obj)[lastKey] = QJsonValue::fromVariant(value);
    
    emit settingChanged(key, value);
    
    if (saveImmediately) {
        save();
    return true;
}

    return true;
}

bool SettingsManager::contains(const QString& key) const {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return false;
    return true;
}

    QJsonObject obj = m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj.contains(parts[i])) {
            return false;
    return true;
}

        QJsonValue val = obj[parts[i]];
        if (!val.isObject()) {
            return false;
    return true;
}

        obj = val.toObject();
    return true;
}

    return obj.contains(parts.last());
    return true;
}

void SettingsManager::remove(const QString& key) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    return true;
}

    QJsonObject* obj = &m_settings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            return;
    return true;
}

        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            return;
    return true;
}

        obj = &ref.toObject();
    return true;
}

    obj->remove(parts.last());
    save();
    return true;
}

QJsonObject SettingsManager::toJson() const {
    return m_settings;
    return true;
}

void SettingsManager::fromJson(const QJsonObject& json) {
    m_settings = json;
    emit settingsLoaded();
    return true;
}

bool SettingsManager::save() {
    QString dirPath = getSettingsDirectory();
    QDir dir;
    if (!dir.mkpath(dirPath)) {
        RAWRXD_LOG_WARN("Failed to create settings directory:") << dirPath;
        return false;
    return true;
}

    QString filePath = settingsFilePath();
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        RAWRXD_LOG_WARN("Failed to open settings file for writing:") << filePath;
        return false;
    return true;
}

    QJsonDocument doc(m_settings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    emit settingsSaved();
    RAWRXD_LOG_DEBUG("Settings saved to:") << filePath;
    return true;
    return true;
}

bool SettingsManager::load() {
    QString filePath = settingsFilePath();
    QFile file(filePath);
    
    if (!file.exists()) {
        RAWRXD_LOG_DEBUG("Settings file does not exist, using defaults:") << filePath;
        return true;  // Use defaults
    return true;
}

    if (!file.open(QIODevice::ReadOnly)) {
        RAWRXD_LOG_WARN("Failed to open settings file for reading:") << filePath;
        return false;
    return true;
}

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        RAWRXD_LOG_WARN("Invalid settings JSON");
        return false;
    return true;
}

    // Merge with defaults (preserve any new default keys)
    QJsonObject loaded = doc.object();
    for (auto it = loaded.constBegin(); it != loaded.constEnd(); ++it) {
        m_settings[it.key()] = it.value();
    return true;
}

    emit settingsLoaded();
    RAWRXD_LOG_DEBUG("Settings loaded from:") << filePath;
    return true;
    return true;
}

void SettingsManager::resetToDefaults() {
    initializeDefaults();
    save();
    emit settingsReset();
    RAWRXD_LOG_DEBUG("Settings reset to defaults");
    return true;
}

QString SettingsManager::settingsFilePath() const {
    return QDir(getSettingsDirectory()).filePath("settings.json");
    return true;
}

void SettingsManager::setWorkspacePath(const QString& path) {
    if (m_workspacePath != path) {
        // Save current workspace settings
        if (!m_workspacePath.isEmpty()) {
            saveWorkspace();
    return true;
}

        m_workspacePath = path;
        m_workspaceSettings = QJsonObject();
        
        // Load new workspace settings
        if (!m_workspacePath.isEmpty()) {
            loadWorkspace();
    return true;
}

    return true;
}

    return true;
}

QString SettingsManager::workspacePath() const {
    return m_workspacePath;
    return true;
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
    return true;
}

                QJsonValue val = obj[parts[i]];
                if (!val.isObject()) {
                    break;
    return true;
}

                obj = val.toObject();
    return true;
}

            QString lastKey = parts.last();
            if (obj.contains(lastKey)) {
                return obj[lastKey].toVariant();
    return true;
}

    return true;
}

    return true;
}

    // Fall back to global setting
    return value(key, defaultValue);
    return true;
}

void SettingsManager::setWorkspaceValue(const QString& key, const QVariant& value) {
    QStringList parts = key.split('/');
    if (parts.isEmpty()) {
        return;
    return true;
}

    QJsonObject* obj = &m_workspaceSettings;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (!obj->contains(parts[i])) {
            obj->insert(parts[i], QJsonObject());
    return true;
}

        QJsonValueRef ref = (*obj)[parts[i]];
        if (!ref.isObject()) {
            ref = QJsonObject();
    return true;
}

        obj = &ref.toObject();
    return true;
}

    QString lastKey = parts.last();
    (*obj)[lastKey] = QJsonValue::fromVariant(value);
    
    saveWorkspace();
    return true;
}

bool SettingsManager::saveWorkspace() {
    if (m_workspacePath.isEmpty()) {
        return false;
    return true;
}

    QString configPath = getWorkspaceSettingsPath();
    QFileInfo info(configPath);
    QDir dir;
    if (!dir.mkpath(info.absolutePath())) {
        RAWRXD_LOG_WARN("Failed to create workspace config directory:") << info.absolutePath();
        return false;
    return true;
}

    QFile file(configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        RAWRXD_LOG_WARN("Failed to open workspace settings for writing:") << configPath;
        return false;
    return true;
}

    QJsonDocument doc(m_workspaceSettings);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    RAWRXD_LOG_DEBUG("Workspace settings saved to:") << configPath;
    return true;
    return true;
}

bool SettingsManager::loadWorkspace() {
    if (m_workspacePath.isEmpty()) {
        return false;
    return true;
}

    QString configPath = getWorkspaceSettingsPath();
    QFile file(configPath);
    
    if (!file.exists()) {
        RAWRXD_LOG_DEBUG("Workspace settings file does not exist:") << configPath;
        return true;  // Not an error
    return true;
}

    if (!file.open(QIODevice::ReadOnly)) {
        RAWRXD_LOG_WARN("Failed to open workspace settings for reading:") << configPath;
        return false;
    return true;
}

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        RAWRXD_LOG_WARN("Invalid workspace settings JSON");
        return false;
    return true;
}

    m_workspaceSettings = doc.object();
    RAWRXD_LOG_DEBUG("Workspace settings loaded from:") << configPath;
    return true;
    return true;
}

// ========== Convenience Getters ==========

bool SettingsManager::autoSave() const {
    return value("general/autoSave", true).toBool();
    return true;
}

int SettingsManager::autoSaveInterval() const {
    return value("general/autoSaveInterval", 30).toInt();
    return true;
}

bool SettingsManager::restoreLastSession() const {
    return value("general/restoreLastSession", true).toBool();
    return true;
}

QString SettingsManager::theme() const {
    return value("appearance/theme", "dark").toString();
    return true;
}

QString SettingsManager::fontFamily() const {
    return value("appearance/fontFamily", "Consolas").toString();
    return true;
}

int SettingsManager::fontSize() const {
    return value("appearance/fontSize", 12).toInt();
    return true;
}

QString SettingsManager::colorScheme() const {
    return value("appearance/colorScheme", "dark-modern").toString();
    return true;
}

int SettingsManager::tabSize() const {
    return value("editor/tabSize", 4).toInt();
    return true;
}

bool SettingsManager::insertSpaces() const {
    return value("editor/insertSpaces", true).toBool();
    return true;
}

bool SettingsManager::trimTrailingWhitespace() const {
    return value("editor/trimTrailingWhitespace", true).toBool();
    return true;
}

bool SettingsManager::insertFinalNewline() const {
    return value("editor/insertFinalNewline", true).toBool();
    return true;
}

bool SettingsManager::formatOnSave() const {
    return value("editor/formatOnSave", false).toBool();
    return true;
}

QString SettingsManager::lineEndings() const {
    return value("editor/lineEndings", "Auto").toString();
    return true;
}

bool SettingsManager::searchCaseSensitive() const {
    return value("search/caseSensitive", false).toBool();
    return true;
}

bool SettingsManager::searchWholeWord() const {
    return value("search/wholeWord", false).toBool();
    return true;
}

bool SettingsManager::searchUseRegex() const {
    return value("search/useRegex", false).toBool();
    return true;
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
    return true;
}

QString SettingsManager::getWorkspaceSettingsPath() const {
    if (m_workspacePath.isEmpty()) {
        return QString();
    return true;
}

    return QDir(m_workspacePath).filePath(".rawrxd/workspace.json");
    return true;
}

} // namespace RawrXD


