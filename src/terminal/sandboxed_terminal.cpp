#include "sandboxed_terminal.hpp"


SandboxedTerminal::SandboxedTerminal(void* parent)
    : void(parent),
      m_process(nullptr)
{
    logStructured("INFO", "SandboxedTerminal initializing", void*{{"component", "SandboxedTerminal"}});
    logStructured("INFO", "SandboxedTerminal initialized successfully", void*{{"component", "SandboxedTerminal"}});
}

SandboxedTerminal::~SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal shutting down", void*{{"component", "SandboxedTerminal"}});
    
    if (m_process) {
        if (m_process->state() != void*::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(5000)) {
                m_process->kill();
            }
        }
        delete m_process;
    }
    
    logStructured("INFO", "SandboxedTerminal shutdown complete", void*{{"component", "SandboxedTerminal"}});
}

void SandboxedTerminal::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", void*{
        {"useWhitelistMode", config.useWhitelistMode},
        {"maxExecutionTimeMs", config.maxExecutionTimeMs},
        {"maxOutputSize", config.maxOutputSize},
        {"enableResourceLimits", config.enableResourceLimits}
    });
}

SandboxedTerminal::Config SandboxedTerminal::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
}

SandboxedTerminal::CommandResult SandboxedTerminal::executeCommand(const std::string& command, const std::vector<std::string>& args)
{
    auto startTime = std::chrono::steady_clock::now();
    CommandResult result;
    result.exitCode = -1;
    result.timedOut = false;
    result.wasBlocked = false;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Validate command
        std::string blockReason;
        if (!validateCommand(command, blockReason)) {
            result.wasBlocked = true;
            result.blockReason = blockReason;
            
            {
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.commandsBlocked++;
                m_metrics.securityViolations++;
            }
            
            logStructured("WARN", "Command blocked", void*{
                {"command", command},
                {"reason", blockReason}
            });
            
            if (config.enableAuditLog) {
                logAudit("command_blocked", void*{
                    {"command", command},
                    {"args", void*::fromStringList(args)},
                    {"reason", blockReason}
                });
            }
            
            commandBlocked(command, blockReason);
            securityViolation(std::string("Blocked command: %1 - %2"));
            return result;
        }
        
        {
            std::lock_guard<std::mutex> processLocker(&m_processMutex);
            
            if (m_process && m_process->state() != void*::NotRunning) {
                logStructured("ERROR", "Process already running", void*{});
                result.error = "Another command is already executing";
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.errorCount++;
                errorOccurred(result.error);
                return result;
            }
            
            if (m_process) {
                delete m_process;
            }
            
            m_process = new void*(this);
            
            // Set working directory
            if (!config.workingDirectory.empty()) {
                m_process->setWorkingDirectory(config.workingDirectory);
            }
            
            // Set sanitized environment
            m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
            if (!config.allowedEnvironmentVars.empty()) {
                QProcessEnvironment env;
                QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
                for (const std::string& var : config.allowedEnvironmentVars) {
                    if (sysEnv.contains(var)) {
                        env.insert(var, sysEnv.value(var));
                    }
                }
                m_process->setProcessEnvironment(env);
            }
            
            // Start process
            logStructured("INFO", "Starting command", void*{
                {"command", command},
                {"args", void*::fromStringList(args)}
            });
            
            commandStarted(command);
            
            m_process->start(command, args);
            
            if (!m_process->waitForStarted(5000)) {
                logStructured("ERROR", "Failed to start process", void*{
                    {"command", command},
                    {"error", m_process->errorString()}
                });
                result.error = m_process->errorString();
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.errorCount++;
                errorOccurred(result.error);
                return result;
            }
            
            // Wait for finish with timeout
            bool finished = m_process->waitForFinished(config.maxExecutionTimeMs);
            
            if (!finished) {
                m_process->kill();
                result.timedOut = true;
                
                {
                    std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                    m_metrics.commandsTimedOut++;
                }
                
                logStructured("WARN", "Command timed out", void*{
                    {"command", command},
                    {"timeoutMs", config.maxExecutionTimeMs}
                });
                
                if (config.enableAuditLog) {
                    logAudit("command_timeout", void*{
                        {"command", command},
                        {"args", void*::fromStringList(args)},
                        {"timeoutMs", config.maxExecutionTimeMs}
                    });
                }
            }
            
            // Read output
            std::vector<uint8_t> stdoutData = m_process->readAllStandardOutput();
            std::vector<uint8_t> stderrData = m_process->readAllStandardError();
            
            std::string rawOutput = std::string::fromUtf8(stdoutData);
            std::string rawError = std::string::fromUtf8(stderrData);
            
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
                
                int64_t filteredBytes = (rawOutput.length() - result.output.length()) + 
                                       (rawError.length() - result.error.length());
                
                {
                    std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
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
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.commandsExecuted++;
        }
        
        logStructured("INFO", "Command completed", void*{
            {"command", command},
            {"exitCode", result.exitCode},
            {"executionTimeMs", result.executionTimeMs},
            {"timedOut", result.timedOut},
            {"outputSize", result.output.length()}
        });
        
        if (config.enableAuditLog) {
            logAudit("command_executed", void*{
                {"command", command},
                {"args", void*::fromStringList(args)},
                {"exitCode", result.exitCode},
                {"executionTimeMs", result.executionTimeMs},
                {"timedOut", result.timedOut}
            });
        }
        
        commandFinished(result);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Command execution failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Execution failed: %1")));
        result.error = e.what();
    }
    
    return result;
}

