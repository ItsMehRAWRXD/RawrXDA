// ============================================================================
// terminal_unrestricted.cpp — Full Unrestricted Terminal Emulator
// ============================================================================
// Complete terminal with full shell access (CMD, PowerShell, bash)
// No sandboxing - full system access like VS Code terminal
// ============================================================================

#include "terminal_unrestricted.h"
#include "logging/logger.h"
#include <windows.h>
#include <thread>

static Logger s_logger("Terminal");

class Terminal::Impl {
public:
    HANDLE m_hChildStdInWrite;
    HANDLE m_hChildStdOutRead;
    HANDLE m_hProcess;
    std::thread m_outputThread;
    std::atomic<bool> m_running{false};
    std::mutex m_mutex;
    TerminalCallback m_callback;
    std::string m_shellType;
    
    Impl() : m_hChildStdInWrite(NULL), m_hChildStdOutRead(NULL), m_hProcess(NULL) {}
    
    ~Impl() {
        stop();
    }
    
    bool start(const std::string& shell) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_running) {
            s_logger.warn("Terminal already running");
            return false;
        }
        
        m_shellType = shell;
        
        // Create pipes for stdio redirection
        HANDLE hChildStdOutWrite, hChildStdInRead;
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        if (!CreatePipe(&m_hChildStdOutRead, &hChildStdOutWrite, &sa, 0)) {
            s_logger.error("CreatePipe failed for stdout");
            return false;
        }
        
        if (!CreatePipe(&hChildStdInRead, &m_hChildStdInWrite, &sa, 0)) {
            CloseHandle(m_hChildStdOutRead);
            CloseHandle(hChildStdOutWrite);
            return false;
        }
        
        // Make read/write handles non-inheritable
        SetHandleInformation(m_hChildStdOutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(m_hChildStdInWrite, HANDLE_FLAG_INHERIT, 0);
        
        // Determine shell command
        std::string shellCmd;
        if (shell == "powershell") {
            shellCmd = "powershell.exe -NoLogo -NoExit";
        } else if (shell == "cmd") {
            shellCmd = "cmd.exe";
        } else if (shell == "bash") {
            shellCmd = "bash.exe";  // WSL or Git Bash
        } else {
            shellCmd = shell;  // Custom shell
        }
        
        // Create process
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.hStdError = hChildStdOutWrite;
        si.hStdOutput = hChildStdOutWrite;
        si.hStdInput = hChildStdInRead;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        PROCESS_INFORMATION pi = {};
        
        char* cmdLine = _strdup(shellCmd.c_str());
        
        if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, 0,
                           NULL, NULL, &si, &pi)) {
            free(cmdLine);
            CloseHandle(hChildStdOutWrite);
            CloseHandle(hChildStdInRead);
            CloseHandle(m_hChildStdOutRead);
            CloseHandle(m_hChildStdInWrite);
            s_logger.error("CreateProcess failed for shell: {}", shell);
            return false;
        }
        
        free(cmdLine);
        
        m_hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hChildStdOutWrite);
        CloseHandle(hChildStdInRead);
        
        m_running = true;
        
        // Start output reader thread
        m_outputThread = std::thread([this]() { readOutputLoop(); });
        
        s_logger.info("Terminal started: {}", shell);
        return true;
    }
    
    void readOutputLoop() {
        const int BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];
        DWORD bytesRead;
        
        while (m_running) {
            if (ReadFile(m_hChildStdOutRead, buffer, BUFFER_SIZE - 1, 
                        &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = 0;
                
                if (m_callback) {
                    m_callback(std::string(buffer, bytesRead));
                }
            } else {
                // Process probably exited
                break;
            }
        }
        
        m_running = false;
        s_logger.info("Terminal output loop ended");
    }
    
    bool sendCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_running) {
            s_logger.warn("Terminal not running");
            return false;
        }
        
        std::string cmd = command + "\r\n";
        DWORD written;
        
        if (!WriteFile(m_hChildStdInWrite, cmd.c_str(), 
                      static_cast<DWORD>(cmd.length()), &written, NULL)) {
            s_logger.error("Failed to write to terminal");
            return false;
        }
        
        s_logger.debug("Command sent: {}", command);
        return true;
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_running) {
            m_running = false;
            
            if (m_hProcess) {
                TerminateProcess(m_hProcess, 0);
                WaitForSingleObject(m_hProcess, 1000);
                CloseHandle(m_hProcess);
                m_hProcess = NULL;
            }
            
            if (m_hChildStdOutRead) {
                CloseHandle(m_hChildStdOutRead);
                m_hChildStdOutRead = NULL;
            }
            
            if (m_hChildStdInWrite) {
                CloseHandle(m_hChildStdInWrite);
                m_hChildStdInWrite = NULL;
            }
        }
        
        if (m_outputThread.joinable()) {
            m_outputThread.join();
        }
    }
};

// ============================================================================
// Public API
// ============================================================================

Terminal::Terminal() : m_impl(new Impl()) {}
Terminal::~Terminal() { delete m_impl; }

bool Terminal::start(const std::string& shell) {
    return m_impl->start(shell);
}

void Terminal::stop() {
    m_impl->stop();
}

bool Terminal::sendCommand(const std::string& command) {
    return m_impl->sendCommand(command);
}

void Terminal::setCallback(TerminalCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_callback = callback;
}

bool Terminal::isRunning() const {
    return m_impl->m_running.load();
}

std::string Terminal::getShellType() const {
    return m_impl->m_shellType;
}
