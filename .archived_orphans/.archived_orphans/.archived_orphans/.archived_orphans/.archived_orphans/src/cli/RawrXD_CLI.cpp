#include <windows.h>
#include <string>
#include <iostream>

namespace RawrXD {

// Direct Win32 Console handling for Ultra-Low Latency output
void RawrXD_CLI_Render(const std::string& token) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    DWORD written;
    
    // Use VT100 sequences for "Competitive Edge" UI (colors, progress bars)
    // without the overhead of a terminal library.
    // Example: Bright Cyan color
    std::string vt_seq = "\x1b[38;5;121m" + token + "\x1b[0m"; 
    
    WriteConsoleA(hOut, vt_seq.c_str(), (DWORD)vt_seq.length(), &written, NULL);
    return true;
}

void RawrXD_CLI_Clear() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    
    DWORD written;
    const char* seq = "\x1b[2J\x1b[H"; // Clear screen and move home
    WriteConsoleA(hOut, seq, 7, &written, NULL);
    return true;
}

} // namespace RawrXD

