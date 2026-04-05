/**
 * @file Win32IDE_Titan_Heartbeat.cpp
 * @brief Titan Paging Heartbeat Implementation (Phase 15.4)
 * 
 * Implements real-time aperture utilization reporting via status bar
 * during long Titan inference operations. Provides visual feedback that
 * the UI is responsive while background VRAM paging occurs.
 * 
 * Phase 15.4: Extended format includes throughput, latency, and cache hit
 * ratio alongside paging utilization.
 *   Format: "42% Paging | 125 tok/s | 32 ms/tok | 87% cache"
 */

#include "Win32IDE.h"
#include "RawrXD_Exports.h"
#include "Win32IDE_Phase16_AgenticController.h"
#include <sstream>
#include <iomanip>

// Timer ID for Titan paging heartbeat
static const UINT_PTR TITAN_PAGING_HEARTBEAT_TIMER_ID = 8889;
static const UINT TITAN_PAGING_HEARTBEAT_INTERVAL_MS = 250;

/**
 * @brief Start the aperture utilization heartbeat timer
 * @param[in] timeoutMs Timeout in milliseconds (0 = infinite)
 */
void Win32IDE::startTitanPagingHeartbeat(uint32_t timeoutMs)
{
    if (m_titanPagingHeartbeatActive)
    {
        // Already running, ignore duplicate call
        return;
    }

    // Initialize heartbeat state
    m_titanPagingHeartbeatActive = true;
    m_titanPagingHeartbeatStartTime = GetTickCount64();
    m_titanPagingHeartbeatTimeoutMs = timeoutMs;

    // Start the timer
    SetTimer(m_hwndMain, TITAN_PAGING_HEARTBEAT_TIMER_ID, TITAN_PAGING_HEARTBEAT_INTERVAL_MS, nullptr);

    // Set initial status bar text
    if (m_hwndStatusBar)
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"[Titan] Paging... 0%");
    }

    OutputDebugStringA("[Titan Heartbeat] Started\n");
}

/**
 * @brief Handle heartbeat timer tick (called from WM_TIMER)
 * 
 * Queries aperture utilization from Titan DLL and updates status bar
 * with formatted percentage. Safely handles API unavailability.
 */
void Win32IDE::onTitanPagingHeartbeatTimer()
{
    if (!m_titanPagingHeartbeatActive)
    {
        return;  // Timer should have been killed, but defensive check
    }

    // Call Titan DLL to get current aperture utilization
    RAWRXD_APERTURE_STATUS status = {};
    RAWRXD_STATUS result = RawrXD_GetApertureUtilization(&status);

    if (result == RAWRXD_SUCCESS && m_hwndStatusBar)
    {
        // Compute utilization percentage from 0-1,000,000 range
        // utilization_pct10000 is in tenths of basis points (0.0001% units)
        uint32_t total_pct = status.utilization_pct10000;

        // Clamp to safe range
        if (total_pct > 1000000)
        {
            total_pct = 1000000;
        }

        // Extract integer percent (round to nearest %)
        uint32_t pct_int = (total_pct + 5000) / 10000;
        if (pct_int > 100) pct_int = 100;

        // Phase 15.4: Query extended metrics
        uint32_t tokPerSec = 0;
        uint32_t msPerTok  = 0;
        uint32_t cacheHit  = 0;

        Rawr_CalculateThroughput(&tokPerSec);
        Rawr_CalculateLatency(&msPerTok);
        Rawr_CalculateCacheHitRatio(&cacheHit);

        // Format the status string
        wchar_t buf[256] = {};

        // Edge case: at 100%, show completion message with extended metrics
        if (pct_int >= 100)
        {
            swprintf_s(buf, sizeof(buf) / sizeof(wchar_t),
                       L"[Titan] Done: 100%% Paging | %u tok/s | %u ms/tok | %u%% cache",
                       tokPerSec, msPerTok, cacheHit);
        }
        else if (tokPerSec > 0 || msPerTok > 0)
        {
            // Phase 15.4 extended format (metrics available)
            swprintf_s(buf, sizeof(buf) / sizeof(wchar_t),
                       L"%u%% Paging | %u tok/s | %u ms/tok | %u%% cache",
                       pct_int, tokPerSec, msPerTok, cacheHit);
        }
        else
        {
            // Phase 15.3 fallback: no inference metrics yet (still warming up)
            swprintf_s(buf, sizeof(buf) / sizeof(wchar_t),
                       L"[Titan] Paging... %u%%", pct_int);
        }

        // Update status bar part 0
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
    }

    // Check if heartbeat timeout has been reached
    if (m_titanPagingHeartbeatTimeoutMs > 0)
    {
        uint64_t elapsed = GetTickCount64() - m_titanPagingHeartbeatStartTime;
        if (elapsed > m_titanPagingHeartbeatTimeoutMs)
        {
            OutputDebugStringA("[Titan Heartbeat] Timeout reached, stopping heartbeat\n");
            stopTitanPagingHeartbeat();
        }
    }

    // Phase 16: drive the agentic executor watchdog and aperture safe-return.
    AgenticControllerHeartbeat();
}

/**
 * @brief Stop the aperture utilization heartbeat timer
 * 
 * Kills the heartbeat timer and restores the status bar to normal "Ready" state.
 */
void Win32IDE::stopTitanPagingHeartbeat()
{
    if (!m_titanPagingHeartbeatActive)
    {
        return;  // Already stopped
    }

    // Kill the timer
    KillTimer(m_hwndMain, TITAN_PAGING_HEARTBEAT_TIMER_ID);

    // Reset state
    m_titanPagingHeartbeatActive = false;
    m_titanPagingHeartbeatStartTime = 0;
    m_titanPagingHeartbeatTimeoutMs = 0;

    // Restore status bar to "Ready"
    if (m_hwndStatusBar)
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
    }

    OutputDebugStringA("[Titan Heartbeat] Stopped\n");
}
