// -----------------------------------------------------------------------------
// Smoke: Tier 1 MonacoCore ghost bridge — WM_APP message contract + dispatch
// -----------------------------------------------------------------------------
// Validates (post-link / CI):
//   1. RAWRXD_WM_GHOST_EDITOR_BRIDGE stays at WM_APP+401 (no accidental drift).
//   2. wParam 0 = accept, non-zero = dismiss — same convention as Win32IDE_Core.cpp.
//   3. Posted messages reach a WndProc via DispatchMessage (mirrors Monaco → IDE).
//
// This does NOT link Win32IDE; it guards the protocol. Full accept/dismiss behavior
// is covered by manual E2E or future integration tests with a test hook.
// -----------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

#include "editor_engine.h"

namespace
{
// Same method names as Win32IDE — verifies dispatch intent, not full IDE state.
struct MockGhostEngine
{
    int acceptCalls = 0;
    int dismissCalls = 0;

    void acceptGhostText() { ++acceptCalls; }
    void dismissGhostText() { ++dismissCalls; }
};

MockGhostEngine g_engine{};
MockGhostEngine* g_enginePtr = nullptr;

// Mirrors Win32IDE_Core.cpp handling for RAWRXD_WM_GHOST_EDITOR_BRIDGE
LRESULT CALLBACK ghostBridgeTestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)hwnd;
    (void)lParam;
    switch (msg)
    {
    case RAWRXD_WM_GHOST_EDITOR_BRIDGE:
        if (!g_enginePtr)
            return 0;
        if (wParam == 0)
            g_enginePtr->acceptGhostText();
        else
            g_enginePtr->dismissGhostText();
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
}  // namespace

int main()
{
    static_assert(RAWRXD_WM_GHOST_EDITOR_BRIDGE == WM_APP + 401,
                  "RAWRXD_WM_GHOST_EDITOR_BRIDGE must match editor_engine.h (WM_APP+401)");

    // WM_GHOST_TEXT_READY is (WM_APP+400) in Win32IDE_Commands.h — must not collide.
    if (RAWRXD_WM_GHOST_EDITOR_BRIDGE == WM_APP + 400)
    {
        std::fprintf(stderr, "[test_monaco_bridge] FAIL: bridge message collides with WM_APP+400\n");
        return 2;
    }

    constexpr wchar_t kClassName[] = L"RawrXD_MonacoBridgeSmokeTest";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ghostBridgeTestWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;

    if (RegisterClassExW(&wc) == 0)
    {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS)
        {
            std::fprintf(stderr, "[test_monaco_bridge] FAIL: RegisterClassExW (%lu)\n",
                         static_cast<unsigned long>(err));
            return 3;
        }
    }

    HWND hwnd = CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (!hwnd)
    {
        std::fprintf(stderr, "[test_monaco_bridge] FAIL: CreateWindowExW (%lu)\n",
                     static_cast<unsigned long>(GetLastError()));
        return 4;
    }

    g_engine = MockGhostEngine{};
    g_enginePtr = &g_engine;

    // Same pattern as MonacoCoreEngine: PostMessageW(parent, RAWRXD_WM_GHOST_EDITOR_BRIDGE, wParam, 0)
    PostMessageW(hwnd, RAWRXD_WM_GHOST_EDITOR_BRIDGE, 0, 0);
    PostMessageW(hwnd, RAWRXD_WM_GHOST_EDITOR_BRIDGE, 1, 0);
    // Any non-zero wParam must take dismiss path (same as IDE: else dismissGhostText())
    PostMessageW(hwnd, RAWRXD_WM_GHOST_EDITOR_BRIDGE, 42, 0);

    MSG msg{};
    while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyWindow(hwnd);
    UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
    g_enginePtr = nullptr;

    if (g_engine.acceptCalls != 1 || g_engine.dismissCalls != 2)
    {
        std::fprintf(stderr,
                     "[test_monaco_bridge] FAIL: expected accept=1 dismiss=2, got accept=%d dismiss=%d\n",
                     g_engine.acceptCalls, g_engine.dismissCalls);
        return 1;
    }

    std::printf("[test_monaco_bridge] OK — WM_APP+401 dispatch (accept/dismiss) verified.\n");
    return 0;
}
