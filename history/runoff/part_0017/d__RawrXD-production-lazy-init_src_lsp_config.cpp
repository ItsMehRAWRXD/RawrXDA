/**
 * \file lsp_config.cpp
 * \brief LSP Configuration Management Implementation
 */

#include "lsp_config.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QDebug>
#include <algorithm>

namespace RawrXD {

LSPConfigManager& LSPConfigManager::instance()
{
    static LSPConfigManager manager;
    return manager;
}

LSPConfigManager::LSPConfigManager()
{
    // Initialize with default configuration
    m_config = QJsonObject();
    m_lspEnabled = true;
}

bool LSPConfigManager::loadFromFile(const QString& configPath)
{
    QFile configFile(configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[LSPConfig] Failed to open config file:" << configPath;
        return false;
    }
    
    try {
        QByteArray data = configFile.readAll();
        configFile.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "[LSPConfig] Invalid JSON configuration";
            return false;
        }
        
        m_config = doc.object();
        m_configPath = configPath;
        
        // Extract LSP configuration
        if (m_config.contains("lsp")) {
            QJsonObject lspConfig = m_config["lsp"].toObject();
            m_lspEnabled = lspConfig.value("enabled", true).toBool();
        }
        
        qInfo() << "[LSPConfig] Configuration loaded from:" << configPath;
        return true;
    } catch (const std::exception& e) {
        qCritical() << "[LSPConfig] Exception loading config:" << e.what();
        return false;
    }
}

void LSPConfigManager::loadFromEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // Check if LSP is enabled
    QString enabledStr = env.value("RAWRXD_LSP_ENABLED", "true");
    m_lspEnabled = (enabledStr.toLower() == "true" || enabledStr == "1");
    
    // Load language-specific settings
    QStringList languages = {"cpp", "c", "python", "typescript", "javascript", "rust", "java"};
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    for (const QString& lang : languages) {
        QString cmdEnv = QString("RAWRXD_LSP_%1_COMMAND").arg(lang.toUpper());
        QString argsEnv = QString("RAWRXD_LSP_%1_ARGS").arg(lang.toUpper());
        
        if (env.contains(cmdEnv)) {
            QJsonObject langConfig = languagesConfig.value(lang).toObject();
            langConfig["command"] = env.value(cmdEnv);
            languagesConfig[lang] = langConfig;
        }
        
        if (env.contains(argsEnv)) {
            QJsonObject langConfig = languagesConfig.value(lang).toObject();
            QStringList args = env.value(argsEnv).split(" ");
            QJsonArray argsArray;
            for (const QString& arg : args) {
                argsArray.append(arg);
            }
            langConfig["arguments"] = argsArray;
            languagesConfig[lang] = langConfig;
        }
    }
    
    lspConfig["languages"] = languagesConfig;
    m_config["lsp"] = lspConfig;
    
    // Load logging configuration
    QString logLevel = env.value("RAWRXD_LSP_LOGGING_LEVEL", "INFO");
    QJsonObject loggingConfig = lspConfig.value("logging").toObject();
    loggingConfig["level"] = logLevel;
    lspConfig["logging"] = loggingConfig;
    
    qInfo() << "[LSPConfig] Configuration loaded from environment variables";
}

QString LSPConfigManager::getServerCommand(const QString& language) const
{
    if (!isLSPEnabled()) return {};
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    if (!languagesConfig.contains(language)) {
        qWarning() << "[LSPConfig] No LSP server configured for language:" << language;
        return {};
    }
    
    QJsonObject langConfig = languagesConfig[language].toObject();
    return langConfig.value("command").toString();
}

QStringList LSPConfigManager::getServerArguments(const QString& language) const
{
    if (!isLSPEnabled()) return {};
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    if (!languagesConfig.contains(language)) {
        return {};
    }
    
    QJsonObject langConfig = languagesConfig[language].toObject();
    QStringList args;
    
    if (langConfig.contains("arguments")) {
        QJsonArray argsArray = langConfig["arguments"].toArray();
        for (const QJsonValue& arg : argsArray) {
            args.append(arg.toString());
        }
    }
    
    return args;
}

bool LSPConfigManager::isFeatureEnabled(const QString& feature) const
{
    if (!isLSPEnabled()) return false;
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    
    // Map feature names to config paths
    static const QMap<QString, QString> featureMap {
        {"completion", "completion.enabled"},
        {"hover", "hover.enabled"},
        {"diagnostics", "diagnostics.enabled"},
        {"rename", "rename.enabled"},
        {"codeActions", "codeActions.enabled"},
        {"formatting", "formatting.enabled"},
        {"signature", "signature.enabled"}
    };
    
    if (featureMap.contains(feature)) {
        QString path = featureMap[feature];
        QVariant value = getConfig(path);
        return value.isValid() ? value.toBool() : true;  // Default to enabled
    }
    
    return true;
}

