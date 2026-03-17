// Win32IDE_Watchdog.cpp
// Parity-Audit: Visibility Watchdog
//
// Starts a low-priority background thread that monitors the main window every
// 2 000 ms and automatically corrects three degenerate states:
//
//   1. Hidden/minimized  — Restores & shows the window.
//   2. Off-screen        — Recenters if the window rect is completely outside
//                          the combined virtual-desktop work area.
//   3. Collapsed layout  — Sends a synthetic WM_SIZE when the client rect is
//                          degenerate (< 400 x 300) so onSize() re-clamps.
//
// The thread is started from startVisibilityWatchdog() (called by the init
// sequence after showWindow) and stopped from stopVisibilityWatchdog()
// (called from onDestroy so it never outlives the HWND).

#include "Win32IDE.h"
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: check whether a window rect is completely outside any monitor's
//  work area.  Returns true if the window should be recentered.
// ─────────────────────────────────────────────────────────────────────────────
static bool IsWindowOffScreen(HWND hwnd)
{
    RECT wr = {};
    if (!GetWindowRect(hwnd, &wr)) return false;

    // HMONITOR nearest to the window — if the intersection is empty, off-screen.
    HMONITOR hMon = MonitorFromRect(&wr, MONITOR_DEFAULTTONULL);
    if (hMon == nullptr) return true;      // NULL = no overlap with any monitor

    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoA(hMon, &mi)) return false;

    // Require at least 100 px of the title bar to be on-screen
    RECT intersect = {};
    RECT titleBarStrip = { wr.left, wr.top, wr.right, wr.top + 30 };
    return !IntersectRect(&intersect, &titleBarStrip, &mi.rcWork);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Recenter the window on the primary monitor's work area.
// ─────────────────────────────────────────────────────────────────────────────
static void RecenterWindow(HWND hwnd)
{
    RECT wa = {};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
    int waW = wa.right  - wa.left;
    int waH = wa.bottom - wa.top;

    RECT wr = {};
    GetWindowRect(hwnd, &wr);
    int winW = wr.right  - wr.left;
    int winH = wr.bottom - wr.top;

    int newX = wa.left + (waW - winW) / 2;
    int newY = wa.top  + (waH - winH) / 2;

    SetWindowPos(hwnd, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    OutputDebugStringA("[Watchdog] Window recentered\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Thread body — runs every 2 000 ms
// ─────────────────────────────────────────────────────────────────────────────
DWORD WINAPI Win32IDE::VisibilityWatchdogThread(LPVOID param)
{
    Win32IDE* ide = static_cast<Win32IDE*>(param);
    assert(ide != nullptr);

    OutputDebugStringA("[Watchdog] Thread started\n");

    while (InterlockedCompareExchange(&ide->m_watchdogRunning, 1, 1) == 1) {
        Sleep(2000);

        HWND hwnd = ide->m_hwndMain;
        if (!hwnd || !IsWindow(hwnd)) break;

        // Re-check stop flag after returning from sleep — IDE may have started
        // shutdown during the 2 s wait.  Exit immediately rather than touching
        // the (potentially partially destroyed) window hierarchy.
        if (InterlockedCompareExchange(&ide->m_watchdogRunning, 1, 1) != 1) break;
        if (ide->m_shuttingDown.load(std::memory_order_acquire))            break;

        // ── 1. Minimized recovery ────────────────────────────────────────────
        if (IsIconic(hwnd)) {
            OutputDebugStringA("[Watchdog] Window minimized — restoring\n");
            PostMessage(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        }

        // ── 2. Hidden recovery ───────────────────────────────────────────────
        if (!IsWindowVisible(hwnd)) {
            OutputDebugStringA("[Watchdog] Window hidden — showing\n");
            // ShowWindow must run on the UI thread; post a custom message that
            // Win32IDE::handleMessage() will process without blocking here.
            PostMessage(hwnd, WM_APP + 1, SW_SHOW, 0);
        }

        // ── 3. Off-screen recovery ───────────────────────────────────────────
        if (IsWindowOffScreen(hwnd)) {
            OutputDebugStringA("[Watchdog] Window off-screen — recentering\n");
            RecenterWindow(hwnd);   // SetWindowPos — always safe cross-thread
        }

        // ── 4. Degenerate client rect — force re-layout ──────────────────────
        // SAFETY: Use PostMessage instead of SendMessage.
        // SendMessage would block this thread until the UI thread processes
        // WM_SIZE.  If onDestroy() is simultaneously waiting for this thread
        // to exit (WaitForSingleObject in stopVisibilityWatchdog), both threads
        // deadlock.  PostMessage is fire-and-forget and never blocks.
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        int cw = rc.right  - rc.left;
        int ch = rc.bottom - rc.top;
        if (cw < 400 || ch < 300) {
            char warn[128];
            snprintf(warn, sizeof(warn),
                     "[Watchdog] Collapsed client rect %dx%d — posting WM_SIZE\n", cw, ch);
            OutputDebugStringA(warn);
            SetWindowPos(hwnd, nullptr, 0, 0, 1280, 800,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            // Post rather than Send — avoids deadlock with stopVisibilityWatchdog()
            PostMessage(hwnd, WM_SIZE, SIZE_RESTORED,
                        MAKELPARAM(1280, 800));
        }
    }

    OutputDebugStringA("[Watchdog] Thread exiting\n");
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public control API
// ─────────────────────────────────────────────────────────────────────────────
void Win32IDE::startVisibilityWatchdog()
{
    if (m_watchdogThread) return;   // already running

    InterlockedExchange(&m_watchdogRunning, 1);
    m_watchdogThread = CreateThread(
        nullptr,
        0,
        VisibilityWatchdogThread,
        this,
        0,
        nullptr
    );

    if (m_watchdogThread) {
        // Run below normal so the watchdog never competes with UI work
        SetThreadPriority(m_watchdogThread, THREAD_PRIORITY_BELOW_NORMAL);
        OutputDebugStringA("[Watchdog] Started (2 s interval)\n");
    } else {
        m_watchdogRunning = 0;
        OutputDebugStringA("[Watchdog] CreateThread failed — watchdog disabled\n");
    }
}

void Win32IDE::stopVisibilityWatchdog()
{
    if (!m_watchdogThread) return;

    InterlockedExchange(&m_watchdogRunning, 0);

    // Give the thread up to 6 s to exit cleanly
    if (WaitForSingleObject(m_watchdogThread, 6000) == WAIT_TIMEOUT) {
        OutputDebugStringA("[Watchdog] Thread did not exit in time — force terminating\n");
        TerminateThread(m_watchdogThread, 0);
    }

    CloseHandle(m_watchdogThread);
    m_watchdogThread = nullptr;
    OutputDebugStringA("[Watchdog] Stopped\n");
}
