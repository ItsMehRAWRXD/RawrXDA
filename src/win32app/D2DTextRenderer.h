// ============================================================================
// D2DTextRenderer.h — Stub header for D2DTextRenderer
// ============================================================================
// The actual implementation was compiled in a previous build (symbols confirmed
// in build2/RawrXD-Win32IDE.map). This stub provides the minimal declaration
// needed for D2DSyntaxBridge.cpp to compile.
//
// If the full implementation is restored, replace this stub with the real header.
// ============================================================================

#pragma once

#include <windows.h>
#include <d2d1.h>

namespace RawrXD {

struct TextColorRun {
    UINT32 start;
    UINT32 length;
    D2D1_COLOR_F color;
};

class D2DTextRenderer {
public:
    D2DTextRenderer();
    ~D2DTextRenderer();

    bool Initialize(HWND hwnd);
    void Shutdown();

    bool IsReady() const;
    bool BeginDraw();
    void EndDraw();
    void Clear();
    void Clear(const D2D1_COLOR_F& color);

    float GetLineHeight() const;

    // Draw a single line with uniform color
    void DrawLine(const wchar_t* text, int len, float x, float y, const D2D1_COLOR_F& color);

    // Draw a line with per-run colors
    void DrawColoredLine(const wchar_t* text, int len, float x, float y,
                         const TextColorRun* runs, int runCount);

    // Draw ghost text (dimmed, italic)
    void DrawGhostText(const wchar_t* text, int len, float x, float y, float opacity);

    // Draw line number in gutter
    void DrawLineNumber(int number, float x, float y, float width, bool isCurrent);

private:
    bool CreateFactories();
    bool CreateRenderTarget(HWND hwnd);
    bool CreateTextFormats();
    void ComputeMetrics();
    void DiscardDeviceResources();

    void* m_pD2DFactory;
    void* m_pRenderTarget;
    void* m_pDWriteFactory;
    void* m_pTextFormat;
    void* m_pGhostTextFormat;
    void* m_pDefaultBrush;
    void* m_pGhostBrush;
    void* m_pLineNumberBrush;
    void* m_pCurrentLineBrush;

    HWND m_hwnd;
    bool m_ready;
    float m_lineHeight;
    float m_charWidth;
    struct { UINT32 width; UINT32 height; } m_size;
};

} // namespace RawrXD
