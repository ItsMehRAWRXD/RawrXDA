/**
 * @file compiler_config.cpp
 * @brief Implementation of Compiler Configuration Manager
 */

#include "compiler_config.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QDebug>

// Global singleton instance
static std::unique_ptr<CompilerConfigManager> g_configManager;

CompilerConfigManager::CompilerConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings(new QSettings("RawrXD", "AgenticIDE"))
    , m_globalTimeoutMs(30000)
    , m_autoDetectLanguage(true)
    , m_verboseOutput(true)
{
    initializeExtensionMapping();
    initializeDefaultConfigs();
    loadConfiguration();
}

CompilerConfigManager::~CompilerConfigManager()
{
    saveConfiguration();
    delete m_settings;
}

void CompilerConfigManager::loadConfiguration()
{
    m_settings->beginGroup("CompilerConfig");
    
    // Load global settings
    m_globalTimeoutMs = m_settings->value("globalTimeout", 30000).toInt();
    m_defaultOutputPath = m_settings->value("defaultOutputPath", 
                                               QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).toString();
    m_autoDetectLanguage = m_settings->value("autoDetectLanguage", true).toBool();
    m_verboseOutput = m_settings->value("verboseOutput", true).toBool();
    
    // Load compiler configurations
    int compilerCount = m_settings->beginReadArray("compilers");
    for (int i = 0; i < compilerCount; ++i) {
        m_settings->setArrayIndex(i);
        
        CompilerConfig config;
        config.language = m_settings->value("language").toString();
        config.compilerPath = m_settings->value("compilerPath").toString();
        config.enabled = m_settings->value("enabled", true).toBool();
        config.timeoutMs = m_settings->value("timeout", m_globalTimeoutMs).toInt();
        config.defaultOutputPath = m_settings->value("defaultOutputPath", m_defaultOutputPath).toString();
        
        // Load default options
        QString optionsJson = m_settings->value("defaultOptions").toString();
        if (!optionsJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(optionsJson.toUtf8());
            config.defaultOptions = doc.object();
        }
        
        // Load file extensions
        config.fileExtensions = m_settings->value("fileExtensions").toStringList();
        
        m_compilerConfigs[config.language] = config;
    }
    m_settings->endArray();
    
    m_settings->endGroup();
}

void CompilerConfigManager::saveConfiguration()
{
    m_settings->beginGroup("CompilerConfig");
    
    // Save global settings
    m_settings->setValue("globalTimeout", m_globalTimeoutMs);
    m_settings->setValue("defaultOutputPath", m_defaultOutputPath);
    m_settings->setValue("autoDetectLanguage", m_autoDetectLanguage);
    m_settings->setValue("verboseOutput", m_verboseOutput);
    
    // Save compiler configurations
    m_settings->beginWriteArray("compilers");
    int index = 0;
    for (auto it = m_compilerConfigs.constBegin(); it != m_compilerConfigs.constEnd(); ++it) {
        m_settings->setArrayIndex(index++);
        
        const CompilerConfig &config = it.value();
        m_settings->setValue("language", config.language);
        m_settings->setValue("compilerPath", config.compilerPath);
        m_settings->setValue("enabled", config.enabled);
        m_settings->setValue("timeout", config.timeoutMs);
        m_settings->setValue("defaultOutputPath", config.defaultOutputPath);
        
        // Save default options
        if (!config.defaultOptions.isEmpty()) {
            QJsonDocument doc(config.defaultOptions);
            m_settings->setValue("defaultOptions", QString::fromUtf8(doc.toJson()));
        }
        
        // Save file extensions
        m_settings->setValue("fileExtensions", config.fileExtensions);
    }
    m_settings->endArray();
    
    m_settings->endGroup();
    m_settings->sync();
    
    emit configurationChanged();
}

void CompilerConfigManager::resetToDefaults()
{
    m_compilerConfigs.clear();
    initializeDefaultConfigs();
    
    m_globalTimeoutMs = 30000;
    m_defaultOutputPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    m_autoDetectLanguage = true;
    m_verboseOutput = true;
    
    saveConfiguration();
}

CompilerConfig CompilerConfigManager::getCompilerConfig(const QString &language) const
{
    if (m_compilerConfigs.contains(language)) {
        return m_compilerConfigs.value(language);
    }
    // Create default config - need to use mutable or const_cast
    CompilerConfigManager* nonConstThis = const_cast<CompilerConfigManager*>(this);
    CompilerConfig defaultConfig = nonConstThis->createDefaultConfig(language);
    return defaultConfig;
}

void CompilerConfigManager::setCompilerConfig(const QString &language, const CompilerConfig &config)
{
    m_compilerConfigs[language] = config;
    saveConfiguration();
    emit compilerConfigUpdated(language);
}

void CompilerConfigManager::removeCompilerConfig(const QString &language)
{
    m_compilerConfigs.remove(language);
    saveConfiguration();
    emit compilerConfigUpdated(language);
}

void CompilerConfigManager::setGlobalTimeout(int timeoutMs)
{
    if (m_globalTimeoutMs != timeoutMs) {
        m_globalTimeoutMs = timeoutMs;
        saveConfiguration();
        emit globalSettingsChanged();
    }
}

int CompilerConfigManager::getGlobalTimeout() const
{
    return m_globalTimeoutMs;
}

