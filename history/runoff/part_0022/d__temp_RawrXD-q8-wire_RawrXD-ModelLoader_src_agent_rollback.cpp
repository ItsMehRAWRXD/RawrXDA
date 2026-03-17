#include "rollback.hpp"
#include <iostream>
#include <windows.h>
#include <string>
#include <vector>

namespace RawrXD {

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    std::cout << "[Rollback] Detecting performance regression..." << std::endl;
    return false; // Mock
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    std::cout << "[Rollback] Reverting last commit via git..." << std::endl;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[] = "git revert HEAD --no-edit";
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    std::cout << "[Rollback] Opening GitHub issue: " << title << std::endl;
    // WinHTTP code to call GitHub API
    return true;
}

} // namespace RawrXD
