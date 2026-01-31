#include "hot_reload.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>

HotReload::HotReload() {}

static bool runBuild(const std::string& target, int timeoutMs) {
    std::string cmd = "cmake --build build --config Release --target " + target;
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    char* cmdLine = _strdup(cmd.c_str());
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLine);
        return false;
    }
    free(cmdLine);
    
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

bool HotReload::reloadQuant(const std::string& quantType) {


    // Step 1: Rebuild only the quant library
    if (!runBuild("quant_ladder_avx2", 30000)) {
        if (onReloadFailed) onReloadFailed("Build failed or timed out for quant_ladder_avx2");
        return false;
    }


    // Step 2: Signal upper layer
    if (onQuantReloaded) onQuantReloaded(quantType);
    
    return true;
}

bool HotReload::reloadModule(const std::string& moduleName) {


    // Build specific target
    if (!runBuild(moduleName, 60000)) {
        if (onReloadFailed) onReloadFailed("Build failed or timed out for " + moduleName);
        return false;
    }
    
    if (onModuleReloaded) onModuleReloaded(moduleName);
    
    return true;
}
