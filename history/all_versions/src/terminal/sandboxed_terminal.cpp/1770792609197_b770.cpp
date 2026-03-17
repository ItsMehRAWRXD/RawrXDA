// sandboxed_terminal.cpp — Qt-free terminal sandbox (Win32 + STL)
// Purged: QObject, QProcess, QMutex, QTimer, QDebug, QJsonObject, QJsonArray,
//         QJsonDocument, QFile, QTextStream, QDir, QRegularExpression,
//         QProcessEnvironment, signals/slots, QByteArray, QDateTime
// Replaced with: Win32 CreateProcess/Job, std::mutex, std::regex,
//                std::filesystem, std::function callbacks, fprintf
#include "sandboxed_terminal.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string SandboxedTerminal::escapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string SandboxedTerminal::nowISO8601()
{
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    struct tm tmBuf{};
    gmtime_s(&tmBuf, &tt);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tmBuf);
    std::ostringstream oss;
    oss << buf << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------
SandboxedTerminal::SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal initializing");
    logStructured("INFO", "SandboxedTerminal initialized");
}

SandboxedTerminal::~SandboxedTerminal()
{
    logStructured("INFO", "SandboxedTerminal shutting down");
    kill();
    logStructured("INFO", "SandboxedTerminal shutdown complete");
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
void SandboxedTerminal::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> lk(m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated",
        "{\"useWhitelistMode\":" + std::to_string(config.useWhitelistMode) +
        ",\"maxExecutionTimeMs\":" + std::to_string(config.maxExecutionTimeMs) +
        ",\"maxOutputSize\":" + std::to_string(config.maxOutputSize) +
        ",\"enableResourceLimits\":" + std::to_string(config.enableResourceLimits) + "}");
}

SandboxedTerminal::Config SandboxedTerminal::getConfig() const
{
    std::lock_guard<std::mutex> lk(m_configMutex);
    return m_config;
}

