// ============================================================================
// ThermalDashboardWidget.cpp — Pure Win32 Native Implementation
// ============================================================================
// Live NVMe thermal visualization widget. Renders temperature bars,
// tier indicator, TurboSparse skip %, and PowerInfer GPU/CPU split.
// Loads pocket_lab_turbo.dll dynamically. Falls back to simulated data.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "ThermalDashboardWidget.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR     = RGB(30, 30, 35);
static constexpr COLORREF CARD_BG      = RGB(40, 40, 45);
static constexpr COLORREF BORDER_CLR   = RGB(60, 60, 65);
static constexpr COLORREF TEXT_CLR     = RGB(220, 220, 220);
static constexpr COLORREF LABEL_CLR   = RGB(140, 140, 140);
static constexpr COLORREF ACCENT_CLR  = RGB(100, 200, 255);
static constexpr COLORREF COOL_CLR    = RGB(0, 180, 255);
static constexpr COLORREF WARM_CLR    = RGB(255, 180, 0);
static constexpr COLORREF HOT_CLR     = RGB(255, 60, 60);

static const wchar_t* THERMAL_CLASS = L"RawrXD_ThermalDashboard";
static bool s_thermalClassRegistered = false;

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK ThermalDashboardWidget::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ThermalDashboardWidget* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<ThermalDashboardWidget*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ThermalDashboardWidget*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        self->refresh();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != self->m_bufW || h != self->m_bufH) {
            if (self->m_backBuffer) DeleteObject(self->m_backBuffer);
            if (self->m_backDC) DeleteDC(self->m_backDC);
            self->m_backDC = CreateCompatibleDC(hdc);
            self->m_backBuffer = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(self->m_backDC, self->m_backBuffer);
            self->m_bufW = w;
            self->m_bufH = h;
        }
        self->paint(self->m_backDC, rc);
        BitBlt(hdc, 0, 0, w, h, self->m_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ThermalDashboardWidget::ThermalDashboardWidget(HWND parentWnd)
    : m_parentWnd(parentWnd)
{
    createWindow(parentWnd);
    loadDll();
    refresh();
}

ThermalDashboardWidget::~ThermalDashboardWidget() {
    if (m_timerId) KillTimer(m_hwnd, m_timerId);
    if (m_pfnShutdown) m_pfnShutdown();
    if (m_hDll) FreeLibrary(m_hDll);
    if (m_backBuffer) DeleteObject(m_backBuffer);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_fontTitle) DeleteObject(m_fontTitle);
    if (m_fontBody) DeleteObject(m_fontBody);
    if (m_fontSmall) DeleteObject(m_fontSmall);
    if (m_fontLarge) DeleteObject(m_fontLarge);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void ThermalDashboardWidget::createWindow(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

    if (!s_thermalClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG_COLOR);
        wc.lpszClassName = THERMAL_CLASS;
        RegisterClassExW(&wc);
        s_thermalClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(0, THERMAL_CLASS, L"Thermal Dashboard",
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 320, 200, parent, nullptr, hInst, this);

    m_fontTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontBody = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontLarge = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    m_timerId = SetTimer(m_hwnd, 1, 1000, nullptr);
}

// ============================================================================
// DLL Loading
// ============================================================================

void ThermalDashboardWidget::loadDll() {
    const char* paths[] = {
        "pocket_lab_turbo.dll",
        "bin\\pocket_lab_turbo.dll",
        "..\\pocket_lab_turbo.dll",
        "D:\\rawrxd\\bin\\pocket_lab_turbo.dll",
    };

    for (const char* path : paths) {
        m_hDll = LoadLibraryA(path);
        if (m_hDll) break;
    }

    if (m_hDll) {
        m_pfnInit = (PFN_THERMAL_INIT)GetProcAddress(m_hDll, "ThermalInit");
        m_pfnGetThermal = (PFN_THERMAL_GET)GetProcAddress(m_hDll, "ThermalGetSnapshot");
        m_pfnShutdown = (PFN_THERMAL_SHUTDOWN)GetProcAddress(m_hDll, "ThermalShutdown");

        if (m_pfnInit && m_pfnGetThermal) {
            m_pfnInit();
            m_dllLoaded = true;
            OutputDebugStringA("[ThermalDashboard] DLL loaded successfully\n");
        } else {
            FreeLibrary(m_hDll);
            m_hDll = nullptr;
            OutputDebugStringA("[ThermalDashboard] DLL missing required exports\n");
        }
    } else {
        OutputDebugStringA("[ThermalDashboard] DLL not found, using simulated data\n");
    }
}

// ============================================================================
// Refresh
// ============================================================================

void ThermalDashboardWidget::refresh() {
    if (m_dllLoaded && m_pfnGetThermal) {
        ThermalSnapshot snap = {};
        if (m_pfnGetThermal(&snap) == 0) {
            m_temps[0] = snap.t0;
            m_temps[1] = snap.t1;
            m_temps[2] = snap.t2;
            m_temps[3] = snap.t3;
            m_temps[4] = snap.t4;
            m_tier = snap.tier;
            m_sparseSkipPct = snap.sparseSkipPct;
            m_gpuSplit = snap.gpuSplit;
        }
    }
    // If DLL not loaded, values stay at defaults (simulated idle)
}

// ============================================================================
// Color Helpers
// ============================================================================

COLORREF ThermalDashboardWidget::tempColor(double tempC) const {
    if (tempC < 40.0) return COOL_CLR;
    if (tempC < 55.0) return WARM_CLR;
    return HOT_CLR;
}

std::wstring ThermalDashboardWidget::tierName(unsigned int tier) const {
    switch (tier) {
        case 0: return L"70B";
        case 1: return L"120B";
        case 2: return L"800B";
        default: return L"Unknown";
    }
}

// ============================================================================
// Paint
// ============================================================================

void ThermalDashboardWidget::paint(HDC hdc, const RECT& rc) {
    HBRUSH bgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 100 || h < 80) return;

    // Title bar
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, m_fontTitle);
    SetTextColor(hdc, ACCENT_CLR);
    RECT titleRect = { 10, 5, w - 10, 25 };
    DrawTextW(hdc, L"NVMe Thermal Monitor", -1, &titleRect, DT_LEFT | DT_SINGLELINE);

    // Connection indicator
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, m_dllLoaded ? RGB(78, 201, 176) : RGB(255, 100, 100));
    RECT connRect = { w - 150, 5, w - 5, 25 };
    DrawTextW(hdc, m_dllLoaded ? L"● Live" : L"○ Simulated", -1, &connRect, DT_RIGHT | DT_SINGLELINE);

    // Temperature bars
    int barAreaY = 30;
    int barAreaH = h - 100;
    int barW = (w - 20) / 5 - 4;
    for (int i = 0; i < 5; ++i) {
        paintTempBar(hdc, 10 + i * (barW + 4), barAreaY, barW, barAreaH, m_temps[i], i);
    }

    // Info panel at bottom
    paintInfoPanel(hdc, 0, h - 65, w, 65);
}

