// ============================================================================
// Win32IDE_Instructions.cpp — Production Instructions Context Panel
// Phase 34: Read tools.instructions.md (All Lines) → Context for GUI
//
// Provides:
//   - initInstructionsProvider()  — loads on IDE startup
//   - showInstructionsDialog()    — displays in scrollable dialog
//   - getInstructionsContent()    — returns content for programmatic use
//   - handleInstructionsCommand() — routes IDM_INSTRUCTIONS_* menu commands
// ============================================================================

#include "Win32IDE.h"
#include "../core/instructions_provider.hpp"
#include <commctrl.h>
#include <sstream>
#include <algorithm>

// ============================================================================
// Initialization — call during IDE startup
// ============================================================================
void Win32IDE::initInstructionsProvider() {
    if (m_instructionsInitialized) return;

    auto& provider = InstructionsProvider::instance();

    // Add workspace-specific search paths
    // (The provider already has defaults from setDefaultSearchPaths)
    provider.addSearchPath(".");
    provider.addSearchPath(".github");

    // Attempt to load
    auto result = provider.loadAll();
    if (result.success) {
        LOG_INFO("Instructions provider: loaded " +
                 std::to_string(provider.getLoadedCount()) + " files");
    } else {
        LOG_WARNING("Instructions provider: " + std::string(result.detail));
    }

    m_instructionsInitialized = true;
}

// ============================================================================
// Get content for programmatic use
// ============================================================================
std::string Win32IDE::getInstructionsContent() const {
    auto& provider = InstructionsProvider::instance();
    if (!provider.isLoaded()) {
        // loadAll is idempotent — non-const call via mutable singleton
        InstructionsProvider::instance().loadAll();
    }
    return provider.getAllContent();
}

// ============================================================================
// Win32 Dialog — show instructions in a scrollable read-only edit control
// ============================================================================

