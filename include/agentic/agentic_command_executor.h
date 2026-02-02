#ifndef AGENTIC_COMMAND_EXECUTOR_H
#define AGENTIC_COMMAND_EXECUTOR_H

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <windows.h>

// Command Execution Wiring - async process launcher with progress.
// Output captured → streamed to Copilot chat (use your HandleCopilotStreamUpdate).
// Auto-approve list: npm test, cargo check, pytest – everything else triggers Keep/Undo dialog.
class AgenticCommandExecutor
{
public:
    using OutputCallback = std::function<void(const std::string&)>;
    using StatusCallback = std::function<void(bool success, int exitCode)>;
    using ApprovalCallback = std::function<bool(const std::string&)>;

    explicit AgenticCommandExecutor();
    ~AgenticCommandExecutor();

    // Set the auto-approve list of commands
    void setAutoApproveList(const std::vector<std::string> &commands);

    // Execute a command (with approval if not in auto-approve list)
    void executeCommand(const std::string &command, const std::vector<std::string> &arguments = {}, bool requireApproval = true);

    // Get command output
    std::string getOutput() const;

    // Cancel running command
    void cancelCommand();

    // Setters for callbacks
    void onOutputReceived(OutputCallback cb);
    void onExecutionFinished(StatusCallback cb);
    void setApprovalCallback(ApprovalCallback cb);

private:
    void runProcess(std::string cmdLine);
    bool checkApproval(const std::string& cmd);
    void appendOutput(const std::string& str);
    bool isAutoApproved(const std::string &command);

    std::vector<std::string> m_autoApproveList;
    std::string m_output;
    mutable std::mutex m_mutex;
    
    OutputCallback m_outputCb;
    StatusCallback m_finishedCb;
    ApprovalCallback m_approvalCb;

    HANDLE m_hChildStd_OUT_Rd = NULL;
    HANDLE m_hChildStd_OUT_Wr = NULL;
    HANDLE m_hProcess = NULL;
    
    bool m_isRunning = false;
};

#endif // AGENTIC_COMMAND_EXECUTOR_H