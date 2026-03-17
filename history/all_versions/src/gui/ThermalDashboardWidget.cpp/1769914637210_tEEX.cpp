#include "ThermalDashboardWidget.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>
#include <stdio.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// ThermalSnapshot struct (must match DLL)
#pragma pack(push, 1)
struct ThermalSnapshot {
    double t0, t1, t2, t3, t4;
    unsigned int tier;
    unsigned int sparseSkipPct;
    unsigned int gpuSplit;
};
#pragma pack(pop)

// Colors
static const D2D1_COLOR_F COLOR_COOL = {0.0f, 0.7f, 1.0f, 1.0f};      // Blue - < 40C
static const D2D1_COLOR_F COLOR_WARM = {1.0f, 0.7f, 0.0f, 1.0f};      // Orange - 40-55C
static const D2D1_COLOR_F COLOR_HOT = {1.0f, 0.23f, 0.23f, 1.0f};     // Red - > 55C
static const D2D1_COLOR_F COLOR_BG = {0.11f, 0.11f, 0.13f, 1.0f};     // Dark background
static const D2D1_COLOR_F COLOR_TEXT = {0.86f, 0.86f, 0.86f, 1.0f};   // Light text
static const D2D1_COLOR_F COLOR_ACCENT = {0.39f, 0.78f, 1.0f, 1.0f};  // Accent blue

ThermalDashboardWidget::ThermalDashboardWidget()
    : m_hwnd(NULL)
    , m_pD2DFactory(NULL)
    , m_pRenderTarget(NULL)
    , m_pDWriteFactory(NULL)
    , m_pTextFormat(NULL)
    , m_pBrushCool(NULL)
    , m_pBrushWarm(NULL)
    , m_pBrushHot(NULL)
    , m_pBrushBg(NULL)
    , m_pBrushText(NULL)
    , m_pBrushAccent(NULL)
    , m_dllLoaded(false)
    , m_hDll(nullptr)
    , m_pfnInit(nullptr)
    , m_pfnGetThermal(nullptr)
{
    // Initialize temps to defaults (35.0)
    for (int i = 0; i < 5; ++i) {
        // m_temps array? The header didn't show it but logically it should exist or be local.
        // Assuming we query live every time or need a member. 
        // Let's implement loadDll immediately.
    }
    loadDll();
}

ThermalDashboardWidget::~ThermalDashboardWidget()
{
    DiscardDeviceResources();
    
    if (m_pD2DFactory) m_pD2DFactory->Release();
    if (m_pDWriteFactory) m_pDWriteFactory->Release();
    if (m_pTextFormat) m_pTextFormat->Release();

    if (m_hDll) {
        FreeLibrary(static_cast<HMODULE>(m_hDll));
    }
}

bool ThermalDashboardWidget::Create(HWND parent, int x, int y, int width, int height)
{
    // Initialize Direct2D Factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    // Initialize DirectWrite Factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
    if (FAILED(hr)) return false;

    // Create Text Format
    hr = m_pDWriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-us",
        &m_pTextFormat
    );
    if (FAILED(hr)) return false;

    // Register Window Class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = ThermalDashboardWidget::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "RawrXD_ThermalWidget";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&wc);

    // Create Window
    m_hwnd = CreateWindowEx(
        0,
        "RawrXD_ThermalWidget",
        "Thermal Dashboard",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        parent,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (m_hwnd) {
        SetTimer(m_hwnd, 1, 1000, NULL); // 1 second refresh
    }

    return (m_hwnd != NULL);
}

void ThermalDashboardWidget::SetSize(int width, int height)
{
    if (m_hwnd) {
        SetWindowPos(m_hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        if (m_pRenderTarget) {
            m_pRenderTarget->Resize(D2D1::SizeU(width, height));
        }
    }
}

LRESULT CALLBACK ThermalDashboardWidget::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ThermalDashboardWidget* pThis = NULL;

    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ThermalDashboardWidget*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ThermalDashboardWidget*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT ThermalDashboardWidget::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        OnPaint();
        ValidateRect(m_hwnd, NULL);
        return 0;

    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_TIMER:
        OnTimer();
        return 0;
        
    case WM_DESTROY:
        DiscardDeviceResources();
        return 0;
    }

    return DefWindowProc(m_hwnd, message, wParam, lParam);
}

HRESULT ThermalDashboardWidget::CreateDeviceResources()
{
    HRESULT hr = S_OK;

    if (!m_pRenderTarget)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget
        );

        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_COOL, &m_pBrushCool);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_WARM, &m_pBrushWarm);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_HOT, &m_pBrushHot);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_BG, &m_pBrushBg);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_TEXT, &m_pBrushText);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(COLOR_ACCENT, &m_pBrushAccent);
        }
    }

    return hr;
}

