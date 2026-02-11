#include "sandboxed_terminal.hpp"
#include "json_types.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// --- Helpers ---
static std::string time_point_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
    return std::string(buf);
}

static std::string vec_to_string(const std::vector<std::string>& v) {
    std::string result = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i > 0) result += ", ";
        result += "\"" + v[i] + "\"";
    }
    result += "]";
    return result;
}

// --- Construction / Destruction ---

SandboxedTerminal::SandboxedTerminal()
    : m_processHandle(nullptr)
{
    logStructured("INFO", "SandboxedTerminal initializing", JsonObject{{"component", "SandboxedTerminal"}});
    logStructured("INFO", "SandboxedTerminal initialized successfully", JsonObject{{"component", "SandboxedTerminal"}});
}

SandboxedTerminal::~SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal shutting down", JsonObject{{"component", "SandboxedTerminal"}});

#ifdef _WIN32
    if (m_processHandle) {
        HANDLE h = static_cast<HANDLE>(m_processHandle);
        DWORD exitCode = 0;
        if (GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(h, 1);
            WaitForSingleObject(h, 5000);
        }
        CloseHandle(h);
        m_processHandle = nullptr;
    }
#endif

    logStructured("INFO", "SandboxedTerminal shutdown complete", JsonObject{{"component", "SandboxedTerminal"}});
}

// --- Configuration ---

void SandboxedTerminal::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", JsonObject{
        {"useWhitelistMode", config.useWhitelistMode},
        {"maxExecutionTimeMs", config.maxExecutionTimeMs},
        {"maxOutputSize", config.maxOutputSize},
        {"enableResourceLimits", config.enableResourceLimits}
    });
}

SandboxedTerminal::Config SandboxedTerminal::getConfig() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    return m_config;
}

// --- Command Execution ---

SandboxedTerminal::CommandResult SandboxedTerminal::executeCommand(
    const std::string& command, const std::vector<std::string>& args)
{
    auto startTime = std::chrono::steady_clock::now();
    CommandResult result;
    result.exitCode = -1;
    result.timedOut = false;
    result.wasBlocked = false;

    try {
        Config config;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            config = m_config;
        }

        // Validate command
        std::string blockReason;
        if (!validateCommand(command, blockReason)) {
            result.wasBlocked = true;
            result.blockReason = blockReason;

            {
                std::lock_guard<std::mutex> lock(m_metricsMutex);
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
                    {"reason", blockReason}
                });
            }

            if (m_blockCb) m_blockCb(m_blockCtx, command.c_str(), blockReason.c_str());
            if (m_secCb) m_secCb(m_secCtx, (std::string("Blocked command: ") + command + " - " + blockReason).c_str());
            return result;
        }

