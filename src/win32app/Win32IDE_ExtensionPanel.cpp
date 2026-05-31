// ============================================================================
// Win32IDE_ExtensionPanel.cpp — LM Studio–style Extension Panel Implementation
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <gdiplus.h>

#include "Win32IDE_ExtensionPanel.h"
#include "../../include/ExtensionUIState.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")

namespace RawrXD {

// ============================================================================
// Helpers
// ============================================================================

std::wstring ExtensionPanelWindow::Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

std::string ExtensionPanelWindow::FormatSize(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    return buf;
}

std::string ExtensionPanelWindow::FormatSpeed(double bps) {
    char buf[64];
    if (bps >= 1024.0 * 1024.0)
        std::snprintf(buf, sizeof(buf), "%.1f MB/s", bps / (1024.0 * 1024.0));
    else if (bps >= 1024.0)
        std::snprintf(buf, sizeof(buf), "%.1f KB/s", bps / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%.0f B/s", bps);
    return buf;
}

COLORREF ExtensionPanelWindow::StatusColor(ExtensionUIStatus s) {
    switch (s) {
        case ExtensionUIStatus::Active:       return RGB(108, 182, 91);
        case ExtensionUIStatus::Installed:    return RGB(150, 150, 150);
        case ExtensionUIStatus::Downloading:  return RGB(70, 140, 210);
        case ExtensionUIStatus::Failed:       return RGB(200, 70, 60);
        default:                            return RGB(130, 130, 130);
    }
}

const char* ExtensionPanelWindow::StatusLabel(ExtensionUIStatus s) {
    switch (s) {
        case ExtensionUIStatus::Active:       return "Active";
        case ExtensionUIStatus::Installed:    return "Installed";
        case ExtensionUIStatus::Downloading:  return "Downloading";
        case ExtensionUIStatus::Failed:       return "Failed";
        default:                            return "Not Installed";
    }
}

// ============================================================================
// Construction / Destruction
// ============================================================================

ExtensionPanelWindow::ExtensionPanelWindow(HWND parentHwnd, HINSTANCE hInst)
    : m_hwndParent(parentHwnd), m_hInst(hInst) {}

ExtensionPanelWindow::~ExtensionPanelWindow() {
    if (m_hFontUI)    DeleteObject(m_hFontUI);
    if (m_hFontBold)  DeleteObject(m_hFontBold);
    if (m_hFontMono)  DeleteObject(m_hFontMono);
    if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
}

// ============================================================================
// Window Creation
// ============================================================================

bool ExtensionPanelWindow::Create() {
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kBgPanel);
    wc.lpszClassName = "RawrXDExtensionPanel";
    RegisterClassExA(&wc); // idempotent

    RECT parentRc = {0, 0, 1024, 768};
    if (m_hwndParent && IsWindow(m_hwndParent))
        GetWindowRect(m_hwndParent, &parentRc);

    const int W = 480;
    const int H = 640;
    const int X = parentRc.right - W - 20;
    const int Y = parentRc.top + 60;

    m_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_APPWINDOW,
        "RawrXDExtensionPanel",
        "Extensions",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_CLIPCHILDREN | WS_VSCROLL,
        X, Y, W, H,
        m_hwndParent,
        nullptr,
        m_hInst,
        this);

    if (!m_hwnd) return false;

    // Register change callback so background threads trigger refresh
    ExtensionUIState::Instance().setChangeCallback([this]() {
        if (m_hwnd && IsWindow(m_hwnd))
            PostMessageA(m_hwnd, WM_EXTENSIONS_REFRESH, 0, 0);
    });

    return true;
}

void ExtensionPanelWindow::Show() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    Refresh();
}

void ExtensionPanelWindow::Hide() {
    if (m_hwnd && IsWindow(m_hwnd)) ShowWindow(m_hwnd, SW_HIDE);
}

bool ExtensionPanelWindow::IsVisible() const {
    return m_hwnd && IsWindow(m_hwnd) && IsWindowVisible(m_hwnd);
}