void ThermalDashboardWidget::paintTempBar(HDC hdc, int x, int y, int w, int h, double tempC, int idx) {
    // Background
    RECT bgRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(CARD_BG);
    FillRect(hdc, &bgRect, bgBr);
    DeleteObject(bgBr);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Temperature bar fill
    double norm = (std::max)(0.0, (std::min)(1.0, tempC / 80.0));
    int barH = static_cast<int>(norm * (h - 30));
    COLORREF barColor = tempColor(tempC);

    // Gradient-like effect: bottom-up fill
    RECT barRect = { x + 4, y + h - 4 - barH, x + w - 4, y + h - 4 };
    HBRUSH barBr = CreateSolidBrush(barColor);
    FillRect(hdc, &barRect, barBr);
    DeleteObject(barBr);

    // Drive label at top
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_CLR);
    wchar_t drvBuf[16];
    swprintf_s(drvBuf, L"Drive %d", idx);
    RECT drvRect = { x, y + 2, x + w, y + 16 };
    DrawTextW(hdc, drvBuf, -1, &drvRect, DT_CENTER | DT_SINGLELINE);

    // Temperature value
    SelectObject(hdc, m_fontBody);
    SetTextColor(hdc, barColor);
    wchar_t tempBuf[16];
    swprintf_s(tempBuf, L"%.0f°C", tempC);
    RECT tempRect = { x, y + 16, x + w, y + 32 };
    DrawTextW(hdc, tempBuf, -1, &tempRect, DT_CENTER | DT_SINGLELINE);
}

