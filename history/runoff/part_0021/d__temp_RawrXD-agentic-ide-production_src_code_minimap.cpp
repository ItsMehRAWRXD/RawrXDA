#include "code_minimap.h"
#include "native_text_editor.h"
#include "native_graphics.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {

CodeMinimap::CodeMinimap(NativeTextEditor* editor)
    : m_editor(editor) {}

void CodeMinimap::setEditor(NativeTextEditor* editor)
{
    m_editor = editor;
    m_cachedLineCount = getLineCountFromEditor();
}

void CodeMinimap::setWidth(int width)
{
    m_minimapWidth = width;
}

void CodeMinimap::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void CodeMinimap::render(void* nativeCanvas, int x, int y, int w, int h)
{
    if (!m_enabled || !m_editor) return;
    // Fill background
    NativeCanvasFillRect(nativeCanvas, x, y, w, h, 0x1E1E1E);
    NativeCanvasDrawLine(nativeCanvas, x, y, x, y + h - 1, 0x3E3E42);

    drawMinimap(nativeCanvas, w, h);
    drawViewportIndicator(nativeCanvas, w, h);
}

void CodeMinimap::drawMinimap(void* nativeCanvas, int w, int h)
{
    if (!m_editor) return;

    int totalLines = getLineCountFromEditor();
    if (totalLines == 0) return;

    double pixelsPerLine = static_cast<double>(h) / totalLines;

    // Simple fixed-width font simulation: draw small bars per line
    int lineNum = 0;
    auto lines = m_editor->getLines();
    for (const auto &line : lines) {
        if (lineNum >= totalLines) break;
        int y = static_cast<int>(lineNum * pixelsPerLine);
        if (y > h) break;

        if (!line.empty()) {
            int lineLength = static_cast<int>(line.size());
            int colorIntensity = std::clamp(150 + (lineLength % 100), 50, 255);
            uint32_t color = ((colorIntensity & 0xFF) << 16) | ((static_cast<int>(colorIntensity * 0.7) & 0xFF) << 8) | (static_cast<int>(colorIntensity * 0.5) & 0xFF);
            NativeCanvasFillRect(nativeCanvas, 5, y, std::max(1, w - 10), std::max(1, static_cast<int>(std::round(pixelsPerLine))), color);
        }
        ++lineNum;
    }
}

void CodeMinimap::drawViewportIndicator(void* nativeCanvas, int w, int h)
{
    if (!m_editor) return;
    int totalLines = getLineCountFromEditor();
    if (totalLines == 0) return;

    double pixelsPerLine = static_cast<double>(h) / totalLines;
    int firstVisible = getFirstVisibleLine();
    int lastVisible = getLastVisibleLine();

    int viewportStart = static_cast<int>(firstVisible * pixelsPerLine);
    int viewportEnd = static_cast<int>(lastVisible * pixelsPerLine);
    int viewportHeight = std::max(10, viewportEnd - viewportStart);

    NativeCanvasFillRect(nativeCanvas, 0, viewportStart, w, viewportHeight, (m_viewportColor & 0xFFFFFF) | (m_viewportAlpha << 24));
    NativeCanvasDrawRect(nativeCanvas, 0, viewportStart, w - 1, viewportHeight - 1, m_viewportColor);
}

void CodeMinimap::onMousePress(int x, int y, int /*button*/)
{
    if (m_enabled) handleNavigationClick(y);
}

void CodeMinimap::onMouseMove(int x, int y, int /*buttons*/)
{
    // If dragging, navigate
    // Host can detect dragging and call this accordingly
}

void CodeMinimap::onWheel(int /*delta*/)
{
    // Forwarding wheel is host responsibility by telling editor to scroll
}

void CodeMinimap::onResize(int /*w*/, int /*h*/)
{
    updateViewport();
}

void CodeMinimap::onEditorTextChanged()
{
    int newLineCount = getLineCountFromEditor();
    if (newLineCount != m_cachedLineCount) {
        m_cachedLineCount = newLineCount;
    }
}

void CodeMinimap::onEditorScrolled()
{
    // Host should call update/render as needed
}

void CodeMinimap::updateViewport()
{
    // No-op: render will pick up latest editor state
}

int CodeMinimap::getLineCountFromEditor() const
{
    if (!m_editor) return 0;
    return static_cast<int>(m_editor->getLines().size());
}

int CodeMinimap::getFirstVisibleLine() const
{
    if (!m_editor) return 0;
    return m_editor->firstVisibleLine();
}

int CodeMinimap::getLastVisibleLine() const
{
    if (!m_editor) return 0;
    return m_editor->lastVisibleLine();
}

} // namespace RawrXD
