#include "code_signer.hpp"
#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

CodeSigner* CodeSigner::s_instance = nullptr;

CodeSigner* CodeSigner::instance() {
    if (!s_instance) {
        s_instance = new CodeSigner();
    }
    return s_instance;
}

CodeSigner::CodeSigner() {}

// Helper to run process
static bool runProcess(const std::string& cmd, const std::vector<std::string>& args) {
    std::string commandLine = cmd;
    for (const auto& arg : args) {
        commandLine += " \"" + arg + "\"";
    }
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    char* cmdLine = _strdup(commandLine.c_str());
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLine);
        return false;
    }
    free(cmdLine);
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

static std::string getEnv(const std::string& name) {
    char* val = nullptr;
    size_t len = 0;
    _dupenv_s(&val, &len, name.c_str());
    if (val && len > 0) {
        std::string s(val);
        free(val);
        return s;
    }
    return "";
}

bool CodeSigner::signWindowsExecutable(const std::string& exePath, 
                                       const std::string& certPath,
                                       const std::string& certPassword) {
#ifdef _WIN32
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!fs::exists(exePath)) {
        std::cerr << "[CodeSigner] Executable not found: " << exePath << std::endl;
        return false;
    }
    
    std::string password = certPassword.empty() 
        ? getEnv("CODE_SIGN_PASSWORD") 
        : certPassword;
    
    // signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /f cert.pfx /p password file.exe
    std::string signtool = "signtool.exe"; // Assumed in PATH
    std::vector<std::string> args = {"sign", "/fd", "SHA256", "/tr", "http://timestamp.digicert.com", "/td", "SHA256"};
    
    if (!certPath.empty()) {
        args.push_back("/f");
        args.push_back(certPath);
        if (!password.empty()) {
            args.push_back("/p");
            args.push_back(password);
        }
    } else {
        // Use certificate from store (auto-select best)
        args.push_back("/a");
    }
    
    args.push_back(exePath);
    
    bool success = runProcess(signtool, args);
    
    if (success) {
        std::cout << "[CodeSigner] Successfully signed: " << exePath << std::endl;
    } else {
        std::cerr << "[CodeSigner] Failed to sign: " << exePath << std::endl;
    }
    
    return success;
#else
    return false;
#endif
}

bool CodeSigner::signMacOSBundle(const std::string& bundlePath, const std::string& identity) {
    // Not implemented for Windows
    return false;
}

bool CodeSigner::verifySignature(const std::string& exePath) {
#ifdef _WIN32
    if (!fs::exists(exePath)) return false;
    
    // signtool verify /pa /v file.exe
    return runProcess("signtool.exe", {"verify", "/pa", "/v", exePath});
#else
    return false;
#endif
}
