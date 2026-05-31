#pragma once
#include <windows.h>
#include <string>

// GhostTextOverlay - Renders AI suggestion as gray italic text at cursor position
// Part of the RawrXD IDE ghost text bridge

class GhostTextOverlay {
    HWND m_hWnd = nullptr;          // Parent window (the editor control)
    std::wstring m_suggestion;      // Current ghost text
    int m_cursorX = 0, m_cursorY = 0; // Pixel position where to draw
    COLORREF m_textColor = RGB(128, 128, 128); // Gray color
    HFONT m_hFont = nullptr;        // Matching editor font
    bool m_visible = false;

public:
    void Initialize(HWND hEdit, HFONT hFont);
    void SetSuggestion(const std::wstring& text, int cursorPixelX, int cursorPixelY);
    void Hide();
    bool IsVisible() const { return m_visible; }
    const std::wstring& GetSuggestion() const { return m_suggestion; }
    void Render(HDC hdc);

private:
    SIZE MeasureText(HDC hdc, const std::wstring& text) const;
};