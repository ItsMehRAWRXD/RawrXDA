/**
 * @file sandboxed_terminal.cpp
 * @brief Qt-free implementation of SandboxedTerminal
 *
 * Process execution via Win32 CreateProcessA + anonymous pipes.
 * All Qt types replaced with STL / Win32 equivalents.
 */
#include "sandboxed_terminal.hpp"
#include "json_types.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

// ---- internal helpers (anonymous namespace) ----
namespace {

struct ProcessInfo {
    HANDLE hProcess    = nullptr;
    HANDLE hThread     = nullptr;
    HANDLE hStdoutRead = nullptr;
    HANDLE hStderrRead = nullptr;
    bool   running     = false;
};

static std::string timePointToISO8601(std::chrono::system_clock::time_point tp)
{
    auto tt = std::chrono::system_clock::to_time_t(tp);
    struct tm tmBuf;
    gmtime_s(&tmBuf, &tt);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmBuf);
    return std::string(buf);
}

static bool vectorContains(const std::vector<std::string>& vec, const std::string& val)
{
    return std::find(vec.begin(), vec.end(), val) != vec.end();
}

static JsonValue vectorToJsonArray(const std::vector<std::string>& vec)
{
    JsonArray arr;
    for (const auto& s : vec) arr.push_back(JsonValue(s));
    return JsonValue(arr);
}

static std::string readAllFromPipe(HANDLE hPipe)
{
    std::string result;
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        result.append(buf, bytesRead);
    }
    return result;
}

static bool isProcessRunning(ProcessInfo* pi)
{
    if (!pi || !pi->hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi->hProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

static void cleanupProcessInfo(ProcessInfo* pi)
{
    if (!pi) return;
    if (pi->hStdoutRead) { CloseHandle(pi->hStdoutRead); pi->hStdoutRead = nullptr; }
    if (pi->hStderrRead) { CloseHandle(pi->hStderrRead); pi->hStderrRead = nullptr; }
    if (pi->hThread)     { CloseHandle(pi->hThread);     pi->hThread     = nullptr; }
    if (pi->hProcess)    { CloseHandle(pi->hProcess);    pi->hProcess    = nullptr; }
    pi->running = false;
}

} // anonymous namespace

// ===== Constructor / Destructor =====

SandboxedTerminal::SandboxedTerminal()
    : m_process(nullptr)
{
    logStructured("INFO", "SandboxedTerminal initializing",
                  JsonObject{{"component", std::string("SandboxedTerminal")}});
    logStructured("INFO", "SandboxedTerminal initialized successfully",
                  JsonObject{{"component", std::string("SandboxedTerminal")}});
}

SandboxedTerminal::~SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal shutting down",
                  JsonObject{{"component", std::string("SandboxedTerminal")}});

    auto* pi = static_cast<ProcessInfo*>(m_process);
    if (pi) {
        if (isProcessRunning(pi)) {
            TerminateProcess(pi->hProcess, 1);
            WaitForSingleObject(pi->hProcess, 5000);
        }
        cleanupProcessInfo(pi);
        delete pi;
        m_process = nullptr;
    }

    logStructured("INFO", "SandboxedTerminal shutdown complete",
                  JsonObject{{"component", std::string("SandboxedTerminal")}});
}

// ===== Configuration =====

void SandboxedTerminal::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", JsonObject{
        {"useWhitelistMode", config.useWhitelistMode},
        {"maxExecutionTimeMs", static_cast<int64_t>(config.maxExecutionTimeMs)},
        {"maxOutputSize", static_cast<int64_t>(config.maxOutputSize)},
        {"enableResourceLimits", config.enableResourceLimits}
    });
}

SandboxedTerminal::Config SandboxedTerminal::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

// ===== Command Execution =====