void ThermalDashboardWidget::paintInfoPanel(HDC hdc, int x, int y, int w, int h) {
    RECT panelRect = { x, y, x + w, y + h };
    HBRUSH panelBr = CreateSolidBrush(RGB(35, 35, 38));
    FillRect(hdc, &panelRect, panelBr);
    DeleteObject(panelBr);

    // Top border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    SelectObject(hdc, pen);
    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, x + w, y);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    int colW = w / 3;

    // Tier
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_CLR);
    RECT tierLbl = { x + 10, y + 5, x + colW, y + 18 };
    DrawTextW(hdc, L"Active Tier:", -1, &tierLbl, DT_LEFT | DT_SINGLELINE);

    SelectObject(hdc, m_fontLarge);
    COLORREF tierColors[] = { RGB(78, 201, 176), RGB(255, 200, 50), RGB(255, 80, 80) };
    int tierIdx = (std::min)(m_tier, 2u);
    SetTextColor(hdc, tierColors[tierIdx]);
    RECT tierVal = { x + 10, y + 20, x + colW, y + h - 5 };
    DrawTextW(hdc, tierName(m_tier).c_str(), -1, &tierVal, DT_LEFT | DT_SINGLELINE);

    // TurboSparse Skip %
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_CLR);
    RECT skipLbl = { x + colW + 10, y + 5, x + 2 * colW, y + 18 };
    DrawTextW(hdc, L"TurboSparse Skip:", -1, &skipLbl, DT_LEFT | DT_SINGLELINE);

    SelectObject(hdc, m_fontLarge);
    SetTextColor(hdc, m_sparseSkipPct > 50 ? RGB(78, 201, 176) : RGB(200, 200, 200));
    wchar_t skipBuf[16];
    swprintf_s(skipBuf, L"%u%%", m_sparseSkipPct);
    RECT skipVal = { x + colW + 10, y + 20, x + 2 * colW, y + h - 5 };
    DrawTextW(hdc, skipBuf, -1, &skipVal, DT_LEFT | DT_SINGLELINE);

    // PowerInfer GPU/CPU Split
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_CLR);
    RECT gpuLbl = { x + 2 * colW + 10, y + 5, x + w - 10, y + 18 };
    DrawTextW(hdc, L"GPU / CPU Split:", -1, &gpuLbl, DT_LEFT | DT_SINGLELINE);

    SelectObject(hdc, m_fontLarge);
    SetTextColor(hdc, ACCENT_CLR);
    wchar_t gpuBuf[32];
    swprintf_s(gpuBuf, L"%u%% / %u%%", m_gpuSplit, 100 - m_gpuSplit);
    RECT gpuVal = { x + 2 * colW + 10, y + 20, x + w - 10, y + h - 5 };
    DrawTextW(hdc, gpuBuf, -1, &gpuVal, DT_LEFT | DT_SINGLELINE);
}

// ============================================================================
// Public API
// ============================================================================

void ThermalDashboardWidget::show() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW);
}

void ThermalDashboardWidget::hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void ThermalDashboardWidget::resize(int x, int y, int w, int h) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, w, h, TRUE);
}
