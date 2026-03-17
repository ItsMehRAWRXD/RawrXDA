#include "sandboxed_terminal.hpp"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QDir>

SandboxedTerminal::SandboxedTerminal(QObject* parent)
    : QObject(parent),
      m_process(nullptr)
{
    logStructured("INFO", "SandboxedTerminal initializing", QJsonObject{{"component", "SandboxedTerminal"}});
    logStructured("INFO", "SandboxedTerminal initialized successfully", QJsonObject{{"component", "SandboxedTerminal"}});
}

SandboxedTerminal::~SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal shutting down", QJsonObject{{"component", "SandboxedTerminal"}});
    
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(5000)) {
                m_process->kill();
            }
        }
        delete m_process;
    }
    
    logStructured("INFO", "SandboxedTerminal shutdown complete", QJsonObject{{"component", "SandboxedTerminal"}});
}

void SandboxedTerminal::setConfig(const Config& config)
{
    QMutexLocker locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", QJsonObject{
        {"useWhitelistMode", config.useWhitelistMode},
        {"maxExecutionTimeMs", config.maxExecutionTimeMs},
        {"maxOutputSize", config.maxOutputSize},
        {"enableResourceLimits", config.enableResourceLimits}
    });
}

SandboxedTerminal::Config SandboxedTerminal::getConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_config;
}