SandboxedTerminal::CommandResult SandboxedTerminal::executeCommand(
    const std::string& command, const std::vector<std::string>& args)
{
    auto startTime = std::chrono::steady_clock::now();
    CommandResult result;
    result.exitCode        = -1;
    result.timedOut        = false;
    result.wasBlocked      = false;
    result.executionTimeMs = 0;

    try {
        Config config;
        {
            std::lock_guard<std::mutex> cfgLock(m_configMutex);
            config = m_config;
        }

        // Validate command
        std::string blockReason;
        if (!validateCommand(command, blockReason)) {
            result.wasBlocked  = true;
            result.blockReason = blockReason;

            {
                std::lock_guard<std::mutex> mLock(m_metricsMutex);
                m_metrics.commandsBlocked++;
                m_metrics.securityViolations++;
            }

            logStructured("WARN", "Command blocked", JsonObject{
                {"command", command},
                {"reason", blockReason}
            });

            if (config.enableAuditLog) {
                logAudit("command_blocked", JsonObject{
                    {"command", command},
                    {"args", vectorToJsonArray(args)},
                    {"reason", blockReason}
                });
            }

            commandBlocked(command, blockReason);
            securityViolation("Blocked command: " + command + " - " + blockReason);
            return result;
        }

        {
            std::lock_guard<std::mutex> pLock(m_processMutex);

            auto* existingPi = static_cast<ProcessInfo*>(m_process);
            if (existingPi && isProcessRunning(existingPi)) {
                logStructured("ERROR", "Process already running", JsonObject{});
                result.error = "Another command is already executing";
                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.errorCount++;
                }
                errorOccurred(result.error);
                return result;
            }

            // Clean up old process if any
            if (existingPi) {
                cleanupProcessInfo(existingPi);
                delete existingPi;
                m_process = nullptr;
            }

            // Build command line string
            std::string cmdLine = command;
            for (const auto& arg : args) {
                cmdLine += " ";
                if (arg.find(' ') != std::string::npos) {
                    cmdLine += "\"" + arg + "\"";
                } else {
                    cmdLine += arg;
                }
            }

            // Create pipes for stdout and stderr
            SECURITY_ATTRIBUTES sa;
            sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle       = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
            HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;

            if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 1048576) ||
                !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 1048576)) {
                logStructured("ERROR", "Failed to create pipes",
                              JsonObject{{"command", command}});
                result.error = "Failed to create process pipes";
                if (hStdoutRead)  CloseHandle(hStdoutRead);
                if (hStdoutWrite) CloseHandle(hStdoutWrite);
                if (hStderrRead)  CloseHandle(hStderrRead);
                if (hStderrWrite) CloseHandle(hStderrWrite);
                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.errorCount++;
                }
                errorOccurred(result.error);
                return result;
            }

            // Ensure read handles are NOT inherited
            SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(si));
            si.cb          = sizeof(si);
            si.dwFlags     = STARTF_USESTDHANDLES;
            si.hStdOutput  = hStdoutWrite;
            si.hStdError   = hStderrWrite;
            si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION procInfo;
            ZeroMemory(&procInfo, sizeof(procInfo));

            const char* workDir = nullptr;
            if (!config.workingDirectory.empty()) {
                workDir = config.workingDirectory.c_str();
            }

            logStructured("INFO", "Starting command", JsonObject{
                {"command", command},
                {"args", vectorToJsonArray(args)}
            });

            commandStarted(command);

            // CreateProcessA needs a mutable command-line buffer
            std::vector<char> cmdLineBuf(cmdLine.begin(), cmdLine.end());
            cmdLineBuf.push_back('\0');

            BOOL created = CreateProcessA(
                nullptr,             // lpApplicationName
                cmdLineBuf.data(),   // lpCommandLine (mutable)
                nullptr,             // lpProcessAttributes
                nullptr,             // lpThreadAttributes
                TRUE,                // bInheritHandles
                0,                   // dwCreationFlags
                nullptr,             // lpEnvironment (inherit parent)
                workDir,             // lpCurrentDirectory
                &si,
                &procInfo
            );

            // Close write ends in parent — must happen before reading
            CloseHandle(hStdoutWrite);
            CloseHandle(hStderrWrite);

            if (!created) {
                DWORD err = GetLastError();
                std::string errMsg = "Failed to create process, error code: "
                                     + std::to_string(err);
                logStructured("ERROR", "Failed to start process", JsonObject{
                    {"command", command},
                    {"error", errMsg}
                });
                result.error = errMsg;
                CloseHandle(hStdoutRead);
                CloseHandle(hStderrRead);
                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.errorCount++;
                }
                errorOccurred(result.error);
                return result;
            }

            // Store process info
            auto* pi        = new ProcessInfo();
            pi->hProcess    = procInfo.hProcess;
            pi->hThread     = procInfo.hThread;
            pi->hStdoutRead = hStdoutRead;
            pi->hStderrRead = hStderrRead;
            pi->running     = true;
            m_process       = pi;

            // Wait for finish with timeout
            DWORD waitResult = WaitForSingleObject(
                pi->hProcess,
                static_cast<DWORD>(config.maxExecutionTimeMs));

            if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(pi->hProcess, 1);
                WaitForSingleObject(pi->hProcess, 1000);
                result.timedOut = true;

                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.commandsTimedOut++;
                }

                logStructured("WARN", "Command timed out", JsonObject{
                    {"command", command},
                    {"timeoutMs", static_cast<int64_t>(config.maxExecutionTimeMs)}
                });

                if (config.enableAuditLog) {
                    logAudit("command_timeout", JsonObject{
                        {"command", command},
                        {"args", vectorToJsonArray(args)},
                        {"timeoutMs", static_cast<int64_t>(config.maxExecutionTimeMs)}
                    });
                }
            }

            // Read output from pipes
            std::string rawOutput = readAllFromPipe(pi->hStdoutRead);
            std::string rawError  = readAllFromPipe(pi->hStderrRead);

            // Limit output size
            if (static_cast<int64_t>(rawOutput.length()) > config.maxOutputSize) {
                rawOutput = rawOutput.substr(0, static_cast<size_t>(config.maxOutputSize))
                            + "\n[OUTPUT TRUNCATED]";
            }
            if (static_cast<int64_t>(rawError.length()) > config.maxOutputSize) {
                rawError = rawError.substr(0, static_cast<size_t>(config.maxOutputSize))
                           + "\n[ERROR OUTPUT TRUNCATED]";
            }

            // Sanitize output
            if (config.enableOutputFiltering) {
                result.output = sanitizeOutput(rawOutput);
                result.error  = sanitizeOutput(rawError);

                int64_t filteredBytes =
                    static_cast<int64_t>(rawOutput.length() - result.output.length()) +
                    static_cast<int64_t>(rawError.length()  - result.error.length());

                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.outputBytesFiltered += filteredBytes;
                }
            } else {
                result.output = rawOutput;
                result.error  = rawError;
            }

            // Get exit code
            DWORD exitCode = 0;
            GetExitCodeProcess(pi->hProcess, &exitCode);
            result.exitCode = static_cast<int>(exitCode);

            // Clean up process handles
            pi->running = false;
            cleanupProcessInfo(pi);
            delete pi;
            m_process = nullptr;
        }

        auto endTime  = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            endTime - startTime);
        result.executionTimeMs = duration.count();

        {
            std::lock_guard<std::mutex> mLock(m_metricsMutex);
            m_metrics.commandsExecuted++;
        }

        recordLatency("command_execution", duration);

        logStructured("INFO", "Command completed", JsonObject{
            {"command", command},
            {"exitCode", static_cast<int64_t>(result.exitCode)},
            {"executionTimeMs", result.executionTimeMs},
            {"timedOut", result.timedOut},
            {"outputSize", static_cast<int64_t>(result.output.length())}
        });

        if (config.enableAuditLog) {
            logAudit("command_executed", JsonObject{
                {"command", command},
                {"args", vectorToJsonArray(args)},
                {"exitCode", static_cast<int64_t>(result.exitCode)},
                {"executionTimeMs", result.executionTimeMs},
                {"timedOut", result.timedOut}
            });
        }

        commandFinished(result);

    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> mLock(m_metricsMutex);
            m_metrics.errorCount++;
        }
        logStructured("ERROR", "Command execution failed",
                      JsonObject{{"error", std::string(e.what())}});
        errorOccurred(std::string("Execution failed: ") + e.what());
        result.error = e.what();
    }

    return result;
}

