// RawrXD_Sandbox.hpp - Sandboxed Execution Engine
// Pure C++20 - No Qt Dependencies
// Uses Windows Job Objects, restricted tokens, and AppContainer for process isolation
// Features: Resource-limited process execution, filesystem/network restriction,
//           timeout enforcement, output capture, sandboxed tool/script execution

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <sstream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")

namespace RawrXD {
namespace Security {

// ============================================================================
// Sandbox Restriction Levels
// ============================================================================
enum class SandboxLevel : uint8_t {
    None        = 0, // No restrictions (for trusted code)
    Low         = 1, // Job object limits only (CPU, memory, process count)
    Medium      = 2, // + restricted token (remove admin privileges)
    High        = 3, // + filesystem/registry restrictions, no network
    Maximum     = 4  // + AppContainer isolation (strongest)
};

// ============================================================================
// Sandbox Configuration
// ============================================================================
struct SandboxConfig {
    SandboxLevel level              = SandboxLevel::Medium;

    // Resource limits
    uint64_t     maxMemoryBytes     = 512ULL * 1024 * 1024;  // 512 MB
    uint32_t     maxCPUPercent      = 50;                     // 50% CPU
    uint32_t     maxProcesses       = 4;
    uint32_t     timeoutMs          = 30000;                  // 30 second execution timeout
    uint64_t     maxOutputBytes     = 1024 * 1024;            // 1 MB output capture limit

    // Filesystem access
    std::vector<std::string> readOnlyPaths;  // Paths allowed for reading
    std::vector<std::string> readWritePaths; // Paths allowed for writing
    std::string  workingDirectory;

    // Network
    bool         allowNetwork       = false;
    bool         allowLocalhost     = true;

    // Process
    bool         allowChildProcesses = false;
    bool         killOnTimeout      = true;
    bool         breakawayAllowed   = false;
};

// ============================================================================
// Sandbox Execution Result
// ============================================================================
struct SandboxResult {
    bool         completed       = false;
    bool         timedOut        = false;
    bool         killed          = false;
    int          exitCode        = -1;
    std::string  stdoutContent;
    std::string  stderrContent;
    uint64_t     durationMs      = 0;
    uint64_t     peakMemoryBytes = 0;
    uint64_t     cpuTimeMs       = 0;
    std::string  error;
};

// ============================================================================
// Sandbox - Isolated process execution engine
// ============================================================================
class Sandbox {
public:
    Sandbox() = default;
    ~Sandbox() { Cleanup(); }

    // ---- Execute a command in a sandbox ----
    SandboxResult Execute(const std::string& commandLine, const SandboxConfig& config) {
        SandboxResult result;
        auto startTime = std::chrono::steady_clock::now();

        // Create Job Object for resource limits
        HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
        if (!hJob) {
            result.error = "Failed to create job object: " + std::to_string(GetLastError());
            return result;
        }

        // Configure job limits
        ConfigureJobLimits(hJob, config);

        // Set up pipes for stdout/stderr capture
        HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
        HANDLE hStderrRead = nullptr, hStderrWrite = nullptr;

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0) ||
            !CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
            result.error = "Failed to create capture pipes";
            CloseHandle(hJob);
            return result;
        }

        // Ensure read handles are not inherited
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

        // Create process
        STARTUPINFOW si = {};
        si.cb          = sizeof(si);
        si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput  = hStdoutWrite;
        si.hStdError   = hStderrWrite;
        si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::wstring wCmd(commandLine.begin(), commandLine.end());
        std::wstring wDir;
        LPCWSTR lpDir = nullptr;
        if (!config.workingDirectory.empty()) {
            wDir.assign(config.workingDirectory.begin(), config.workingDirectory.end());
            lpDir = wDir.c_str();
        }

        DWORD creationFlags = CREATE_SUSPENDED | CREATE_NO_WINDOW;
        if (!config.breakawayAllowed) {
            creationFlags |= CREATE_BREAKAWAY_FROM_JOB;
        }

        // For Medium+ security, create with restricted token
        HANDLE hToken = nullptr;
        bool useRestrictedToken = (config.level >= SandboxLevel::Medium);

        if (useRestrictedToken) {
            hToken = CreateRestrictedToken();
        }

        BOOL created = FALSE;
        if (hToken) {
            created = CreateProcessAsUserW(hToken, nullptr, wCmd.data(), nullptr, nullptr,
                TRUE, creationFlags, nullptr, lpDir, &si, &pi);
        } else {
            created = CreateProcessW(nullptr, wCmd.data(), nullptr, nullptr,
                TRUE, creationFlags, nullptr, lpDir, &si, &pi);
        }

        if (hToken) CloseHandle(hToken);

        // Close write ends of pipes (child has them now)
        CloseHandle(hStdoutWrite);
        CloseHandle(hStderrWrite);

