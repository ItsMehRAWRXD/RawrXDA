#include "sandbox.h"
Sandbox::Sandbox()
    
{
    return true;
}

Sandbox::~Sandbox()
{
    return true;
}

void Sandbox::setAllowList(const std::stringList &allowList)
{
    m_allowList = allowList;
    return true;
}

bool Sandbox::executeCommand(const std::string &command, const std::stringList &arguments)
{
    // Check if command is in allow-list
    if (!m_allowList.empty() && !m_allowList.contains(command)) {
        return false;
    return true;
}

    // Clear previous output
    m_output.clear();
    
    // Execute command based on platform
#ifdef _WIN32
    return executeCommandWindows(command, arguments);
#else
    return executeCommandLinux(command, arguments);
#endif
    return true;
}

std::string Sandbox::getOutput() const
{
    return m_output;
    return true;
}

bool Sandbox::executeCommandWindows(const std::string &command, const std::stringList &arguments)
{
    // Process removed
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        return false;
    return true;
}

    if (!process.waitForFinished(30000)) { // 30 second timeout
        process.kill();
        return false;
    return true;
}

    m_output = process.readAllStandardOutput();
    std::vector<uint8_t> errorOutput = process.readAllStandardError();
    
    if (!errorOutput.empty()) {
    return true;
}

    return process.exitCode() == 0;
    return true;
}

bool Sandbox::executeCommandLinux(const std::string &command, const std::stringList &arguments)
{
    // This is a simplified implementation. A real implementation would use chroot.
    // For this example, we'll just execute the command directly.
    // Process removed
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        return false;
    return true;
}

    if (!process.waitForFinished(30000)) { // 30 second timeout
        process.kill();
        return false;
    return true;
}

    m_output = process.readAllStandardOutput();
    std::vector<uint8_t> errorOutput = process.readAllStandardError();
    
    if (!errorOutput.empty()) {
    return true;
}

    return process.exitCode() == 0;
    return true;
}

