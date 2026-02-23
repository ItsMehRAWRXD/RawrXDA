/**
 * @file RAWRXD_ThermalDashboard.hpp
 * @brief Win32 Thermal Dashboard UI — pure C++20, zero Qt.
 *
 * Uses Win32 static/button/progress controls, custom GDI paint for
 * color-coded temperature bars and throttle display.
 */
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <algorithm>
#include "thermal_dashboard_plugin.hpp"

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboard — full panel
// ═══════════════════════════════════════════════════════════════════════════════

class ThermalDashboard {
public:
    explicit ThermalDashboard(HWND hwndParent = nullptr);
    ~ThermalDashboard();

    ThermalDashboard(const ThermalDashboard&) = delete;
    ThermalDashboard& operator=(const ThermalDashboard&) = delete;

    void show();
    void hide();
    void onThermalUpdate(const ThermalSnapshot& snapshot);

    /// Callback for burst-mode changes (connect to governor).
    using BurstModeCallback = void(*)(int mode);
    void setBurstModeCallback(BurstModeCallback cb) { m_burstCb = cb; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(HWND, UINT, WPARAM, LPARAM);
    void createControls(HWND hWnd);
    void updateNVMeDisplay(int idx, float temp);
    void updateGPUDisplay(float temp);
    void updateCPUDisplay(float temp);
    void updateThrottleDisplay(int throttle);
    static COLORREF tempToColor(float temp);

    HWND m_hwndParent = nullptr;
    HWND m_hWnd       = nullptr;

    struct NVMeRow {
        HWND hLabel   = nullptr;
        HWND hBar     = nullptr;
        HWND hTemp    = nullptr;
    };
    NVMeRow m_nvme[5]{};

    HWND m_hwndGpuBar      = nullptr;
    HWND m_hwndGpuLabel    = nullptr;
    HWND m_hwndCpuBar      = nullptr;
    HWND m_hwndCpuLabel    = nullptr;
    HWND m_hwndThrottleBar = nullptr;
    HWND m_hwndThrottleLbl = nullptr;
    HWND m_hwndModeCombo   = nullptr;
    HWND m_hwndApplyBtn    = nullptr;
    HWND m_hwndStatusLabel = nullptr;

    BurstModeCallback m_burstCb = nullptr;

    enum {
        IDC_TD_MODE_COMBO = 3001,
        IDC_TD_APPLY_BTN  = 3002,
    };
};

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalCompactWidget — small status-bar widget
// ═══════════════════════════════════════════════════════════════════════════════

class ThermalCompactWidget {
public:
    explicit ThermalCompactWidget(HWND hwndParent = nullptr);
    ~ThermalCompactWidget();

    void show();
    void onThermalUpdate(const ThermalSnapshot& snapshot);

private:
    HWND m_hwndParent   = nullptr;
    HWND m_hWnd         = nullptr;
    HWND m_hwndTempLbl  = nullptr;
    HWND m_hwndThrottle = nullptr;
    HWND m_hwndMode     = nullptr;
};

} // namespace rawrxd::thermal