// ===== Query helpers =====

bool SandboxedTerminal::isCommandAllowed(const std::string& command) const
{
    std::string blockReason;
    return validateCommand(command, blockReason);
}

std::string SandboxedTerminal::sanitizeOutput(const std::string& output) const
{
    return filterSensitiveData(output);
}

// ===== Process management =====

bool SandboxedTerminal::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_processMutex);
    auto* pi = static_cast<ProcessInfo*>(m_process);
    return pi && isProcessRunning(pi);
}

void SandboxedTerminal::terminate()
{
    std::lock_guard<std::mutex> lock(m_processMutex);
    auto* pi = static_cast<ProcessInfo*>(m_process);
    if (pi && isProcessRunning(pi)) {
        TerminateProcess(pi->hProcess, 1);
        logStructured("INFO", "Process terminated", JsonObject{});
    }
}

void SandboxedTerminal::kill()
{
    std::lock_guard<std::mutex> lock(m_processMutex);
    auto* pi = static_cast<ProcessInfo*>(m_process);
    if (pi && isProcessRunning(pi)) {
        TerminateProcess(pi->hProcess, 9);
        logStructured("INFO", "Process killed", JsonObject{});
    }
}

// ===== Metrics =====

SandboxedTerminal::Metrics SandboxedTerminal::getMetrics() const
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    return m_metrics;
}

