/**
 * @file RAWRXD_ThermalDashboard.cpp
 * @brief Win32 Thermal Dashboard UI — pure C++20, zero Qt.
 */
#include "RAWRXD_ThermalDashboard.hpp"
#include <cstdio>

#pragma comment(lib, "comctl32.lib")

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboard
// ═══════════════════════════════════════════════════════════════════════════════

static const wchar_t* kThermalClass = L"RawrXD_ThermalDashboard";
static bool s_classRegistered = false;

ThermalDashboard::ThermalDashboard(HWND hwndParent)
    : m_hwndParent(hwndParent)
{
}

ThermalDashboard::~ThermalDashboard()
{
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}

void ThermalDashboard::show()
{
    if (m_hWnd && IsWindow(m_hWnd)) { SetForegroundWindow(m_hWnd); return; }

    HINSTANCE hInst = GetModuleHandle(nullptr);
    if (!s_classRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = WndProc;
        wc.hInstance      = hInst;
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(26, 26, 26));
        wc.lpszClassName = kThermalClass;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    RECT rc;
    if (m_hwndParent) GetWindowRect(m_hwndParent, &rc);
    else { rc.left = 200; rc.top = 100; }

    m_hWnd = CreateWindowExW(WS_EX_TOOLWINDOW,
        kThermalClass, L"\xD83C\xDF21 RawrXD Thermal Dashboard",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        rc.left + 60, rc.top + 40, 620, 540,
        m_hwndParent, nullptr, hInst, nullptr);

    SetWindowLongPtrW(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    createControls(m_hWnd);
    ShowWindow(m_hWnd, SW_SHOW);
}

void ThermalDashboard::hide()
{
    if (m_hWnd) ShowWindow(m_hWnd, SW_HIDE);
}

// ═══════════════════════════════════════════════════════════════════════════════
// WndProc
// ═══════════════════════════════════════════════════════════════════════════════

LRESULT CALLBACK ThermalDashboard::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<ThermalDashboard*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (self) return self->handleMessage(hWnd, msg, wParam, lParam);
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT ThermalDashboard::handleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TD_APPLY_BTN) {
            int sel = static_cast<int>(SendMessageW(m_hwndModeCombo, CB_GETCURSEL, 0, 0));
            if (m_burstCb) m_burstCb(sel);
            return 0;
        }
        break;
    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Create child controls
// ═══════════════════════════════════════════════════════════════════════════════

