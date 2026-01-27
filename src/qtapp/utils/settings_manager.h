#pragma once
/**
 * \file settings_manager.h
 * \brief Centralized application settings with JSON persistence
 * \author RawrXD Team
 * \date 2025-12-05
 * 
 * Features:
 * - JSON-based storage in ~/.rawrxd/settings.json
 * - Global application settings
 * - Workspace-specific overrides (.rawrxd/workspace.json)
 * - Type-safe getters/setters
 * - Automatic save on change
 * - Default values
 * - Settings change notifications
 */

#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QObject>

namespace RawrXD {

/**
 * \class SettingsManager
 * \brief Singleton settings manager with JSON persistence
 * 
 * Manages all application settings with automatic persistence to disk.
 * Supports both global settings and workspace-specific overrides.
 * 
 * Usage:
 * \code
 * SettingsManager& settings = SettingsManager::instance();
 * settings.setValue("editor/fontSize", 14);
 * int fontSize = settings.value("editor/fontSize", 12).toInt();
 * \endcode
 */
class SettingsManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * \brief Get singleton instance
     * \return Reference to global settings manager
     */
    static SettingsManager& instance();
    
    // No copy/move
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;
    
    /**
     * \brief Get setting value
     * \param key Setting key (use / for hierarchy, e.g., "editor/fontSize")
     * \param defaultValue Value to return if key doesn't exist
     * \return Setting value or default
     */
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    /**
     * \brief Set setting value
     * \param key Setting key
     * \param value New value
     * \param saveImmediately If true, save to disk immediately
     */
    void setValue(const QString& key, const QVariant& value, bool saveImmediately = true);
    
    /**
     * \brief Check if setting exists
     * \param key Setting key
     * \return true if setting exists
     */
    bool contains(const QString& key) const;
    
    /**
     * \brief Remove setting
     * \param key Setting key
     */
    void remove(const QString& key);
    
    /**
     * \brief Get all settings as JSON
     * \return JSON object with all settings
     */
    QJsonObject toJson() const;
    
    /**
     * \brief Set all settings from JSON
     * \param json JSON object with settings
     */
    void fromJson(const QJsonObject& json);
    
    /**
     * \brief Save settings to disk
     * \return true if successful
     */
    bool save();
    
    /**
     * \brief Load settings from disk
     * \return true if successful
     */
    bool load();
    
    /**
     * \brief Reset all settings to defaults
     */
    void resetToDefaults();
    
    /**
     * \brief Get settings file path
     * \return Absolute path to settings.json
     */
    QString settingsFilePath() const;
    
    // ========== Workspace Settings ==========
    
    /**
     * \brief Set current workspace path
     * \param path Absolute path to workspace root
     */
    void setWorkspacePath(const QString& path);
    
    /**
     * \brief Get current workspace path
     * \return Workspace root path
     */
    QString workspacePath() const;
    
    /**
     * \brief Get workspace-specific setting (overrides global)
     * \param key Setting key
     * \param defaultValue Default value
     * \return Workspace setting value, or global value if not set
     */
    QVariant workspaceValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    
    /**
     * \brief Set workspace-specific setting
     * \param key Setting key
     * \param value New value
     */
    void setWorkspaceValue(const QString& key, const QVariant& value);
    
    /**
     * \brief Save workspace settings to .rawrxd/workspace.json
     * \return true if successful
     */
    bool saveWorkspace();
    
    /**
     * \brief Load workspace settings
     * \return true if successful
     */
    bool loadWorkspace();
    
    // ========== Convenience Getters ==========
    
    // General settings
    bool autoSave() const;
    int autoSaveInterval() const;  // seconds
    bool restoreLastSession() const;
    
    // Appearance settings
    QString theme() const;
    QString fontFamily() const;
    int fontSize() const;
    QString colorScheme() const;
    
    // Editor settings
    int tabSize() const;
    bool insertSpaces() const;  // true = spaces, false = tabs
    bool trimTrailingWhitespace() const;
    bool insertFinalNewline() const;
    bool formatOnSave() const;
    QString lineEndings() const;  // "LF", "CRLF", "Auto"
    
    // Search settings
    bool searchCaseSensitive() const;
    bool searchWholeWord() const;
    bool searchUseRegex() const;
    
signals:
    /**
     * \brief Emitted when any setting changes
     * \param key Setting key that changed
     * \param value New value
     */
    void settingChanged(const QString& key, const QVariant& value);
    
    /**
     * \brief Emitted when settings are reset to defaults
     */
    void settingsReset();
    
    /**
     * \brief Emitted when settings are saved
     */
    void settingsSaved();
    
    /**
     * \brief Emitted when settings are loaded
     */
    void settingsLoaded();
    
private:
    SettingsManager();
    ~SettingsManager() override;
    
    void initializeDefaults();
    QString getSettingsDirectory() const;
    QString getWorkspaceSettingsPath() const;
    
    QJsonObject m_settings;           ///< Global settings
    QJsonObject m_workspaceSettings;  ///< Workspace-specific settings
    QString m_workspacePath;          ///< Current workspace path
};

} // namespace RawrXD
