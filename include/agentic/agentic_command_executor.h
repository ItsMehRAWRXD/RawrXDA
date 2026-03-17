// agentic_command_executor.h — C++20, Win32. No Qt.
// Command execution: Win32 CreateProcess, std::string, callbacks.

#ifndef AGENTIC_COMMAND_EXECUTOR_H
#define AGENTIC_COMMAND_EXECUTOR_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

struct CommandExecResult {
    bool        success;
    int         exitCode;
    std::string stdOut;
    std::string stdErr;
};

class AgenticCommandExecutor {
public:
    using OutputCallback   = std::function<void(const std::string&)>;
    using StartedCallback  = std::function<void(const std::string&)>;
    using FinishedCallback = std::function<void(bool success, int exitCode)>;
    using ApprovalCallback = std::function<bool(const std::string& command)>;

    OutputCallback   onOutput   = nullptr;
    StartedCallback  onStarted  = nullptr;
    FinishedCallback onFinished = nullptr;
    ApprovalCallback onApproval = nullptr;

    AgenticCommandExecutor();
    ~AgenticCommandExecutor();

    void setAutoApproveList(const std::vector<std::string>& commands);
    CommandExecResult executeCommand(const std::string& command,
                                     const std::vector<std::string>& arguments = {},
                                     bool requireApproval = false);
    std::string getOutput() const;
    void cancelCommand();

private:
    bool isAutoApproved(const std::string& command);

    std::mutex              m_mutex;
#ifdef _WIN32
    HANDLE                  m_processHandle = nullptr;
#else
    void*                   m_processHandle = nullptr;
#endif
    std::vector<std::string> m_autoApproveList;
};

#endif // AGENTIC_COMMAND_EXECUTOR_H