SandboxedTerminal::CommandResult SandboxedTerminal::executeCommand(const QString& command, const QStringList& args)
{
    auto startTime = std::chrono::steady_clock::now();
    CommandResult result;
    result.exitCode = -1;
    result.timedOut = false;
    result.wasBlocked = false;
    
    try {
        Config config;
        {
            QMutexLocker configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Validate command
        QString blockReason;
        if (!validateCommand(command, blockReason)) {
            result.wasBlocked = true;
            result.blockReason = blockReason;
            
            {
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.commandsBlocked++;
                m_metrics.securityViolations++;
            }
            
            logStructured("WARN", "Command blocked", QJsonObject{
                {"command", command},
                {"reason", blockReason}
            });
            
            if (config.enableAuditLog) {
                logAudit("command_blocked", QJsonObject{
                    {"command", command},
                    {"args", QJsonArray::fromStringList(args)},
                    {"reason", blockReason}
                });
            }
            
            emit commandBlocked(command, blockReason);
            emit securityViolation(QString("Blocked command: %1 - %2").arg(command, blockReason));
            return result;
        }
        
        {
            QMutexLocker processLocker(&m_processMutex);
            
            if (m_process && m_process->state() != QProcess::NotRunning) {
                logStructured("ERROR", "Process already running", QJsonObject{});
                result.error = "Another command is already executing";
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.errorCount++;
                emit errorOccurred(result.error);
                return result;
            }
            
            if (m_process) {
                delete m_process;
            }
            
            m_process = new QProcess(this);
            
            // Set working directory
            if (!config.workingDirectory.isEmpty()) {
                m_process->setWorkingDirectory(config.workingDirectory);
            }
            
            // Set sanitized environment
            m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
            if (!config.allowedEnvironmentVars.isEmpty()) {
                QProcessEnvironment env;
                QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
                for (const QString& var : config.allowedEnvironmentVars) {
                    if (sysEnv.contains(var)) {
                        env.insert(var, sysEnv.value(var));
                    }
                }
                m_process->setProcessEnvironment(env);
            }
            
            // Start process
            logStructured("INFO", "Starting command", QJsonObject{
                {"command", command},
                {"args", QJsonArray::fromStringList(args)}
            });
            
            emit commandStarted(command);
            
            m_process->start(command, args);
            
            if (!m_process->waitForStarted(5000)) {
                logStructured("ERROR", "Failed to start process", QJsonObject{
                    {"command", command},
                    {"error", m_process->errorString()}
                });
                result.error = m_process->errorString();
                QMutexLocker metricsLocker(&m_metricsMutex);
                m_metrics.errorCount++;
                emit errorOccurred(result.error);
                return result;
            }
            
            // Wait for finish with timeout
            bool finished = m_process->waitForFinished(config.maxExecutionTimeMs);
            
            if (!finished) {
                m_process->kill();
                result.timedOut = true;
                
                {
                    QMutexLocker metricsLocker(&m_metricsMutex);
                    m_metrics.commandsTimedOut++;
                }
                
                logStructured("WARN", "Command timed out", QJsonObject{
                    {"command", command},
                    {"timeoutMs", config.maxExecutionTimeMs}
                });
                
                if (config.enableAuditLog) {
                    logAudit("command_timeout", QJsonObject{
                        {"command", command},
                        {"args", QJsonArray::fromStringList(args)},
                        {"timeoutMs", config.maxExecutionTimeMs}
                    });
                }
            }
            
            // Read output
            QByteArray stdoutData = m_process->readAllStandardOutput();
            QByteArray stderrData = m_process->readAllStandardError();
            
            QString rawOutput = QString::fromUtf8(stdoutData);
            QString rawError = QString::fromUtf8(stderrData);
            
            // Limit output size
            if (rawOutput.length() > config.maxOutputSize) {
                rawOutput = rawOutput.left(config.maxOutputSize) + "\n[OUTPUT TRUNCATED]";
            }
            if (rawError.length() > config.maxOutputSize) {
                rawError = rawError.left(config.maxOutputSize) + "\n[ERROR OUTPUT TRUNCATED]";
            }
            
            // Sanitize output
            if (config.enableOutputFiltering) {
                result.output = sanitizeOutput(rawOutput);
                result.error = sanitizeOutput(rawError);
                
                qint64 filteredBytes = (rawOutput.length() - result.output.length()) + 
                                       (rawError.length() - result.error.length());
                
                {
                    QMutexLocker metricsLocker(&m_metricsMutex);
                    m_metrics.outputBytesFiltered += filteredBytes;
                }
            } else {
                result.output = rawOutput;
                result.error = rawError;
            }
            
            result.exitCode = m_process->exitCode();
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        result.executionTimeMs = duration.count();
        recordLatency("command_execution", duration);
        
        {
            QMutexLocker metricsLocker(&m_metricsMutex);
            m_metrics.commandsExecuted++;
        }
        
        logStructured("INFO", "Command completed", QJsonObject{
            {"command", command},
            {"exitCode", result.exitCode},
            {"executionTimeMs", result.executionTimeMs},
            {"timedOut", result.timedOut},
            {"outputSize", result.output.length()}
        });
        
        if (config.enableAuditLog) {
            logAudit("command_executed", QJsonObject{
                {"command", command},
                {"args", QJsonArray::fromStringList(args)},
                {"exitCode", result.exitCode},
                {"executionTimeMs", result.executionTimeMs},
                {"timedOut", result.timedOut}
            });
        }
        
        emit commandFinished(result);
        
    } catch (const std::exception& e) {
        QMutexLocker metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Command execution failed", QJsonObject{{"error", e.what()}});
        emit errorOccurred(QString("Execution failed: %1").arg(e.what()));
        result.error = e.what();
    }
    
    return result;
}

bool SandboxedTerminal::isCommandAllowed(const QString& command) const
{
    QString blockReason;
    return validateCommand(command, blockReason);
}

QString SandboxedTerminal::sanitizeOutput(const QString& output) const
{
    return filterSensitiveData(output);
}

bool SandboxedTerminal::isRunning() const
{
    QMutexLocker locker(&m_processMutex);
    return m_process && m_process->state() != QProcess::NotRunning;
}

void SandboxedTerminal::terminate()
{
    QMutexLocker locker(&m_processMutex);
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        logStructured("INFO", "Process terminated", QJsonObject{});
    }
}

void SandboxedTerminal::kill()
{
    QMutexLocker locker(&m_processMutex);
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        logStructured("INFO", "Process killed", QJsonObject{});
    }
}

SandboxedTerminal::Metrics SandboxedTerminal::getMetrics() const
{
    QMutexLocker locker(&m_metricsMutex);
    return m_metrics;
}

void SandboxedTerminal::resetMetrics()
{
    QMutexLocker locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", QJsonObject{});
}

void SandboxedTerminal::logStructured(const QString& level, const QString& message, const QJsonObject& context)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "SandboxedTerminal";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    QJsonDocument doc(logEntry);
    qDebug().noquote() << doc.toJson(QJsonDocument::Compact);
}