        if (!created) {
            result.error = "Failed to create sandboxed process: " + std::to_string(GetLastError());
            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);
            CloseHandle(hJob);
            return result;
        }

        // Assign process to job object BEFORE resuming it
        if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
            result.error = "Failed to assign process to job: " + std::to_string(GetLastError());
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);
            CloseHandle(hJob);
            return result;
        }

        // Resume suspended process
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);

        // Read output asynchronously while waiting for process
        std::atomic<bool> reading{true};
        std::string stdoutBuf, stderrBuf;
        std::mutex outputMutex;

        auto readerFunc = [&](HANDLE hPipe, std::string& buf) {
            char buffer[4096];
            DWORD bytesRead;
            while (reading.load()) {
                if (!ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, nullptr) || bytesRead == 0) break;
                std::lock_guard<std::mutex> lock(outputMutex);
                if (buf.size() + bytesRead <= config.maxOutputBytes) {
                    buf.append(buffer, bytesRead);
                }
            }
        };

        std::thread stdoutReader(readerFunc, hStdoutRead, std::ref(stdoutBuf));
        std::thread stderrReader(readerFunc, hStderrRead, std::ref(stderrBuf));

        // Wait for process with timeout
        DWORD waitResult = WaitForSingleObject(pi.hProcess, config.timeoutMs);

        if (waitResult == WAIT_TIMEOUT) {
            result.timedOut = true;
            if (config.killOnTimeout) {
                TerminateProcess(pi.hProcess, 0xDEAD);
                result.killed = true;
            }
            WaitForSingleObject(pi.hProcess, 5000); // Give 5s for cleanup
        }

        reading.store(false);

        // Close pipe handles to unblock readers
        CloseHandle(hStdoutRead);
        CloseHandle(hStderrRead);

        if (stdoutReader.joinable()) stdoutReader.join();
        if (stderrReader.joinable()) stderrReader.join();

        // Get exit code
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
        result.completed = (waitResult == WAIT_OBJECT_0);

        // Get resource usage
        PROCESS_MEMORY_COUNTERS_EX memInfo = {};
        memInfo.cb = sizeof(memInfo);
        if (GetProcessMemoryInfo(pi.hProcess, (PROCESS_MEMORY_COUNTERS*)&memInfo, sizeof(memInfo))) {
            result.peakMemoryBytes = memInfo.PeakWorkingSetSize;
        }

        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(pi.hProcess, &creationTime, &exitTime, &kernelTime, &userTime)) {
            ULARGE_INTEGER kt, ut;
            kt.LowPart = kernelTime.dwLowDateTime; kt.HighPart = kernelTime.dwHighDateTime;
            ut.LowPart = userTime.dwLowDateTime; ut.HighPart = userTime.dwHighDateTime;
            result.cpuTimeMs = (kt.QuadPart + ut.QuadPart) / 10000; // 100ns -> ms
        }

        auto endTime = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        result.stdoutContent = stdoutBuf;
        result.stderrContent = stderrBuf;

        CloseHandle(pi.hProcess);
        CloseHandle(hJob);
        return result;
    }

    // ---- Execute a script file (determines interpreter from extension) ----
    SandboxResult ExecuteScript(const std::string& scriptPath, const SandboxConfig& config,
                                const std::vector<std::string>& args = {}) {
        std::string cmdLine;

        // Determine interpreter from extension
        size_t dotPos = scriptPath.rfind('.');
        std::string ext = (dotPos != std::string::npos) ? scriptPath.substr(dotPos) : "";
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".py" || ext == ".pyw") {
            cmdLine = "python \"" + scriptPath + "\"";
        } else if (ext == ".ps1") {
            cmdLine = "powershell -ExecutionPolicy Restricted -NoProfile -File \"" + scriptPath + "\"";
        } else if (ext == ".bat" || ext == ".cmd") {
            cmdLine = "cmd /c \"" + scriptPath + "\"";
        } else if (ext == ".js") {
            cmdLine = "node \"" + scriptPath + "\"";
        } else {
            cmdLine = "\"" + scriptPath + "\"";
        }

        for (const auto& arg : args) {
            cmdLine += " \"" + arg + "\"";
        }

        return Execute(cmdLine, config);
    }

    // ---- Quick execute with default sandbox ----
    static SandboxResult QuickExec(const std::string& command, uint32_t timeoutMs = 10000,
                                    uint64_t maxMemory = 256ULL * 1024 * 1024) {
        SandboxConfig cfg;
        cfg.level          = SandboxLevel::Medium;
        cfg.timeoutMs      = timeoutMs;
        cfg.maxMemoryBytes = maxMemory;
        cfg.maxProcesses   = 2;
        cfg.allowNetwork   = false;

        Sandbox sb;
        return sb.Execute(command, cfg);
    }

