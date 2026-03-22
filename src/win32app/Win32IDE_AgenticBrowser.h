#pragma once

// Win32IDE bridge: embedded Agentic Browser pane (WebView2) + C API for main_win32 / commands.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /// Remember the IDE main HWND (required before toggle).
    void Win32IDE_AgenticBrowser_NotifyMainWindow(HWND hwndMain);

    /// Create host + WebView2 on first call; subsequent calls show/hide the bottom pane.
    void Win32IDE_AgenticBrowser_Toggle(void);

    /// Release WebView2 and host window before process exit.
    void Win32IDE_AgenticBrowser_Shutdown(void);

    /// Reposition the host to the bottom third of the main window (call from WM_SIZE / onSize).
    void Win32IDE_AgenticBrowser_Relayout(void);

#ifdef __cplusplus
}

namespace RawrXD::Ide
{
class AgenticBrowserLayer;
}
/// C++ access for command handlers / agent wiring (may be nullptr if never toggled on).
RawrXD::Ide::AgenticBrowserLayer* Win32IDE_AgenticBrowser_GetLayer(void);

#endif
