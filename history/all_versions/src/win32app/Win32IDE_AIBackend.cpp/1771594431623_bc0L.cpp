// Win32IDE_AIBackend.cpp - AI Backend Verification & Production Deployment Integration
// Implements: onAIBackendVerified(), updateToolsMenu(), initializeAIBackend()
// Leverages existing: VerifyAIBackend(), runDebugTestAI() (in Win32IDE_Core.cpp)

#include "Win32IDE.h"
#include <string>
#include <thread>

#ifndef IDM_DEBUG_TEST_AI
#define IDM_DEBUG_TEST_AI 5190
#endif

// ============================================================================
// onAIBackendVerified - Handle async verification result
// Called from WM_AI_BACKEND_VERIFIED / WM_AI_BACKEND_FAILED message handlers
// Updates m_aiAvailable, status bar, and dynamically enables/disables menu items
// ============================================================================
void Win32IDE::onAIBackendVerified(bool success)
{
    m_aiAvailable = success;

    // Update status bar
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
    {
        const char* text = success
            ? "AI: Connected (Ollama)"
            : "AI: Offline (start `ollama serve`)";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)text);
    }

    // Enable/disable AI-related menu items
    updateToolsMenu();

    // Log
    OutputDebugStringA(success
        ? "[AIBackend] Verification succeeded — AI available\n"
        : "[AIBackend] Verification failed — AI offline\n");
}

// ============================================================================
// updateToolsMenu - Dynamically update "Test AI Backend" menu item state
// Inserts the item if not present; enables/disables based on m_aiAvailable
// ============================================================================
void Win32IDE::updateToolsMenu()
{
    if (!m_hMenu)
        return;

    // Find the Tools menu by searching top-level menu bar
    HMENU hToolsMenu = nullptr;
    int menuCount = GetMenuItemCount(m_hMenu);
    for (int i = 0; i < menuCount; ++i)
    {
        wchar_t buf[64] = {};
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STRING | MIIM_SUBMENU;
        mii.dwTypeData = buf;
        mii.cch = _countof(buf);
        if (GetMenuItemInfoW(m_hMenu, i, TRUE, &mii) && mii.hSubMenu)
        {
            // Check for "&Tools" label
            if (wcsstr(buf, L"Tools") != nullptr)
            {
                hToolsMenu = mii.hSubMenu;
                break;
            }
        }
    }

    if (!hToolsMenu)
        return;

    // Check if IDM_DEBUG_TEST_AI already exists in Tools menu
    MENUITEMINFOW check = {};
    check.cbSize = sizeof(check);
    check.fMask = MIIM_ID;
    bool exists = false;

    int itemCount = GetMenuItemCount(hToolsMenu);
    for (int i = 0; i < itemCount; ++i)
    {
        if (GetMenuItemInfoW(hToolsMenu, i, TRUE, &check) && check.wID == IDM_DEBUG_TEST_AI)
        {
            exists = true;
            break;
        }
    }

    if (!exists)
    {
        // Insert separator + "Test AI Backend" after SLO Dashboard
        AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hToolsMenu, MF_STRING, IDM_DEBUG_TEST_AI,
                    L"\U0001F9E0 &Test AI Backend\tCtrl+Shift+T");
    }

    // Enable/disable based on availability
    UINT flags = m_aiAvailable ? (MF_BYCOMMAND | MF_ENABLED) : (MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(hToolsMenu, IDM_DEBUG_TEST_AI, flags);

    // Refresh
    DrawMenuBar(m_hwndMain);
}

// ============================================================================
// initializeAIBackend - Deferred initialization hook
// Called from deferredHeavyInitBody() after all subsystems are ready
// Spawns background verification and sets up UI state
// ============================================================================
void Win32IDE::initializeAIBackend()
{
    OutputDebugStringA("[AIBackend] Initializing AI backend integration...\n");

    // Default state — assume offline until verified
    m_aiAvailable = false;

    // Verify in background thread (non-blocking)
    // VerifyAIBackend() is already designed for background use — it posts
    // WM_AI_BACKEND_STATUS which is handled by the existing WindowProc
    // dispatcher in Win32IDE_Core.cpp (line ~615).
    //
    // We wrap it here to ensure the onAIBackendVerified() handler is also
    // called when the result arrives.
    HWND hwnd = m_hwndMain;
    std::string endpoint = m_ollamaBaseUrl.empty() ? "http://localhost:11434" : m_ollamaBaseUrl;

    std::thread([hwnd, endpoint]() {
        ModelConnection conn(endpoint);
        bool ok = conn.checkConnection();

        // Post result — this triggers WindowProc → onAIBackendVerified()
        if (hwnd && IsWindow(hwnd))
        {
            PostMessage(hwnd, WM_AI_BACKEND_STATUS, ok ? 1 : 0, 0);
        }
    }).detach();

    OutputDebugStringA("[AIBackend] Background verification thread spawned\n");
}
