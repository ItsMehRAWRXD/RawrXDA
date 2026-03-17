/**
 * \file scripting_engine.cpp
 * \brief Implementation of scripting engine
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "scripting_engine.h"
#include <QFile>
#include <QDebug>
#include <QVariant>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

using namespace RawrXD::ReverseEngineering;

ScriptingEngine::ScriptingEngine() = default;

ScriptingEngine::~ScriptingEngine() = default;

QString ScriptingEngine::executePython(const QString& script, const QMap<QString, QVariant>& context) {
    // Validate Python is available
    QProcess python;
    python.start("python", QStringList() << "--version");
    if (!python.waitForFinished() || python.exitCode() != 0) {
        m_lastError = "Python is not available on this system";
        qWarning() << m_lastError;
        return "";
    }
    
    // Create temporary script file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString scriptPath = QDir(tempDir).filePath("rawr_script_" + QString::number(QCoreApplication::applicationPid()) + ".py");
    
    // Prepare script with context injection
    QString fullScript = "#!/usr/bin/env python3\n";
    fullScript += "import json\n";
    fullScript += "import sys\n\n";
    
    // Inject context variables as JSON
    fullScript += "# Context variables\n";
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        QString varName = it.key();
        QVariant value = it.value();
        
        if (value.type() == QVariant::String) {
            fullScript += QString("%1 = %2\n").arg(varName, "'" + value.toString() + "'");
        } else if (value.type() == QVariant::Int) {
            fullScript += QString("%1 = %2\n").arg(varName).arg(value.toInt());
        } else if (value.type() == QVariant::Double) {
            fullScript += QString("%1 = %2\n").arg(varName).arg(value.toDouble());
        }
    }
    
    fullScript += "\n# User script\n";
    fullScript += script;
    fullScript += "\n";
    
    // Write temporary script
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Failed to create temporary script: %1").arg(scriptFile.errorString());
        return "";
    }
    scriptFile.write(fullScript.toUtf8());
    scriptFile.close();
    
    // Execute script
    QProcess proc;
    proc.start("python", QStringList() << scriptPath);
    
    if (!proc.waitForFinished(30000)) {  // 30 second timeout
        m_lastError = "Script execution timeout";
        scriptFile.remove();
        return "";
    }
    
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    if (proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError());
    }
    
    // Cleanup
    scriptFile.remove();
    
    return formatOutput(output);
}

QString ScriptingEngine::executeLua(const QString& script, const QMap<QString, QVariant>& context) {
    // Validate Lua is available
    QProcess lua;
    lua.start("lua", QStringList() << "-v");
    if (!lua.waitForFinished() || lua.exitCode() != 0) {
        m_lastError = "Lua is not available on this system";
        qWarning() << m_lastError;
        return "";
    }
    
    // Create temporary script file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString scriptPath = QDir(tempDir).filePath("rawr_script_" + QString::number(QCoreApplication::applicationPid()) + ".lua");
    
    // Prepare script with context injection
    QString fullScript;
    
    // Inject context variables
    fullScript += "-- Context variables\n";
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        QString varName = it.key();
        QVariant value = it.value();
        
        if (value.type() == QVariant::String) {
            fullScript += QString("%1 = %2\n").arg(varName, "'" + value.toString() + "'");
        } else if (value.type() == QVariant::Int) {
            fullScript += QString("%1 = %2\n").arg(varName).arg(value.toInt());
        } else if (value.type() == QVariant::Double) {
            fullScript += QString("%1 = %2\n").arg(varName).arg(value.toDouble());
        }
    }
    
    fullScript += "\n-- User script\n";
    fullScript += script;
    fullScript += "\n";
    
    // Write temporary script
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Failed to create temporary script: %1").arg(scriptFile.errorString());
        return "";
    }
    scriptFile.write(fullScript.toUtf8());
    scriptFile.close();
    
    // Execute script
    QProcess proc;
    proc.start("lua", QStringList() << scriptPath);
    
    if (!proc.waitForFinished(30000)) {  // 30 second timeout
        m_lastError = "Script execution timeout";
        scriptFile.remove();
        return "";
    }
    
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    if (proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError());
    }
    
    // Cleanup
    scriptFile.remove();
    
    return formatOutput(output);
}

QString ScriptingEngine::executeJavaScript(const QString& script, const QMap<QString, QVariant>& context) {
    // Validate Node.js is available
    QProcess node;
    node.start("node", QStringList() << "--version");
    if (!node.waitForFinished() || node.exitCode() != 0) {
        m_lastError = "Node.js is not available on this system";
        qWarning() << m_lastError;
        return "";
    }
    
    // Create temporary script file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString scriptPath = QDir(tempDir).filePath("rawr_script_" + QString::number(QCoreApplication::applicationPid()) + ".js");
    
    // Prepare script with context injection
    QString fullScript = "#!/usr/bin/env node\n";
    
    // Inject context variables
    fullScript += "// Context variables\n";
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        QString varName = it.key();
        QVariant value = it.value();
        
        if (value.type() == QVariant::String) {
            fullScript += QString("const %1 = '%2';\n").arg(varName, value.toString());
        } else if (value.type() == QVariant::Int) {
            fullScript += QString("const %1 = %2;\n").arg(varName).arg(value.toInt());
        } else if (value.type() == QVariant::Double) {
            fullScript += QString("const %1 = %2;\n").arg(varName).arg(value.toDouble());
        }
    }
    
    fullScript += "\n// User script\n";
    fullScript += script;
    fullScript += "\n";
    
    // Write temporary script
    QFile scriptFile(scriptPath);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_lastError = QString("Failed to create temporary script: %1").arg(scriptFile.errorString());
        return "";
    }
    scriptFile.write(fullScript.toUtf8());
    scriptFile.close();
    
    // Execute script
    QProcess proc;
    proc.start("node", QStringList() << scriptPath);
    
    if (!proc.waitForFinished(30000)) {  // 30 second timeout
        m_lastError = "Script execution timeout";
        scriptFile.remove();
        return "";
    }
    
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    if (proc.exitCode() != 0) {
        m_lastError = QString::fromUtf8(proc.readAllStandardError());
    }
    
    // Cleanup
    scriptFile.remove();
    
    return formatOutput(output);
}

void ScriptingEngine::registerFunction(const QString& name, std::function<QString(const QVector<QString>&)> func) {
    m_functions[name] = func;
    qDebug() << "Registered script function:" << name;
}

QString ScriptingEngine::loadScript(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to load script:" << filePath;
        return "";
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    return content;
}

bool ScriptingEngine::saveScript(const QString& filePath, const QString& script) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to save script:" << filePath;
        return false;
    }
    
    file.write(script.toUtf8());
    file.close();
    
    return true;
}

void ScriptingEngine::initializeContextVariables(const QMap<QString, QVariant>& context) {
    // Context-specific initialization can go here
    qDebug() << "Initialized context with" << context.size() << "variables";
}

QString ScriptingEngine::formatOutput(const QString& raw) {
    // Basic formatting - could add color/styling later
    return raw.trimmed();
}