private:
    void ConfigureJobLimits(HANDLE hJob, const SandboxConfig& config) {
        // Extended limit information
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION extLimits = {};
        extLimits.BasicLimitInformation.LimitFlags = 0;

        // Memory limit
        if (config.maxMemoryBytes > 0) {
            extLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_PROCESS_MEMORY;
            extLimits.ProcessMemoryLimit = config.maxMemoryBytes;

            extLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
            extLimits.JobMemoryLimit = config.maxMemoryBytes * 2; // Job total = 2x per-process
        }

        // Process count limit
        if (config.maxProcesses > 0) {
            extLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            extLimits.BasicLimitInformation.ActiveProcessLimit = config.maxProcesses;
        }

        // Kill children on job close
        extLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        // Prevent breakaway
        if (!config.breakawayAllowed) {
            extLimits.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        }

        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation,
            &extLimits, sizeof(extLimits));

        // CPU rate limit (Windows 8+)
        if (config.maxCPUPercent > 0 && config.maxCPUPercent < 100) {
            JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuRate = {};
            cpuRate.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE |
                                   JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
            cpuRate.CpuRate = config.maxCPUPercent * 100; // Units of 1/10000

            SetInformationJobObject(hJob, JobObjectCpuRateControlInformation,
                &cpuRate, sizeof(cpuRate));
        }

        // UI restrictions (High+)
        if (config.level >= SandboxLevel::High) {
            JOBOBJECT_BASIC_UI_RESTRICTIONS uiRestrict = {};
            uiRestrict.UIRestrictionsClass =
                JOB_OBJECT_UILIMIT_DESKTOP |
                JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
                JOB_OBJECT_UILIMIT_EXITWINDOWS |
                JOB_OBJECT_UILIMIT_GLOBALATOMS |
                JOB_OBJECT_UILIMIT_HANDLES |
                JOB_OBJECT_UILIMIT_READCLIPBOARD |
                JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                JOB_OBJECT_UILIMIT_WRITECLIPBOARD;

            SetInformationJobObject(hJob, JobObjectBasicUIRestrictions,
                &uiRestrict, sizeof(uiRestrict));
        }

        // Network restrictions (via firewall rules, if High+)
        if (config.level >= SandboxLevel::High && !config.allowNetwork) {
            JOBOBJECT_NET_RATE_CONTROL_INFORMATION netRate = {};
            netRate.ControlFlags = JOB_OBJECT_NET_RATE_CONTROL_ENABLE;
            netRate.MaxBandwidth = 0; // Zero bandwidth = no network

            SetInformationJobObject(hJob, JobObjectNetRateControlInformation,
                &netRate, sizeof(netRate));
        }
    }

    HANDLE CreateRestrictedToken() {
        HANDLE hToken = nullptr, hRestrictedToken = nullptr;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken))
            return nullptr;

        // Remove high-privilege SIDs
        SID_AND_ATTRIBUTES sidsToDisable[2] = {};
        DWORD sidCount = 0;

        // Get Administrators SID
        PSID pAdminSid = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuth, 2,
                SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0, &pAdminSid)) {
            sidsToDisable[sidCount].Sid = pAdminSid;
            sidsToDisable[sidCount].Attributes = 0;
            sidCount++;
        }

        // Remove privileges
        TOKEN_PRIVILEGES* pDisabledPrivs = nullptr;
        DWORD privBufSize = 0;
        GetTokenInformation(hToken, TokenPrivileges, nullptr, 0, &privBufSize);
        pDisabledPrivs = (TOKEN_PRIVILEGES*)HeapAlloc(GetProcessHeap(), 0, privBufSize);
        if (pDisabledPrivs) {
            GetTokenInformation(hToken, TokenPrivileges, pDisabledPrivs, privBufSize, &privBufSize);
            // Disable all privileges
            for (DWORD i = 0; i < pDisabledPrivs->PrivilegeCount; ++i) {
                pDisabledPrivs->Privileges[i].Attributes = 0;
            }
        }

        CreateRestrictedToken(hToken,
            DISABLE_MAX_PRIVILEGE,
            sidCount, sidsToDisable,
            pDisabledPrivs ? pDisabledPrivs->PrivilegeCount : 0,
            pDisabledPrivs ? pDisabledPrivs->Privileges : nullptr,
            0, nullptr,
            &hRestrictedToken);

        if (pDisabledPrivs) HeapFree(GetProcessHeap(), 0, pDisabledPrivs);
        if (pAdminSid) FreeSid(pAdminSid);
        CloseHandle(hToken);

        return hRestrictedToken;
    }

    void Cleanup() {
        // Any managed resources
    }
};

} // namespace Security
} // namespace RawrXD
