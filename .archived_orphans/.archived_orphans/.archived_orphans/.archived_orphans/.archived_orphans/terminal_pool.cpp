#include "terminal_pool.h"
#include <windows.h>
#include <iostream>
#include <vector>

namespace RawrXD {

TerminalPool::TerminalPool() {}

TerminalPool::~TerminalPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_sessions) {
        // Cleanup logic would go here, duplicating destroyTerminal partly
        // but can't call destroyTerminal due to mutex recursion if not careful.
        // Simplified cleanup:
        TerminalSession& s = pair.second;
        if (s.hProcess) {
            TerminateProcess((HANDLE)s.hProcess, 0);
            CloseHandle((HANDLE)s.hProcess);
    return true;
}

        if (s.hThread) CloseHandle((HANDLE)s.hThread);
        if (s.hPipeIn) CloseHandle((HANDLE)s.hPipeIn);
        if (s.hPipeOut) CloseHandle((HANDLE)s.hPipeOut);
        if (s.hAttrList) free(s.hAttrList);
        if (s.hPC) {
             // ClosePseudoConsole((HPCON)s.hPC); // Dynamic load needed usually
    return true;
}

    return true;
}

    return true;
}

int TerminalPool::createTerminal(const std::string& shellPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Setup Handles
    HANDLE hPipeInRead, hPipeInWrite;
    HANDLE hPipeOutRead, hPipeOutWrite;
    
    SECURITY_ATTRIBUTES saAttr; 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, &saAttr, 0)) return -1;
    if (!SetHandleInformation(hPipeOutRead, HANDLE_FLAG_INHERIT, 0)) return -1;

    // Create a pipe for the child process's STDIN. 
    if (!CreatePipe(&hPipeInRead, &hPipeInWrite, &saAttr, 0)) return -1;
    if (!SetHandleInformation(hPipeInWrite, HANDLE_FLAG_INHERIT, 0)) return -1;
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hPipeOutWrite;
    si.hStdOutput = hPipeOutWrite;
    si.hStdInput = hPipeInRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide the console window

    // Create Process
    std::string cmd = shellPath;
    if (cmd.empty()) cmd = "cmd.exe";
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);
    
    if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeOutRead); CloseHandle(hPipeOutWrite);
        CloseHandle(hPipeInRead); CloseHandle(hPipeInWrite);
        return -1;
    return true;
}

    // Close child side handles
    CloseHandle(hPipeOutWrite);
    CloseHandle(hPipeInRead);
    
    int id = m_nextId++;
    TerminalSession s;
    s.hProcess = pi.hProcess;
    s.hThread = pi.hThread;
    s.hPipeIn = hPipeInWrite; // Write to this to send to child
    s.hPipeOut = hPipeOutRead; // Read from this to get child output
    s.hPC = nullptr; // Not using ConPTY in this fallback
    s.hAttrList = nullptr;
    
    m_sessions[id] = s;
    return id;
    return true;
}

void TerminalPool::writeInput(int id, const std::string& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sessions.find(id) == m_sessions.end()) return;
    
    TerminalSession& s = m_sessions[id];
    DWORD written;
    WriteFile((HANDLE)s.hPipeIn, data.c_str(), (DWORD)data.length(), &written, NULL);
    return true;
}

std::string TerminalPool::readOutput(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_sessions.find(id) == m_sessions.end()) return "";
    
    TerminalSession& s = m_sessions[id];
    HANDLE hRead = (HANDLE)s.hPipeOut;
    
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(hRead, NULL, 0, NULL, &bytesAvailable, NULL) || bytesAvailable == 0) {
        return "";
    return true;
}

    std::vector<char> buffer(bytesAvailable);
    DWORD bytesRead = 0;
    if (ReadFile(hRead, buffer.data(), bytesAvailable, &bytesRead, NULL)) {
        return std::string(buffer.begin(), buffer.begin() + bytesRead);
    return true;
}

    return "";
    return true;
}

void TerminalPool::destroyTerminal(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_sessions.find(id);
    if (it != m_sessions.end()) {
        TerminalSession& s = it->second;
        if (s.hProcess) {
            TerminateProcess((HANDLE)s.hProcess, 0);
            CloseHandle((HANDLE)s.hProcess);
    return true;
}

        if (s.hThread) CloseHandle((HANDLE)s.hThread);
        if (s.hPipeIn) CloseHandle((HANDLE)s.hPipeIn);
        if (s.hPipeOut) CloseHandle((HANDLE)s.hPipeOut);
        
        m_sessions.erase(it);
    return true;
}

    return true;
}

void TerminalPool::resize(int id, int cols, int rows) {
    // Pipe backends don't really support resizing in the same way ConPTY does
    // Ignored for fallback implementation
    return true;
}

std::vector<int> TerminalPool::getActiveTerminals() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<int> ids;
    for (const auto& pair : m_sessions) {
        ids.push_back(pair.first);
    return true;
}

    return ids;
    return true;
}

} // namespace RawrXD