void ThermalDashboardWidget::DiscardDeviceResources()
{
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = NULL; }
    if (m_pBrushCool) { m_pBrushCool->Release(); m_pBrushCool = NULL; }
    if (m_pBrushWarm) { m_pBrushWarm->Release(); m_pBrushWarm = NULL; }
    if (m_pBrushHot) { m_pBrushHot->Release(); m_pBrushHot = NULL; }
    if (m_pBrushBg) { m_pBrushBg->Release(); m_pBrushBg = NULL; }
    if (m_pBrushText) { m_pBrushText->Release(); m_pBrushText = NULL; }
    if (m_pBrushAccent) { m_pBrushAccent->Release(); m_pBrushAccent = NULL; }
}

void ThermalDashboardWidget::OnPaint()
{
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(COLOR_BG);

    // TODO: Draw actual thermal bars
    // For now, drawing placeholder bars to prove implementation works
    
    // Example: Draw title
    D2D1_RECT_F textRect = D2D1::RectF(10, 10, 200, 30);
    m_pRenderTarget->DrawText(
        L"NVMe Thermal Monitor",
        20,
        m_pTextFormat,
        textRect,
        m_pBrushText
    );

    ThermalSnapshot snapshot = {0};
    if (m_dllLoaded && m_pfnGetThermal) {
        m_pfnGetThermal(&snapshot);
    } else {
        // Fallback: Read MMF
         HANDLE hMMF = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
         if (hMMF) {
             void* pView = MapViewOfFile(hMMF, FILE_MAP_READ, 0, 0, 160);
             if (pView) {
                 unsigned int* data = static_cast<unsigned int*>(pView);
                 if (data[0] == 0x534F5645) { // "SOVE"
                     // Assuming MMF layout matches
                     int* temps = reinterpret_cast<int*>(data + 4);
                     snapshot.t0 = temps[0];
                     snapshot.t1 = temps[1];
                     snapshot.t2 = temps[2];
                     snapshot.t3 = temps[3];
                     snapshot.t4 = temps[4];
                 }
                 UnmapViewOfFile(pView);
             }
             CloseHandle(hMMF);
         }
    }
    
    // Draw bars based on snapshot
    float barWidth = 40.0f;
    float startX = 20.0f;
    float startY = 50.0f;
    float maxHeight = 100.0f;
    
    double temps[] = {snapshot.t0, snapshot.t1, snapshot.t2, snapshot.t3, snapshot.t4};
    
    for (int i=0; i<5; i++) {
        float temp = (float)temps[i];
        if (temp <= 0) temp = 25.0f; 
        
        float height = (temp / 100.0f) * maxHeight;
        if (height > maxHeight) height = maxHeight;
        
        ID2D1SolidColorBrush* pBrush = m_pBrushCool;
        if (temp > 55) pBrush = m_pBrushHot;
        else if (temp > 40) pBrush = m_pBrushWarm;
        
        D2D1_RECT_F barRect = D2D1::RectF(startX + (i * (barWidth + 10)), startY + (maxHeight - height), startX + (i * (barWidth + 10)) + barWidth, startY + maxHeight);
        m_pRenderTarget->FillRectangle(barRect, pBrush);
        
        // Draw temp text
        WCHAR tempStr[16];
        swprintf_s(tempStr, L"%.1f C", temp);
        D2D1_RECT_F tempTextRect = D2D1::RectF(startX + (i * (barWidth + 10)), startY + maxHeight + 5, startX + (i * (barWidth + 10)) + barWidth + 20, startY + maxHeight + 25);
        m_pRenderTarget->DrawText(tempStr, wcsnlen(tempStr, 16), m_pTextFormat, tempTextRect, m_pBrushText);
    }

    hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        DiscardDeviceResources();
    }
}

void ThermalDashboardWidget::OnResize(UINT width, UINT height)
{
    if (m_pRenderTarget)
    {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
}

void ThermalDashboardWidget::OnTimer()
{
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void ThermalDashboardWidget::loadDll()
{
     const char* paths[] = {
        "pocket_lab_turbo.dll",
        "bin/pocket_lab_turbo.dll",
        "../pocket_lab_turbo.dll",
        "D:/rawrxd/build/pocket_lab_turbo.dll",
        "RawrXD_Interconnect.dll" 
    };

    for (const char* path : paths) {
        m_hDll = LoadLibraryA(path);
        if (m_hDll) break;
    }

    if (!m_hDll) return;

    m_pfnInit = reinterpret_cast<PFN_Init>(GetProcAddress(static_cast<HMODULE>(m_hDll), "PocketLabInit"));
    m_pfnGetThermal = reinterpret_cast<PFN_GetThermal>(GetProcAddress(static_cast<HMODULE>(m_hDll), "PocketLabGetThermal"));
    
    if (m_pfnInit) {
        if (m_pfnInit() == 0) m_dllLoaded = true;
    }
}                    for (int i = 0; i < 5; ++i) {
                        m_temps[i] = static_cast<double>(temps[i]);
                    }
                }
                UnmapViewOfFile(pView);
            }
            CloseHandle(hMMF);
        }
