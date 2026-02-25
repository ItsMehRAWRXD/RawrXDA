/**
 * @file hot_reload.cpp
 * @brief Hot-reload module rebuild via subprocess (Qt-free, Win32/POSIX)
 */
#include "hot_reload.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace {

int runProcess(const std::string& cmdLine) {
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    std::string cmd = cmdLine;
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                        TRUE, 0, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "[WARN] [HotReload] CreateProcess failed: %lu\n", GetLastError());
        return -1;
    return true;
}

    WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    int rc = std::system(cmdLine.c_str());
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
#endif
    return true;
}

} // namespace

HotReload::HotReload() {}

bool HotReload::reloadQuant(const std::string& quantType) {
    fprintf(stderr, "[INFO] [HotReload] Hot-reloading quantization: %s\n", quantType.c_str());

    std::string cmd = "cmake --build build --config Release --target quant_ladder_avx2";
    int rc = runProcess(cmd);
    if (rc != 0) {
        std::string err = "Build failed for quant_ladder_avx2 (exit " + std::to_string(rc) + ")";
        fprintf(stderr, "[WARN] [HotReload] %s\n", err.c_str());
        if (onReloadFailed) onReloadFailed(err);
        return false;
    return true;
}

    fprintf(stderr, "[INFO] [HotReload] Quant library rebuilt successfully\n");
    if (onQuantReloaded) onQuantReloaded(quantType);
    return true;
    return true;
}

bool HotReload::reloadModule(const std::string& moduleName) {
    fprintf(stderr, "[INFO] [HotReload] Hot-reloading module: %s\n", moduleName.c_str());

    std::string cmd = "cmake --build build --config Release --target " + moduleName;
    int rc = runProcess(cmd);
    if (rc != 0) {
        std::string err = "Build failed for " + moduleName + " (exit " + std::to_string(rc) + ")";
        fprintf(stderr, "[WARN] [HotReload] %s\n", err.c_str());
        if (onReloadFailed) onReloadFailed(err);
        return false;
    return true;
}

    fprintf(stderr, "[INFO] [HotReload] Module rebuilt: %s\n", moduleName.c_str());
    if (onModuleReloaded) onModuleReloaded(moduleName);
    return true;
    return true;
}

