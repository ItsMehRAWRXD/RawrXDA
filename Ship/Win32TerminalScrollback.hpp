#pragma once
// Shared RichEdit scrollback cap for integrated terminal + PowerShell panel (VS Code–class buffers).

#include <cstdint>
#include <cstdlib>
#include <windows.h>
#include <richedit.h>

namespace RawrXD {
    // Terminal scrollback size constants (in characters)
    constexpr uint32_t MinTerminalScrollbackChars = 262144u;    // 256 KB minimum
    constexpr uint32_t MaxTerminalScrollbackChars = 16777216u;  // 16 MB maximum
} // namespace RawrXD

inline void RawrXD_ApplyTerminalRichEditScrollback(HWND hwnd, uint32_t settingsCapChars)
{
    if (!hwnd || !IsWindow(hwnd))
        return;
    uint32_t capChars = settingsCapChars;
    char envCap[32]{};
    if (GetEnvironmentVariableA("RAWRXD_TERMINAL_SCROLLBACK_CHARS", envCap, sizeof(envCap)) > 0)
        capChars = static_cast<uint32_t>(strtoul(envCap, nullptr, 10));
    if (capChars < RawrXD::MinTerminalScrollbackChars)
        capChars = RawrXD::MinTerminalScrollbackChars;
    if (capChars > RawrXD::MaxTerminalScrollbackChars)
        capChars = RawrXD::MaxTerminalScrollbackChars;
    SendMessage(hwnd, EM_EXLIMITTEXT, 0, static_cast<LPARAM>(capChars));
}
