/**
 * @file compiler_config.h
 * @brief Compiler Configuration Manager for PowerShell Compiler Integration
 * 
 * This module provides:
 * - Persistent compiler configuration storage
 * - Language-specific compiler settings
 * - Compiler path management
 * - Default compiler options
 * - Compiler feature toggles
 * 
 * @author RawrXD Agent Team
 * @version 1.0.0
 * @date 2025-12-17
 */

#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

/**
 * @struct CompilerConfig
 * @brief Configuration for a specific compiler
 */
struct CompilerConfig {
    QString language;           ///< Programming language
    QString compilerPath;       ///< Path to compiler script
    bool enabled;               ///< Whether compiler is enabled
    int timeoutMs;             ///< Execution timeout
    QString defaultOutputPath; ///< Default output directory
    QJsonObject defaultOptions; ///< Default compiler options
    QStringList fileExtensions; ///< Supported file extensions
};

/**
 * @class CompilerConfigManager
 * @brief Manages compiler configuration and settings
 */
class CompilerConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit CompilerConfigManager(QObject *parent = nullptr);
    ~CompilerConfigManager();

    // Configuration Management
    void loadConfiguration();
    void saveConfiguration();
    void resetToDefaults();
    
    // Compiler Configuration
    CompilerConfig getCompilerConfig(const QString &language) const;
    void setCompilerConfig(const QString &language, const CompilerConfig &config);
    void removeCompilerConfig(const QString &language);
    
    // Global Settings
    void setGlobalTimeout(int timeoutMs);
    int getGlobalTimeout() const;
    
    void setDefaultOutputPath(const QString &path);
    QString getDefaultOutputPath() const;
    
    void setAutoDetectLanguage(bool enabled);
    bool getAutoDetectLanguage() const;
    
    void setVerboseOutput(bool enabled);
    bool getVerboseOutput() const;
    
    // Compiler Discovery
    QStringList discoverCompilers(const QString &basePath);
    bool validateCompiler(const QString &compilerPath);
    
    // Language Mapping
    QString detectLanguageFromExtension(const QString &filePath);
    QStringList getSupportedLanguages() const;
    
signals:
    void configurationChanged();
    void compilerConfigUpdated(const QString &language);
    void globalSettingsChanged();

private:
    QSettings *m_settings;
    QMap<QString, CompilerConfig> m_compilerConfigs;
    
    // Global Settings
    int m_globalTimeoutMs;
    QString m_defaultOutputPath;
    bool m_autoDetectLanguage;
    bool m_verboseOutput;
    
    // Language-Extension Mapping
    QMap<QString, QString> m_extensionToLanguage;
    
    void initializeDefaultConfigs();
    void initializeExtensionMapping();
    CompilerConfig createDefaultConfig(const QString &language);
};

/**
 * @brief Global instance accessor for CompilerConfigManager
 * @return Singleton instance
 */
CompilerConfigManager* getCompilerConfigManager();