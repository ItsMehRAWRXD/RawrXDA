/**
 * @file ThermalDashboardWidget.h
 * @brief Win32/Direct2D widget displaying live NVMe thermal data + inference stats
 *
 * Connects to Pocket-Lab Turbo DLL for:
 * - Real-time NVMe temperatures (5 drives)
 * - Auto-detected tier (70B/120B/800B)
 * - TurboSparse skip percentage
 * - PowerInfer GPU/CPU split
 */
#pragma once

#include "../RawrXD_Foundation.h"
#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class ThermalDashboardWidget {
public:
    ThermalDashboardWidget();
    ~ThermalDashboardWidget();

    /// Create the Win32 window
    bool Create(HWND parent, int x, int y, int width, int height);

    /// Check if DLL is loaded and functional
    bool isConnected() const { return m_dllLoaded; }

    HWND GetHwnd() const { return m_hwnd; }

    void SetSize(int width, int height);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    void OnPaint();
    void OnResize(UINT width, UINT height);
    void OnTimer();

    void loadDll();
    
    // Window Handle
    HWND m_hwnd;

    // Direct2D Resources
    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    IDWriteFactory* m_pDWriteFactory;
    IDWriteTextFormat* m_pTextFormat;

    // Brushes
    ID2D1SolidColorBrush* m_pBrushCool;
    ID2D1SolidColorBrush* m_pBrushWarm;
    ID2D1SolidColorBrush* m_pBrushHot;
    ID2D1SolidColorBrush* m_pBrushBg;
    ID2D1SolidColorBrush* m_pBrushText;
    ID2D1SolidColorBrush* m_pBrushAccent;

    // DLL state
    bool m_dllLoaded;
    void* m_hDll;

    // Function pointers
    typedef int (__stdcall *PFN_Init)(void);
    typedef void (__stdcall *PFN_GetThermal)(void*);

    PFN_Init m_pfnInit;
    PFN_GetThermal m_pfnGetThermal;

    // Cached thermal data
    double m_temps[5];
    unsigned int m_tier;
    unsigned int m_sparseSkipPct;
    unsigned int m_gpuSplit;

    // Thresholds
    static constexpr int TEMP_WARN = 45;
    static constexpr int TEMP_CRIT = 55;
};

