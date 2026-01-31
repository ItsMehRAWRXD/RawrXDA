#include "sign_binary.hpp"
#include <vector>
#include <algorithm>
#include <string>
#include <windows.h>
#include <iostream>

static std::string getEnvVar(const std::string& name) {
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, name.c_str()) == 0 && val) {
        std::string s(val);
        free(val);
        return s;
    }
    return "";
}

bool signBinary(const std::string& exePath) {
    std::string signtool = getEnvVar("SIGNTOOL_PATH");
    if (signtool.empty()) signtool = "signtool.exe";
    
    std::string cert = getEnvVar("CERT_PATH");
    std::string pass = getEnvVar("CERT_PASS");
    
    // If cert not configured, skip assume success (dev mode)
    if (cert.empty() || pass.empty()) {
        return true;
    }
    
    std::string commandLine = "\"" + signtool + "\" sign /f \"" + cert + "\" /p \"" + pass + "\" " +
                              "/fd sha256 /tr http://timestamp.digicert.com /td sha256 \"" + exePath + "\"";

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    char* cmd = _strdup(commandLine.c_str());
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmd);
        std::cerr << "Failed to run signtool" << std::endl;
        return false;
    }
    free(cmd);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000); // 60s timeout
    bool success = false;
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        std::cerr << "Signtool timed out" << std::endl;
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        success = (exitCode == 0);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return success;
}
