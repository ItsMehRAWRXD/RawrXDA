#pragma once
// sandboxed_terminal.hpp — Qt-free terminal sandbox (Win32 + STL)
// Purged: QObject, QProcess, QMutex, QVector, QJsonObject, QRegularExpression,
//         Q_OBJECT, signals/slots
// Replaced with: Win32 CreateProcess/Job, std::mutex, std::function callbacks
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "json_types.hpp"
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <regex>
#include <algorithm>

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
class SandboxedTerminal {
public:
    explicit SandboxedTerminal();
    ~SandboxedTerminal();

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
        int64_t maxMemoryBytes = 536870912;   // 512MB
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
        int64_t executionTimeMs;
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
        int64_t commandsExecuted = 0;
        int64_t commandsBlocked = 0;
        int64_t commandsTimedOut = 0;
        int64_t outputBytesFiltered = 0;
        int64_t securityViolations = 0;
        int64_t errorCount = 0;
        double avgExecutionTimeMs = 0.0;
    };

    Metrics getMetrics() const;
    void resetMetrics();

    // ---------- Callbacks (replace Qt signals) ----------
    using StringCb     = std::function<void(const std::string&)>;
    using StringPairCb = std::function<void(const std::string&, const std::string&)>;
    using ResultCb     = std::function<void(const CommandResult&)>;
    using MetricsCb    = std::function<void(const Metrics&)>;

    StringCb     onCommandStarted    = nullptr;
    ResultCb     onCommandFinished   = nullptr;
    StringPairCb onCommandBlocked    = nullptr;
    StringCb     onSecurityViolation = nullptr;
    StringCb     onErrorOccurred     = nullptr;
    MetricsCb    onMetricsUpdated    = nullptr;

private:
    // Configuration
    Config m_config;
    mutable std::mutex m_configMutex;

    // Process (Win32 HANDLE)
    HANDLE m_processHandle = nullptr;
    mutable std::mutex m_processMutex;

    // Metrics
    Metrics m_metrics;
    mutable std::mutex m_metricsMutex;

    // Helper methods
    void logStructured(const std::string& level, const std::string& message, const JsonObject& context = JsonObject());
    void recordLatency(const std::string& operation, const std::chrono::milliseconds& duration);
    void logAudit(const std::string& action, const JsonObject& details);
    bool validateCommand(const std::string& command, std::string& blockReason);
    bool validateCommand(const std::string& command, std::string& blockReason) const;  // const overload
    std::vector<std::string> buildSanitizedEnvironment() const;
    bool enforceResourceLimits();
    std::string filterSensitiveData(const std::string& data) const;
};
