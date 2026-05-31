#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <thread>

namespace rawrxd::terminal {

class EmbeddedTerminal {
public:
    using OutputCallback = std::function<void(const std::string& output)>;
    using ExitCallback = std::function<void(int exit_code)>;
    
    static EmbeddedTerminal& instance() {
        static EmbeddedTerminal instance;
        return instance;
    }
    
    // Initialize ConPTY (Windows 10+) or legacy console
    bool initialize(void* parent_hwnd, int width, int height);
    
    // Execute command and capture output
    bool executeCommand(const std::string& command, 
                       OutputCallback output_cb,
                       ExitCallback exit_cb);
    
    // Send input to running process
    bool sendInput(const std::string& input);
    
    // Resize terminal
    void resize(int cols, int rows);
    
    // Check if command running
    bool isRunning() const;
    
    // Get last exit code
    int getLastExitCode() const;
    
    // For agent tool integration
    std::string executeAndCapture(const std::string& command, 
                                   uint32_t timeout_ms = 30000);
    
private:
    EmbeddedTerminal() = default;
    ~EmbeddedTerminal() {
        terminate();
        if (hConPTY_in_  != INVALID_HANDLE_VALUE) CloseHandle(hConPTY_in_);
        if (hConPTY_out_ != INVALID_HANDLE_VALUE) CloseHandle(hConPTY_out_);
        if (hChildStdIn_ != INVALID_HANDLE_VALUE) CloseHandle(hChildStdIn_);
        if (hChildStdOut_ != INVALID_HANDLE_VALUE) CloseHandle(hChildStdOut_);
        if (hProcess_    != INVALID_HANDLE_VALUE) CloseHandle(hProcess_);
    }
    EmbeddedTerminal(const EmbeddedTerminal&) = delete;
    EmbeddedTerminal& operator=(const EmbeddedTerminal&) = delete;

    HANDLE hConPTY_in_ = INVALID_HANDLE_VALUE;
    HANDLE hConPTY_out_ = INVALID_HANDLE_VALUE;
    HANDLE hChildStdIn_ = INVALID_HANDLE_VALUE;
    HANDLE hChildStdOut_ = INVALID_HANDLE_VALUE;
    HANDLE hProcess_ = INVALID_HANDLE_VALUE;
    std::thread output_thread_;
    std::thread error_thread_;
    int last_exit_code_ = 0;
    bool running_ = false;
    
    void outputReader(OutputCallback cb);
    void errorReader(OutputCallback cb);
    bool initializeLegacy(void* parent_hwnd);
    void terminate();
};

} // namespace rawrxd::terminal
