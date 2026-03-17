// ============================================================================
// sovereign_dashboard_widget.cpp — Pure Win32 Native Implementation
// ============================================================================
// Production real-time inference dashboard using Win32 GDI rendering.
// Reads stats from MMF shared memory. Displays tokens/s, thermal bars,
// skip rate, GPU/CPU utilization, tier indicator, and uptime.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "sovereign_dashboard_widget.h"
#include <cstdio>
#include <cmath>
#include <algorithm>

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR       = RGB(25, 25, 28);
static constexpr COLORREF CARD_BG        = RGB(37, 37, 40);
static constexpr COLORREF BORDER_COLOR   = RGB(55, 55, 58);
static constexpr COLORREF TEXT_COLOR      = RGB(210, 210, 210);
static constexpr COLORREF LABEL_COLOR    = RGB(140, 140, 140);
static constexpr COLORREF GREEN_COLOR    = RGB(78, 201, 176);
static constexpr COLORREF YELLOW_COLOR   = RGB(255, 200, 50);
static constexpr COLORREF RED_COLOR      = RGB(244, 71, 71);
static constexpr COLORREF BLUE_COLOR     = RGB(86, 156, 214);

static const wchar_t* SOV_CLASS = L"RawrXD_SovereignDashboard";
static bool s_sovClassRegistered = false;

// ============================================================================
// Window Class Registration
// ============================================================================

static void RegisterSovClass(HINSTANCE hInst) {
    if (s_sovClassRegistered) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SovereignDashboardWidget::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG_COLOR);
    wc.lpszClassName = SOV_CLASS;
    RegisterClassExW(&wc);
    s_sovClassRegistered = true;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SovereignDashboardWidget::SovereignDashboardWidget(HWND parentWnd)
    : m_parentWnd(parentWnd)
{
    memset(&m_cachedStats, 0, sizeof(m_cachedStats));
    createWindow(parentWnd);
}

SovereignDashboardWidget::~SovereignDashboardWidget() {
    detachSharedMemory();
    if (m_timerId) KillTimer(m_hwnd, m_timerId);
    if (m_backBuffer) DeleteObject(m_backBuffer);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_fontLarge) DeleteObject(m_fontLarge);
    if (m_fontMedium) DeleteObject(m_fontMedium);
    if (m_fontSmall) DeleteObject(m_fontSmall);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void SovereignDashboardWidget::createWindow(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    RegisterSovClass(hInst);

    m_hwnd = CreateWindowExW(0, SOV_CLASS, L"Sovereign Dashboard",
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 400, 300, parent, nullptr, hInst, this);

    m_fontLarge = CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontMedium = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // 60 FPS refresh timer
    m_timerId = SetTimer(m_hwnd, 1, 16, nullptr);
}

// ============================================================================
// Shared Memory
// ============================================================================

void SovereignDashboardWidget::attachSharedMemory(const std::string& name) {
    if (m_mmfView) return;

    m_mmfHandle = OpenFileMappingA(FILE_MAP_READ, FALSE, name.c_str());
    if (m_mmfHandle) {
        m_mmfView = reinterpret_cast<const SovereignStatsBlock*>(
            MapViewOfFile(m_mmfHandle, FILE_MAP_READ, 0, 0, sizeof(SovereignStatsBlock)));
        if (m_mmfView && m_mmfView->magic == SOVEREIGN_STATS_MAGIC) {
            m_connected = true;
            OutputDebugStringA("[SovereignDashboard] Connected to shared memory\n");
        }
    }
}

void SovereignDashboardWidget::detachSharedMemory() {
    if (m_mmfView) {
        UnmapViewOfFile(m_mmfView);
        m_mmfView = nullptr;
    }
    if (m_mmfHandle) {
        CloseHandle(m_mmfHandle);
        m_mmfHandle = nullptr;
    }
    m_connected = false;
}

// ============================================================================
// Update
// ============================================================================