#ifdef _WIN32
        {
            std::lock_guard<std::mutex> lock(m_processMutex);

            // Check if a process is already running
            if (m_processHandle) {
                HANDLE h = static_cast<HANDLE>(m_processHandle);
                DWORD exitCode = 0;
                if (GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE) {
                    logStructured("ERROR", "Process already running", JsonObject{});
                    result.error = "Another command is already executing";
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.errorCount++;
                    if (m_errCb) m_errCb(m_errCtx, result.error.c_str());
                    return result;
                }
                CloseHandle(h);
                m_processHandle = nullptr;
            }

            // Build command line
            std::string cmdLine = command;
            for (const auto& arg : args) {
                cmdLine += " " + arg;
            }

            // Set up pipes for stdout/stderr
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
            HANDLE hStdErrRead = nullptr, hStdErrWrite = nullptr;
            CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
            CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdErrWrite;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            ZeroMemory(&pi, sizeof(pi));

            logStructured("INFO", "Starting command", JsonObject{
                {"command", command}
            });

            if (m_startCb) m_startCb(m_startCtx, command.c_str());

            // Create process
            std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
            cmdBuf.push_back('\0');

            BOOL created = CreateProcessA(
                nullptr,
                cmdBuf.data(),
                nullptr, nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                config.workingDirectory.empty() ? nullptr : config.workingDirectory.c_str(),
                &si,
                &pi
            );

            // Close write ends of pipes
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdErrWrite);

            if (!created) {
                DWORD err = GetLastError();
                logStructured("ERROR", "Failed to start process", JsonObject{
                    {"command", command},
                    {"error", std::to_string(err)}
                });
                result.error = "CreateProcess failed with error " + std::to_string(err);
                CloseHandle(hStdOutRead);
                CloseHandle(hStdErrRead);
                std::lock_guard<std::mutex> mLock(m_metricsMutex);
                m_metrics.errorCount++;
                if (m_errCb) m_errCb(m_errCtx, result.error.c_str());
                return result;
            }

            m_processHandle = pi.hProcess;

            // Wait for finish with timeout
            DWORD waitResult = WaitForSingleObject(pi.hProcess,
                static_cast<DWORD>(config.maxExecutionTimeMs));

            if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(pi.hProcess, 1);
                result.timedOut = true;

                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.commandsTimedOut++;
                }

                logStructured("WARN", "Command timed out", JsonObject{
                    {"command", command},
                    {"timeoutMs", config.maxExecutionTimeMs}
                });

                if (config.enableAuditLog) {
                    logAudit("command_timeout", JsonObject{
                        {"command", command},
                        {"timeoutMs", config.maxExecutionTimeMs}
                    });
                }
            }

            // Read output from pipes
            auto readPipe = [](HANDLE pipe, int maxSize) -> std::string {
                std::string output;
                char buf[4096];
                DWORD bytesRead = 0;
                while (ReadFile(pipe, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
                    output.append(buf, bytesRead);
                    if (static_cast<int>(output.size()) > maxSize) {
                        output.resize(static_cast<size_t>(maxSize));
                        output += "\n[OUTPUT TRUNCATED]";
                        break;
                    }
                }
                return output;
            };

            std::string rawOutput = readPipe(hStdOutRead, config.maxOutputSize);
            std::string rawError = readPipe(hStdErrRead, config.maxOutputSize);

            CloseHandle(hStdOutRead);
            CloseHandle(hStdErrRead);

            // Sanitize output
            if (config.enableOutputFiltering) {
                result.output = sanitizeOutput(rawOutput);
                result.error = sanitizeOutput(rawError);

                int64_t filteredBytes = static_cast<int64_t>(rawOutput.size() - result.output.size())
                                      + static_cast<int64_t>(rawError.size() - result.error.size());

                {
                    std::lock_guard<std::mutex> mLock(m_metricsMutex);
                    m_metrics.outputBytesFiltered += filteredBytes;
                }
            } else {
                result.output = rawOutput;
                result.error = rawError;
            }

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            result.exitCode = static_cast<int>(exitCode);

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            m_processHandle = nullptr;
        }
#else
        // POSIX fallback (stub)
        result.error = "POSIX process execution not yet implemented";
        result.exitCode = -1;
#endif

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        result.executionTimeMs = duration.count();
        recordLatency("command_execution", duration);

        {
            std::lock_guard<std::mutex> lock(m_metricsMutex);
            m_metrics.commandsExecuted++;
        }

        logStructured("INFO", "Command completed", JsonObject{
            {"command", command},
            {"exitCode", static_cast<int64_t>(result.exitCode)},
            {"executionTimeMs", result.executionTimeMs},
            {"timedOut", result.timedOut},
            {"outputSize", static_cast<int64_t>(result.output.size())}
        });

        if (config.enableAuditLog) {
            logAudit("command_executed", JsonObject{
                {"command", command},
                {"exitCode", static_cast<int64_t>(result.exitCode)},
                {"executionTimeMs", result.executionTimeMs},
                {"timedOut", result.timedOut}
            });
        }

        if (m_finishCb) m_finishCb(m_finishCtx, &result);

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Command execution failed", JsonObject{{"error", e.what()}});
        if (m_errCb) m_errCb(m_errCtx, (std::string("Execution failed: ") + e.what()).c_str());
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
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_processMutex);
    if (!m_processHandle) return false;
    DWORD exitCode = 0;
    GetExitCodeProcess(static_cast<HANDLE>(m_processHandle), &exitCode);
    return exitCode == STILL_ACTIVE;
#else
    return false;
#endif
}

void SandboxedTerminal::terminate()
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_processMutex);
    if (m_processHandle) {
        TerminateProcess(static_cast<HANDLE>(m_processHandle), 1);
        logStructured("INFO", "Process terminated", JsonObject{});
    }
#endif
}

void SandboxedTerminal::kill()
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(m_processMutex);
    if (m_processHandle) {
        TerminateProcess(static_cast<HANDLE>(m_processHandle), 9);
        logStructured("INFO", "Process killed", JsonObject{});
    }
#endif
}

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

// --- Private helpers ---

void SandboxedTerminal::logStructured(const std::string& level, const std::string& message,
                                       const JsonObject& context)
{
    JsonObject logEntry;
    logEntry["timestamp"] = time_point_iso8601();
    logEntry["level"] = level;
    logEntry["component"] = "SandboxedTerminal";
    logEntry["message"] = message;
    logEntry["context"] = JsonValue(context);

    std::string json = JsonDoc::toJson(logEntry);
    fprintf(stderr, "%s\n", json.c_str());
}