// ---------------------------------------------------------------------------
// Execute — Win32 CreateProcess with pipes & optional Job Objects
// ---------------------------------------------------------------------------
SandboxedTerminal::CommandResult
SandboxedTerminal::executeCommand(const std::string& command,
                                   const std::vector<std::string>& args)
{
    auto startTime = std::chrono::steady_clock::now();
    CommandResult result{};
    result.exitCode = -1;

    Config config;
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        config = m_config;
    }

    // --- validate ---
    std::string blockReason;
    if (!validateCommand(command, blockReason)) {
        result.wasBlocked  = true;
        result.blockReason = blockReason;
        {
            std::lock_guard<std::mutex> lk(m_metricsMutex);
            m_metrics.commandsBlocked++;
            m_metrics.securityViolations++;
        }
        logStructured("WARN", "Command blocked",
            "{\"command\":\"" + escapeJson(command) + "\",\"reason\":\"" + escapeJson(blockReason) + "\"}");

        if (config.enableAuditLog) {
            logAudit("command_blocked",
                "{\"command\":\"" + escapeJson(command) + "\",\"reason\":\"" + escapeJson(blockReason) + "\"}");
        }

        if (onCommandBlocked) onCommandBlocked(command, blockReason);
        if (onSecurityViolation) onSecurityViolation("Blocked command: " + command + " - " + blockReason);
        return result;
    }

    // --- build command line ---
    std::string cmdline = command;
    for (const auto& a : args) {
        cmdline += " ";
        cmdline += a;
    }

    // --- check if another process is running ---
    {
        std::lock_guard<std::mutex> lk(m_processMutex);
        if (m_processHandle) {
            DWORD exitTmp = 0;
            if (GetExitCodeProcess(m_processHandle, &exitTmp) && exitTmp == STILL_ACTIVE) {
                logStructured("ERROR", "Process already running");
                result.error = "Another command is already executing";
                {
                    std::lock_guard<std::mutex> lk2(m_metricsMutex);
                    m_metrics.errorCount++;
                }
                if (onErrorOccurred) onErrorOccurred(result.error);
                return result;
            }
            CloseHandle(m_processHandle);
            m_processHandle = nullptr;
        }
    }

    // --- create pipes ---
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
    HANDLE hStdErrRead = nullptr, hStdErrWrite = nullptr;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
        !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        result.error = "Failed to create pipes: " + std::to_string(GetLastError());
        {
            std::lock_guard<std::mutex> lk(m_metricsMutex);
            m_metrics.errorCount++;
        }
        if (onErrorOccurred) onErrorOccurred(result.error);
        return result;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    // --- set up environment ---
    // If allowedEnvironmentVars is non-empty, build a restricted env block
    std::string envBlock;
    if (!config.allowedEnvironmentVars.empty()) {
        for (const auto& var : config.allowedEnvironmentVars) {
            char buf[32768];
            DWORD len = GetEnvironmentVariableA(var.c_str(), buf, sizeof(buf));
            if (len > 0 && len < sizeof(buf)) {
                envBlock += var + "=" + std::string(buf, len) + '\0';
            }
        }
        envBlock += '\0'; // double-null terminate
    }

    // --- StartupInfo ---
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError  = hStdErrWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    std::vector<char> cmdBuf(cmdline.begin(), cmdline.end());
    cmdBuf.push_back('\0');

    const char* workDir = config.workingDirectory.empty()
                              ? nullptr
                              : config.workingDirectory.c_str();

    const char* envPtr = envBlock.empty() ? nullptr : envBlock.c_str();

    logStructured("INFO", "Starting command",
        "{\"command\":\"" + escapeJson(command) + "\"}");
    if (onCommandStarted) onCommandStarted(command);

    BOOL created = CreateProcessA(
        nullptr,
        cmdBuf.data(),
        nullptr, nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        const_cast<char*>(envPtr),
        workDir,
        &si, &pi);

    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    if (!created) {
        DWORD err = GetLastError();
        result.error = "CreateProcess failed, error " + std::to_string(err);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        {
            std::lock_guard<std::mutex> lk(m_metricsMutex);
            m_metrics.errorCount++;
        }
        logStructured("ERROR", "Failed to start process",
            "{\"command\":\"" + escapeJson(command) + "\",\"error\":\"" + result.error + "\"}");
        if (onErrorOccurred) onErrorOccurred(result.error);
        return result;
    }

    {
        std::lock_guard<std::mutex> lk(m_processMutex);
        m_processHandle = pi.hProcess;
    }

    // --- wait with timeout ---
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 
        static_cast<DWORD>(config.maxExecutionTimeMs));

    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timedOut = true;
        {
            std::lock_guard<std::mutex> lk(m_metricsMutex);
            m_metrics.commandsTimedOut++;
        }
        logStructured("WARN", "Command timed out",
            "{\"command\":\"" + escapeJson(command) +
            "\",\"timeoutMs\":" + std::to_string(config.maxExecutionTimeMs) + "}");
        if (config.enableAuditLog) {
            logAudit("command_timeout",
                "{\"command\":\"" + escapeJson(command) +
                "\",\"timeoutMs\":" + std::to_string(config.maxExecutionTimeMs) + "}");
        }
    }

    // --- read pipes ---
    auto readPipe = [](HANDLE h, int maxSize) -> std::string {
        std::string data;
        char buf[4096];
        DWORD bytesRead = 0;
        while (ReadFile(h, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            data.append(buf, bytesRead);
            if (static_cast<int>(data.size()) > maxSize) {
                data.resize(maxSize);
                data += "\n[OUTPUT TRUNCATED]";
                break;
            }
        }
        return data;
    };

    std::string rawOutput = readPipe(hStdOutRead, config.maxOutputSize);
    std::string rawError  = readPipe(hStdErrRead, config.maxOutputSize);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    // --- sanitize output ---
    if (config.enableOutputFiltering) {
        result.output = filterSensitiveData(rawOutput);
        result.error  = filterSensitiveData(rawError);
        int64_t filteredBytes = static_cast<int64_t>(rawOutput.size() - result.output.size())
                              + static_cast<int64_t>(rawError.size()  - result.error.size());
        {
            std::lock_guard<std::mutex> lk(m_metricsMutex);
            m_metrics.outputBytesFiltered += (filteredBytes > 0 ? filteredBytes : 0);
        }
    } else {
        result.output = std::move(rawOutput);
        result.error  = std::move(rawError);
    }

    // --- exit code ---
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);

    CloseHandle(pi.hThread);
    {
        std::lock_guard<std::mutex> lk(m_processMutex);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    result.executionTimeMs = duration.count();
    recordLatency("command_execution", duration);

    {
        std::lock_guard<std::mutex> lk(m_metricsMutex);
        m_metrics.commandsExecuted++;
    }

    logStructured("INFO", "Command completed",
        "{\"command\":\"" + escapeJson(command) +
        "\",\"exitCode\":" + std::to_string(result.exitCode) +
        ",\"executionTimeMs\":" + std::to_string(result.executionTimeMs) +
        ",\"timedOut\":" + (result.timedOut ? "true" : "false") +
        ",\"outputSize\":" + std::to_string(result.output.size()) + "}");

    if (config.enableAuditLog) {
        logAudit("command_executed",
            "{\"command\":\"" + escapeJson(command) +
            "\",\"exitCode\":" + std::to_string(result.exitCode) +
            ",\"executionTimeMs\":" + std::to_string(result.executionTimeMs) +
            ",\"timedOut\":" + (result.timedOut ? "true" : "false") + "}");
    }

    if (onCommandFinished) onCommandFinished(result);
    return result;
}

