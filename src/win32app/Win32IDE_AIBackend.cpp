// Win32IDE_AIBackend.cpp - AI Backend Verification & Production Deployment Integration
// Implements: onAIBackendVerified(), updateToolsMenu(), initializeAIBackend()
// Leverages existing: VerifyAIBackend(), runDebugTestAI() (in Win32IDE_Core.cpp)

#include "Win32IDE.h"
#include <string>
#include <thread>
#include <cstdlib>
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <string>
#include <windows.h>

#ifndef IDM_DEBUG_TEST_AI
#define IDM_DEBUG_TEST_AI 5190
#endif

// ============================================================================
// ModelConnection - Simple HTTP client for Ollama API verification
// ============================================================================
class ModelConnection {
private:
    std::string baseUrl;

public:
    ModelConnection(const std::string& url) : baseUrl(url) {}

    bool checkConnection() {
        bool secure = false;
        std::string host = "localhost";
        INTERNET_PORT port = 11434;
        std::string path = "/api/tags";

        std::string endpoint = baseUrl;
        if (!endpoint.empty()) {
            if (endpoint.rfind("https://", 0) == 0) {
                secure = true;
                endpoint.erase(0, 8);
                if (port == 11434) port = 443;
            } else if (endpoint.rfind("http://", 0) == 0) {
                secure = false;
                endpoint.erase(0, 7);
            }

            size_t slash = endpoint.find('/');
            std::string hostPort = (slash == std::string::npos) ? endpoint : endpoint.substr(0, slash);
            if (slash != std::string::npos) {
                path = endpoint.substr(slash);
                if (path.empty()) path = "/api/tags";
            }

            size_t colon = hostPort.find(':');
            if (colon == std::string::npos) {
                if (!hostPort.empty()) host = hostPort;
            } else {
                host = hostPort.substr(0, colon);
                const std::string portText = hostPort.substr(colon + 1);
                if (!portText.empty()) {
                    unsigned long parsed = std::strtoul(portText.c_str(), nullptr, 10);
                    if (parsed > 0 && parsed <= 65535) {
                        port = static_cast<INTERNET_PORT>(parsed);
                    }
                }
            }
        }

        std::wstring wHost(host.begin(), host.end());
        std::wstring wPath(path.begin(), path.end());

        HINTERNET hSession = WinHttpOpen(L"Win32IDE/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;
        WinHttpSetTimeouts(hSession, 1500, 1500, 3000, 3000);

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), NULL,
                                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                secure ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        bool result = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (result) {
            result = WinHttpReceiveResponse(hRequest, NULL);
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        if (result) {
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                NULL, &statusCode, &statusSize, NULL);
            result = (statusCode == 200);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return result;
    }
};

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
        std::string backendName = getActiveBackendName();
        const std::string msg = success
            ? "AI: Connected (" + backendName + ")"
            : "AI: Offline (" + backendName + " unavailable)";
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)msg.c_str());
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

