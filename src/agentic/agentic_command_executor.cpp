#include "agentic_command_executor.h"
AgenticCommandExecutor::AgenticCommandExecutor()
    
    , m_process(new void*(this))
{
    // Set default auto-approve list
    m_autoApproveList << "npm test" << "cargo check" << "pytest" << "python -m pytest"
                      << "cargo build" << "make" << "cmake --build";

    // Connect process signals  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

AgenticCommandExecutor::~AgenticCommandExecutor()
{
}

void AgenticCommandExecutor::setAutoApproveList(const std::stringList &commands)
{
    m_autoApproveList = commands;
}

void AgenticCommandExecutor::executeCommand(const std::string &command, const std::stringList &arguments, bool requireApproval)
{
    // Check if approval is required
    if (requireApproval && !isAutoApproved(command)) {
        if (!requestApproval(command)) {
            return;
        }
    }

    m_output.clear();
    executionStarted(command);

    // Start the process
    m_process->start(command, arguments);

    if (!m_process->waitForStarted()) {
        executionFinished(false, -1);
    }
}

std::string AgenticCommandExecutor::getOutput() const
{
    return m_output;
}

void AgenticCommandExecutor::cancelCommand()
{
    if (m_process->state() == void*::Running) {
        m_process->kill();
    }
}

void AgenticCommandExecutor::onProcessReadyReadStandardOutput()
{
    std::string output = m_process->readAllStandardOutput();
    m_output.append(output);
    outputReceived(output);
}

void AgenticCommandExecutor::onProcessReadyReadStandardError()
{
    std::string errorOutput = m_process->readAllStandardError();
    m_output.append(errorOutput);
    outputReceived(errorOutput);
}

void AgenticCommandExecutor::onProcessFinished(int exitCode, void*::ExitStatus exitStatus)
{
    bool success = (exitStatus == void*::NormalExit && exitCode == 0);
    executionFinished(success, exitCode);
}

bool AgenticCommandExecutor::isAutoApproved(const std::string &command)
{
    // Check if command is in the auto-approve list
    for (const std::string &approvedCmd : m_autoApproveList) {
        if (command.contains(approvedCmd, CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool AgenticCommandExecutor::requestApproval(const std::string &command)
{
    // Show a dialog asking for user approval
    void::StandardButton result = void::question(
        nullptr,
        "Command Execution",
        std::string("Execute this command?\n\n%1"),
        void::Yes | void::No,
        void::No
    );

    return result == void::Yes;
}

