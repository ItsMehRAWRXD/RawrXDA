// ============================================================================
// diff_dock.cpp — Pure Win32 Native Diff Preview Dock
// ============================================================================
// Side-by-side diff viewer with color-coded panes (red=original, green=suggested)
// Accept/Reject buttons. Dark VS Code theme.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "diff_dock.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR      = RGB(30, 30, 35);
static constexpr COLORREF BORDER_CLR    = RGB(60, 60, 65);
static constexpr COLORREF TEXT_CLR      = RGB(220, 220, 220);
static constexpr COLORREF ORIG_BG       = RGB(76, 31, 36);   // Red tint
static constexpr COLORREF ORIG_TEXT     = RGB(244, 135, 113);
static constexpr COLORREF SUGG_BG       = RGB(30, 70, 32);   // Green tint
static constexpr COLORREF SUGG_TEXT     = RGB(78, 201, 176);
static constexpr COLORREF ACCEPT_CLR    = RGB(22, 130, 93);
static constexpr COLORREF ACCEPT_HOVER  = RGB(26, 156, 111);
static constexpr COLORREF REJECT_CLR    = RGB(197, 50, 61);
static constexpr COLORREF REJECT_HOVER  = RGB(229, 62, 73);
static constexpr COLORREF LABEL_CLR     = RGB(180, 180, 180);

static const wchar_t* DIFF_CLASS = L"RawrXD_DiffDock";
bool DiffDock::s_classRegistered = false;

#define IDC_BTN_ACCEPT  5001
#define IDC_BTN_REJECT  5002

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK DiffDock::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DiffDock* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<DiffDock*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DiffDock*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_ACCEPT) self->onAccept();
        else if (LOWORD(wParam) == IDC_BTN_REJECT) self->onReject();
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        int btnH = 35;
        int btnW = 120;
        int gap = 10;
        if (self->m_btnReject)
            MoveWindow(self->m_btnReject, gap, h - btnH - 5, btnW, btnH, TRUE);
        if (self->m_btnAccept)
            MoveWindow(self->m_btnAccept, w - btnW - gap, h - btnH - 5, btnW, btnH, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != self->m_bufW || h != self->m_bufH) {
            if (self->m_backBuf) DeleteObject(self->m_backBuf);
            if (self->m_backDC) DeleteDC(self->m_backDC);
            self->m_backDC = CreateCompatibleDC(hdc);
            self->m_backBuf = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(self->m_backDC, self->m_backBuf);
            self->m_bufW = w;
            self->m_bufH = h;
        }
        self->paint(self->m_backDC, rc);
        BitBlt(hdc, 0, 0, w, h, self->m_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

DiffDock::DiffDock(HWND parent) {
    createWindow(parent);
    OutputDebugStringA("[DiffDock] Initialized\n");
}

DiffDock::~DiffDock() {
    if (m_originalText) { free(m_originalText); m_originalText = nullptr; }
    if (m_suggestedText) { free(m_suggestedText); m_suggestedText = nullptr; }
    if (m_backBuf) DeleteObject(m_backBuf);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_fontTitle) DeleteObject(m_fontTitle);
    if (m_fontMono) DeleteObject(m_fontMono);
    if (m_fontBtn) DeleteObject(m_fontBtn);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void DiffDock::createWindow(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInst) hInst = GetModuleHandle(nullptr);

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG_COLOR);
        wc.lpszClassName = DIFF_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    m_hwnd = CreateWindowExW(0, DIFF_CLASS, L"Refactor Preview",
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 600, 300, parent, nullptr, hInst, this);

    m_fontTitle = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    m_fontBtn = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // Reject button (left)
    m_btnReject = CreateWindowExW(0, L"BUTTON", L"\x2717 Reject",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 260, 120, 35, m_hwnd, (HMENU)IDC_BTN_REJECT, hInst, nullptr);
    SendMessageW(m_btnReject, WM_SETFONT, (WPARAM)m_fontBtn, TRUE);

    // Accept button (right)
    m_btnAccept = CreateWindowExW(0, L"BUTTON", L"\x2713 Accept",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        470, 260, 120, 35, m_hwnd, (HMENU)IDC_BTN_ACCEPT, hInst, nullptr);
    SendMessageW(m_btnAccept, WM_SETFONT, (WPARAM)m_fontBtn, TRUE);
}

// ============================================================================
// setDiff
// ============================================================================

void DiffDock::setDiff(const wchar_t* original, const wchar_t* suggested) {
    if (m_originalText) free(m_originalText);
    if (m_suggestedText) free(m_suggestedText);

    m_originalText = _wcsdup(original ? original : L"");
    m_suggestedText = _wcsdup(suggested ? suggested : L"");

    InvalidateRect(m_hwnd, nullptr, FALSE);
    ShowWindow(m_hwnd, SW_SHOW);

    char log[256];
    sprintf_s(log, "[DiffDock] Showing diff: original %zu chars, suggested %zu chars\n",
        wcslen(m_originalText), wcslen(m_suggestedText));
    OutputDebugStringA(log);
}

