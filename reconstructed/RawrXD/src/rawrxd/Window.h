#pragma once
#include "RawrXD_SignalSlot.h"
#include "RawrXD_Foundation.h"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

namespace RawrXD {

class Window {
protected:
    // Canonical HWND member — 'm_hwnd' alias provided for widget compatibility
    HWND hwnd = nullptr;

    static LRESULT CALLBACK WndProcRouter(HWND, UINT, WPARAM, LPARAM);

    // ── Dual dispatch ──────────────────────────────────────────────
    // New-style widgets override WndProc(); legacy code overrides handleMessage().
    // Default handleMessage() calls WndProc() so either path works.
    virtual LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    // Override this in dock widgets (ObservabilityDashboard, TrainingDialog, etc.)
    virtual LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    // Convenience event handlers (called from default handleMessage)
    virtual void paintEvent(PAINTSTRUCT& ps);
    virtual void resizeEvent(int w, int h);
    virtual void mousePressEvent(int x, int y, int button);
    virtual void mouseReleaseEvent(int x, int y, int button);
    virtual void mouseMoveEvent(int x, int y, int mods);
    virtual void keyPressEvent(int key, int mods);
    virtual void charEvent(wchar_t c);
    virtual void closeEvent();

public:
    Window* parent = nullptr;
    std::vector<Window*> children;

    Window() = default;
    Window(Window* p) : parent(p) {}

    // Widget-style constructor: registers a custom window class name
    explicit Window(const wchar_t* className) : m_className(className ? className : L"") {}

    virtual ~Window();

    // ── Original API (Editor, ChatPanel, etc.) ─────────────────────
    void create(Window* parent, const String& title,
                DWORD style = WS_OVERLAPPEDWINDOW, DWORD exStyle = 0);

    // ── Widget API (docks, dialogs — takes HWND parent + RECT) ────
    void Create(HWND parentHwnd, const RECT& rect, DWORD style);

    void show();
    void hide();
    void move(int x, int y);
    void resize(int w, int h);
    void setGeometry(int x, int y, int w, int h);

    void setTitle(const String& s);
    String title() const;

    HWND nativeHandle() const { return hwnd; }
    void update(); // InvalidateRect

    int width() const;
    int height() const;

    // Child management
    void addChild(Window* child);
    void removeChild(Window* child);

    // ── Helper for setting default GUI font on child controls ──────
    void SetFont(HWND ctrl) {
        static HFONT hFont = nullptr;
        if (!hFont) {
            NONCLIENTMETRICSW ncm = { sizeof(ncm) };
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            hFont = CreateFontIndirectW(&ncm.lfMessageFont);
        }
        SendMessageW(ctrl, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // ── Alias so widget code can use m_hwnd ────────────────────────
    // (MSVC & GCC allow reference-to-member in same object)
    HWND& m_hwnd = hwnd;

private:
    std::wstring m_className;          // Custom class name for widget-style ctor
    bool         m_classRegistered = false;

    void ensureClassRegistered(const wchar_t* name);
};

} // namespace RawrXD
