#pragma once
#include "RawrXD_Window.h"
#include "RawrXD_SignalSlot.h"
#include <d2d1.h>
#include <dwrite.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace RawrXD {
struct MetricPoint {
    float value;
    ULONGLONG timestamp;
    std::wstring label;
};
class ObservabilityDashboard : public Window {
    ID2D1Factory* d2dFactory;
    ID2D1HwndRenderTarget* rt;
    IDWriteFactory* dwFactory;
    IDWriteTextFormat* textFormat;
    Signal<const std::wstring&> onMetricSelected;
    std::vector<MetricPoint> cpuHistory;
    std::vector<MetricPoint> memHistory;
    std::vector<MetricPoint> inferenceHistory;
    HWND hCombo;
public:
    ObservabilityDashboard() : Window(L"RawrXD_Observability"), d2dFactory(nullptr), rt(nullptr), dwFactory(nullptr), textFormat(nullptr), hCombo(nullptr) {}
    void Create(HWND parent, const RECT& rect) {
        Window::Create(parent, rect, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN);
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dwFactory);
        dwFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &textFormat);
        hCombo = CreateWindowExW(0, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST|WS_CHILD|WS_VISIBLE,
            10, 10, 150, 200, m_hwnd, (HMENU)201, GetModuleHandle(nullptr), nullptr);
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"CPU");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Memory");
        SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Inference");
        SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
        CreateRenderTarget();
    }
    void AddMetric(const std::wstring& category, float val) {
        MetricPoint p = {val, GetTickCount64(), category};
        if(category == L"CPU") cpuHistory.push_back(p);
        else if(category == L"Memory") memHistory.push_back(p);
        else if(category == L"Inference") inferenceHistory.push_back(p);
        if(cpuHistory.size() > 300) cpuHistory.erase(cpuHistory.begin());
        if(memHistory.size() > 300) memHistory.erase(memHistory.begin());
        if(inferenceHistory.size() > 300) inferenceHistory.erase(inferenceHistory.begin());
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
protected:
    void CreateRenderTarget() {
        if(rt) rt->Release();
        RECT rc; GetClientRect(m_hwnd, &rc);
        d2dFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, D2D1::SizeU(rc.right-rc.left, rc.bottom-rc.top)), &rt);
    }
    void DrawGraph(const std::vector<MetricPoint>& data, D2D1_COLOR_F color, float x, float y, float w, float h) {
        if(data.empty() || !rt) return;
        ID2D1SolidColorBrush* brush;
        rt->CreateSolidColorBrush(color, &brush);
        float step = w / (float)data.size();
        float maxVal = 100.0f;
        for(const auto& p : data) if(p.value > maxVal) maxVal = p.value;
        for(size_t i=1; i<data.size(); i++) {
            D2D1_POINT_2F p0 = {x + (i-1)*step, y + h - (data[i-1].value/maxVal)*h};
            D2D1_POINT_2F p1 = {x + i*step, y + h - (data[i].value/maxVal)*h};
            rt->DrawLine(p0, p1, brush, 2.0f);
        }
        brush->Release();
    }
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) override {
        if(msg == WM_SIZE) { if(rt) rt->Release(); rt=nullptr; CreateRenderTarget(); InvalidateRect(m_hwnd, nullptr, FALSE); return 0; }
        if(msg == WM_PAINT) {
            PAINTSTRUCT ps; BeginPaint(m_hwnd, &ps);
            if(!rt) CreateRenderTarget();
            rt->BeginDraw();
            rt->Clear(D2D1::ColorF(D2D1::ColorF::Black));
            RECT rc; GetClientRect(m_hwnd, &rc);
            int sel = (int)SendMessageW(hCombo, CB_GETCURSEL, 0, 0);
            if(sel == 0) DrawGraph(cpuHistory, D2D1::ColorF(0.0f, 1.0f, 0.0f), 10, 40, (float)(rc.right-20), (float)(rc.bottom-50));
            else if(sel == 1) DrawGraph(memHistory, D2D1::ColorF(0.0f, 0.5f, 1.0f), 10, 40, (float)(rc.right-20), (float)(rc.bottom-50));
            else if(sel == 2) DrawGraph(inferenceHistory, D2D1::ColorF(1.0f, 0.5f, 0.0f), 10, 40, (float)(rc.right-20), (float)(rc.bottom-50));
            rt->EndDraw();
            EndPaint(m_hwnd, &ps);
            return 0;
        }
        return Window::WndProc(msg, wp, lp);
    }
};
} // namespace RawrXD
