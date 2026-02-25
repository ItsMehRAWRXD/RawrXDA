#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace RawrXD {

struct TerminalSession {
    void* hProcess = nullptr;
    void* hThread = nullptr;
    void* hPipeIn = nullptr;
    void* hPipeOut = nullptr;
    void* hPC = nullptr;
    void* hAttrList = nullptr;
};

class TerminalPool {
public:
    TerminalPool();
    ~TerminalPool();

    int createTerminal(const std::string& shellPath);
    void writeInput(int id, const std::string& data);
    std::string readOutput(int id);
    void destroyTerminal(int id);
    void resize(int id, int cols, int rows);
    std::vector<int> getActiveTerminals() const;

private:
    std::map<int, TerminalSession> m_sessions;
    int m_nextId = 1;
    mutable std::mutex m_mutex;
};
} // namespace RawrXD
