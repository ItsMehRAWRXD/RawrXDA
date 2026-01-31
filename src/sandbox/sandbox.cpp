#include "sandbox.h"
Sandbox::Sandbox()
    
{
}

Sandbox::~Sandbox()
{
}

void Sandbox::setAllowList(const std::stringList &allowList)
{
    m_allowList = allowList;
}

bool Sandbox::executeCommand(const std::string &command, const std::stringList &arguments)
{
    // Check if command is in allow-list
    if (!m_allowList.empty() && !m_allowList.contains(command)) {
        // // qWarning:  "Command not in allow-list:" << command;
        return false;
    }
    
    // Clear previous output
    m_output.clear();
    
    // Execute command based on platform
#ifdef _WIN32
    return executeCommandWindows(command, arguments);
#else
    return executeCommandLinux(command, arguments);
#endif
}

std::string Sandbox::getOutput() const
{
    return m_output;
}

bool Sandbox::executeCommandWindows(const std::string &command, const std::stringList &arguments)
{
    // Process removed
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        // // qWarning:  "Failed to start process:" << command << process.errorString();
        return false;
    }
    
    if (!process.waitForFinished(30000)) { // 30 second timeout
        // // qWarning:  "Process timed out:" << command;
        process.kill();
        return false;
    }
    
    m_output = process.readAllStandardOutput();
    std::vector<uint8_t> errorOutput = process.readAllStandardError();
    
    if (!errorOutput.empty()) {
        // // qWarning:  "Process error output:" << errorOutput;
    }
    
    return process.exitCode() == 0;
}

bool Sandbox::executeCommandLinux(const std::string &command, const std::stringList &arguments)
{
    // This is a simplified implementation. A real implementation would use chroot.
    // For this example, we'll just execute the command directly.
    // Process removed
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        // // qWarning:  "Failed to start process:" << command << process.errorString();
        return false;
    }
    
    if (!process.waitForFinished(30000)) { // 30 second timeout
        // // qWarning:  "Process timed out:" << command;
        process.kill();
        return false;
    }
    
    m_output = process.readAllStandardOutput();
    std::vector<uint8_t> errorOutput = process.readAllStandardError();
    
    if (!errorOutput.empty()) {
        // // qWarning:  "Process error output:" << errorOutput;
    }
    
    return process.exitCode() == 0;
}






