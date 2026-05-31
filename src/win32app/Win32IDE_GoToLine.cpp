// ============================================================================
// Win32IDE_GoToLine.cpp — Go To Line Dialog (Ctrl+G)
// ============================================================================
// VS Code-style "Go to Line" input popup. Presents a centered overlay with
// a numeric EDIT control.  The user types a line number, presses Enter →
// editor jumps to that line via gotoLine().  Esc or clicking outside dismisses.
//
// Architecture:
//   - WS_POPUP overlay window with EDIT + "Go to Line:" label
//   - Input filtered to digits only (ES_NUMBER)
//   - Enter commits, Esc cancels
//   - Positions at the top-center of the main window (like VS Code)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include <cstdlib>
#include <string>

// ── File-static state ───────────────────────────────────────────────────────
static struct GoToLineState {
    HWND hwndOverlay  = nullptr;
    HWND hwndEdit     = nullptr;
    HWND hwndLabel    = nullptr;
    WNDPROC oldEditProc = nullptr;
    Win32IDE* pIDE    = nullptr;
    bool active       = false;
} s_gtl;

#define IDC_GTL_EDIT  9901
#define IDC_GTL_LABEL 9902

// ── Forward declarations ────────────────────────────────────────────────────
static LRESULT CALLBACK GoToLineOverlayProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK GoToLineEditProc(HWND, UINT, WPARAM, LPARAM);

// ============================================================================
// GoToLine — commit: jump to the line in the EDIT field
// ============================================================================
static void commitGoToLine()
{
    if (!s_gtl.hwndEdit || !s_gtl.pIDE) return;

    wchar_t buf[32] = {};
    GetWindowTextW(s_gtl.hwndEdit, buf, _countof(buf));
    int line = _wtoi(buf);
    if (line < 1) line = 1;

    // Jump — use navigateToLine (public member) to avoid private access
    s_gtl.pIDE->navigateToLine(line);

    // Dismiss
    s_gtl.pIDE->hideGoToLineDialog();
}

// ============================================================================
// showGoToLineDialog — create (once) and show the overlay
// ============================================================================
void Win32IDE::showGoToLineDialog()
{
    if (s_gtl.active) {
        // Already open → just refocus
        SetFocus(s_gtl.hwndEdit);
        SendMessage(s_gtl.hwndEdit, EM_SETSEL, 0, -1);
        return;
    }

    s_gtl.pIDE = this;

    // ── Register class once ─────────────────────────────────────────────────
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = GoToLineOverlayProc;
        wc.hInstance      = m_hInstance;
        wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground  = CreateSolidBrush(RGB(37, 37, 38));
        wc.lpszClassName  = L"RawrXD_GoToLine";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    // ── Calculate position: top-center of main window ───────────────────────
    RECT rcMain = {};
    GetWindowRect(m_hwndMain, &rcMain);
    const int popW = 380;
    const int popH = 42;
    const int popX = rcMain.left + (rcMain.right - rcMain.left - popW) / 2;
    const int popY = rcMain.top + 50;  // offset from top like VS Code

    s_gtl.hwndOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"RawrXD_GoToLine", L"",
        WS_POPUP | WS_BORDER,
        popX, popY, popW, popH,
        m_hwndMain, nullptr, m_hInstance, nullptr);

    // ── Label ───────────────────────────────────────────────────────────────
    s_gtl.hwndLabel = CreateWindowExW(
        0, L"STATIC", L"Go to Line:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, 10, 80, 20,
        s_gtl.hwndOverlay, (HMENU)(UINT_PTR)IDC_GTL_LABEL,
        m_hInstance, nullptr);

    // ── EDIT ────────────────────────────────────────────────────────────────
    s_gtl.hwndEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL | WS_BORDER,
        92, 8, popW - 100, 24,
        s_gtl.hwndOverlay, (HMENU)(UINT_PTR)IDC_GTL_EDIT,
        m_hInstance, nullptr);

    // Subclass edit for Enter/Esc handling
    s_gtl.oldEditProc = (WNDPROC)SetWindowLongPtrW(s_gtl.hwndEdit, GWLP_WNDPROC,
                                                     (LONG_PTR)GoToLineEditProc);

    // ── Style the controls ──────────────────────────────────────────────────
    // Dark theme font
    HFONT hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessage(s_gtl.hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(s_gtl.hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Pre-fill with current line number
    int curLine = m_currentLine;
    if (curLine < 1) curLine = 1;
    wchar_t initText[32];
    _snwprintf_s(initText, _countof(initText), _TRUNCATE, L"%d", curLine);
    SetWindowTextW(s_gtl.hwndEdit, initText);

    ShowWindow(s_gtl.hwndOverlay, SW_SHOWNA);
    SetForegroundWindow(s_gtl.hwndOverlay);
    SetFocus(s_gtl.hwndEdit);
    SendMessage(s_gtl.hwndEdit, EM_SETSEL, 0, -1);

    s_gtl.active = true;
}

// ============================================================================
// hideGoToLineDialog — dismiss the overlay
// ============================================================================
void Win32IDE::hideGoToLineDialog()
{
    if (!s_gtl.active) return;

    if (s_gtl.hwndOverlay) {
        DestroyWindow(s_gtl.hwndOverlay);
        s_gtl.hwndOverlay = nullptr;
        s_gtl.hwndEdit    = nullptr;
        s_gtl.hwndLabel   = nullptr;
    }
    s_gtl.active = false;

    // Return focus to editor
    if (m_hwndEditor && IsWindow(m_hwndEditor))
        SetFocus(m_hwndEditor);
}

bool Win32IDE::isGoToLineDialogVisible() const
{
    return s_gtl.active;
}

// ============================================================================
// navigateToLine — public entry point wrapping private gotoLine()
// ============================================================================
void Win32IDE::navigateToLine(int line)
{
    gotoLine(line);
}

// ============================================================================
// GoToLine overlay WndProc — handle paint/close/activate
// ============================================================================
static LRESULT CALLBACK GoToLineOverlayProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(204, 204, 204));
        SetBkColor(hdc, RGB(37, 37, 38));
        static HBRUSH s_brush = CreateSolidBrush(RGB(37, 37, 38));
        return (LRESULT)s_brush;
    }

    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) {
            // Clicked outside → dismiss
            if (s_gtl.pIDE)
                s_gtl.pIDE->hideGoToLineDialog();
        }
        return 0;

    case WM_CLOSE:
        if (s_gtl.pIDE)
            s_gtl.pIDE->hideGoToLineDialog();
        return 0;

    case WM_DESTROY:
        s_gtl.hwndOverlay = nullptr;
        s_gtl.active = false;
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// GoToLine EDIT subclass — intercept Enter/Esc
// ============================================================================
static LRESULT CALLBACK GoToLineEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            commitGoToLine();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            if (s_gtl.pIDE)
                s_gtl.pIDE->hideGoToLineDialog();
            return 0;
        }
        break;

    case WM_CHAR:
        // Suppress the beep for Enter
        if (wParam == '\r' || wParam == '\n')
            return 0;
        break;
    }

    return CallWindowProcW(s_gtl.oldEditProc, hwnd, msg, wParam, lParam);
}
