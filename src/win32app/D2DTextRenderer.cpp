// ============================================================================
// D2DTextRenderer.cpp — Stub implementation for D2DTextRenderer
// ============================================================================
// Minimal stub that compiles and links. Replace with full implementation
// when restoring the original source.
// ============================================================================

#include "D2DTextRenderer.h"
#include <new>

namespace RawrXD {

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
    m_ready = true;
    return true;
}

void D2DTextRenderer::Shutdown() {
    m_ready = false;
    m_hwnd = nullptr;
}

bool D2DTextRenderer::IsReady() const {
    return m_ready;
}

bool D2DTextRenderer::BeginDraw() {
    return m_ready;
}

void D2DTextRenderer::EndDraw() {
}

void D2DTextRenderer::Clear() {
}

void D2DTextRenderer::Clear(const D2D1_COLOR_F&) {
}

float D2DTextRenderer::GetLineHeight() const {
    return m_lineHeight;
}

void D2DTextRenderer::DrawLine(const wchar_t*, int, float, float, const D2D1_COLOR_F&) {
}

void D2DTextRenderer::DrawColoredLine(const wchar_t*, int, float, float,
                                      const TextColorRun*, int) {
}

void D2DTextRenderer::DrawGhostText(const wchar_t*, int, float, float, float) {
}

void D2DTextRenderer::DrawLineNumber(int, float, float, float, bool) {
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
}

void D2DTextRenderer::DiscardDeviceResources() {
}

} // namespace RawrXD