void ThermalDashboard::createControls(HWND hWnd)
{
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int y = 10;
    const int LBL_W = 180, BAR_W = 320, TEMP_W = 60;

    // ---- NVMe drives ----
    CreateWindowExW(0, L"STATIC", L"NVMe Drives",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 200, 18, hWnd, nullptr, hInst, nullptr);
    y += 22;

    const wchar_t* nvmeNames[] = {
        L"NVMe0 (SK hynix P41)", L"NVMe1 (Samsung 990 PRO)",
        L"NVMe2 (WD Black SN850X)", L"NVMe3 (Samsung 990 PRO 4TB)",
        L"NVMe4 (Crucial T705 4TB)"
    };
    for (int i = 0; i < 5; ++i) {
        m_nvme[i].hLabel = CreateWindowExW(0, L"STATIC", nvmeNames[i],
            WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 2, LBL_W, 18, hWnd, nullptr, hInst, nullptr);
        m_nvme[i].hBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH, LBL_W + 16, y, BAR_W, 18, hWnd, nullptr, hInst, nullptr);
        SendMessageW(m_nvme[i].hBar, PBM_SETRANGE32, 0, 100);
        m_nvme[i].hTemp = CreateWindowExW(0, L"STATIC", L"--\xB0C",
            WS_CHILD | WS_VISIBLE | SS_RIGHT, LBL_W + BAR_W + 22, y + 2, TEMP_W, 18, hWnd, nullptr, hInst, nullptr);
        y += 24;
    }
    y += 8;

    // ---- GPU / CPU ----
    CreateWindowExW(0, L"STATIC", L"System Thermals",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 200, 18, hWnd, nullptr, hInst, nullptr);
    y += 22;

    CreateWindowExW(0, L"STATIC", L"7800 XT Junction",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 2, LBL_W, 18, hWnd, nullptr, hInst, nullptr);
    m_hwndGpuBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH, LBL_W + 16, y, BAR_W, 18, hWnd, nullptr, hInst, nullptr);
    SendMessageW(m_hwndGpuBar, PBM_SETRANGE32, 0, 110);
    m_hwndGpuLabel = CreateWindowExW(0, L"STATIC", L"--\xB0C",
        WS_CHILD | WS_VISIBLE | SS_RIGHT, LBL_W + BAR_W + 22, y + 2, TEMP_W, 18, hWnd, nullptr, hInst, nullptr);
    y += 24;

    CreateWindowExW(0, L"STATIC", L"7800X3D Package",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 2, LBL_W, 18, hWnd, nullptr, hInst, nullptr);
    m_hwndCpuBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH, LBL_W + 16, y, BAR_W, 18, hWnd, nullptr, hInst, nullptr);
    SendMessageW(m_hwndCpuBar, PBM_SETRANGE32, 0, 95);
    m_hwndCpuLabel = CreateWindowExW(0, L"STATIC", L"--\xB0C",
        WS_CHILD | WS_VISIBLE | SS_RIGHT, LBL_W + BAR_W + 22, y + 2, TEMP_W, 18, hWnd, nullptr, hInst, nullptr);
    y += 32;

    // ---- Throttle ----
    CreateWindowExW(0, L"STATIC", L"Burst Governor",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 200, 18, hWnd, nullptr, hInst, nullptr);
    y += 22;

    CreateWindowExW(0, L"STATIC", L"Current Throttle",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 2, LBL_W, 18, hWnd, nullptr, hInst, nullptr);
    m_hwndThrottleBar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH, LBL_W + 16, y, BAR_W, 18, hWnd, nullptr, hInst, nullptr);
    SendMessageW(m_hwndThrottleBar, PBM_SETRANGE32, 0, 100);
    m_hwndThrottleLbl = CreateWindowExW(0, L"STATIC", L"0%",
        WS_CHILD | WS_VISIBLE | SS_RIGHT, LBL_W + BAR_W + 22, y + 2, TEMP_W, 18, hWnd, nullptr, hInst, nullptr);
    y += 30;

    // ---- Mode selector ----
    CreateWindowExW(0, L"STATIC", L"Burst Mode:",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y + 4, 80, 18, hWnd, nullptr, hInst, nullptr);
    m_hwndModeCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        96, y, 350, 120, hWnd, reinterpret_cast<HMENU>(IDC_TD_MODE_COMBO), hInst, nullptr);
    SendMessageW(m_hwndModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\xD83D\xDE80 SOVEREIGN-MAX (142\x03BCs)"));
    SendMessageW(m_hwndModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\xD83C\xDF21 THERMAL-GOVERNED (237\x03BCs)"));
    SendMessageW(m_hwndModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\x26A1 ADAPTIVE-HYBRID (dynamic)"));
    SendMessageW(m_hwndModeCombo, CB_SETCURSEL, 2, 0);

    m_hwndApplyBtn = CreateWindowExW(0, L"BUTTON", L"Apply",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        454, y, 90, 26, hWnd, reinterpret_cast<HMENU>(IDC_TD_APPLY_BTN), hInst, nullptr);
    y += 38;

    // ---- Status ----
    m_hwndStatusLabel = CreateWindowExW(0, L"STATIC",
        L"\x231B Initializing thermal monitoring...",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 12, y, 560, 18, hWnd, nullptr, hInst, nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Runtime updates
// ═══════════════════════════════════════════════════════════════════════════════

COLORREF ThermalDashboard::tempToColor(float temp)
{
    if (temp < 55.0f) return RGB(0, 255, 0);
    if (temp < 65.0f) return RGB(136, 255, 0);
    if (temp < 72.0f) return RGB(255, 204, 0);
    if (temp < 80.0f) return RGB(255, 136, 0);
    return RGB(255, 51, 51);
}

void ThermalDashboard::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i)
        updateNVMeDisplay(i, snapshot.nvmeTemps[i]);
    for (int i = snapshot.activeDriveCount; i < 5; ++i) {
        ShowWindow(m_nvme[i].hLabel, SW_HIDE);
        ShowWindow(m_nvme[i].hBar,   SW_HIDE);
        ShowWindow(m_nvme[i].hTemp,  SW_HIDE);
    }
    updateGPUDisplay(snapshot.gpuTemp);
    updateCPUDisplay(snapshot.cpuTemp);
    updateThrottleDisplay(snapshot.currentThrottle);

    wchar_t buf[128];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE,
        L"\x2713 %d drives active", snapshot.activeDriveCount);
    SetWindowTextW(m_hwndStatusLabel, buf);
}

void ThermalDashboard::updateNVMeDisplay(int idx, float temp)
{
    if (idx < 0 || idx >= 5) return;
    SendMessageW(m_nvme[idx].hBar, PBM_SETPOS, static_cast<int>(temp), 0);
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.0f\xB0C", temp);
    SetWindowTextW(m_nvme[idx].hTemp, buf);
}

void ThermalDashboard::updateGPUDisplay(float temp)
{
    SendMessageW(m_hwndGpuBar, PBM_SETPOS, static_cast<int>(temp), 0);
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.0f\xB0C", temp);
    SetWindowTextW(m_hwndGpuLabel, buf);
}

void ThermalDashboard::updateCPUDisplay(float temp)
{
    SendMessageW(m_hwndCpuBar, PBM_SETPOS, static_cast<int>(temp), 0);
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.0f\xB0C", temp);
    SetWindowTextW(m_hwndCpuLabel, buf);
}

void ThermalDashboard::updateThrottleDisplay(int throttle)
{
    SendMessageW(m_hwndThrottleBar, PBM_SETPOS, throttle, 0);
    wchar_t buf[16];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d%%", throttle);
    SetWindowTextW(m_hwndThrottleLbl, buf);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalCompactWidget
// ═══════════════════════════════════════════════════════════════════════════════

ThermalCompactWidget::ThermalCompactWidget(HWND hwndParent)
    : m_hwndParent(hwndParent)
{
}

ThermalCompactWidget::~ThermalCompactWidget()
{
    if (m_hWnd && IsWindow(m_hWnd)) DestroyWindow(m_hWnd);
}

void ThermalCompactWidget::show()
{
    if (m_hWnd && IsWindow(m_hWnd)) return;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    m_hWnd = CreateWindowExW(0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_SUNKEN,
        0, 0, 260, 28, m_hwndParent, nullptr, hInst, nullptr);

    m_hwndTempLbl  = CreateWindowExW(0, L"STATIC", L"\xD83C\xDF21 --\xB0C",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 4, 4, 80, 18, m_hWnd, nullptr, hInst, nullptr);
    m_hwndThrottle = CreateWindowExW(0, L"STATIC", L"\x26A1",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 88, 4, 30, 18, m_hWnd, nullptr, hInst, nullptr);
    m_hwndMode     = CreateWindowExW(0, L"STATIC", L"\xD83D\xDD04",
        WS_CHILD | WS_VISIBLE | SS_CENTER, 122, 4, 30, 18, m_hWnd, nullptr, hInst, nullptr);
}

void ThermalCompactWidget::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    float maxTemp = snapshot.gpuTemp;
    for (int i = 0; i < snapshot.activeDriveCount; ++i)
        maxTemp = (std::max)(maxTemp, snapshot.nvmeTemps[i]);
    maxTemp = (std::max)(maxTemp, snapshot.cpuTemp);

    wchar_t buf[32];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"\xD83C\xDF21 %.0f\xB0C", maxTemp);
    SetWindowTextW(m_hwndTempLbl, buf);

    if (snapshot.currentThrottle == 0)
        SetWindowTextW(m_hwndThrottle, L"\x26A1");
    else if (snapshot.currentThrottle < 30)
        SetWindowTextW(m_hwndThrottle, L"\xD83D\xDD0B");
    else
        SetWindowTextW(m_hwndThrottle, L"\xD83D\xDC22");
}

} // namespace rawrxd::thermal