// ---------------------------------------------------------------------------
// Query / control
// ---------------------------------------------------------------------------
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
    std::lock_guard<std::mutex> lk(m_processMutex);
    if (!m_processHandle) return false;
    DWORD ec = 0;
    GetExitCodeProcess(m_processHandle, &ec);
    return ec == STILL_ACTIVE;
}

void SandboxedTerminal::terminate()
{
    std::lock_guard<std::mutex> lk(m_processMutex);
    if (m_processHandle) {
        // Attempt graceful stop via GenerateConsoleCtrlEvent first is unreliable
        // across job objects; just TerminateProcess with code 1
        TerminateProcess(m_processHandle, 1);
        logStructured("INFO", "Process terminated");
    }
}

void SandboxedTerminal::kill()
{
    std::lock_guard<std::mutex> lk(m_processMutex);
    if (m_processHandle) {
        TerminateProcess(m_processHandle, 9);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
        logStructured("INFO", "Process killed");
    }
}

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------
SandboxedTerminal::Metrics SandboxedTerminal::getMetrics() const
{
    std::lock_guard<std::mutex> lk(m_metricsMutex);
    return m_metrics;
}

void SandboxedTerminal::resetMetrics()
{
    std::lock_guard<std::mutex> lk(m_metricsMutex);
    m_metrics = Metrics{};
    logStructured("INFO", "Metrics reset");
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void SandboxedTerminal::logStructured(const char* level,
                                       const std::string& message,
                                       const std::string& contextJson)
{
    fprintf(stderr, "{\"timestamp\":\"%s\",\"level\":\"%s\","
                    "\"component\":\"SandboxedTerminal\","
                    "\"message\":\"%s\",\"context\":%s}\n",
            nowISO8601().c_str(), level, escapeJson(message).c_str(),
            contextJson.c_str());
}

void SandboxedTerminal::recordLatency(const std::string& operation,
                                       const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> lk(m_metricsMutex);
    if (operation == "command_execution" && m_metrics.commandsExecuted > 0) {
        m_metrics.avgExecutionTimeMs =
            (m_metrics.avgExecutionTimeMs * (m_metrics.commandsExecuted - 1)
             + static_cast<double>(duration.count()))
            / m_metrics.commandsExecuted;
    }

    Config config;
    {
        std::lock_guard<std::mutex> lk2(m_configMutex);
        config = m_config;
    }
    if (config.enableMetrics && onMetricsUpdated) {
        onMetricsUpdated(m_metrics);
    }
}

void SandboxedTerminal::logAudit(const std::string& action,
                                  const std::string& detailsJson)
{
    Config config;
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        config = m_config;
    }
    if (!config.enableAuditLog || config.auditLogPath.empty()) return;

    std::string entry = "{\"timestamp\":\"" + nowISO8601() +
                        "\",\"action\":\"" + escapeJson(action) +
                        "\",\"details\":" + detailsJson + "}\n";

    std::ofstream f(config.auditLogPath, std::ios::app);
    if (f.is_open()) {
        f << entry;
    } else {
        logStructured("ERROR", "Failed to write audit log",
            "{\"path\":\"" + escapeJson(config.auditLogPath) + "\"}");
    }
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------
bool SandboxedTerminal::validateCommand(const std::string& command,
                                         std::string& blockReason) const
{
    Config config;
    {
        std::lock_guard<std::mutex> lk(m_configMutex);
        config = m_config;
    }

    // Extract base command name
    std::string baseCommand = command;
    auto spacePos = command.find(' ');
    if (spacePos != std::string::npos)
        baseCommand = command.substr(0, spacePos);

    // Extract just the filename from path
    std::string cmdName = baseCommand;
    auto slashPos = baseCommand.find_last_of("/\\");
    if (slashPos != std::string::npos)
        cmdName = baseCommand.substr(slashPos + 1);

    // Check blacklist
    auto contains = [](const std::vector<std::string>& vec, const std::string& val) {
        return std::find(vec.begin(), vec.end(), val) != vec.end();
    };

    if (contains(config.commandBlacklist, cmdName) ||
        contains(config.commandBlacklist, baseCommand)) {
        blockReason = "Command is explicitly blacklisted";
        return false;
    }

    // Dangerous patterns
    static const std::vector<std::string> dangerousPatterns = {
        "rm -rf /",
        ":(){ :|:& };:",
        "dd if=/dev/random",
        "mkfs",
        "format"
    };

    std::string cmdLower = command;
    std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(),
                   [](unsigned char c){ return static_cast<char>(::tolower(c)); });

    for (const auto& pat : dangerousPatterns) {
        std::string patLower = pat;
        std::transform(patLower.begin(), patLower.end(), patLower.begin(),
                       [](unsigned char c){ return static_cast<char>(::tolower(c)); });
        if (cmdLower.find(patLower) != std::string::npos) {
            blockReason = "Dangerous pattern detected: " + pat;
            return false;
        }
    }

    // Whitelist mode
    if (config.useWhitelistMode) {
        if (!contains(config.commandWhitelist, cmdName) &&
            !contains(config.commandWhitelist, baseCommand)) {
            blockReason = "Command not in whitelist";
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Sensitive data filtering — replaces QRegularExpression
// ---------------------------------------------------------------------------
std::string SandboxedTerminal::filterSensitiveData(const std::string& data) const
{
    std::string filtered = data;
    try {
        std::regex apiKeyRe(R"(api[_\-]?key[\s=:]+['\"]?([a-zA-Z0-9_\-]{20,})['\"]?)",
                            std::regex_constants::icase);
        std::regex passwordRe(R"(password[\s=:]+['\"]?([^'\"\s]+)['\"]?)",
                              std::regex_constants::icase);
        std::regex tokenRe(R"(token[\s=:]+['\"]?([a-zA-Z0-9_\-]{20,})['\"]?)",
                           std::regex_constants::icase);
        std::regex emailRe(R"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})");
        std::regex ipRe(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)");

        filtered = std::regex_replace(filtered, apiKeyRe,   "api_key=[REDACTED]");
        filtered = std::regex_replace(filtered, passwordRe, "password=[REDACTED]");
        filtered = std::regex_replace(filtered, tokenRe,    "token=[REDACTED]");
        filtered = std::regex_replace(filtered, emailRe,    "[EMAIL_REDACTED]");
        filtered = std::regex_replace(filtered, ipRe,       "[IP_REDACTED]");
    } catch (const std::regex_error& e) {
        fprintf(stderr, "[SandboxedTerminal] regex error: %s\n", e.what());
    }
    return filtered;
}
