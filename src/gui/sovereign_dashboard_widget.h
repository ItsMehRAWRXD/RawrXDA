#pragma once
// ============================================================================
// sovereign_dashboard_widget.h — Pure Win32 Native Implementation
// ============================================================================
// Real-time inference dashboard: tokens/s, thermal, skip rate, GPU stats.
// Reads from MMF (Memory-Mapped File) shared with inference engine.
// ============================================================================

#include <windows.h>
#include <string>
#include <cstdint>

#define SOVEREIGN_STATS_MAGIC 0x534F5652  // "SOVR"

#pragma pack(push, 1)
struct SovereignStatsBlock {
    uint32_t magic;
    float tokensPerSec;
    float driveTemps[5];
    float skipRate;
    uint32_t activeTier;       // 0=70B, 1=120B, 2=800B
    float gpuUtilization;
    float cpuUtilization;
    float memoryUsedGB;
    float memoryTotalGB;
    uint32_t activeNodes;
    uint64_t totalTokensGenerated;
    float avgLatencyMs;
    uint32_t uptime_seconds;
};
#pragma pack(pop)

class SovereignDashboardWidget {
public:
    explicit SovereignDashboardWidget(HWND parentWnd);
    ~SovereignDashboardWidget();

    void attachSharedMemory(const std::string& name);
    void detachSharedMemory();

    HWND getHwnd() const { return m_hwnd; }
    void show();
    void hide();
    void resize(int x, int y, int w, int h);

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void createWindow(HWND parent);
    void updateDashboard();
    void paint(HDC hdc, const RECT& rc);
    void paintMetricCard(HDC hdc, int x, int y, int w, int h,
                         const wchar_t* label, const wchar_t* value, COLORREF valueColor);
    void paintThermalBar(HDC hdc, int x, int y, int w, int h, float tempC, int driveIdx);
    void paintTierIndicator(HDC hdc, int x, int y, int w, int h);

    static void CALLBACK TimerCallback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

    HWND m_hwnd = nullptr;
    HWND m_parentWnd = nullptr;

    // Shared memory
    HANDLE m_mmfHandle = nullptr;
    const SovereignStatsBlock* m_mmfView = nullptr;

    // Cached stats (for rendering when MMF unavailable)
    SovereignStatsBlock m_cachedStats = {};
    bool m_connected = false;

    // GDI resources
    HBITMAP m_backBuffer = nullptr;
    HDC m_backDC = nullptr;
    int m_bufW = 0, m_bufH = 0;
    HFONT m_fontLarge = nullptr;
    HFONT m_fontMedium = nullptr;
    HFONT m_fontSmall = nullptr;

    UINT_PTR m_timerId = 0;
};