void SandboxedTerminal::resetMetrics()
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", JsonObject{});
}

// ===== Callbacks (event notifications — replace Qt signals) =====

void SandboxedTerminal::commandStarted(const std::string& command)
{
    fprintf(stderr, "[SandboxedTerminal] Command started: %s\n", command.c_str());
}

void SandboxedTerminal::commandFinished(const CommandResult& result)
{
    fprintf(stderr, "[SandboxedTerminal] Command finished: exitCode=%d timedOut=%s\n",
            result.exitCode, result.timedOut ? "true" : "false");
}

void SandboxedTerminal::commandBlocked(const std::string& command, const std::string& reason)
{
    fprintf(stderr, "[SandboxedTerminal] Command blocked: %s reason=%s\n",
            command.c_str(), reason.c_str());
}

void SandboxedTerminal::securityViolation(const std::string& violation)
{
    fprintf(stderr, "[SandboxedTerminal] Security violation: %s\n", violation.c_str());
}

void SandboxedTerminal::errorOccurred(const std::string& error)
{
    fprintf(stderr, "[SandboxedTerminal] Error: %s\n", error.c_str());
}

void SandboxedTerminal::metricsUpdated(const Metrics& metrics)
{
    fprintf(stderr,
            "[SandboxedTerminal] Metrics updated: executed=%lld blocked=%lld errors=%lld\n",
            static_cast<long long>(metrics.commandsExecuted),
            static_cast<long long>(metrics.commandsBlocked),
            static_cast<long long>(metrics.errorCount));
}

// ===== Private helpers =====

void SandboxedTerminal::logStructured(const std::string& level,
                                      const std::string& message,
                                      const JsonObject& context)
{
    JsonObject logEntry;
    logEntry["timestamp"] = timePointToISO8601(std::chrono::system_clock::now());
    logEntry["level"]     = level;
    logEntry["component"] = std::string("SandboxedTerminal");
    logEntry["message"]   = message;
    logEntry["context"]   = JsonValue(context);

    std::string json = JsonDoc::toJson(logEntry);
    fprintf(stderr, "[SandboxedTerminal] %s\n", json.c_str());
}

void SandboxedTerminal::recordLatency(const std::string& operation,
                                      const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> mLock(m_metricsMutex);

    if (operation == "command_execution" && m_metrics.commandsExecuted > 0) {
        m_metrics.avgExecutionTimeMs =
            (m_metrics.avgExecutionTimeMs
                * static_cast<double>(m_metrics.commandsExecuted - 1)
             + static_cast<double>(duration.count()))
            / static_cast<double>(m_metrics.commandsExecuted);
    }

    Config config;
    {
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        config = m_config;
    }

    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    }
}

void SandboxedTerminal::logAudit(const std::string& action, const JsonObject& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        config = m_config;
    }

    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }

    JsonObject auditEntry;
    auditEntry["timestamp"] = timePointToISO8601(std::chrono::system_clock::now());
    auditEntry["action"]    = action;
    auditEntry["details"]   = JsonValue(details);

    std::ofstream auditFile(config.auditLogPath, std::ios::out | std::ios::app);
    if (auditFile.is_open()) {
        auditFile << JsonDoc::toJson(auditEntry) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log",
                      JsonObject{{"path", config.auditLogPath}});
    }
}

