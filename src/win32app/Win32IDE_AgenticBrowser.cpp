// Win32IDE_AgenticBrowser.cpp — host child window + AgenticBrowserLayer singleton for the IDE.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "Win32IDE_AgenticBrowser.h"

#include "IDELogger.h"
#include "rawrxd/ide/AgenticBrowserLayer.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>

namespace
{

HWND g_main = nullptr;
HWND g_host = nullptr;
std::unique_ptr<RawrXD::Ide::AgenticBrowserLayer> g_layer;
bool g_visible = false;

constexpr wchar_t kClassName[] = L"RawrXDAgenticBrowserHost";

void layoutHostToBottomThird()
{
    if (!g_main || !g_host)
    {
        return;
    }
    RECT cr{};
    GetClientRect(g_main, &cr);
    int h = static_cast<int>((cr.bottom - cr.top) / 3);
    if (h < 120)
    {
        h = 120;
    }
    const int top = cr.bottom - h;
    SetWindowPos(g_host, nullptr, 0, top, cr.right - cr.left, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK hostWndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_SIZE && g_layer)
    {
        RECT rc{};
        GetClientRect(h, &rc);
        g_layer->setBounds(rc);
        return 0;
    }
    if (m == WM_DESTROY)
    {
        if (g_layer)
        {
            g_layer->destroy();
            g_layer.reset();
        }
    }
    return DefWindowProcW(h, m, w, l);
}

void ensureClassRegistered(HINSTANCE hi)
{
    static bool done = false;
    if (done)
    {
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = hostWndProc;
    wc.hInstance = hi;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        RAWRXD_LOG_ERROR("AgenticBrowser") << "RegisterClassW failed";
    }
    done = true;
}

}  // namespace

#ifdef __cplusplus
extern "C"
{
#endif

    void Win32IDE_AgenticBrowser_NotifyMainWindow(HWND hwndMain)
    {
        g_main = hwndMain;
    }

    void Win32IDE_AgenticBrowser_Toggle(void)
    {
        if (!g_main)
        {
            RAWRXD_LOG_WARNING("AgenticBrowser") << "Toggle: main HWND not set";
            return;
        }

        HINSTANCE hi = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(g_main, GWLP_HINSTANCE));
        ensureClassRegistered(hi);

        if (!g_host)
        {
            RECT cr{};
            GetClientRect(g_main, &cr);
            int h = static_cast<int>((cr.bottom - cr.top) / 3);
            if (h < 120)
            {
                h = 120;
            }
            const int top = cr.bottom - h;
            g_host = CreateWindowExW(WS_EX_STATICEDGE, kClassName, L"Agentic Browser",
                                     WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, top, cr.right - cr.left, h,
                                     g_main, nullptr, hi, nullptr);
            if (!g_host)
            {
                RAWRXD_LOG_ERROR("AgenticBrowser") << "CreateWindowExW failed";
                return;
            }

            RECT rc{};
            GetClientRect(g_host, &rc);
            g_layer = std::make_unique<RawrXD::Ide::AgenticBrowserLayer>();
            RawrXD::Ide::AgenticBrowserConfig cfg;
            if (!g_layer->create(g_host, rc, cfg))
            {
                RAWRXD_LOG_ERROR("AgenticBrowser") << "AgenticBrowserLayer::create failed";
                g_layer.reset();
                DestroyWindow(g_host);
                g_host = nullptr;
                return;
            }

            g_layer->setPageMessageHandler(
                [](const char* utf8Json, void* /*user*/)
                {
                    if (utf8Json)
                    {
                        RAWRXD_LOG_INFO("AgenticBrowser") << "page msg: " << utf8Json;
                    }
                },
                nullptr);
        }

        g_visible = !g_visible;
        ShowWindow(g_host, g_visible ? SW_SHOW : SW_HIDE);
        if (g_layer)
        {
            if (g_visible)
            {
                g_layer->show();
            }
            else
            {
                g_layer->hide();
            }
        }
        if (g_visible)
        {
            SetWindowPos(g_host, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }

    void Win32IDE_AgenticBrowser_Shutdown(void)
    {
        if (g_layer)
        {
            g_layer->destroy();
            g_layer.reset();
        }
        if (g_host)
        {
            DestroyWindow(g_host);
            g_host = nullptr;
        }
        g_visible = false;
        g_main = nullptr;
    }

    void Win32IDE_AgenticBrowser_Relayout(void)
    {
        layoutHostToBottomThird();
        if (g_host && g_layer)
        {
            RECT rc{};
            GetClientRect(g_host, &rc);
            g_layer->setBounds(rc);
        }
    }

#ifdef __cplusplus
}
#endif

RawrXD::Ide::AgenticBrowserLayer* Win32IDE_AgenticBrowser_GetLayer()
{
    return g_layer.get();
}