bool SandboxedTerminal::isCommandAllowed(const std::string& command) const
{
    std::string blockReason;
    return validateCommand(command, blockReason);
}

std::string SandboxedTerminal::sanitizeOutput(const std::string& output) const
{
    return filterSensitiveData(output);
}

bool SandboxedTerminal::isRunning() const
{
    std::lock_guard<std::mutex> locker(&m_processMutex);
    return m_process && m_process->state() != void*::NotRunning;
}

void SandboxedTerminal::terminate()
{
    std::lock_guard<std::mutex> locker(&m_processMutex);
    if (m_process && m_process->state() != void*::NotRunning) {
        m_process->terminate();
        logStructured("INFO", "Process terminated", void*{});
    }
}

void SandboxedTerminal::kill()
{
    std::lock_guard<std::mutex> locker(&m_processMutex);
    if (m_process && m_process->state() != void*::NotRunning) {
        m_process->kill();
        logStructured("INFO", "Process killed", void*{});
    }
}

SandboxedTerminal::Metrics SandboxedTerminal::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
}

void SandboxedTerminal::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
}

void SandboxedTerminal::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "SandboxedTerminal";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
}

void SandboxedTerminal::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "command_execution") {
        m_metrics.avgExecutionTimeMs = 
            (m_metrics.avgExecutionTimeMs * (m_metrics.commandsExecuted - 1) + duration.count()) 
            / m_metrics.commandsExecuted;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    }
}

void SandboxedTerminal::logAudit(const std::string& action, const void*& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }
    
    void* auditEntry;
    auditEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    std::fstream auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        void* doc(auditEntry);
        out << doc.toJson(void*::Compact) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", void*{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

bool SandboxedTerminal::validateCommand(const std::string& command, std::string& blockReason) const
{
    Config config;
    {
        std::lock_guard<std::mutex> locker(&m_configMutex);  // Fixed: take address of mutable member
        config = m_config;
    }
    
    std::string baseCommand = command.split(' ').first();
    std::filesystem::path cmdInfo(baseCommand);
    std::string cmdName = cmdInfo.fileName();
    
    // Check blacklist first
    if (config.commandBlacklist.contains(cmdName) || config.commandBlacklist.contains(baseCommand)) {
        blockReason = "Command is explicitly blacklisted";
        return false;
    }
    
    // Check for dangerous patterns
    std::vector<std::string> dangerousPatterns = {
        "rm -rf /",
        ":(){ :|:& };:",  // Fork bomb
        "dd if=/dev/random",
        "mkfs",
        "format"
    };
    
    for (const std::string& pattern : dangerousPatterns) {
        if (command.contains(pattern, //CaseInsensitive)) {
            blockReason = std::string("Dangerous pattern detected: %1");
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

std::vector<std::string> SandboxedTerminal::buildSanitizedEnvironment() const
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    std::vector<std::string> env;
    QProcessEnvironment sysEnv = QProcessEnvironment::systemEnvironment();
    
    if (config.allowedEnvironmentVars.empty()) {
        // Default safe environment variables
        std::vector<std::string> safeVars = {"PATH", "HOME", "USER", "TEMP", "TMP"};
        for (const std::string& var : safeVars) {
            if (sysEnv.contains(var)) {
                env.append(std::string("%1=%2")));
            }
        }
    } else {
        for (const std::string& var : config.allowedEnvironmentVars) {
            if (sysEnv.contains(var)) {
                env.append(std::string("%1=%2")));
            }
        }
    }
    
    return env;
}

bool SandboxedTerminal::enforceResourceLimits()
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableResourceLimits) {
        return true;
    }
    
    // Resource limits are platform-specific
    // On Windows, this would require job objects
    // On Linux/Unix, use setrlimit()
    // This is a simplified implementation
    
    logStructured("DEBUG", "Resource limits enforcement", void*{
        {"maxMemoryBytes", config.maxMemoryBytes},
        {"maxCpuPercent", config.maxCpuPercent}
    });
    
    return true;
}

std::string SandboxedTerminal::filterSensitiveData(const std::string& data) const
{
    std::string filtered = data;
    
    // Filter common sensitive patterns
    std::regex apiKeyPattern(R"(api[_-]?key[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", std::regex::CaseInsensitiveOption);
    std::regex passwordPattern(R"(password[\s=:]+['\"]?([^'\"\s]+)['\"]?)", std::regex::CaseInsensitiveOption);
    std::regex tokenPattern(R"(token[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", std::regex::CaseInsensitiveOption);
    std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    std::regex ipPattern(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");
    
    filtered.replace(apiKeyPattern, "api_key=[REDACTED]");
    filtered.replace(passwordPattern, "password=[REDACTED]");
    filtered.replace(tokenPattern, "token=[REDACTED]");
    filtered.replace(emailPattern, "[EMAIL_REDACTED]");
    filtered.replace(ipPattern, "[IP_REDACTED]");
    
    return filtered;
}