// Dialog procedure (static, not a member)
static INT_PTR CALLBACK InstructionsDialogProc(HWND hDlg, UINT msg,
                                                WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG: {
        // Center the dialog
        RECT rc;
        GetWindowRect(hDlg, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        int sx = GetSystemMetrics(SM_CXSCREEN);
        int sy = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(hDlg, HWND_TOP, (sx - w) / 2, (sy - h) / 2, 0, 0,
                     SWP_NOSIZE);

        // Set content from lParam (passed as string pointer)
        const char* content = reinterpret_cast<const char*>(lParam);
        if (content) {
            HWND hEdit = GetDlgItem(hDlg, 101);
            if (hEdit) {
                // Convert LF to CRLF for proper display in edit control
                std::string text(content);
                std::string crlf;
                crlf.reserve(text.size() + text.size() / 10);
                for (size_t i = 0; i < text.size(); i++) {
                    if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
                        crlf += "\r\n";
                    } else {
                        crlf += text[i];
                    }
                }
                SetWindowTextA(hEdit, crlf.c_str());

                // Set monospace font
                HFONT hFont = CreateFontA(
                    -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
                if (hFont) {
                    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
                }
            }

            // Set title with file count
            auto& provider = InstructionsProvider::instance();
            std::string title = "Production Instructions (" +
                std::to_string(provider.getLoadedCount()) + " files, " +
                std::to_string(strlen(content)) + " bytes)";
            SetWindowTextA(hDlg, title.c_str());
        }
        return TRUE;
    }

    case WM_SIZE: {
        HWND hEdit = GetDlgItem(hDlg, 101);
        HWND hClose = GetDlgItem(hDlg, IDCANCEL);
        HWND hReload = GetDlgItem(hDlg, 102);
        HWND hCopy = GetDlgItem(hDlg, 103);
        if (hEdit) {
            RECT rc;
            GetClientRect(hDlg, &rc);
            int btnH = 30;
            int pad = 8;
            MoveWindow(hEdit, pad, pad,
                       rc.right - 2 * pad, rc.bottom - btnH - 3 * pad, TRUE);
            int btnW = 100;
            int btnY = rc.bottom - btnH - pad;
            if (hClose)
                MoveWindow(hClose, rc.right - btnW - pad, btnY, btnW, btnH, TRUE);
            if (hReload)
                MoveWindow(hReload, pad, btnY, btnW, btnH, TRUE);
            if (hCopy)
                MoveWindow(hCopy, pad + btnW + pad, btnY, btnW, btnH, TRUE);
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        case 102: { // Reload
            auto& provider = InstructionsProvider::instance();
            auto r = provider.reload();
            if (r.success) {
                std::string content = provider.getAllContent();
                // Convert LF to CRLF
                std::string crlf;
                crlf.reserve(content.size() + content.size() / 10);
                for (size_t i = 0; i < content.size(); i++) {
                    if (content[i] == '\n' && (i == 0 || content[i - 1] != '\r')) {
                        crlf += "\r\n";
                    } else {
                        crlf += content[i];
                    }
                }
                HWND hEdit = GetDlgItem(hDlg, 101);
                if (hEdit) SetWindowTextA(hEdit, crlf.c_str());
                std::string title = "Production Instructions (" +
                    std::to_string(provider.getLoadedCount()) + " files, " +
                    std::to_string(content.size()) + " bytes) — Reloaded";
                SetWindowTextA(hDlg, title.c_str());
            } else {
                MessageBoxA(hDlg, r.detail, "Reload Failed", MB_ICONERROR);
            }
            return TRUE;
        }
        case 103: { // Copy to clipboard
            HWND hEdit = GetDlgItem(hDlg, 101);
            if (hEdit) {
                int len = GetWindowTextLengthA(hEdit);
                if (len > 0) {
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
                    if (hMem) {
                        char* pMem = (char*)GlobalLock(hMem);
                        if (pMem) {
                            GetWindowTextA(hEdit, pMem, len + 1);
                            GlobalUnlock(hMem);
                            if (OpenClipboard(hDlg)) {
                                EmptyClipboard();
                                SetClipboardData(CF_TEXT, hMem);
                                CloseClipboard();
                                MessageBoxA(hDlg, "Instructions copied to clipboard.",
                                           "Copy", MB_ICONINFORMATION);
                            } else {
                                GlobalFree(hMem);
                            }
                        } else {
                            GlobalFree(hMem);
                        }
                    }
                }
            }
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void Win32IDE::showInstructionsDialog() {
    // Ensure loaded
    if (!m_instructionsInitialized) {
        initInstructionsProvider();
    }

    auto& provider = InstructionsProvider::instance();
    if (!provider.isLoaded()) {
        provider.loadAll();
    }

    std::string content = provider.getAllContent();
    if (content.empty()) {
        MessageBoxA(m_hwndMain, 
            "No instruction files found.\n\n"
            "Expected: tools.instructions.md\n"
            "Search paths include:\n"
            "  - Executable directory\n"
            "  - ~/.aitk/instructions/\n"
            "  - .github/\n"
            "  - Current directory",
            "Instructions Not Found", MB_ICONWARNING);
        return;
    }

    // Create dialog template in memory
    // Layout: multiline edit (101) + reload button (102) + copy button (103) + close (IDCANCEL)
    struct {
        DLGTEMPLATE dlg;
        WORD noMenu;
        WORD noClass;
        WORD noTitle;
    } dlgBuf = {};

    dlgBuf.dlg.style = DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU |
                        WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    dlgBuf.dlg.cdit = 0;
    dlgBuf.dlg.x = 0;
    dlgBuf.dlg.y = 0;
    dlgBuf.dlg.cx = 400;
    dlgBuf.dlg.cy = 300;

    // Create modeless dialog from template, but we use custom window instead
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "#32770", // Dialog class
        "Production Instructions",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
        WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
        m_hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!hDlg) {
        // Fallback: use MessageBox with truncated content
        std::string preview = content.substr(0, 4000);
        if (content.size() > 4000) preview += "\n\n... (truncated, " + 
            std::to_string(content.size()) + " bytes total)";
        MessageBoxA(m_hwndMain, preview.c_str(), "Production Instructions", MB_OK);
        return;
    }

    // Set the dialog proc
    SetWindowLongPtrA(hDlg, GWLP_WNDPROC,
                      (LONG_PTR)InstructionsDialogProc);

    // Create child controls
    HINSTANCE hInst = GetModuleHandle(nullptr);
    RECT rc;
    GetClientRect(hDlg, &rc);

    int pad = 8;
    int btnH = 30;
    int btnW = 100;

    // Multiline edit control (scrollable, read-only)
    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
        pad, pad, rc.right - 2 * pad, rc.bottom - btnH - 3 * pad,
        hDlg, (HMENU)101, hInst, nullptr);

    // Reload button
    CreateWindowExA(0, "BUTTON", "Reload",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        pad, rc.bottom - btnH - pad, btnW, btnH,
        hDlg, (HMENU)102, hInst, nullptr);

    // Copy button
    CreateWindowExA(0, "BUTTON", "Copy All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        pad + btnW + pad, rc.bottom - btnH - pad, btnW, btnH,
        hDlg, (HMENU)103, hInst, nullptr);

    // Close button
    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        rc.right - btnW - pad, rc.bottom - btnH - pad, btnW, btnH,
        hDlg, (HMENU)IDCANCEL, hInst, nullptr);

    // Set content
    if (hEdit) {
        // Convert LF to CRLF
        std::string crlf;
        crlf.reserve(content.size() + content.size() / 10);
        for (size_t i = 0; i < content.size(); i++) {
            if (content[i] == '\n' && (i == 0 || content[i - 1] != '\r')) {
                crlf += "\r\n";
            } else {
                crlf += content[i];
            }
        }
        SetWindowTextA(hEdit, crlf.c_str());

        // Monospace font
        HFONT hFont = CreateFontA(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        if (hFont) {
            SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        }
    }

    // Update title
    std::string title = "Production Instructions (" +
        std::to_string(provider.getLoadedCount()) + " files, " +
        std::to_string(content.size()) + " bytes)";
    SetWindowTextA(hDlg, title.c_str());

    // Center
    int w = 900, h = 700;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hDlg, HWND_TOP, (sx - w) / 2, (sy - h) / 2, w, h, 0);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}