void CompilerConfigManager::setDefaultOutputPath(const QString &path)
{
    if (m_defaultOutputPath != path) {
        m_defaultOutputPath = path;
        saveConfiguration();
        emit globalSettingsChanged();
    }
}

QString CompilerConfigManager::getDefaultOutputPath() const
{
    return m_defaultOutputPath;
}

void CompilerConfigManager::setAutoDetectLanguage(bool enabled)
{
    if (m_autoDetectLanguage != enabled) {
        m_autoDetectLanguage = enabled;
        saveConfiguration();
        emit globalSettingsChanged();
    }
}

bool CompilerConfigManager::getAutoDetectLanguage() const
{
    return m_autoDetectLanguage;
}

void CompilerConfigManager::setVerboseOutput(bool enabled)
{
    if (m_verboseOutput != enabled) {
        m_verboseOutput = enabled;
        saveConfiguration();
        emit globalSettingsChanged();
    }
}

bool CompilerConfigManager::getVerboseOutput() const
{
    return m_verboseOutput;
}

QStringList CompilerConfigManager::discoverCompilers(const QString &basePath)
{
    QStringList compilers;
    
    QDir compilersDir(basePath);
    if (!compilersDir.exists()) {
        return compilers;
    }
    
    // Look for PowerShell scripts
    QStringList filters;
    filters << "*.ps1";
    QFileInfoList files = compilersDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : files) {
        if (validateCompiler(fileInfo.absoluteFilePath())) {
            compilers << fileInfo.absoluteFilePath();
        }
    }
    
    return compilers;
}

bool CompilerConfigManager::validateCompiler(const QString &compilerPath)
{
    if (!QFile::exists(compilerPath)) {
        return false;
    }
    
    QFile file(compilerPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    // Basic validation: check if it contains PowerShell content
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return content.contains("param(") || content.contains("function") || 
           content.contains("Write-Output") || content.contains("Write-Error");
}

QString CompilerConfigManager::detectLanguageFromExtension(const QString &filePath)
{
    if (!m_autoDetectLanguage) {
        return "";
    }
    
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    return m_extensionToLanguage.value(extension, "");
}

QStringList CompilerConfigManager::getSupportedLanguages() const
{
    return m_compilerConfigs.keys();
}

void CompilerConfigManager::initializeDefaultConfigs()
{
    // Create default configurations for common languages
    QStringList languages = {"cpp", "java", "python", "rust", "go", "javascript", "typescript"};
    
    for (const QString &language : languages) {
        if (!m_compilerConfigs.contains(language)) {
            m_compilerConfigs[language] = createDefaultConfig(language);
        }
    }
}

void CompilerConfigManager::initializeExtensionMapping()
{
    // Map file extensions to languages
    m_extensionToLanguage["cpp"] = "cpp";
    m_extensionToLanguage["c"] = "cpp";
    m_extensionToLanguage["h"] = "cpp";
    m_extensionToLanguage["hpp"] = "cpp";
    m_extensionToLanguage["cc"] = "cpp";
    m_extensionToLanguage["cxx"] = "cpp";
    
    m_extensionToLanguage["java"] = "java";
    
    m_extensionToLanguage["py"] = "python";
    
    m_extensionToLanguage["rs"] = "rust";
    
    m_extensionToLanguage["go"] = "go";
    
    m_extensionToLanguage["js"] = "javascript";
    m_extensionToLanguage["mjs"] = "javascript";
    
    m_extensionToLanguage["ts"] = "typescript";
    m_extensionToLanguage["tsx"] = "typescript";
    
    // Add more mappings as needed
}

CompilerConfig CompilerConfigManager::createDefaultConfig(const QString &language)
{
    CompilerConfig config;
    config.language = language;
    config.enabled = true;
    config.timeoutMs = m_globalTimeoutMs;
    config.defaultOutputPath = m_defaultOutputPath;
    
    // Set default compiler path based on language
    QString defaultPath = QApplication::applicationDirPath() + "/compilers/" + language + "_compiler.ps1";
    config.compilerPath = defaultPath;
    
    // Set default options
    config.defaultOptions["verbose"] = m_verboseOutput;
    config.defaultOptions["optimize"] = true;
    config.defaultOptions["debug"] = false;
    
    // Set file extensions based on language
    if (language == "cpp") {
        config.fileExtensions = QStringList() << ".cpp" << ".c" << ".h" << ".hpp";
    } else if (language == "java") {
        config.fileExtensions = QStringList() << ".java";
    } else if (language == "python") {
        config.fileExtensions = QStringList() << ".py";
    } else if (language == "rust") {
        config.fileExtensions = QStringList() << ".rs";
    } else if (language == "go") {
        config.fileExtensions = QStringList() << ".go";
    } else if (language == "javascript") {
        config.fileExtensions = QStringList() << ".js" << ".mjs";
    } else if (language == "typescript") {
        config.fileExtensions = QStringList() << ".ts" << ".tsx";
    } else {
        config.fileExtensions = QStringList() << "." + language;
    }
    
    return config;
}

CompilerConfigManager* getCompilerConfigManager()
{
    if (!g_configManager) {
        g_configManager = std::make_unique<CompilerConfigManager>();
    }
    return g_configManager.get();
}