#include "NeuralHeatmapRenderer.h"
#include <iostream>
#include <algorithm>

namespace RawrXD {
namespace IDE {

NeuralHeatmapRenderer::NeuralHeatmapRenderer(HWND targetWnd) : m_hWnd(targetWnd) {
    std::cout << "[NeuralHeatmapRenderer] Initialized DirectWrite overlay for HWND " << m_hWnd << std::endl;
}

NeuralHeatmapRenderer::~NeuralHeatmapRenderer() {
    // Cleanup GDI+/DirectWrite resources
    if (m_hWnd) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
    m_attentionPoints.clear();
    std::cout << "[NeuralHeatmapRenderer] Cleanup complete" << std::endl;
}

void NeuralHeatmapRenderer::updateAttentionData(const std::vector<AttentionPoint>& points) {
    m_attentionPoints = points;
    // Invalidate window to trigger redraw
    if (m_visible) {
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void NeuralHeatmapRenderer::render(HDC hdc, const RECT& clientRect) {
    if (!m_visible) return;

    // Phase 2: Design Stub - Render attention as GDI-accelerated heatmap overlay
    for (const auto& pt : m_attentionPoints) {
        drawHeatPoint(hdc, pt);
    }
}

void NeuralHeatmapRenderer::drawHeatPoint(HDC hdc, const AttentionPoint& pt) {
    // Neural Heatmap Phase 2 Implementation logic
    // Using weight-based coloring (blue to red gradient) for token-level attention
    COLORREF color = RGB((BYTE)(255 * pt.weight), (BYTE)(100 * (1.0f - pt.weight)), (BYTE)(200 * (1.0f - pt.weight)));
    HBRUSH brush = CreateSolidBrush(color);
    
    // Calculate rect based on line/column - stub logic
    RECT rect;
    rect.left = pt.column * 10;
    rect.top = pt.line * 18;
    rect.right = rect.left + 8;
    rect.bottom = rect.top + 16;
    
    // Semi-transparent alpha blend placeholder (Win32 AlphaBlend)
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

} // namespace IDE
} // namespace RawrXD