QVariant LSPConfigManager::getConfig(const QString& path, const QVariant& defaultValue) const
{
    QVariant result = getNestedValue(path);
    return result.isValid() ? result : defaultValue;
}

void LSPConfigManager::setConfig(const QString& path, const QVariant& value)
{
    setNestedValue(path, value);
}

QStringList LSPConfigManager::getAvailableLanguages() const
{
    if (!isLSPEnabled()) return {};
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    return languagesConfig.keys();
}

bool LSPConfigManager::isLanguageSupported(const QString& language) const
{
    if (!isLSPEnabled()) return false;
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    return languagesConfig.contains(language);
}

QMap<QString, QString> LSPConfigManager::getEnvironmentVariables(const QString& language) const
{
    QMap<QString, QString> envVars;
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject languagesConfig = lspConfig.value("languages").toObject();
    
    if (languagesConfig.contains(language)) {
        QJsonObject langConfig = languagesConfig[language].toObject();
        if (langConfig.contains("env")) {
            QJsonObject env = langConfig["env"].toObject();
            for (auto it = env.begin(); it != env.end(); ++it) {
                envVars[it.key()] = it.value().toString();
            }
        }
    }
    
    return envVars;
}

void LSPConfigManager::setLSPEnabled(bool enabled)
{
    m_lspEnabled = enabled;
    
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    lspConfig["enabled"] = enabled;
    m_config["lsp"] = lspConfig;
}

bool LSPConfigManager::isLSPEnabled() const
{
    return m_lspEnabled;
}

QString LSPConfigManager::getLoggingLevel() const
{
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject loggingConfig = lspConfig.value("logging").toObject();
    return loggingConfig.value("level", "INFO").toString();
}

QString LSPConfigManager::getLogFilePath() const
{
    QJsonObject lspConfig = m_config.value("lsp").toObject();
    QJsonObject loggingConfig = lspConfig.value("logging").toObject();
    
    if (!loggingConfig.value("enableFile", true).toBool()) {
        return {};
    }
    
    QString logFile = loggingConfig.value("file", "~/.rawrxd/lsp-debug.log").toString();
    
    // Expand ~ to home directory
    if (logFile.startsWith("~")) {
        logFile.replace(0, 1, QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    }
    
    return logFile;
}

void LSPConfigManager::reload()
{
    if (!m_configPath.isEmpty()) {
        loadFromFile(m_configPath);
    }
    loadFromEnvironment();
    qInfo() << "[LSPConfig] Configuration reloaded";
}

QVariant LSPConfigManager::getNestedValue(const QString& path) const
{
    QStringList parts = path.split(".");
    QJsonValue current = m_config;
    
    for (const QString& part : parts) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject()[part];
    }
    
    if (current.isBool()) return current.toBool();
    if (current.isDouble()) return current.toDouble();
    if (current.isString()) return current.toString();
    if (current.isArray()) return current.toArray();
    if (current.isObject()) return current.toObject();
    
    return {};
}

void LSPConfigManager::setNestedValue(const QString& path, const QVariant& value)
{
    QStringList parts = path.split(".");
    QJsonObject* current = &m_config;
    
    // Navigate/create nested structure
    for (int i = 0; i < parts.size() - 1; ++i) {
        QString part = parts[i];
        if (!current->contains(part) || !current->value(part).isObject()) {
            (*current)[part] = QJsonObject();
        }
        
        QJsonValue nextValue = current->value(part);
        current = nullptr;  // Will be reassigned in next iteration
        
        // This is simplified - in production, would need more careful handling
        if (i + 1 < parts.size() - 1) {
            // Continue to next level
            QJsonObject obj = nextValue.toObject();
            current = &obj;
        }
    }
    
    // Set final value
    if (!parts.isEmpty() && current) {
        QString lastPart = parts.last();
        if (value.type() == QVariant::Bool) {
            (*current)[lastPart] = value.toBool();
        } else if (value.type() == QVariant::Double) {
            (*current)[lastPart] = value.toDouble();
        } else if (value.type() == QVariant::String) {
            (*current)[lastPart] = value.toString();
        }
    }
}

} // namespace RawrXD