bool SandboxedTerminal::validateCommand(const std::string& command,
                                        std::string& blockReason)
{
    return static_cast<const SandboxedTerminal*>(this)->validateCommand(
        command, blockReason);
}

bool SandboxedTerminal::validateCommand(const std::string& command,
                                        std::string& blockReason) const
{
    Config config;
    {
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        config = m_config;
    }

    // Extract base command (first token) and filename
    std::string baseCommand = command;
    auto spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        baseCommand = command.substr(0, spacePos);
    }
    std::filesystem::path cmdPath(baseCommand);
    std::string cmdName = cmdPath.filename().string();

    // Check blacklist first
    if (vectorContains(config.commandBlacklist, cmdName) ||
        vectorContains(config.commandBlacklist, baseCommand)) {
        blockReason = "Command is explicitly blacklisted";
        return false;
    }

    // Check for dangerous patterns
    std::vector<std::string> dangerousPatterns = {
        "rm -rf /",
        ":(){ :|:& };:",   // Fork bomb
        "dd if=/dev/random",
        "mkfs",
        "format"
    };

    for (const std::string& pattern : dangerousPatterns) {
        if (command.find(pattern) != std::string::npos) {
            blockReason = "Dangerous pattern detected: " + pattern;
            return false;
        }
    }

    // Whitelist mode
    if (config.useWhitelistMode) {
        bool allowed = vectorContains(config.commandWhitelist, cmdName) ||
                       vectorContains(config.commandWhitelist, baseCommand);
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
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        config = m_config;
    }

    std::vector<std::string> env;

    auto getEnvVar = [](const std::string& name) -> std::string {
        char buf[32768];
        DWORD len = GetEnvironmentVariableA(name.c_str(), buf, sizeof(buf));
        if (len > 0 && len < sizeof(buf)) return std::string(buf, len);
        return "";
    };

    auto addIfExists = [&](const std::string& var) {
        std::string val = getEnvVar(var);
        if (!val.empty()) {
            env.push_back(var + "=" + val);
        }
    };

    if (config.allowedEnvironmentVars.empty()) {
        // Default safe environment variables
        std::vector<std::string> safeVars = {
            "PATH", "HOME", "USER", "TEMP", "TMP", "USERPROFILE", "SystemRoot"
        };
        for (const auto& var : safeVars) addIfExists(var);
    } else {
        for (const auto& var : config.allowedEnvironmentVars) addIfExists(var);
    }

    return env;
}

bool SandboxedTerminal::enforceResourceLimits()
{
    Config config;
    {
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        config = m_config;
    }

    if (!config.enableResourceLimits) {
        return true;
    }

    // Resource limits are platform-specific.
    // On Windows use Job Objects (CreateJobObject / AssignProcessToJobObject).
    // On POSIX use setrlimit(). Simplified stub here.
    logStructured("DEBUG", "Resource limits enforcement", JsonObject{
        {"maxMemoryBytes", config.maxMemoryBytes},
        {"maxCpuPercent", static_cast<int64_t>(config.maxCpuPercent)}
    });

    return true;
}

std::string SandboxedTerminal::filterSensitiveData(const std::string& data) const
{
    std::string filtered = data;

    // Filter common sensitive patterns
    std::regex apiKeyPattern(
        R"(api[_-]?key[\s=:]+['\"]?([a-zA-Z0-9_\-]{20,})['\"]?)",
        std::regex::icase);
    std::regex passwordPattern(
        R"(password[\s=:]+['\"]?([^'\"\s]+)['\"]?)",
        std::regex::icase);
    std::regex tokenPattern(
        R"(token[\s=:]+['\"]?([a-zA-Z0-9_\-]{20,})['\"]?)",
        std::regex::icase);
    std::regex emailPattern(
        R"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})");
    std::regex ipPattern(
        R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");

    filtered = std::regex_replace(filtered, apiKeyPattern,   "api_key=[REDACTED]");
    filtered = std::regex_replace(filtered, passwordPattern, "password=[REDACTED]");
    filtered = std::regex_replace(filtered, tokenPattern,    "token=[REDACTED]");
    filtered = std::regex_replace(filtered, emailPattern,    "[EMAIL_REDACTED]");
    filtered = std::regex_replace(filtered, ipPattern,       "[IP_REDACTED]");

    return filtered;
}
