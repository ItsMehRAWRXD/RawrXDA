#include "TitanLoaderDiagnostics.h"
#include <windows.h>
#include <iostream>

using namespace RawrXD::Inference;

TitanDiagnostics TitanDiagnostics::probe() {
    TitanDiagnostics diag{};
    HMODULE hMod = GetModuleHandleA("RawrXD_Titan.dll");
    if (!hMod) hMod = LoadLibraryA("RawrXD_Titan.dll");
    
    if (hMod) {
        diag.dll_present = true;
        diag.proc_table_valid = GetProcAddress(hMod, "Titan_Init") != nullptr;
        // In a real implementation, we'd query versioning and CPU features via exported calls
    } else {
        diag.dll_present = false;
        char buf[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       buf, (sizeof(buf) / sizeof(char)), NULL);
        diag.last_error = buf;
    }
    
    return diag;
}

void TitanDiagnostics::alert_user_on_fallback(const TitanDiagnostics& diag) {
    std::cerr << "[CRITICAL] RawrXD Titan ASM Backend unavailable. Falling back to C++ reference implementation." << std::endl;
    if (!diag.dll_present) {
        std::cerr << "Reason: RawrXD_Titan.dll not found in search path. Error: " << diag.last_error << std::endl;
    } else if (!diag.proc_table_valid) {
        std::cerr << "Reason: RawrXD_Titan.dll is corrupted or version mismatch (proc table invalid)." << std::endl;
    }
}
