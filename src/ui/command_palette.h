// ============================================================================
// command_palette.h — RawrXD Command Palette (Ctrl+Shift+P)
// ============================================================================
// Pure Win32 implementation. Fuzzy-searches CommandRegistry entries and
// dispatches selection back through CommandRegistry::execute().
// Available to end users, extensions, and agentic code alike.
// ============================================================================
#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <string>
#include <vector>
#include "command_registry.h"

// ---- Palette configuration ------------------------------------------------
struct CommandPaletteConfig {
    int   maxResults       = 50;     // Maximum items shown in list
    bool  showCategory     = true;   // "File: New File" vs "New File"
    bool  showKeybinding   = true;   // Show keybinding hint in list
    bool  fuzzyMatch       = true;   // Fuzzy vs prefix matching
    bool  showRecentFirst  = true;   // Float recently used commands to top
};

// ---- Result entry ---------------------------------------------------------
struct PaletteEntry {
    std::string id;
    std::string display;      // Full display label
    std::string keybinding;
    float       score;        // Match score for sorting
};

// ---- Main palette class ---------------------------------------------------
class CommandPalette {
public:
    explicit CommandPalette(HWND parentHwnd,
                            const CommandPaletteConfig& cfg = {});
    ~CommandPalette();

    // Show the palette modal dialog.
    // Returns the selected command ID, or empty string if dismissed.
    std::string show(const std::string& prefill = "");

    // Non-modal: show the palette and execute the selected command when picked.
    void showAndExecute(const std::string& prefill = "");

    // Programmatic query — used by agentic code without showing the UI.
    std::vector<PaletteEntry> query(const std::string& input,
                                    unsigned int accessFilter = CMD_ACCESS_PALETTE) const;

    // Register the window class (call once per process).
    static bool registerWindowClass(HINSTANCE hInstance);

    // Win32 dialog procedure (public for registration).
    static INT_PTR CALLBACK dialogProc(HWND hDlg, UINT msg,
                                       WPARAM wParam, LPARAM lParam);

private:
    struct Impl;
    Impl* m_impl;
};

// ---- Palette hot-key installation helper ----------------------------------
// Call after main window is created to intercept Ctrl+Shift+P and Ctrl+P.
bool InstallCommandPaletteHotkeys(HWND mainHwnd);

// ---- Handle WM_COMMAND dispatch through the registry --------------------
// Drop this call into your WM_COMMAND handler to automatically dispatch
// all mapped menu IDs through CommandRegistry.
bool HandleMenuCommandViaRegistry(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