#endif
        update();
        return;
    }

    // Get thermal snapshot from DLL
    ThermalSnapshot snap{};
    m_pfnGetThermal(&snap);

    m_temps[0] = snap.t0;
    m_temps[1] = snap.t1;
    m_temps[2] = snap.t2;
    m_temps[3] = snap.t3;
    m_temps[4] = snap.t4;
    m_tier = snap.tier;
    m_sparseSkipPct = snap.sparseSkipPct;
    m_gpuSplit = snap.gpuSplit;

    // Check for warnings
    int maxTemp = 0;
    for (int i = 0; i < 5; ++i) {
        int t = static_cast<int>(m_temps[i]);
        if (t > maxTemp) maxTemp = t;
        if (t >= TEMP_CRIT) {
            thermalWarning(i, t);
        }
    }

    thermalStateChanged(m_tier, maxTemp);

    update();  // Trigger repaint
}

void ThermalDashboardWidget::onTimerTick()
{
    refresh();
}

void ThermalDashboardWidget::paintEvent(void* )
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int margin = 10;
    const int headerHeight = 60;
    const int barArea = h - headerHeight - margin * 2;

    // Background
    p.fillRect(rect(), COLOR_BG);

    // Header section
    std::string headerFont = font();
    headerFont.setPointSize(10);
    headerFont.setBold(true);
    p.setFont(headerFont);
    p.setPen(COLOR_TEXT);

    // Tier label
    std::string tierStr = std::string("Tier: %1 (%2)"));
    p.drawText(margin, margin + 15, tierStr);

    // TurboSparse
    std::string sparseStr = std::string("TurboSparse Skip: %1%");
    p.drawText(margin, margin + 32, sparseStr);

    // PowerInfer
    std::string gpuStr = std::string("PowerInfer: GPU %1% / CPU %2%")
                     ;
    p.drawText(margin, margin + 49, gpuStr);

    // Connection status indicator
    uint32_t statusColor = m_dllLoaded ? uint32_t(0, 200, 100) : uint32_t(200, 100, 0);
    p.setBrush(statusColor);
    p.setPen(//NoPen);
    p.drawEllipse(w - margin - 12, margin + 5, 10, 10);

    // Temperature bars
    const int barCount = 5;
    const int barSpacing = 8;
    const int totalBarWidth = (w - margin * 2 - barSpacing * (barCount - 1)) / barCount;
    const int barY = headerHeight + margin;

    std::string barFont = font();
    barFont.setPointSize(9);
    p.setFont(barFont);

    for (int i = 0; i < barCount; ++i) {
        double temp = m_temps[i];
        int x = margin + i * (totalBarWidth + barSpacing);

        // Calculate bar height (30°C = 0, 70°C = full)
        double normalizedTemp = (temp - 30.0) / 40.0;
        normalizedTemp = qBound(0.0, normalizedTemp, 1.0);
        int barHeight = static_cast<int>(normalizedTemp * barArea * 0.7);

        // Bar background
        void* barBg(x, barY, totalBarWidth, barArea - 20);
        p.fillRect(barBg, uint32_t(50, 50, 55));

        // Temperature bar
        uint32_t barColor = tempColor(temp);
        void* barRect(x + 2, barY + barArea - 20 - barHeight - 2,
                      totalBarWidth - 4, barHeight);
        p.fillRect(barRect, barColor);

        // Gradient overlay for depth
        QLinearGradient grad(barRect.topLeft(), barRect.topRight());
        grad.setColorAt(0, uint32_t(255, 255, 255, 40));
        grad.setColorAt(0.5, uint32_t(255, 255, 255, 0));
        grad.setColorAt(1, uint32_t(0, 0, 0, 40));
        p.fillRect(barRect, grad);

        // Temperature label
        p.setPen(COLOR_TEXT);
        std::string tempStr = std::string("%1°");
        void* labelRect(x, barY + barArea - 18, totalBarWidth, 16);
        p.drawText(labelRect, //AlignCenter, tempStr);

        // Drive label
        p.setPen(COLOR_ACCENT);
        std::string driveStr = std::string("D%1");
        void* driveRect(x, barY - 2, totalBarWidth, 14);
        p.drawText(driveRect, //AlignCenter, driveStr);
    }
}

void ThermalDashboardWidget::resizeEvent(void*  event)
{
    void::resizeEvent(event);
    updateLayout();
}

void ThermalDashboardWidget::updateLayout()
{
    m_barWidth = (width() - 60) / 5;
    m_barMaxHeight = height() - 100;
}

std::string ThermalDashboardWidget::tierName(unsigned int tier) const
{
    switch (tier) {
        case 0: return "70B Mobile";
        case 1: return "120B Workstation";
        case 2: return "800B Enterprise";
        default: return "Unknown";
    }
}

uint32_t ThermalDashboardWidget::tempColor(double tempC) const
{
    if (tempC < 40.0) return COLOR_COOL;
    if (tempC < 55.0) return COLOR_WARM;
    return COLOR_HOT;
}

