#ifndef SANDBOX_H
#define SANDBOX_H

// C++20 / Win32. Command sandbox: allow-list; Win32 Job Objects, no Qt.

#include <string>
#include <vector>

class Sandbox
{
public:
    Sandbox() = default;
    ~Sandbox() = default;

    void setAllowList(const std::vector<std::string>& allowList);

    /** Execute command in sandbox. Returns true on success. */
    bool executeCommand(const std::string& command, const std::vector<std::string>& arguments = {});

    std::string getOutput() const { return m_output; }

private:
    bool executeCommandWindows(const std::string& command, const std::vector<std::string>& arguments);
    bool executeCommandLinux(const std::string& command, const std::vector<std::string>& arguments);

    std::vector<std::string> m_allowList;
    std::string m_output;
};

#endif // SANDBOX_H
