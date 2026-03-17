#include "code_signer.hpp"
#include "license_enforcement.h"
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <sstream>

namespace fs = std::filesystem;

CodeSigner* CodeSigner::s_instance = nullptr;

CodeSigner* CodeSigner::instance() {
    if (!s_instance) {
        s_instance = new CodeSigner();
    }
    return s_instance;
}

CodeSigner::CodeSigner() {}
CodeSigner::~CodeSigner() {}

bool CodeSigner::signWindowsExecutable(const std::string& exePath, 
                                       const std::string& certPath,
                                       const std::string& certPassword) {
    auto start = std::chrono::steady_clock::now();
    
    if (!fs::exists(exePath)) {
        return false;
    }
    
    std::string password = certPassword;
    if (password.empty()) {
        char* envPwd = std::getenv("CODE_SIGN_PASSWORD");
        if (envPwd) password = envPwd;
    }
    
    std::vector<std::string> args;
    args.push_back("sign");
    args.push_back("/fd"); args.push_back("SHA256");
    args.push_back("/tr"); args.push_back("http://timestamp.digicert.com");
    args.push_back("/td"); args.push_back("SHA256");
    
    if (!certPath.empty()) {
        args.push_back("/f"); args.push_back(certPath);
        if (!password.empty()) {
            args.push_back("/p"); args.push_back(password);
        }
    } else {
        args.push_back("/a");
    }
    
    args.push_back(exePath);
    
    bool success = executeCommand("signtool.exe", args);
    
    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    if (m_onSignatureCompleted) {
        m_onSignatureCompleted(exePath, success);
    }
    
    return success;
}

bool CodeSigner::verifySignature(const std::string& exePath) {
    std::vector<std::string> args;
    args.push_back("verify");
    args.push_back("/pa");
    args.push_back(exePath);
    return executeCommand("signtool.exe", args);
}

bool CodeSigner::signMacOSBundle(const std::string& bundlePath, const std::string& identity) {
    return false; // Windows implementation only for now
}

bool CodeSigner::executeCommand(const std::string& command, const std::vector<std::string>& args) {
    std::string cmdLine = "\"" + command + "\"";
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    }

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    if (!CreateProcessA(NULL, (char*)cmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, 300000); // 5 min timeout
    bool success = false;
    if (wait == WAIT_OBJECT_0) {
        DWORD exitCode;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            success = (exitCode == 0);
        }
    } else {
        TerminateProcess(pi.hProcess, 1);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return success;
}
