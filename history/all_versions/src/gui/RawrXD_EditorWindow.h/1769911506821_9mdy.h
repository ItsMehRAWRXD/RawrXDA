#pragma once
// RawrXD_EditorWindow.h
// Direct2D-based Text Editor replacement for QTextEdit
// Supports syntax highlighting, scrolling, selection, and high performance rendering

#ifndef RAWRXD_EDITORWINDOW_H
#define RAWRXD_EDITORWINDOW_H

#include "../RawrXD_Foundation.h"
#include <d2d1.h>
#include <dwrite.h>
#include <deque>

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
    ID2D1SolidColorBrush* pBrushGhost;
    
    // Text Content (Rope or Vector of Lines for now)
    Vector<String> lines;
    
    // View State
    int scrollX, scrollY;
    Point cursorPos; // Line, Column
    Point anchorPos; // For selection
    bool isSelecting = false;
    String ghostText; // Auto-completion suggestion

    
    // Metrics
    float lineHeight;
    float charWidth;
    int visibleLines;
    
    // Command History
    struct EditorCommand {
        enum Type { Insert, Delete } type;
        Point pos;
        String text;
    };
    std::deque<EditorCommand> undoStack;
    std::deque<EditorCommand> redoStack;
    void pushCommand(const EditorCommand& cmd);

    // Methods
    void createDeviceResources();
    void discardDeviceResources();
    void onPaint();
    void onResize(int w, int h);
    void onKeyDown(int key);
    void onChar(wchar_t ch);
    void onScroll(int dx, int dy);
    void onLButtonDown(int x, int y);
    void onLButtonUp(int x, int y);
    void onMouseMove(int x, int y);
    
    Point hitTest(int x, int y);
    void ensureCursorVisible();
    
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
    
    // State Access
    Point getCursorPosition() const { return cursorPos; }
    String getLine(int index) const {
        if (index >= 0 && index < lines.count()) return lines[index];
        return L"";
    }
    int getLineCount() const { return lines.count(); }

    // Commands
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    
    // Ghost Text
    void setGhostText(const String& text);
    void acceptGhostText();
};

} // namespace RawrXD

#endif // RAWRXD_EDITORWINDOW_H