void ExtensionPanelWindow::Refresh() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    PostMessageA(m_hwnd, WM_EXTENSIONS_REFRESH, 0, 0);
}

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK ExtensionPanelWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    ExtensionPanelWindow* self = nullptr;
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
        self = reinterpret_cast<ExtensionPanelWindow*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ExtensionPanelWindow*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

LRESULT ExtensionPanelWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            OnCreate(hwnd);
            return 0;

        case WM_SIZE: {
            int w = LOWORD(lp);
            int h = HIWORD(lp);
            OnSize(w, h);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            OnMouseMove(x, y);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            OnLButtonDown(x, y);
            return 0;
        }

        case WM_EXTENSIONS_REFRESH:
            RebuildSnapshot();
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;

        case WM_DESTROY:
            m_hwnd = nullptr;
            return 0;

        default:
            return DefWindowProcA(hwnd, msg, wp, lp);
    }
}

// ============================================================================
// Creation / Layout
// ============================================================================

void ExtensionPanelWindow::OnCreate(HWND hwnd) {
    NONCLIENTMETRICSA ncm = {};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    m_hFontUI   = CreateFontIndirectA(&ncm.lfMessageFont);
    m_hFontBold = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    m_hFontMono = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");

    RebuildSnapshot();
}

void ExtensionPanelWindow::OnSize(int w, int h) {
    // Update scroll range based on content
    int contentHeight = static_cast<int>(m_snapshot.size()) * kItemHeight + kMargin * 2;
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE;
    si.nMin   = 0;
    si.nMax   = contentHeight;
    si.nPage  = h;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

// ============================================================================
// Rendering
// ============================================================================

void ExtensionPanelWindow::OnPaint(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    // Double-buffer
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    // Background
    FillRect(hdcMem, &rcClient, (HBRUSH)GetStockObject(BLACK_BRUSH));
    HBRUSH hbrBg = CreateSolidBrush(kBgPanel);
    FillRect(hdcMem, &rcClient, hbrBg);
    DeleteObject(hbrBg);

    // Scroll offset
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    GetScrollInfo(m_hwnd, SB_VERT, &si);
    int scrollY = si.nPos;

    // Title
    RECT rcTitle = {kMargin, kMargin, rcClient.right - kMargin, kMargin + 24};
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, kTextMain);
    SelectObject(hdcMem, m_hFontBold);
    DrawTextA(hdcMem, "Extensions", -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Items
    int y = kMargin + 28;
    for (size_t i = 0; i < m_snapshot.size(); ++i) {
        RECT rcItem = {kMargin, y - scrollY, rcClient.right - kMargin, y + kItemHeight - scrollY};
        if (rcItem.bottom >= 0 && rcItem.top < rcClient.bottom) {
            bool hovered = (static_cast<int>(i) == m_hoverItem);
            DrawItem(hdcMem, rcItem, m_snapshot[i], static_cast<int>(i), hovered);
        }
        y += kItemHeight + kGap;
    }

    // If empty
    if (m_snapshot.empty()) {
        RECT rcEmpty = {kMargin, rcClient.bottom / 2 - 20, rcClient.right - kMargin, rcClient.bottom / 2 + 20};
        SetTextColor(hdcMem, kTextSub);
        SelectObject(hdcMem, m_hFontUI);
        DrawTextA(hdcMem, "No extensions installed. Use the marketplace to discover extensions.",
                  -1, &rcEmpty, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
    }

    BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);

    SelectObject(hdcMem, hbmOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void ExtensionPanelWindow::DrawItem(HDC hdc, const RECT& rc, const ExtensionUIEntry& item,
                                     int idx, bool hovered) {
    // Background
    COLORREF bg = hovered ? kBgItemHover : kBgItem;
    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, kBorderItem);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);

    int x = rc.left + 8;
    int y = rc.top + 6;

    // Name + version
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextMain);
    SelectObject(hdc, m_hFontBold);
    std::string nameVer = item.name;
    if (!item.version.empty()) nameVer += " v" + item.version;
    RECT rcName = {x, y, rc.right - 8, y + 18};
    DrawTextA(hdc, nameVer.c_str(), -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Status badge (right side)
    COLORREF statusClr = StatusColor(item.status);
    const char* statusText = StatusLabel(item.status);
    SIZE szStatus;
    SelectObject(hdc, m_hFontMono);
    GetTextExtentPoint32A(hdc, statusText, static_cast<int>(strlen(statusText)), &szStatus);
    RECT rcStatus = {rc.right - 12 - szStatus.cx - 8, y, rc.right - 12, y + 18};
    HBRUSH hbrStatus = CreateSolidBrush(statusClr);
    FillRect(hdc, &rcStatus, hbrStatus);
    DeleteObject(hbrStatus);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextA(hdc, statusText, -1, &rcStatus, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Description / ID
    y += 20;
    SetTextColor(hdc, kTextSub);
    SelectObject(hdc, m_hFontUI);
    std::string desc = item.description.empty() ? item.id : item.description;
    RECT rcDesc = {x, y, rc.right - 8, y + 16};
    DrawTextA(hdc, desc.c_str(), -1, &rcDesc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Progress bar (if downloading)
    if (item.status == ExtensionUIStatus::Downloading) {
        y += 18;
        RECT rcProgBg = {x, y, rc.right - 8, y + kProgressHeight};
        HBRUSH hbrProgBg = CreateSolidBrush(kClrProgressBg);
        FillRect(hdc, &rcProgBg, hbrProgBg);
        DeleteObject(hbrProgBg);

        int fillW = static_cast<int>((rcProgBg.right - rcProgBg.left) * item.progress);
        if (fillW > 0) {
            RECT rcProgFill = {rcProgBg.left, rcProgBg.top, rcProgBg.left + fillW, rcProgBg.bottom};
            HBRUSH hbrProg = CreateSolidBrush(kClrProgress);
            FillRect(hdc, &rcProgFill, hbrProg);
            DeleteObject(hbrProg);
        }

        // Progress text
        y += kProgressHeight + 2;
        char progText[128];
        std::snprintf(progText, sizeof(progText), "%.0f%%  %s / %s  (%s)",
                      item.progress * 100.0f,
                      FormatSize(item.bytesDownloaded).c_str(),
                      FormatSize(item.totalBytes).c_str(),
                      FormatSpeed(item.speedBps).c_str());
        SetTextColor(hdc, kTextSub);
        SelectObject(hdc, m_hFontMono);
        RECT rcProgText = {x, y, rc.right - 8, y + 14};
        DrawTextA(hdc, progText, -1, &rcProgText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        // Action buttons (only if not downloading)
        y += 18;
        int btnX = rc.right - 8;

        // Determine which buttons to show
        bool showRemove = true;
        bool showActivate = (item.status == ExtensionUIStatus::Installed);
        bool showDeactivate = (item.status == ExtensionUIStatus::Active);

        if (showDeactivate) {
            btnX -= kButtonWidth + 4;
            RECT rcBtn = {btnX, y, btnX + kButtonWidth, y + kButtonHeight};
            bool btnHover = (hovered && m_hoverButton == HitTestResult::Deactivate);
            DrawButton(hdc, rcBtn, "Deactivate", btnHover, false);
        }
        if (showActivate) {
            btnX -= kButtonWidth + 4;
            RECT rcBtn = {btnX, y, btnX + kButtonWidth, y + kButtonHeight};
            bool btnHover = (hovered && m_hoverButton == HitTestResult::Activate);
            DrawButton(hdc, rcBtn, "Activate", btnHover, true);
        }
        if (showRemove) {
            btnX -= kButtonWidth + 4;
            RECT rcBtn = {btnX, y, btnX + kButtonWidth, y + kButtonHeight};
            bool btnHover = (hovered && m_hoverButton == HitTestResult::Remove);
            DrawButton(hdc, rcBtn, "Remove", btnHover, false);
        }
    }

    // Error text
    if (!item.lastError.empty()) {
        y += 18;
        SetTextColor(hdc, kTextError);
        SelectObject(hdc, m_hFontMono);
        RECT rcErr = {x, y, rc.right - 8, y + 14};
        DrawTextA(hdc, item.lastError.c_str(), -1, &rcErr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

void ExtensionPanelWindow::DrawButton(HDC hdc, const RECT& rc, const char* text,
                                         bool hovered, bool primary) {
    COLORREF bg = primary
        ? (hovered ? kBtnPrimaryHover : kBtnPrimary)
        : (hovered ? kBtnSecondaryHover : kBtnSecondary);

    HBRUSH hbr = CreateSolidBrush(bg);
    FillRect(hdc, &rc, hbr);
    DeleteObject(hbr);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, bg);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kBtnText);
    SelectObject(hdc, m_hFontUI);
    DrawTextA(hdc, text, -1, const_cast<RECT*>(&rc), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ============================================================================
// Hit Testing
// ============================================================================

ExtensionPanelWindow::HitTestResult ExtensionPanelWindow::HitTest(int x, int y) const {
    HitTestResult result;
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS;
    GetScrollInfo(m_hwnd, SB_VERT, &si);
    int scrollY = si.nPos;

    int itemY = kMargin + 28;
    for (size_t i = 0; i < m_snapshot.size(); ++i) {
        RECT rcItem = {kMargin, itemY - scrollY, rcClient.right - kMargin, itemY + kItemHeight - scrollY};
        if (x >= rcItem.left && x < rcItem.right && y >= rcItem.top && y < rcItem.bottom) {
            result.itemIndex = static_cast<int>(i);

            // Check buttons (only if not downloading)
            if (m_snapshot[i].status != ExtensionUIStatus::Downloading) {
                int btnY = rcItem.top + 44; // approximate button Y
                int btnX = rcItem.right - 8;

                bool showRemove = true;
                bool showActivate = (m_snapshot[i].status == ExtensionUIStatus::Installed);
                bool showDeactivate = (m_snapshot[i].status == ExtensionUIStatus::Active);

                if (showDeactivate) {
                    btnX -= kButtonWidth + 4;
                    if (x >= btnX && x < btnX + kButtonWidth && y >= btnY && y < btnY + kButtonHeight)
                        result.button = HitTestResult::Deactivate;
                }
                if (showActivate && result.button == HitTestResult::None) {
                    btnX -= kButtonWidth + 4;
                    if (x >= btnX && x < btnX + kButtonWidth && y >= btnY && y < btnY + kButtonHeight)
                        result.button = HitTestResult::Activate;
                }
                if (showRemove && result.button == HitTestResult::None) {
                    btnX -= kButtonWidth + 4;
                    if (x >= btnX && x < btnX + kButtonWidth && y >= btnY && y < btnY + kButtonHeight)
                        result.button = HitTestResult::Remove;
                }
            }
            break;
        }
        itemY += kItemHeight + kGap;
    }
    return result;
}

// ============================================================================
// Input
// ============================================================================

void ExtensionPanelWindow::OnMouseMove(int x, int y) {
    auto hit = HitTest(x, y);
    bool changed = (hit.itemIndex != m_hoverItem || hit.button != m_hoverButton);
    m_hoverItem = hit.itemIndex;
    m_hoverButton = hit.button;
    if (changed) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
        // Set cursor
        if (hit.button != HitTestResult::None)
            SetCursor(LoadCursor(nullptr, IDC_HAND));
        else
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
}

void ExtensionPanelWindow::OnLButtonDown(int x, int y) {
    auto hit = HitTest(x, y);
    if (hit.itemIndex >= 0 && hit.itemIndex < static_cast<int>(m_snapshot.size())) {
        const std::string& id = m_snapshot[hit.itemIndex].id;
        switch (hit.button) {
            case HitTestResult::Activate:
                if (m_actionCb) m_actionCb(id, "activate");
                break;
            case HitTestResult::Deactivate:
                if (m_actionCb) m_actionCb(id, "deactivate");
                break;
            case HitTestResult::Remove:
                if (m_actionCb) m_actionCb(id, "remove");
                break;
            default:
                break;
        }
    }
}

// ============================================================================
// Data
// ============================================================================

void ExtensionPanelWindow::RebuildSnapshot() {
    m_snapshot = ExtensionUIState::Instance().snapshot();
    // Recompute scroll
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    OnSize(rcClient.right, rcClient.bottom);
}

} // namespace RawrXD