void SovereignDashboardWidget::updateDashboard() {
    if (!m_connected) {
        attachSharedMemory("Global\\SOVEREIGN_STATS");
    }
    if (m_mmfView && m_mmfView->magic == SOVEREIGN_STATS_MAGIC) {
        memcpy(&m_cachedStats, m_mmfView, sizeof(SovereignStatsBlock));
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK SovereignDashboardWidget::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SovereignDashboardWidget* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SovereignDashboardWidget*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<SovereignDashboardWidget*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        self->updateDashboard();
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
// Paint
// ============================================================================

void SovereignDashboardWidget::paint(HDC hdc, const RECT& rc) {
    HBRUSH bgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 100 || h < 100) return;

    const auto& s = m_cachedStats;
    int cardW = (w - 30) / 3;
    int cardH = 80;
    int margin = 10;
    int y = margin;

    // Row 1: Tokens/s, Latency, Tier
    wchar_t buf[64];

    swprintf_s(buf, L"%.1f t/s", s.tokensPerSec);
    COLORREF tpsColor = s.tokensPerSec > 30 ? GREEN_COLOR : s.tokensPerSec > 10 ? YELLOW_COLOR : RED_COLOR;
    paintMetricCard(hdc, margin, y, cardW, cardH, L"Inference Rate", buf, tpsColor);

    swprintf_s(buf, L"%.0f ms", s.avgLatencyMs);
    COLORREF latColor = s.avgLatencyMs < 50 ? GREEN_COLOR : s.avgLatencyMs < 200 ? YELLOW_COLOR : RED_COLOR;
    paintMetricCard(hdc, margin + cardW + 5, y, cardW, cardH, L"Avg Latency", buf, latColor);

    paintTierIndicator(hdc, margin + 2 * (cardW + 5), y, cardW, cardH);
    y += cardH + 5;

    // Row 2: GPU Utilization, CPU, Memory
    swprintf_s(buf, L"%.0f%%", s.gpuUtilization);
    COLORREF gpuColor = s.gpuUtilization > 80 ? GREEN_COLOR : s.gpuUtilization > 40 ? YELLOW_COLOR : LABEL_COLOR;
    paintMetricCard(hdc, margin, y, cardW, cardH, L"GPU Utilization", buf, gpuColor);

    swprintf_s(buf, L"%.0f%%", s.cpuUtilization);
    paintMetricCard(hdc, margin + cardW + 5, y, cardW, cardH, L"CPU Utilization", buf, BLUE_COLOR);

    swprintf_s(buf, L"%.1f / %.1f GB", s.memoryUsedGB, s.memoryTotalGB);
    float memPct = s.memoryTotalGB > 0 ? s.memoryUsedGB / s.memoryTotalGB : 0;
    COLORREF memColor = memPct > 0.9f ? RED_COLOR : memPct > 0.7f ? YELLOW_COLOR : GREEN_COLOR;
    paintMetricCard(hdc, margin + 2 * (cardW + 5), y, cardW, cardH, L"Memory", buf, memColor);
    y += cardH + 5;

    // Row 3: Thermal bars for 5 drives
    int thermalW = (w - 20) / 5 - 4;
    for (int i = 0; i < 5; ++i) {
        paintThermalBar(hdc, margin + i * (thermalW + 4), y, thermalW, 50, s.driveTemps[i], i);
    }
    y += 55;

    // Row 4: Skip rate, Nodes, Total tokens, Uptime
    int row4W = (w - 30) / 4;

    swprintf_s(buf, L"%.1f%%", s.skipRate);
    paintMetricCard(hdc, margin, y, row4W, 60, L"Skip Rate", buf, s.skipRate > 50 ? GREEN_COLOR : LABEL_COLOR);

    swprintf_s(buf, L"%u", s.activeNodes);
    paintMetricCard(hdc, margin + row4W + 5, y, row4W, 60, L"Active Nodes", buf, BLUE_COLOR);

    swprintf_s(buf, L"%llu", s.totalTokensGenerated);
    paintMetricCard(hdc, margin + 2 * (row4W + 5), y, row4W, 60, L"Total Tokens", buf, TEXT_COLOR);

    int hrs = s.uptime_seconds / 3600;
    int mins = (s.uptime_seconds % 3600) / 60;
    swprintf_s(buf, L"%dh %dm", hrs, mins);
    paintMetricCard(hdc, margin + 3 * (row4W + 5), y, row4W, 60, L"Uptime", buf, LABEL_COLOR);

    // Connection status indicator
    SelectObject(hdc, m_fontSmall);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, m_connected ? GREEN_COLOR : RED_COLOR);
    RECT statusRect = { w - 120, h - 18, w - 5, h - 2 };
    DrawTextW(hdc, m_connected ? L"● Connected" : L"○ Disconnected", -1, &statusRect, DT_RIGHT | DT_SINGLELINE);
}

