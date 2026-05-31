// ============================================================================
// D2DTextRenderer.cpp — GDI-backed fallback implementation
// ============================================================================
// This keeps the D2DTextRenderer API live even when Direct2D resources are
// unavailable, ensuring GUI syntax/ghost overlays are rendered through Win32 GDI
// instead of silently no-op stubs.
// ============================================================================

#include "D2DTextRenderer.h"
#include <string>

namespace RawrXD {

namespace {
COLORREF ToColorRef(const D2D1_COLOR_F& c) {
    auto clamp = [](float v) -> int {
        if (v < 0.0f) return 0;
        if (v > 1.0f) return 255;
        return static_cast<int>(v * 255.0f);
    };
    return RGB(clamp(c.r), clamp(c.g), clamp(c.b));
}

HDC AsHdc(void* p) {
    return reinterpret_cast<HDC>(p);
}
} // namespace

D2DTextRenderer::D2DTextRenderer()
    : m_pD2DFactory(nullptr)
    , m_pRenderTarget(nullptr)
    , m_pDWriteFactory(nullptr)
    , m_pTextFormat(nullptr)
    , m_pGhostTextFormat(nullptr)
    , m_pDefaultBrush(nullptr)
    , m_pGhostBrush(nullptr)
    , m_pLineNumberBrush(nullptr)
    , m_pCurrentLineBrush(nullptr)
    , m_hwnd(nullptr)
    , m_ready(false)
    , m_lineHeight(16.0f)
    , m_charWidth(8.0f)
    , m_size{}
{
}

D2DTextRenderer::~D2DTextRenderer() {
    Shutdown();
}

bool D2DTextRenderer::Initialize(HWND hwnd) {
    m_hwnd = hwnd;
    m_ready = (m_hwnd != nullptr && IsWindow(m_hwnd));
    if (!m_ready) {
        return false;
    }
    ComputeMetrics();
    m_ready = true;
    return true;
}

void D2DTextRenderer::Shutdown() {
    EndDraw();
    m_ready = false;
    m_hwnd = nullptr;
}

bool D2DTextRenderer::IsReady() const {
    return m_ready;
}

bool D2DTextRenderer::BeginDraw() {
    if (!m_ready || !m_hwnd || !IsWindow(m_hwnd)) {
        return false;
    }
    if (m_pRenderTarget == nullptr) {
        HDC hdc = GetDC(m_hwnd);
        if (!hdc) {
            return false;
        }
        SetBkMode(hdc, TRANSPARENT);
        m_pRenderTarget = hdc;
    }
    return true;
}

void D2DTextRenderer::EndDraw() {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (hdc && m_hwnd && IsWindow(m_hwnd)) {
        ReleaseDC(m_hwnd, hdc);
    }
    m_pRenderTarget = nullptr;
}

void D2DTextRenderer::Clear() {
    Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f));
}

void D2DTextRenderer::Clear(const D2D1_COLOR_F& color) {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (!hdc || !m_hwnd) {
        return;
    }
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    HBRUSH brush = CreateSolidBrush(ToColorRef(color));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

float D2DTextRenderer::GetLineHeight() const {
    return m_lineHeight;
}

void D2DTextRenderer::DrawLine(const wchar_t* text, int len, float x, float y, const D2D1_COLOR_F& color) {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (!hdc || !text || len <= 0) {
        return;
    }
    SetTextColor(hdc, ToColorRef(color));
    TextOutW(hdc, static_cast<int>(x), static_cast<int>(y), text, len);
}

void D2DTextRenderer::DrawColoredLine(const wchar_t* text, int len, float x, float y,
                                      const TextColorRun* runs, int runCount) {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (!hdc || !text || len <= 0) {
        return;
    }

    if (!runs || runCount <= 0) {
        DrawLine(text, len, x, y, D2D1::ColorF(0.83f, 0.83f, 0.83f));
        return;
    }

    float cursorX = x;
    for (int i = 0; i < runCount; ++i) {
        const TextColorRun& run = runs[i];
        if (run.start >= static_cast<UINT32>(len) || run.length == 0) {
            continue;
        }
        const int safeLen = static_cast<int>((run.start + run.length > static_cast<UINT32>(len))
                                                 ? (static_cast<UINT32>(len) - run.start)
                                                 : run.length);
        if (safeLen <= 0) {
            continue;
        }

        const wchar_t* runText = text + run.start;
        SetTextColor(hdc, ToColorRef(run.color));
        TextOutW(hdc, static_cast<int>(cursorX), static_cast<int>(y), runText, safeLen);
        cursorX += (static_cast<float>(safeLen) * m_charWidth);
    }
}

void D2DTextRenderer::DrawGhostText(const wchar_t* text, int len, float x, float y, float opacity) {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (!hdc || !text || len <= 0) {
        return;
    }
    const float alpha = (opacity < 0.0f) ? 0.0f : ((opacity > 1.0f) ? 1.0f : opacity);
    const int g = static_cast<int>(140.0f * alpha);
    SetTextColor(hdc, RGB(g, g, g));
    TextOutW(hdc, static_cast<int>(x), static_cast<int>(y), text, len);
}

void D2DTextRenderer::DrawLineNumber(int number, float x, float y, float width, bool isCurrent) {
    HDC hdc = AsHdc(m_pRenderTarget);
    if (!hdc) {
        return;
    }
    wchar_t buf[32]{};
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d", number);
    SetTextColor(hdc, isCurrent ? RGB(220, 220, 220) : RGB(120, 120, 120));
    const int textX = static_cast<int>((x + width) - (static_cast<float>(wcslen(buf)) * m_charWidth));
    TextOutW(hdc, textX, static_cast<int>(y), buf, static_cast<int>(wcslen(buf)));
}

bool D2DTextRenderer::CreateFactories() {
    return true;
}

bool D2DTextRenderer::CreateRenderTarget(HWND) {
    return true;
}

bool D2DTextRenderer::CreateTextFormats() {
    return true;
}

void D2DTextRenderer::ComputeMetrics() {
    if (!m_hwnd || !IsWindow(m_hwnd)) {
        return;
    }
    HDC hdc = GetDC(m_hwnd);
    if (!hdc) {
        return;
    }
    TEXTMETRICW tm{};
    if (GetTextMetricsW(hdc, &tm)) {
        m_lineHeight = static_cast<float>(tm.tmHeight + tm.tmExternalLeading);
        m_charWidth = static_cast<float>((tm.tmAveCharWidth > 0) ? tm.tmAveCharWidth : 8);
    }
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    m_size.width = (rc.right > rc.left) ? static_cast<UINT32>(rc.right - rc.left) : 0;
    m_size.height = (rc.bottom > rc.top) ? static_cast<UINT32>(rc.bottom - rc.top) : 0;
    ReleaseDC(m_hwnd, hdc);
}

void D2DTextRenderer::DiscardDeviceResources() {
    EndDraw();
}

} // namespace RawrXD
