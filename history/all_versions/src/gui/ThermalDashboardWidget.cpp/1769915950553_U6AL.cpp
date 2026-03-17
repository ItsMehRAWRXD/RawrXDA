/**
 * @file ThermalDashboardWidget.cpp
 * @brief Win32/Direct2D widget for live NVMe thermal visualization
 *
 * Displays:
 * - 5 temperature bars (blue=cool, orange=warm, red=hot)
 * - Current tier (70B/120B/800B)
 * - TurboSparse skip percentage
 * - PowerInfer GPU/CPU split
 *
 * Refreshes every 1 second from pocket_lab_turbo.dll
 */
#include "ThermalDashboardWidget.h"
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

// Safe Release helper
template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease != NULL)
    {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = NULL;
    }
}

// ThermalSnapshot struct (must match DLL)
#pragma pack(push, 1)
struct ThermalSnapshot {
    double t0, t1, t2, t3, t4;
    unsigned int tier;
    unsigned int sparseSkipPct;
    unsigned int gpuSplit;
};
#pragma pack(pop)

ThermalDashboardWidget::ThermalDashboardWidget() :
    m_hwnd(NULL),
    m_pD2DFactory(NULL),
    m_pRenderTarget(NULL),
    m_pDWriteFactory(NULL),
    m_pTextFormat(NULL),
    m_pBrushCool(NULL),
    m_pBrushWarm(NULL),
    m_pBrushHot(NULL),
    m_pBrushBg(NULL),
    m_pBrushText(NULL),
    m_pBrushAccent(NULL),
    m_dllLoaded(false),
    m_hDll(NULL),
    m_pfnInit(NULL),
    m_pfnGetThermal(NULL),
    m_tier(0),
    m_sparseSkipPct(0),
    m_gpuSplit(100)
{
    for (int i = 0; i < 5; ++i) m_temps[i] = 0.0;
}

ThermalDashboardWidget::~ThermalDashboardWidget()
{
    DiscardDeviceResources();
    SafeRelease(&m_pD2DFactory);
    SafeRelease(&m_pDWriteFactory);
    SafeRelease(&m_pTextFormat);

    if (m_hDll) {
        FreeLibrary(static_cast<HMODULE>(m_hDll));
    }
}

bool ThermalDashboardWidget::Create(HWND parent, int x, int y, int width, int height)
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
    if (FAILED(hr)) return false;

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

    WNDCLASS wc = {0};
    wc.lpfnWndProc = ThermalDashboardWidget::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "RawrXDThermalWidget";

    RegisterClass(&wc);

    m_hwnd = CreateWindowEx(
        0,
        "RawrXDThermalWidget",
        "Thermal Dashboard",
        WS_CHILD | WS_VISIBLE,
        x, y, width, height,
        parent,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (m_hwnd) {
        SetTimer(m_hwnd, 1, 1000, NULL);
        loadDll();
        return true;
    }
    return false;
}

void ThermalDashboardWidget::SetSize(int width, int height)
{
    if (m_hwnd) {
        SetWindowPos(m_hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

LRESULT CALLBACK ThermalDashboardWidget::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    ThermalDashboardWidget* pThis = NULL;

    if (message == WM_CREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        pThis = (ThermalDashboardWidget*)pcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    }
    else
    {
        pThis = (ThermalDashboardWidget*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis)
    {
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

    case WM_ERASEBKGND:
        return 1; 

    case WM_DESTROY:
        KillTimer(m_hwnd, 1);
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

        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.7f, 1.0f), &m_pBrushCool);
        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.7f, 0.0f), &m_pBrushWarm);
        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.2f, 0.2f), &m_pBrushHot);
        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.14f), &m_pBrushBg);
        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f), &m_pBrushText);
        if (SUCCEEDED(hr)) hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.4f, 0.8f, 1.0f), &m_pBrushAccent);
    }
    return hr;
}

void ThermalDashboardWidget::DiscardDeviceResources()
{
    SafeRelease(&m_pRenderTarget);
    SafeRelease(&m_pBrushCool);
    SafeRelease(&m_pBrushWarm);
    SafeRelease(&m_pBrushHot);
    SafeRelease(&m_pBrushBg);
    SafeRelease(&m_pBrushText);
    SafeRelease(&m_pBrushAccent);
}

void ThermalDashboardWidget::OnResize(UINT width, UINT height)
{
    if (m_pRenderTarget) m_pRenderTarget->Resize(D2D1::SizeU(width, height));
}