void SandboxedTerminal::recordLatency(const std::string& operation, std::chrono::milliseconds duration)
{
    std::lock_guard<std::mutex> lock(m_metricsMutex);

    if (operation == "command_execution" && m_metrics.commandsExecuted > 0) {
        m_metrics.avgExecutionTimeMs =
            (m_metrics.avgExecutionTimeMs * (m_metrics.commandsExecuted - 1) + static_cast<double>(duration.count()))
            / m_metrics.commandsExecuted;
    }

    Config config;
    {
        std::lock_guard<std::mutex> cLock(m_configMutex);
        config = m_config;
    }

    if (config.enableMetrics && m_metCb) {
        m_metCb(m_metCtx, &m_metrics);
    }
}

void SandboxedTerminal::logAudit(const std::string& action, const JsonObject& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        config = m_config;
    }

    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }

    JsonObject auditEntry;
    auditEntry["timestamp"] = time_point_iso8601();
    auditEntry["action"] = action;
    auditEntry["details"] = JsonValue(details);

    std::ofstream auditFile(config.auditLogPath, std::ios::out | std::ios::app);
    if (auditFile.is_open()) {
        std::string json = JsonDoc::toJson(auditEntry);
        auditFile << json << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", JsonObject{
            {"path", config.auditLogPath}
        });
    }
}

bool SandboxedTerminal::validateCommand(const std::string& command, std::string& blockReason) const
{
    Config config;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        config = m_config;
    }

    // Extract base command name
    std::string baseCommand = command;
    auto spacePos = command.find(' ');
    if (spacePos != std::string::npos) {
        baseCommand = command.substr(0, spacePos);
    }

    std::filesystem::path cmdPath(baseCommand);
    std::string cmdName = cmdPath.filename().string();

    // Check blacklist first
    auto contains = [](const std::vector<std::string>& vec, const std::string& val) {
        for (const auto& v : vec) {
            if (v == val) return true;
        }
        return false;
    };

    if (contains(config.commandBlacklist, cmdName) || contains(config.commandBlacklist, baseCommand)) {
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
        if (command.find(pattern) != std::string::npos) {
            blockReason = "Dangerous pattern detected: " + pattern;
            return false;
        }
    }

    // Whitelist mode
    if (config.useWhitelistMode) {
        bool allowed = contains(config.commandWhitelist, cmdName) ||
                       contains(config.commandWhitelist, baseCommand);
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
        std::lock_guard<std::mutex> lock(m_configMutex);
        config = m_config;
    }

    std::vector<std::string> env;

    auto getEnv = [](const std::string& name) -> std::string {
        const char* val = std::getenv(name.c_str());
        return val ? std::string(val) : std::string();
    };

    std::vector<std::string> vars = config.allowedEnvironmentVars;
    if (vars.empty()) {
        // Default safe environment variables
        vars = {"PATH", "HOME", "USER", "TEMP", "TMP", "USERPROFILE", "SystemRoot"};
    }

    for (const std::string& var : vars) {
        std::string val = getEnv(var);
        if (!val.empty()) {
            env.push_back(var + "=" + val);
        }
    }

    return env;
}

bool SandboxedTerminal::enforceResourceLimits()
{
    Config config;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        config = m_config;
    }

    if (!config.enableResourceLimits) {
        return true;
    }

    // Resource limits are platform-specific
    // On Windows, this would require job objects
    // On Linux/Unix, use setrlimit()
    logStructured("DEBUG", "Resource limits enforcement", JsonObject{
        {"maxMemoryBytes", config.maxMemoryBytes},
        {"maxCpuPercent", config.maxCpuPercent}
    });

    return true;
}

std::string SandboxedTerminal::filterSensitiveData(const std::string& data) const
{
    std::string filtered = data;

    // Filter common sensitive patterns
    std::regex apiKeyPattern(R"(api[_-]?key[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", std::regex::icase);
    std::regex passwordPattern(R"(password[\s=:]+['\"]?([^'\"\s]+)['\"]?)", std::regex::icase);
    std::regex tokenPattern(R"(token[\s=:]+['\"]?([a-zA-Z0-9_-]{20,})['\"]?)", std::regex::icase);
    std::regex emailPattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    std::regex ipPattern(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");

    filtered = std::regex_replace(filtered, apiKeyPattern, "api_key=[REDACTED]");
    filtered = std::regex_replace(filtered, passwordPattern, "password=[REDACTED]");
    filtered = std::regex_replace(filtered, tokenPattern, "token=[REDACTED]");
    filtered = std::regex_replace(filtered, emailPattern, "[EMAIL_REDACTED]");
    filtered = std::regex_replace(filtered, ipPattern, "[IP_REDACTED]");

    return filtered;
}
