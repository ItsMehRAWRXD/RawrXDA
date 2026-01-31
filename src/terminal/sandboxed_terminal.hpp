#pragma once


#include <chrono>
#include <memory>

/**
 * @class SandboxedTerminal
 * @brief Production-ready terminal with security isolation and command filtering
 * 
 * Features:
 * - Process isolation with resource limits
 * - Command filtering with whitelist/blacklist
 * - Output filtering and sanitization
 * - Security auditing with comprehensive logging
 * - Structured logging with metrics
 * - Configuration-driven security policies
 * - Timeout management
 * - Environment variable control
 */
class SandboxedTerminal : public void {

public:
    explicit SandboxedTerminal(void* parent = nullptr);
    ~SandboxedTerminal() override;

    // Configuration
    struct Config {
        std::vector<std::string> commandWhitelist;        // Allowed commands
        std::vector<std::string> commandBlacklist;        // Explicitly forbidden commands
        bool useWhitelistMode = true;        // If true, only whitelist allowed; if false, blacklist forbidden
        int maxExecutionTimeMs = 30000;      // Command timeout
        int maxOutputSize = 1048576;         // 1MB max output
        bool enableOutputFiltering = true;   // Filter sensitive data from output
        bool enableAuditLog = true;
        std::string auditLogPath;
        std::string workingDirectory;
        std::vector<std::string> allowedEnvironmentVars;  // Environment variables to preserve
        bool enableResourceLimits = true;
        qint64 maxMemoryBytes = 536870912;   // 512MB
        int maxCpuPercent = 80;
        bool enableMetrics = true;
    };

    void setConfig(const Config& config);
    Config getConfig() const;

    // Command execution
    struct CommandResult {
        std::string output;
        std::string error;
        int exitCode;
        bool timedOut;
        bool wasBlocked;
        std::string blockReason;
        qint64 executionTimeMs;
    };

    CommandResult executeCommand(const std::string& command, const std::vector<std::string>& args = std::vector<std::string>());
    bool isCommandAllowed(const std::string& command) const;
    std::string sanitizeOutput(const std::string& output) const;
    
    // Process management
    bool isRunning() const;
    void terminate();
    void kill();

    // Metrics
    struct Metrics {
        qint64 commandsExecuted = 0;
        qint64 commandsBlocked = 0;
        qint64 commandsTimedOut = 0;
        qint64 outputBytesFiltered = 0;
        qint64 securityViolations = 0;
        qint64 errorCount = 0;
        double avgExecutionTimeMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

    void commandStarted(const std::string& command);
    void commandFinished(const CommandResult& result);
    void commandBlocked(const std::string& command, const std::string& reason);
    void securityViolation(const std::string& violation);
    void errorOccurred(const std::string& error);
    void metricsUpdated(const Metrics& metrics);

private:
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;

    // Process
    QProcess* m_process;
    mutable std::mutex m_processMutex;

    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Helper methods
    void logStructured(const std::string& level, const std::string& message, const void*& context = void*());
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void logAudit(const std::string& action, const void*& details);
    bool validateCommand(const std::string& command, std::string& blockReason);
    bool validateCommand(const std::string& command, std::string& blockReason) const;  // const overload
    std::vector<std::string> buildSanitizedEnvironment() const;
    bool enforceResourceLimits();
    std::string filterSensitiveData(const std::string& data) const;
};

