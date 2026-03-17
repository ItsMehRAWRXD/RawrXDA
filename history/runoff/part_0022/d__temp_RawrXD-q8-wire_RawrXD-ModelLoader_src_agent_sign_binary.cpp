#include "sign_binary.hpp"
#include <windows.h>
#include <iostream>
#include <string>

namespace RawrXD {

bool signBinary(const std::string& exePath) {
    std::cout << "[SignBinary] Signing executable: " << exePath << std::endl;
    
    // signtool.exe /a /pa /t http://timestamp.digicert.com <exePath>
    std::string cmd = "signtool.exe sign /a /pa /t http://timestamp.digicert.com \"" + exePath + "\"";
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    
    std::cerr << "[SignBinary] Failed to launch signtool.exe" << std::endl;
    return false;
}

} // namespace RawrXD
