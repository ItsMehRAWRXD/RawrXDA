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
    if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return false;
    
    // In a production environment, we would use CreatePseudoConsole here.
    // Simplifying to standard pipes for Track B v14.7 stability.
    hConPTY_in_ = hInWrite;
    hConPTY_out_ = hOutRead;
    
    return true;
}

bool EmbeddedTerminal::executeCommand(const std::string& command,
                                     OutputCallback output_cb,
                                     ExitCallback exit_cb) {
    if (running_) return false;
    
    std::string full_cmd = "cmd.exe /c " + command;
    STARTUPINFO si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = hConPTY_in_;
    si.hStdOutput = hConPTY_out_;
    si.hStdError = hConPTY_out_;
    
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcess(
        nullptr,
        const_cast<char*>(full_cmd.c_str()),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si,
        &pi
    );
    
    if (!created) return false;
    
    hProcess_ = pi.hProcess;
    running_ = true;
    
    output_thread_ = std::thread([this, output_cb]() {
        outputReader(output_cb);
    });
    
    std::thread([this, exit_cb, pi]() {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code;
        GetExitCodeProcess(pi.hProcess, &code);
        last_exit_code_ = static_cast<int>(code);
        running_ = false;
        exit_cb(last_exit_code_);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
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
}

bool EmbeddedTerminal::isRunning() const { return running_; }
int EmbeddedTerminal::getLastExitCode() const { return last_exit_code_; }

} // namespace rawrxd::terminal
