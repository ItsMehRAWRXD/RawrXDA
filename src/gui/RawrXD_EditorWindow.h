#pragma once
// RawrXD_EditorWindow.h
// Direct2D-based Text Editor replacement for QTextEdit
// Supports syntax highlighting, scrolling, selection, and high performance rendering
// INTEGRATED: SovereignTextBuffer for maximum TPS performance

#ifndef RAWRXD_EDITORWINDOW_H
#define RAWRXD_EDITORWINDOW_H

#include "../RawrXD_Foundation.h"
#include "../../include/minimonaco.h"
#include <d2d1.h>
#include <dwrite.h>
#include <chrono>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace RawrXD {

class EditorWindow {
    HWND hwnd;
    HWND hParent;
    
    // Direct2D Resources
    ID2D1Factory* pD2DFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    IDWriteFactory* pDWriteFactory;
    IDWriteTextFormat* pTextFormat;
    ID2D1SolidColorBrush* pBrushText;
    ID2D1SolidColorBrush* pBrushSelection;
    ID2D1SolidColorBrush* pBrushBackground;
    ID2D1SolidColorBrush* pBrushLineNumber;
    
    // Text Content - MiniMonaco gap buffer for maximum TPS
    MiniMonaco::TextBuffer buffer_;
    
    // Performance monitoring
    struct PerformanceMetrics {
        std::chrono::nanoseconds last_edit_time{0};
        std::chrono::nanoseconds total_edit_time{0};
        uint64_t edit_count{0};
        size_t max_throughput{0};
        
        void record_edit(std::chrono::nanoseconds duration) {
            last_edit_time = duration;
            total_edit_time += duration;
            edit_count++;
            
            if (edit_count > 0) {
                double ops_per_sec = 1.0 / (std::chrono::duration<double>(duration).count());
                max_throughput = std::max(max_throughput, static_cast<size_t>(ops_per_sec));
            }
        }
        
        double avg_time_ms() const {
            if (edit_count == 0) return 0.0;
            return std::chrono::duration<double, std::milli>(total_edit_time).count() / edit_count;
        }
    } perf_metrics_;
    
    // View State
    int scrollX, scrollY;
    Point cursorPos; // Line, Column
    Point anchorPos; // For selection
    
    // Metrics
    float lineHeight;
    float charWidth;
    int visibleLines;
    
    // Methods
    void createDeviceResources();
    void discardDeviceResources();
    void onPaint();
    void onResize(int w, int h);
    void onKeyDown(int key);
    void onChar(wchar_t ch);
    void onScroll(int dx, int dy);
    void onLButtonDown(int x, int y);
    void onMouseMove(int x, int y);
    
    Point hitTest(int x, int y);
    void ensureCursorVisible();
    
    // Sovereign buffer helpers
    size_t convertCursorToBufferOffset(const Point& cursor) const;
    Point convertBufferOffsetToCursor(size_t offset) const;
    void updateCursorPosition(size_t buffer_pos);
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
public:
    EditorWindow();
    ~EditorWindow();
    
    bool create(HWND parent, int x, int y, int w, int h);
    HWND handle() const { return hwnd; }
    
    void setText(const String& text);
    String getText() const;
    void appendText(const String& text);
    
    void setFont(const String& family, float size);
    
    // Commands
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    
    // Performance monitoring
    void dumpPerformanceStats() const;
    PerformanceMetrics getPerformanceMetrics() const { return perf_metrics_; }
};

} // namespace RawrXD

#endif // RAWRXD_EDITORWINDOW_H
