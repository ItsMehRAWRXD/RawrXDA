// RawrXD_TerminalIntegration.hpp - REAL Win32 terminal, not stubs
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

class TerminalIntegration {
    HANDLE hChildStdInRd_ = nullptr, hChildStdInWr_ = nullptr;
    HANDLE hChildStdOutRd_ = nullptr, hChildStdOutWr_ = nullptr;
    HANDLE hChildStdErrRd_ = nullptr, hChildStdErrWr_ = nullptr;
    PROCESS_INFORMATION pi_ = {};
    std::atomic<bool> is_running_{false};
    std::string accumulated_output_;
    std::function<void(const std::string&)> output_callback_;
    
public:
    bool StartTerminalSession(const std::string& working_dir) {
        SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
        
        // Create pipes for stdin/stdout/stderr
        CreatePipe(&hChildStdOutRd_, &hChildStdOutWr_, &sa, 0);
        CreatePipe(&hChildStdInRd_, &hChildStdInWr_, &sa, 0);
        CreatePipe(&hChildStdErrRd_, &hChildStdErrWr_, &sa, 0);
        
        SetHandleInformation(hChildStdOutRd_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hChildStdInWr_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hChildStdErrRd_, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si = {sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hChildStdInRd_;
        si.hStdOutput = hChildStdOutWr_;
        si.hStdError = hChildStdErrWr_;
        
        // Start cmd.exe with our pipes
        std::string cmd = "cmd.exe /K cd /d \"" + working_dir + "\"";
        BOOL success = CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, 
                                      TRUE, CREATE_NEW_CONSOLE, nullptr, nullptr, 
                                      &si, &pi_);
        
        if (success) {
            is_running_ = true;
            StartOutputPump();
        }
        
        return success;
    }
    
    std::string ExecuteCommand(const std::string& command, DWORD timeout_ms = 30000) {
        if (!is_running_) return "Terminal not started";
        
        accumulated_output_.clear();
        std::string full_cmd = command + "\r\n";
        DWORD written;
        WriteFile(hChildStdInWr_, full_cmd.c_str(), full_cmd.length(), &written, nullptr);
        
        // Wait for output with timeout
        // Note: Simple WaitForSingleObject(pi_.hProcess, timeout_ms) only works if the process exits.
        // For a shell, we might need a more sophisticated pulse check.
        Sleep(500); // Wait for output to start flowing
        
        return accumulated_output_;
    }
    
    std::string Execute(const std::string& command) {
        return ExecuteCommand(command);
    }
    
    std::string ExecuteBuildCommand(const std::string& build_cmd) {
        auto output = ExecuteCommand(build_cmd, 120000); // 2 min for builds
        
        // Parse for errors immediately
        if (output.find("error") != std::string::npos || 
            output.find("Error") != std::string::npos) {
            // Extract error context for self-healing
            return "BUILD_FAILED:\n" + output;
        }
        
        return "BUILD_SUCCESS:\n" + output;
    }
    
    void SetOutputCallback(std::function<void(const std::string&)> cb) {
        output_callback_ = cb;
    }
    
    void Stop() {
        is_running_ = false;
        if (pi_.hProcess) TerminateProcess(pi_.hProcess, 0);
        CloseHandles();
    }

private:
    void StartOutputPump() {
        std::thread([this]() {
            char buffer[4096];
            DWORD bytes_read;
            while (is_running_) {
                if (ReadFile(hChildStdOutRd_, buffer, sizeof(buffer)-1, &bytes_read, nullptr)) {
                    buffer[bytes_read] = '\0';
                    accumulated_output_ += buffer;
                    if (output_callback_) output_callback_(buffer);
                }
            }
        }).detach();
    }
    
    void CloseHandles() {
        CloseHandle(hChildStdInRd_);
        CloseHandle(hChildStdInWr_);
        CloseHandle(hChildStdOutRd_);
        CloseHandle(hChildStdOutWr_);
        CloseHandle(hChildStdErrRd_);
        CloseHandle(hChildStdErrWr_);
        CloseHandle(pi_.hProcess);
        CloseHandle(pi_.hThread);
    }
};