void ThermalDashboardWidget::loadDll()
{
    const char* paths[] = {
        "pocket_lab_turbo.dll",
        "bin/pocket_lab_turbo.dll",
        "../pocket_lab_turbo.dll",
        "c:/pocket_lab_turbo.dll",
        "RawrXD_Interconnect.dll"
    };

    for (const char* path : paths) {
        m_hDll = LoadLibraryA(path);
        if (m_hDll) break;
    }

    if (m_hDll) {
        m_pfnInit = (PFN_Init)GetProcAddress((HMODULE)m_hDll, "plt_init_context");
        if (!m_pfnInit) m_pfnInit = (PFN_Init)GetProcAddress((HMODULE)m_hDll, "PocketLabInit");
        
        m_pfnGetThermal = (PFN_GetThermal)GetProcAddress((HMODULE)m_hDll, "plt_get_latest_snapshot");
        if (!m_pfnGetThermal) m_pfnGetThermal = (PFN_GetThermal)GetProcAddress((HMODULE)m_hDll, "PocketLabGetThermal");

        if (m_pfnInit && m_pfnInit() == 0) m_dllLoaded = true;
    }
    
    // Fallback: If no hardware monitor DLL found, don't simulate random values.
    // Explicitly check Windows Pdh for basic disk temp if available, or stay at 0.
    if (!m_dllLoaded) {
        // [Explicit] Attempt WMI or SMART via standard API if simple.
        // For now, leave m_dllLoaded=false to indicate no data rather than simulated data.
        // This widget is strictly for visualizing real hardware data.
    }
}

void ThermalDashboardWidget::OnTimer()
{
    bool updated = false;
    if (m_dllLoaded && m_pfnGetThermal) {
        ThermalSnapshot snap = {0};
        m_pfnGetThermal(&snap);
        m_temps[0] = snap.t0; m_temps[1] = snap.t1; m_temps[2] = snap.t2; m_temps[3] = snap.t3; m_temps[4] = snap.t4;
        m_tier = snap.tier; m_sparseSkipPct = snap.sparseSkipPct; m_gpuSplit = snap.gpuSplit;
        updated = true;
    } 
    
    if (!updated) {
        HANDLE hMMF = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
        if (hMMF) {
            void* pView = MapViewOfFile(hMMF, FILE_MAP_READ, 0, 0, 160);
            if (pView) {
                unsigned int* data = static_cast<unsigned int*>(pView);
                if (data[0] == 0x534F5645) { // "SOVE"
                    int* temps = reinterpret_cast<int*>(data + 4); 
                    for (int i=0; i<5; ++i) m_temps[i] = static_cast<double>(temps[i]);
                    updated = true;
                }
                UnmapViewOfFile(pView);
            }
            CloseHandle(hMMF);
        }
    }

    if (!m_dllLoaded) loadDll();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void ThermalDashboardWidget::OnPaint()
{
    HRESULT hr = CreateDeviceResources();
    if (FAILED(hr)) return;

    if (!(m_pRenderTarget->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
    {
        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(D2D1::ColorF(0.12f, 0.12f, 0.14f)); 

        D2D1_SIZE_F size = m_pRenderTarget->GetSize();
        float margin = 10.0f;
        float graphH = size.height - 40.0f;
        if (graphH < 10) graphH = 10;
        float barWidth = (size.width - (margin * 6)) / 5.0f;
        if (barWidth < 1) barWidth = 1;

        std::wstringstream ss;
        ss << L"Tier: " << m_tier << L" | Skip: " << m_sparseSkipPct << L"% | GPU: " << m_gpuSplit << L"%";
        std::wstring stats = ss.str();
        D2D1_RECT_F textRect = D2D1::RectF(margin, margin, size.width - margin, 35.0f);
        m_pRenderTarget->DrawText(stats.c_str(), stats.length(), m_pTextFormat, textRect, m_pBrushText);

        float yBase = size.height - margin;
        float maxTemp = 90.0f; 

        for (int i = 0; i < 5; ++i) {
            float temp = static_cast<float>(m_temps[i]);
            if (temp < 0) temp = 0;
            float h = (temp / maxTemp) * (graphH - 40.0f); 
            if (h > graphH - 40.0f) h = graphH - 40.0f;

            float x = margin + i * (barWidth + margin);
            float y = yBase - h;

            D2D1_RECT_F barRect = D2D1::RectF(x, y, x + barWidth, yBase);
            ID2D1SolidColorBrush* brush = m_pBrushCool;
            if (temp > TEMP_CRIT) brush = m_pBrushHot;
            else if (temp > TEMP_WARN) brush = m_pBrushWarm;

            m_pRenderTarget->FillRectangle(barRect, brush);

            if (h > 20) {
                std::wstringstream tss;
                tss << std::fixed << std::setprecision(1) << temp;
                std::wstring tStr = tss.str();
                D2D1_RECT_F valRect = D2D1::RectF(x, y, x + barWidth, y + 20.0f);
                m_pRenderTarget->DrawText(tStr.c_str(), tStr.length(), m_pTextFormat, valRect, m_pBrushBg); 
            }
        }
        hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) DiscardDeviceResources();
    }
}

