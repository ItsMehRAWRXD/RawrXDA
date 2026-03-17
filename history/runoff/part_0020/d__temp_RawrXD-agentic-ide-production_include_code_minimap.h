#pragma once

// Native minimap implementation (non-Qt)
#include <memory>
#include <vector>
#include <functional>
#include <cstdint>

namespace RawrXD {

class NativeTextEditor; // forward-declared native editor interface

class CodeMinimap {
public:
    explicit CodeMinimap(NativeTextEditor* editor = nullptr);
    ~CodeMinimap() = default;

    void setEditor(NativeTextEditor* editor);
    NativeTextEditor* editor() const { return m_editor; }

    void setWidth(int width);
    int width() const { return m_minimapWidth; }

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setLineHeight(int height) { m_lineHeight = height; }
    int lineHeight() const { return m_lineHeight; }

    void setZoomFactor(double factor) { m_zoomFactor = factor; }
    double zoomFactor() const { return m_zoomFactor; }

    // Rendering interface (called by host UI loop)
    void render(void* nativeCanvas, int x, int y, int w, int h);

    // Input handling (host forwards mouse events)
    void onMousePress(int x, int y, int button);
    void onMouseMove(int x, int y, int buttons);
    void onWheel(int delta);
    void onResize(int w, int h);

    // Update hooks
    void onEditorTextChanged();
    void onEditorScrolled();
    void updateViewport();

private:
    void drawMinimap(void* nativeCanvas, int w, int h);
    void drawViewportIndicator(void* nativeCanvas, int w, int h);
    void handleNavigationClick(int y);
    int getLineCountFromEditor() const;
    int getFirstVisibleLine() const;
    int getLastVisibleLine() const;

    NativeTextEditor* m_editor = nullptr;
    bool m_enabled = true;
    int m_minimapWidth = 120;
    int m_lineHeight = 2;
    double m_zoomFactor = 1.0;
    int m_cachedLineCount = 0;
    uint32_t m_textColor = 0xB4B4B4; // RGB24
    uint32_t m_viewportColor = 0x007ACC;
    uint32_t m_viewportAlpha = 0x1E; // alpha for viewport overlay
};

} // namespace RawrXD
