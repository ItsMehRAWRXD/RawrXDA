// agentic_command_executor.cpp — Qt-free Win32 process execution
// Purged: QObject, QProcess, QMessageBox, QDebug, signals/slots
// Replaced with: Win32 CreateProcess, std::string, function pointer callbacks
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <functional>

// ---------------------------------------------------------------------------
// AgenticCommandExecutor — pure Win32/C++20, zero Qt
// ---------------------------------------------------------------------------
struct CommandExecResult {
    bool    success;
    int     exitCode;
    std::string stdOut;
    std::string stdErr;
};

class AgenticCommandExecutor {
public:
    // Callbacks — replace Qt signals
    using OutputCallback    = std::function<void(const std::string&)>;
    using StartedCallback   = std::function<void(const std::string&)>;
    using FinishedCallback  = std::function<void(bool success, int exitCode)>;

    OutputCallback   onOutput   = nullptr;
    StartedCallback  onStarted  = nullptr;
    FinishedCallback onFinished = nullptr;

    // Approval callback — replaces QMessageBox::question
    // Return true to allow execution.  nullptr => auto-approve everything.
    using ApprovalCallback = std::function<bool(const std::string& command)>;
    ApprovalCallback onApproval = nullptr;

    AgenticCommandExecutor()
    {
        m_autoApproveList = {
            "npm test", "cargo check", "pytest", "python -m pytest",
            "cargo build", "make", "cmake --build"
        };
    }

    ~AgenticCommandExecutor()
    {
        cancelCommand();
    }

    // ------- configuration -------
    void setAutoApproveList(const std::vector<std::string>& commands)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_autoApproveList = commands;
    }

    // ------- execute -------
    CommandExecResult executeCommand(const std::string& command,
                                     const std::vector<std::string>& arguments,
                                     bool requireApproval = false)
    {
        CommandExecResult res{};
        res.exitCode = -1;

        if (requireApproval && !isAutoApproved(command)) {
            if (onApproval && !onApproval(command)) {
                fprintf(stderr, "[AgenticCmdExec] Command rejected: %s\n", command.c_str());
                return res;
            }
        }

        // Build full command line
        std::string cmdline = command;
        for (const auto& arg : arguments) {
            cmdline += " ";
            cmdline += arg;
        }

        if (onStarted) onStarted(command);
        fprintf(stderr, "[AgenticCmdExec] Executing: %s\n", cmdline.c_str());

        // --- Win32 CreateProcess with pipes ---
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr;
        HANDLE hStdErrRead = nullptr, hStdErrWrite = nullptr;

        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
            !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
            res.stdErr = "Failed to create pipes";
            if (onFinished) onFinished(false, -1);
            return res;
        }

        // Prevent child from inheriting read handles
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hStdOutWrite;
        si.hStdError  = hStdErrWrite;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi{};

        // CreateProcess needs mutable buffer
        std::vector<char> cmdBuf(cmdline.begin(), cmdline.end());
        cmdBuf.push_back('\0');

        BOOL created = CreateProcessA(
            nullptr,
            cmdBuf.data(),
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi);

        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);

        if (!created) {
            res.stdErr = "CreateProcess failed, error " + std::to_string(GetLastError());
            CloseHandle(hStdOutRead);
            CloseHandle(hStdErrRead);
            if (onFinished) onFinished(false, -1);
            return res;
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_processHandle = pi.hProcess;
        }

        // Read stdout/stderr
        auto readPipe = [](HANDLE h) -> std::string {
            std::string result;
            char buf[4096];
            DWORD bytesRead = 0;
            while (ReadFile(h, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                result.append(buf, bytesRead);
            }
            return result;
        };

        res.stdOut = readPipe(hStdOutRead);
        res.stdErr = readPipe(hStdErrRead);

        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        res.exitCode = static_cast<int>(exitCode);
        res.success  = (exitCode == 0);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_processHandle = nullptr;
        }

        if (onOutput && !res.stdOut.empty()) onOutput(res.stdOut);
        if (onOutput && !res.stdErr.empty()) onOutput(res.stdErr);

        fprintf(stderr, "[AgenticCmdExec] Finished: exit=%d success=%d\n",
                res.exitCode, res.success ? 1 : 0);

        if (onFinished) onFinished(res.success, res.exitCode);
        return res;
    }

    std::string getOutput() const
    {
        // Kept for API compat — callers should use CommandExecResult instead
        return {};
    }

    void cancelCommand()
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_processHandle) {
            TerminateProcess(m_processHandle, 1);
            CloseHandle(m_processHandle);
            m_processHandle = nullptr;
            fprintf(stderr, "[AgenticCmdExec] Command cancelled\n");
        }
    }

private:
    bool isAutoApproved(const std::string& command)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        // Case-insensitive substring match
        std::string cmdLower = command;
        std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(),
                       [](unsigned char c){ return static_cast<char>(::tolower(c)); });

        for (const auto& approved : m_autoApproveList) {
            std::string approvedLower = approved;
            std::transform(approvedLower.begin(), approvedLower.end(), approvedLower.begin(),
                           [](unsigned char c){ return static_cast<char>(::tolower(c)); });
            if (cmdLower.find(approvedLower) != std::string::npos) {
                fprintf(stderr, "[AgenticCmdExec] Auto-approved: %s\n", command.c_str());
                return true;
            }
        }
        return false;
    }

    std::mutex                   m_mutex;
    HANDLE                       m_processHandle = nullptr;
    std::vector<std::string>     m_autoApproveList;
};