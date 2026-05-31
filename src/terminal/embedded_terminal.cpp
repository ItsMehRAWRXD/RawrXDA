#include "embedded_terminal.hpp"
#include <iostream>
#include <sstream>
#include <chrono>

namespace rawrxd::terminal {

bool EmbeddedTerminal::initialize(void* parent_hwnd, int width, int height) {
    COORD size{};
    size.X = static_cast<SHORT>(width);
    size.Y = static_cast<SHORT>(height);
    
    HANDLE hInRead, hInWrite;
    HANDLE hOutRead, hOutWrite;
    
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    if (!CreatePipe(&hInRead, &hInWrite, &sa, 0)) return false;
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) {
        CloseHandle(hInRead);
        CloseHandle(hInWrite);
        return false;
    }
    
    // Prevent host-side handles from being inherited by child processes
    SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Host writes to hInWrite → child reads from hInRead (stdin)
    // Child writes to hOutWrite (stdout) → host reads from hOutRead
    hConPTY_in_ = hInWrite;
    hConPTY_out_ = hOutRead;
    hChildStdIn_ = hInRead;
    hChildStdOut_ = hOutWrite;
    
    return true;
}

bool EmbeddedTerminal::executeCommand(const std::string& command,
                                     OutputCallback output_cb,
                                     ExitCallback exit_cb) {
    if (running_) return false;
    
    // Close previous process handle if still held
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hProcess_);
        hProcess_ = INVALID_HANDLE_VALUE;
    }
    
    // Wait for previous output thread to finish
    if (output_thread_.joinable()) {
        output_thread_.join();
    }
    
    std::string full_cmd = "cmd.exe /c " + command;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hChildStdIn_;
    si.hStdOutput = hChildStdOut_;
    si.hStdError = hChildStdOut_;
    
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessA(
        nullptr,
        const_cast<char*>(full_cmd.c_str()),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si,
        &pi
    );
    
    if (!created) return false;
    
    CloseHandle(pi.hThread);
    hProcess_ = pi.hProcess;
    running_ = true;
    
    output_thread_ = std::thread([this, output_cb]() {
        outputReader(output_cb);
    });
    
    std::thread([this, exit_cb]() {
        WaitForSingleObject(hProcess_, INFINITE);
        DWORD code;
        GetExitCodeProcess(hProcess_, &code);
        last_exit_code_ = static_cast<int>(code);
        running_ = false;
        if (exit_cb) exit_cb(last_exit_code_);
    }).detach();
    
    return true;
}

void EmbeddedTerminal::outputReader(OutputCallback cb) {
    char buffer[4096];
    DWORD bytesRead;
    while (running_ && ReadFile(hConPTY_out_, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        cb(std::string(buffer));
    }
}

std::string EmbeddedTerminal::executeAndCapture(const std::string& command,
                                               uint32_t timeout_ms) {
    std::string output;
    bool finished = false;
    
    auto cb = [&output](const std::string& data) {
        output += data;
    };
    
    if (!executeCommand(command, cb, [&finished](int) { finished = true; })) {
        return "[ERROR] Process creation failed.";
    }
    
    auto start = std::chrono::steady_clock::now();
    while (!finished) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms) {
            terminate();
            return output + "\n[TIMEOUT]";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return output;
}

void EmbeddedTerminal::terminate() {
    if (hProcess_ != INVALID_HANDLE_VALUE) {
        TerminateProcess(hProcess_, 1);
        running_ = false;
    }
    if (output_thread_.joinable()) {
        output_thread_.join();
    }
}

bool EmbeddedTerminal::sendInput(const std::string& input) {
    if (hConPTY_in_ == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(hConPTY_in_, input.data(),
                     static_cast<DWORD>(input.size()), &written, nullptr) && written > 0;
}

void EmbeddedTerminal::resize(int cols, int rows) {
    (void)cols;
    (void)rows;
    // ConPTY resize via ResizePseudoConsole would go here;
    // standard pipe mode does not support console resize.
}

bool EmbeddedTerminal::isRunning() const { return running_; }
int EmbeddedTerminal::getLastExitCode() const { return last_exit_code_; }

} // namespace rawrxd::terminal