void SovereignDashboardWidget::paintMetricCard(HDC hdc, int x, int y, int w, int h,
                                                const wchar_t* label, const wchar_t* value, COLORREF valueColor) {
    RECT cardRect = { x, y, x + w, y + h };
    HBRUSH cardBrush = CreateSolidBrush(CARD_BG);
    FillRect(hdc, &cardRect, cardBrush);
    DeleteObject(cardBrush);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);

    // Label
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_COLOR);
    RECT labelRect = { x + 8, y + 6, x + w - 8, y + 20 };
    DrawTextW(hdc, label, -1, &labelRect, DT_LEFT | DT_SINGLELINE);

    // Value
    SelectObject(hdc, m_fontLarge);
    SetTextColor(hdc, valueColor);
    RECT valRect = { x + 8, y + 22, x + w - 8, y + h - 4 };
    DrawTextW(hdc, value, -1, &valRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void SovereignDashboardWidget::paintThermalBar(HDC hdc, int x, int y, int w, int h, float tempC, int driveIdx) {
    // Background
    RECT bgRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(CARD_BG);
    FillRect(hdc, &bgRect, bgBr);
    DeleteObject(bgBr);

    // Bar fill
    float norm = (std::max)(0.0f, (std::min)(1.0f, tempC / 80.0f));
    int barH = static_cast<int>(norm * (h - 16));
    COLORREF barColor = tempC < 40 ? RGB(0, 180, 255) : tempC < 55 ? RGB(255, 180, 0) : RGB(255, 60, 60);
    RECT barRect = { x + 2, y + h - 2 - barH, x + w - 2, y + h - 2 };
    HBRUSH barBr = CreateSolidBrush(barColor);
    FillRect(hdc, &barRect, barBr);
    DeleteObject(barBr);

    // Label
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, TEXT_COLOR);
    wchar_t buf[32];
    swprintf_s(buf, L"D%d: %.0f°C", driveIdx, tempC);
    RECT lblRect = { x, y + 1, x + w, y + 14 };
    DrawTextW(hdc, buf, -1, &lblRect, DT_CENTER | DT_SINGLELINE);
}

void SovereignDashboardWidget::paintTierIndicator(HDC hdc, int x, int y, int w, int h) {
    RECT cardRect = { x, y, x + w, y + h };
    HBRUSH cardBrush = CreateSolidBrush(CARD_BG);
    FillRect(hdc, &cardRect, cardBrush);
    DeleteObject(cardBrush);

    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBrush);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, LABEL_COLOR);
    RECT labelRect = { x + 8, y + 6, x + w - 8, y + 20 };
    DrawTextW(hdc, L"Active Tier", -1, &labelRect, DT_LEFT | DT_SINGLELINE);

    const wchar_t* tierNames[] = { L"70B", L"120B", L"800B" };
    int tier = m_cachedStats.activeTier;
    if (tier > 2) tier = 0;
    COLORREF tierColors[] = { GREEN_COLOR, YELLOW_COLOR, RED_COLOR };

    SelectObject(hdc, m_fontLarge);
    SetTextColor(hdc, tierColors[tier]);
    RECT valRect = { x + 8, y + 22, x + w - 8, y + h - 4 };
    DrawTextW(hdc, tierNames[tier], -1, &valRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ============================================================================
// Public API
// ============================================================================

void SovereignDashboardWidget::show() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW);
}

void SovereignDashboardWidget::hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void SovereignDashboardWidget::resize(int x, int y, int w, int h) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, w, h, TRUE);
}