// ============================================================================
// Paint
// ============================================================================

void DiffDock::paint(HDC hdc, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HBRUSH bgBr = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBr);
    DeleteObject(bgBr);

    SetBkMode(hdc, TRANSPARENT);

    // Header labels
    int headerH = 22;
    int paneGap = 6;
    int paneW = (w - paneGap - 10) / 2;
    int btnAreaH = 45;
    int paneH = h - headerH - btnAreaH - 8;
    if (paneH < 20) paneH = 20;

    // Left header: "Original Code"
    SelectObject(hdc, m_fontTitle);
    SetTextColor(hdc, ORIG_TEXT);
    RECT leftHdr = { 5, 2, 5 + paneW, headerH };
    DrawTextW(hdc, L"Original Code", -1, &leftHdr, DT_LEFT | DT_SINGLELINE);

    // Right header: "Suggested Code"
    SetTextColor(hdc, SUGG_TEXT);
    RECT rightHdr = { 5 + paneW + paneGap, 2, w - 5, headerH };
    DrawTextW(hdc, L"Suggested Code", -1, &rightHdr, DT_LEFT | DT_SINGLELINE);

    // Left pane (original, red tint)
    paintTextPane(hdc, 5, headerH + 2, paneW, paneH,
        m_originalText ? m_originalText : L"", ORIG_BG, ORIG_TEXT, nullptr);

    // Right pane (suggested, green tint)
    paintTextPane(hdc, 5 + paneW + paneGap, headerH + 2, paneW, paneH,
        m_suggestedText ? m_suggestedText : L"", SUGG_BG, SUGG_TEXT, nullptr);

    // Divider line between panes
    HPEN divPen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    SelectObject(hdc, divPen);
    int divX = 5 + paneW + paneGap / 2;
    MoveToEx(hdc, divX, headerH, nullptr);
    LineTo(hdc, divX, headerH + paneH);
    DeleteObject(divPen);
}

void DiffDock::paintTextPane(HDC hdc, int x, int y, int w, int h,
    const wchar_t* text, COLORREF bgClr, COLORREF textClr, const wchar_t* label)
{
    // Background
    RECT paneRect = { x, y, x + w, y + h };
    HBRUSH bgBr = CreateSolidBrush(bgClr);
    FillRect(hdc, &paneRect, bgBr);
    DeleteObject(bgBr);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_CLR);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, x, y, x + w, y + h);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // Text content
    if (text && text[0]) {
        SelectObject(hdc, m_fontMono);
        SetTextColor(hdc, textClr);
        RECT textRect = { x + 6, y + 4, x + w - 6, y + h - 4 };
        DrawTextW(hdc, text, -1, &textRect, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS);
    }
}

// ============================================================================
// Accept / Reject
// ============================================================================

void DiffDock::onAccept() {
    OutputDebugStringA("[DiffDock] User accepted refactor\n");
    if (m_pfnAccepted && m_suggestedText) {
        m_pfnAccepted(m_suggestedText, m_cbUserdata);
    }
    ShowWindow(m_hwnd, SW_HIDE);
}

void DiffDock::onReject() {
    OutputDebugStringA("[DiffDock] User rejected refactor\n");
    if (m_pfnRejected) {
        m_pfnRejected(m_cbUserdata);
    }
    ShowWindow(m_hwnd, SW_HIDE);
}

// ============================================================================
// Public Methods
// ============================================================================

void DiffDock::setCallbacks(PFN_DIFF_ACCEPTED acceptFn, PFN_DIFF_REJECTED rejectFn, void* userdata) {
    m_pfnAccepted = acceptFn;
    m_pfnRejected = rejectFn;
    m_cbUserdata = userdata;
}

void DiffDock::show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
void DiffDock::hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

void DiffDock::resize(int x, int y, int w, int h) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, w, h, TRUE);
}

// ============================================================================
// C API
// ============================================================================

extern "C" {

DiffDock* DiffDock_Create(HWND parent) {
    if (!parent) return nullptr;
    return new DiffDock(parent);
}

void DiffDock_SetDiff(DiffDock* d, const wchar_t* original, const wchar_t* suggested) {
    if (d) d->setDiff(original, suggested);
}

void DiffDock_SetCallbacks(DiffDock* d, PFN_DIFF_ACCEPTED acceptFn, PFN_DIFF_REJECTED rejectFn, void* ud) {
    if (d) d->setCallbacks(acceptFn, rejectFn, ud);
}

void DiffDock_Show(DiffDock* d) { if (d) d->show(); }
void DiffDock_Hide(DiffDock* d) { if (d) d->hide(); }

void DiffDock_Resize(DiffDock* d, int x, int y, int w, int h) {
    if (d) d->resize(x, y, w, h);
}

void DiffDock_Destroy(DiffDock* d) {
    delete d;
}

}
