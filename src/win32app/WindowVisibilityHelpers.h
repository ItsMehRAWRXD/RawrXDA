#pragma once

#include <windows.h>
#include <algorithm>
#include <cstdio>

namespace RawrXD::Win32Visibility
{
inline bool IsPlacementTelemetryEnabled()
{
    static bool initialized = false;
    static bool enabled = false;
    if (!initialized)
    {
        initialized = true;
        char value[16] = {};
        DWORD n = GetEnvironmentVariableA("RAWRXD_LOG_WINDOW_PLACEMENT", value, (DWORD)sizeof(value));
        enabled = (n > 0 && value[0] == '1');
    }
    return enabled;
}

inline void LogPlacementSnapshot(HWND hwnd, const char* phase)
{
    if (!IsPlacementTelemetryEnabled() || !hwnd || !phase)
        return;

    RECT rc = {};
    if (!GetWindowRect(hwnd, &rc))
        return;

    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    bool hasPlacement = GetWindowPlacement(hwnd, &wp) != FALSE;

    char msg[384] = {};
    if (hasPlacement)
    {
        std::snprintf(msg, sizeof(msg),
                      "[Win32Visibility] %s rect=(%ld,%ld)-(%ld,%ld) normal=(%ld,%ld)-(%ld,%ld) showCmd=%u visible=%d iconic=%d\n",
                      phase, rc.left, rc.top, rc.right, rc.bottom, wp.rcNormalPosition.left, wp.rcNormalPosition.top,
                      wp.rcNormalPosition.right, wp.rcNormalPosition.bottom, (unsigned)wp.showCmd,
                      IsWindowVisible(hwnd) ? 1 : 0, IsIconic(hwnd) ? 1 : 0);
    }
    else
    {
        std::snprintf(msg, sizeof(msg),
                      "[Win32Visibility] %s rect=(%ld,%ld)-(%ld,%ld) showCmd=unknown visible=%d iconic=%d\n",
                      phase, rc.left, rc.top, rc.right, rc.bottom, IsWindowVisible(hwnd) ? 1 : 0,
                      IsIconic(hwnd) ? 1 : 0);
    }
    OutputDebugStringA(msg);
}

inline bool NormalizePlacementForVisibility(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return false;

    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    if (!GetWindowPlacement(hwnd, &wp))
        return false;

    bool changed = false;
    if (wp.showCmd != SW_SHOWNORMAL && wp.showCmd != SW_SHOW)
    {
        wp.showCmd = SW_SHOWNORMAL;
        changed = true;
    }

    const RECT virtualDesktop = {
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)};

    const RECT wr = wp.rcNormalPosition;
    if (wr.left >= virtualDesktop.right || wr.right <= virtualDesktop.left || wr.top >= virtualDesktop.bottom ||
        wr.bottom <= virtualDesktop.top)
    {
        HMONITOR hMon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = {sizeof(mi)};
        if (hMon && GetMonitorInfoA(hMon, &mi))
        {
            const RECT& work = mi.rcWork;
            int width = (std::min)((int)(wr.right - wr.left), (int)(work.right - work.left) - 100);
            int height = (std::min)((int)(wr.bottom - wr.top), (int)(work.bottom - work.top) - 100);
            if (width < 200)
                width = (std::max)(200, (int)(work.right - work.left) - 100);
            if (height < 200)
                height = (std::max)(200, (int)(work.bottom - work.top) - 100);

            wp.rcNormalPosition = {work.left + 50, work.top + 50, work.left + 50 + width, work.top + 50 + height};
            wp.showCmd = SW_SHOWNORMAL;
            changed = true;
        }
    }

    if (changed)
        SetWindowPlacement(hwnd, &wp);

    return changed;
}
} // namespace RawrXD::Win32Visibility
