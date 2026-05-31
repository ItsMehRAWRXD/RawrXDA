// ============================================================================
// Win32IDE_ExtensionPanel.h — LM Studio–style Extension Panel
// ============================================================================
// Floating panel showing:
//   - Installed extensions (name, version, status badge)
//   - In-progress downloads (progress bar, %, MB/s, ETA)
//   - Action buttons per row: [Install] [Activate] [Deactivate] [Remove]
//
// Thread-safe: background threads update ExtensionUIState; panel posts
// WM_EXTENSIONS_REFRESH to itself to redraw.
// ============================================================================

#pragma once
#include "../../include/ExtensionUIState.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

#define WM_EXTENSIONS_REFRESH (WM_APP + 360)

namespace RawrXD {

class ExtensionPanelWindow {
public:
    explicit ExtensionPanelWindow(HWND parentHwnd, HINSTANCE hInst);
    ~ExtensionPanelWindow();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const;
    void Refresh();

    HWND GetHWND() const { return m_hwnd; }

    // Set callback when user clicks an action button
    // params: (extensionId, action) where action = "install"|"activate"|"deactivate"|"remove"
    using ActionCallback = std::function<void(const std::string&, const std::string&)>;
    void SetActionCallback(ActionCallback cb) { m_actionCb = std::move(cb); }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void OnCreate(HWND hwnd);
    void OnSize(int w, int h);
    void OnPaint(HDC hdc);
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void RebuildSnapshot();
    void DrawItem(HDC hdc, const RECT& rc, const ExtensionUIEntry& item, int idx, bool hovered);
    void DrawProgressBar(HDC hdc, const RECT& rc, float pct);
    void DrawButton(HDC hdc, const RECT& rc, const char* text, bool hovered, bool primary);

    static std::wstring Utf8ToWide(const std::string& s);
    static std::string FormatSize(uint64_t bytes);
    static std::string FormatSpeed(double bps);
    static COLORREF StatusColor(ExtensionUIStatus s);
    static const char* StatusLabel(ExtensionUIStatus s);

    // Hit-test: which item index and which button (if any) is at (x,y)
    struct HitTestResult {
        int itemIndex = -1;
        enum { None, Install, Activate, Deactivate, Remove } button = None;
    };
    HitTestResult HitTest(int x, int y) const;

    HWND m_hwnd = nullptr;
    HWND m_hwndParent = nullptr;
    HINSTANCE m_hInst = nullptr;

    HFONT m_hFontUI = nullptr;
    HFONT m_hFontBold = nullptr;
    HFONT m_hFontMono = nullptr;

    std::vector<ExtensionUIEntry> m_snapshot;
    int m_hoverItem = -1;
    int m_hoverButton = HitTestResult::None;

    ActionCallback m_actionCb;

    // Layout constants
    static constexpr int kItemHeight = 72;
    static constexpr int kMargin = 8;
    static constexpr int kButtonWidth = 70;
    static constexpr int kButtonHeight = 22;
    static constexpr int kProgressHeight = 8;
    static constexpr int kGap = 4;

    static constexpr COLORREF kBgPanel = RGB(30, 30, 30);
    static constexpr COLORREF kBgItem = RGB(37, 37, 38);
    static constexpr COLORREF kBgItemHover = RGB(42, 42, 44);
    static constexpr COLORREF kBorderItem = RGB(55, 55, 57);
    static constexpr COLORREF kTextMain = RGB(212, 212, 212);
    static constexpr COLORREF kTextSub = RGB(150, 150, 150);
    static constexpr COLORREF kTextError = RGB(200, 70, 60);
    static constexpr COLORREF kClrProgress = RGB(70, 140, 210);
    static constexpr COLORREF kClrProgressBg = RGB(50, 50, 50);
    static constexpr COLORREF kBtnPrimary = RGB(0, 112, 224);
    static constexpr COLORREF kBtnPrimaryHover = RGB(0, 130, 255);
    static constexpr COLORREF kBtnSecondary = RGB(60, 60, 60);
    static constexpr COLORREF kBtnSecondaryHover = RGB(80, 80, 80);
    static constexpr COLORREF kBtnText = RGB(255, 255, 255);
};

} // namespace RawrXD
