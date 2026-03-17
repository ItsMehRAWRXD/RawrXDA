#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace RawrXD {

class TerminalPool {
public:
    TerminalPool();
    ~TerminalPool();
    
    int createTerminal(const std::string& shellPath = "cmd.exe");
    void writeInput(int id, const std::string& data);
    std::string readOutput(int id);
    void resize(int id, int cols, int rows);
    void destroyTerminal(int id);
    
    // Helper to get all active IDs
    std::vector<int> getActiveTerminals() const;

private:
    struct TerminalSession {
        void* hPC;     // HPCON
        void* hPipeIn; // HANDLE
        void* hPipeOut;// HANDLE
        void* hProcess;// HANDLE
        void* hThread; // HANDLE
        void* hAttrList; // LPPROC_THREAD_ATTRIBUTE_LIST
    };
    
    std::map<int, TerminalSession> m_sessions;
    mutable std::mutex m_mutex;
    int m_nextId = 1;
};

} // namespace RawrXD
