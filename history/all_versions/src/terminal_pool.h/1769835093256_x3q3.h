#pragma once
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <windows.h>

struct TerminalInstance {
    HANDLE hPty;
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hInRead, hInWrite; // For fallback
    HANDLE hOutRead, hOutWrite; // For fallback
    bool useConPty;
};

class TerminalPool {
public:
    TerminalPool();
    ~TerminalPool();
    
    bool createTerminal(const std::string& name, const std::string& shellCmd = "cmd.exe");
    bool runCommand(const std::string& name, const std::string& cmd);
    std::string readOutput(const std::string& name);
    void closeTerminal(const std::string& name);
    
    std::vector<std::string> listTerminals() const;

private:
    std::map<std::string, TerminalInstance> m_terminals;
    mutable std::mutex m_mutex;
    
    // ConPTY Helpers
    HRESULT CreatePseudoConsoleAndPipes(HPCON* phPC, HANDLE* phPipeIn, HANDLE* phPipeOut);
    HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX* pStartupInfo, HPCON hPC);
};