void SandboxedTerminal::recordLatency(const QString& operation, const std::chrono::milliseconds& duration)
{
    QMutexLocker locker(&m_metricsMutex);
    
    if (operation == "command_execution") {
        m_metrics.avgExecutionTimeMs = 
            (m_metrics.avgExecutionTimeMs * (m_metrics.commandsExecuted - 1) + duration.count()) 
            / m_metrics.commandsExecuted;
    }
    
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        emit metricsUpdated(m_metrics);
    }
}

void SandboxedTerminal::logAudit(const QString& action, const QJsonObject& details)
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.isEmpty()) {
        return;
    }
    
    QJsonObject auditEntry;
    auditEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    QFile auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        QJsonDocument doc(auditEntry);
        out << doc.toJson(QJsonDocument::Compact) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", QJsonObject{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

bool SandboxedTerminal::validateCommand(const QString& command, QString& blockReason) const
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    QString baseCommand = command.split(' ').first();
    QFileInfo cmdInfo(baseCommand);
    QString cmdName = cmdInfo.fileName();
    
    // Check blacklist first
    if (config.commandBlacklist.contains(cmdName) || config.commandBlacklist.contains(baseCommand)) {
        blockReason = "Command is explicitly blacklisted";
        return false;
    }
    
    // Check for dangerous patterns
    QStringList dangerousPatterns = {
        "rm -rf /",
        ":(){ :|:& };:",  // Fork bomb
        "dd if=/dev/random",
        "mkfs",
        "format"
    };
    
    for (const QString& pattern : dangerousPatterns) {
        if (command.contains(pattern, Qt::CaseInsensitive)) {
            blockReason = QString("Dangerous pattern detected: %1").arg(pattern);
            return false;
        }
    }
    
    // Whitelist mode
    if (config.useWhitelistMode) {
        bool allowed = config.commandWhitelist.contains(cmdName) || 
                       config.commandWhitelist.contains(baseCommand);
        if (!allowed) {
            blockReason = "Command not in whitelist";
            return false;
        }
    }
    
    return true;
}

QStringList SandboxedTerminal::buildSanitizedEnvironment() const
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    QStringList env;
    QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
    
    if (config.allowedEnvironmentVars.isEmpty()) {
        // Default safe environment variables
        QStringList safeVars = {"PATH", "HOME", "USER", "TEMP", "TMP"};
        for (const QString& var : safeVars) {
            if (sysEnv.contains(var)) {
                env.append(QString("%1=%2").arg(var, sysEnv.value(var)));
            }
        }
    } else {
        for (const QString& var : config.allowedEnvironmentVars) {
            if (sysEnv.contains(var)) {
                env.append(QString("%1=%2").arg(var, sysEnv.value(var)));
            }
        }
    }
    
    return env;
}

bool SandboxedTerminal::enforceResourceLimits()
{
    Config config;
    {
        QMutexLocker configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableResourceLimits) {
        return true;
    }
    
    // Resource limits are platform-specific
    // On Windows, this would require job objects
    // On Linux/Unix, use setrlimit()
    // This is a simplified implementation
    
    logStructured("DEBUG", "Resource limits enforcement", QJsonObject{
        {"maxMemoryBytes", config.maxMemoryBytes},
        {"maxCpuPercent", config.maxCpuPercent}
    });
    
    return true;
}

QString SandboxedTerminal::filterSensitiveData(const QString& data) const
{
    QString filtered = data;
    
    // Filter common sensitive patterns
    QRegularExpression apiKeyPattern(R"(api[_-]?key[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression passwordPattern(R"(password[\s=:]+['\"]?([^'\"\s]+)['\"]?)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression tokenPattern(R"(token[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    QRegularExpression ipPattern(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
    
    filtered.replace(apiKeyPattern, "api_key=[REDACTED]");
    filtered.replace(passwordPattern, "password=[REDACTED]");
    filtered.replace(tokenPattern, "token=[REDACTED]");
    filtered.replace(emailPattern, "[EMAIL_REDACTED]");
    filtered.replace(ipPattern, "[IP_REDACTED]");
    
    return filtered;
}
