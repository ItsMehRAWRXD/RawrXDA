#pragma once
// ============================================================================
// ThermalDashboardWidget.h — Pure Win32 Native Implementation
// ============================================================================
// Live NVMe thermal visualization with tier detection, TurboSparse stats,
// and PowerInfer GPU/CPU split display. Loads pocket_lab_turbo.dll at runtime.
// ============================================================================

#include <windows.h>
#include <string>
#include <cstdint>

#pragma pack(push, 1)
struct ThermalSnapshot {
    double t0, t1, t2, t3, t4;
    unsigned int tier;
    unsigned int sparseSkipPct;
    unsigned int gpuSplit;
};
#pragma pack(pop)

typedef int  (__stdcall *PFN_THERMAL_INIT)();
typedef int  (__stdcall *PFN_THERMAL_GET)(ThermalSnapshot*);
typedef void (__stdcall *PFN_THERMAL_SHUTDOWN)();

class ThermalDashboardWidget {
public:
    explicit ThermalDashboardWidget(HWND parentWnd);
    ~ThermalDashboardWidget();

    bool isConnected() const { return m_dllLoaded; }
    void refresh();

    HWND getHwnd() const { return m_hwnd; }
    void show();
    void hide();
    void resize(int x, int y, int w, int h);

private:
    void createWindow(HWND parent);
    void loadDll();
    void paint(HDC hdc, const RECT& rc);
    void paintTempBar(HDC hdc, int x, int y, int w, int h, double tempC, int idx);
    void paintInfoPanel(HDC hdc, int x, int y, int w, int h);
    std::wstring tierName(unsigned int tier) const;
    COLORREF tempColor(double tempC) const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    HWND m_parentWnd = nullptr;

    // DLL state
    bool m_dllLoaded = false;
    HMODULE m_hDll = nullptr;
    PFN_THERMAL_INIT m_pfnInit = nullptr;
    PFN_THERMAL_GET m_pfnGetThermal = nullptr;
    PFN_THERMAL_SHUTDOWN m_pfnShutdown = nullptr;

    // Snapshot data
    double m_temps[5] = { 35.0, 35.0, 35.0, 35.0, 35.0 };
    unsigned int m_tier = 0;
    unsigned int m_sparseSkipPct = 0;
    unsigned int m_gpuSplit = 100;

    // GDI resources
    HBITMAP m_backBuffer = nullptr;
    HDC m_backDC = nullptr;
    int m_bufW = 0, m_bufH = 0;
    HFONT m_fontTitle = nullptr;
    HFONT m_fontBody = nullptr;
    HFONT m_fontSmall = nullptr;
    HFONT m_fontLarge = nullptr;

    UINT_PTR m_timerId = 0;
};
