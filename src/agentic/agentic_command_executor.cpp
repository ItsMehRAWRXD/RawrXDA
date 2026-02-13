// agentic_command_executor.cpp — Qt-free Win32 process execution (C++20, no Qt)
// Uses header include/agentic/agentic_command_executor.h

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "agentic/agentic_command_executor.h"
#include <algorithm>
#include <cstdio>
#include <cctype>

AgenticCommandExecutor::AgenticCommandExecutor()
{
    m_autoApproveList = {
        "npm test", "cargo check", "pytest", "python -m pytest",
        "cargo build", "make", "cmake --build"
    };
}

AgenticCommandExecutor::~AgenticCommandExecutor()
{
    cancelCommand();
}

void AgenticCommandExecutor::setAutoApproveList(const std::vector<std::string>& commands)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_autoApproveList = commands;
}

CommandExecResult AgenticCommandExecutor::executeCommand(const std::string& command,
                                                         const std::vector<std::string>& arguments,
                                                         bool requireApproval)
{
    CommandExecResult res{};
    res.exitCode = -1;

    if (requireApproval && !isAutoApproved(command)) {
        if (onApproval && !onApproval(command)) {
            fprintf(stderr, "[AgenticCmdExec] Command rejected: %s\n", command.c_str());
            return res;
        }
    }

    std::string cmdline = command;
    for (const auto& arg : arguments) {
        cmdline += " ";
        cmdline += arg;
    }

    if (onStarted) onStarted(command);
    fprintf(stderr, "[AgenticCmdExec] Executing: %s\n", cmdline.c_str());

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

    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError  = hStdErrWrite;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

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

std::string AgenticCommandExecutor::getOutput() const
{
    return {};
}

void AgenticCommandExecutor::cancelCommand()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_processHandle) {
        TerminateProcess(m_processHandle, 1);
        CloseHandle(m_processHandle);
        m_processHandle = nullptr;
        fprintf(stderr, "[AgenticCmdExec] Command cancelled\n");
    }
}

bool AgenticCommandExecutor::isAutoApproved(const std::string& command)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::string cmdLower = command;
    std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(),
                  [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    for (const auto& approved : m_autoApproveList) {
        std::string approvedLower = approved;
        std::transform(approvedLower.begin(), approvedLower.end(), approvedLower.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (cmdLower.find(approvedLower) != std::string::npos) {
            fprintf(stderr, "[AgenticCmdExec] Auto-approved: %s\n", command.c_str());
            return true;
        }
    }
    return false;
}
