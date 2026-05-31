// ============================================================================
// Win32IDE_HoverTooltips.cpp — Feature 13: Hover Documentation Tooltips
// Inline hover popup with Markdown-rendered LSP hover info near cursor
// ============================================================================
#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <sstream>
#include <thread>

// ── Colors ─────────────────────────────────────────────────────────────────
static const COLORREF HOVER_BG             = RGB(37, 37, 38);
static const COLORREF HOVER_BORDER         = RGB(69, 69, 69);
static const COLORREF HOVER_TEXT           = RGB(204, 204, 204);
static const COLORREF HOVER_CODE_BG        = RGB(30, 30, 30);
static const COLORREF HOVER_KEYWORD        = RGB(86, 156, 214);
static const COLORREF HOVER_TYPE           = RGB(78, 201, 176);
static const COLORREF HOVER_LINK           = RGB(55, 148, 255);
static const COLORREF HOVER_SEPARATOR      = RGB(60, 60, 60);
static const COLORREF HOVER_PARAM          = RGB(220, 220, 170);

static const int HOVER_MAX_WIDTH  = 520;
static const int HOVER_MAX_HEIGHT = 320;
static const int HOVER_PADDING    = 10;

// ── Registered window class for hover popup ────────────────────────────────
static bool s_hoverClassRegistered = false;
static const char* HOVER_CLASS_NAME = "RawrXD_HoverPopup";

static void ensureHoverClassRegistered(HINSTANCE hInst) {
    if (s_hoverClassRegistered) return;
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(HOVER_BG);
    wc.lpszClassName = HOVER_CLASS_NAME;
    RegisterClassExA(&wc);
    s_hoverClassRegistered = true;
}

// ── Init / Shutdown ────────────────────────────────────────────────────────
void Win32IDE::initHoverTooltips() {
    m_hoverState = HoverTooltipState{};
    m_hoverState.hFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    m_hoverState.hBoldFont = CreateFontA(-dpiScale(13), 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    ensureHoverClassRegistered(GetModuleHandle(NULL));
    LOG_INFO("[HoverTooltips] Initialized");
}

void Win32IDE::shutdownHoverTooltips() {
    dismissHoverTooltip();
    if (m_hoverState.hFont) { DeleteObject(m_hoverState.hFont); m_hoverState.hFont = nullptr; }
    if (m_hoverState.hBoldFont) { DeleteObject(m_hoverState.hBoldFont); m_hoverState.hBoldFont = nullptr; }
    LOG_INFO("[HoverTooltips] Shutdown");
}

// ── Mouse Hover Trigger ────────────────────────────────────────────────────
void Win32IDE::onEditorMouseHover(int x, int y) {
    // Convert pixel to editor line/column
    if (!m_hwndEditor) return;

    // Get character position from mouse coords
    POINTL pt = {x, y};
    int charIndex = (int)SendMessageA(m_hwndEditor, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    if (charIndex < 0) return;

    int line = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, charIndex, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);
    int col = charIndex - lineStart;

    // Don't re-trigger if same position
    if (m_hoverState.pending && m_hoverState.line == line && m_hoverState.column == col)
        return;

    // Kill existing timer, start debounce
    KillTimer(m_hwndMain, HOVER_TIMER_ID);
    m_hoverState.line = line;
    m_hoverState.column = col;
    m_hoverState.pending = true;

    // Store screen position for popup placement
    POINT screenPt = {x, y};
    ClientToScreen(m_hwndEditor, &screenPt);
    m_hoverState.screenPos = screenPt;

    SetTimer(m_hwndMain, HOVER_TIMER_ID, HOVER_TIMER_DELAY, NULL);
}

// ── Timer fires → request hover from LSP ───────────────────────────────────
void Win32IDE::onHoverTimer() {
    KillTimer(m_hwndMain, HOVER_TIMER_ID);
    if (!m_hoverState.pending) return;

    int line = m_hoverState.line;
    int col = m_hoverState.column;

    // Background thread: request LSP hover
    std::thread([this, line, col]() {
        std::string uri = filePathToUri(m_currentFile);
        LSPHoverInfo hover = lspHover(uri, line, col);

        if (!hover.contents.empty()) {
            PostMessageA(m_hwndMain, WM_HOVER_READY, 0, 0);
            // Store content for UI thread
            std::lock_guard<std::mutex> lock(m_lspMutex);
            m_hoverState.content = hover.contents;
        } else {
            m_hoverState.pending = false;
        }
    }).detach();
}

// ── Hover result ready ─────────────────────────────────────────────────────
void Win32IDE::onHoverReady(const std::string& content, int line, int col) {
    if (content.empty()) {
        m_hoverState.pending = false;
        return;
    }
    m_hoverState.content = content;
    showHoverTooltip(m_hoverState.screenPos.x, m_hoverState.screenPos.y + 20, content);
}

// ── Show / Dismiss ─────────────────────────────────────────────────────────
void Win32IDE::showHoverTooltip(int screenX, int screenY, const std::string& content) {
    dismissHoverTooltip();

    if (content.empty()) return;

    // Calculate required size
    HDC hScreenDC = GetDC(NULL);
    HFONT oldFont = (HFONT)SelectObject(hScreenDC, m_hoverState.hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hScreenDC, &tm);
    int lineH = tm.tmHeight + 2;
    int charW = tm.tmAveCharWidth;

    // Count lines in content
    int numLines = 1;
    int maxLineLen = 0;
    int curLineLen = 0;
    for (char c : content) {
        if (c == '\n') { numLines++; maxLineLen = (std::max)(maxLineLen, curLineLen); curLineLen = 0; }
        else curLineLen++;
    }
    maxLineLen = (std::max)(maxLineLen, curLineLen);

    int popupW = (std::min)(maxLineLen * charW + HOVER_PADDING * 2 + 8, HOVER_MAX_WIDTH);
    int popupH = (std::min)(numLines * lineH + HOVER_PADDING * 2 + 4, HOVER_MAX_HEIGHT);

    // Clamp to reasonable minimum
    popupW = (std::max)(popupW, 120);
    popupH = (std::max)(popupH, lineH + HOVER_PADDING * 2);

    SelectObject(hScreenDC, oldFont);
    ReleaseDC(NULL, hScreenDC);

    // Ensure popup stays on screen
    HMONITOR hMon = MonitorFromPoint({screenX, screenY}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoA(hMon, &mi);
    if (screenX + popupW > mi.rcWork.right) screenX = mi.rcWork.right - popupW;
    if (screenY + popupH > mi.rcWork.bottom) screenY = screenY - popupH - 25; // above cursor

    // Create popup window
    m_hoverState.hwndPopup = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        HOVER_CLASS_NAME, "",
        WS_POPUP | WS_BORDER,
        screenX, screenY, popupW, popupH,
        m_hwndMain, NULL, GetModuleHandle(NULL), NULL);

    SetPropA(m_hoverState.hwndPopup, "IDE_PTR", (HANDLE)this);
    SetWindowLongPtrA(m_hoverState.hwndPopup, GWLP_WNDPROC, (LONG_PTR)HoverTooltipProc);

    ShowWindow(m_hoverState.hwndPopup, SW_SHOWNOACTIVATE);
    m_hoverState.visible = true;
    m_hoverState.pending = false;
}

void Win32IDE::dismissHoverTooltip() {
    if (m_hoverState.hwndPopup) {
        DestroyWindow(m_hoverState.hwndPopup);
        m_hoverState.hwndPopup = nullptr;
    }
    m_hoverState.visible = false;
    m_hoverState.content.clear();
    m_hoverState.pending = false;
}

// ── Render Content (simplified Markdown) ───────────────────────────────────
void Win32IDE::renderHoverContent(HDC hdc, RECT rc, const std::string& content) {
    int pad = HOVER_PADDING;
    RECT textRect = {rc.left + pad, rc.top + pad, rc.right - pad, rc.bottom - pad};

    // Background
    HBRUSH bgBrush = CreateSolidBrush(HOVER_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hoverState.hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineH = tm.tmHeight + 2;

    int y = textRect.top;
    bool inCodeBlock = false;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line) && y < textRect.bottom) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Detect code blocks (```)
        if (line.find("```") == 0) {
            inCodeBlock = !inCodeBlock;
            if (inCodeBlock) {
                // Draw code block background start
                HBRUSH codeBg = CreateSolidBrush(HOVER_CODE_BG);
                RECT codeRect = {textRect.left - 4, y, textRect.right + 4, y + lineH};
                FillRect(hdc, &codeRect, codeBg);
                DeleteObject(codeBg);
            }
            continue;
        }

        // Code block rendering
        if (inCodeBlock) {
            HBRUSH codeBg = CreateSolidBrush(HOVER_CODE_BG);
            RECT codeRect = {textRect.left - 4, y, textRect.right + 4, y + lineH};
            FillRect(hdc, &codeRect, codeBg);
            DeleteObject(codeBg);
            SetTextColor(hdc, HOVER_TYPE);
            TextOutA(hdc, textRect.left + 4, y, line.c_str(), (int)line.size());
            y += lineH;
            continue;
        }

        // Separator (---)
        if (line == "---" || line == "***") {
            HPEN sepPen = CreatePen(PS_SOLID, 1, HOVER_SEPARATOR);
            HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
            MoveToEx(hdc, textRect.left, y + lineH / 2, NULL);
            LineTo(hdc, textRect.right, y + lineH / 2);
            SelectObject(hdc, oldPen);
            DeleteObject(sepPen);
            y += lineH;
            continue;
        }

        // Bold (**text**)
        bool isBold = false;
        std::string displayLine = line;
        if (line.size() > 4 && line.substr(0, 2) == "**" &&
            line.substr(line.size() - 2) == "**") {
            displayLine = line.substr(2, line.size() - 4);
            isBold = true;
        }

        // Inline code (`code`)
        bool isInlineCode = false;
        if (displayLine.size() > 2 && displayLine.front() == '`' && displayLine.back() == '`') {
            displayLine = displayLine.substr(1, displayLine.size() - 2);
            isInlineCode = true;
        }

        HFONT renderFont = isBold ? m_hoverState.hBoldFont : m_hoverState.hFont;
        SelectObject(hdc, renderFont);

        if (isInlineCode) {
            SetTextColor(hdc, HOVER_TYPE);
        } else {
            SetTextColor(hdc, HOVER_TEXT);
        }

        TextOutA(hdc, textRect.left, y, displayLine.c_str(), (int)displayLine.size());
        y += lineH;
    }

    SelectObject(hdc, oldFont);
}

// ── Window Proc ────────────────────────────────────────────────────────────
LRESULT CALLBACK Win32IDE::HoverTooltipProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ide) {
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Border
            HBRUSH borderBrush = CreateSolidBrush(HOVER_BORDER);
            FrameRect(hdc, &rc, borderBrush);
            DeleteObject(borderBrush);

            RECT inner = {rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1};
            ide->renderHoverContent(hdc, inner, ide->m_hoverState.content);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        // Keep visible while mouse is over popup
        return 0;
    case WM_MOUSELEAVE:
        if (ide) ide->dismissHoverTooltip();
        return 0;
    case WM_LBUTTONDOWN:
        if (ide) ide->dismissHoverTooltip();
        return 0;
    case WM_KILLFOCUS:
        if (ide) ide->dismissHoverTooltip();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// DEBUGGER HOVER VALUE INTEGRATION (Phase 1C)
// Shows variable values and expressions during debugging sessions
// ============================================================================

// Hover value state (separate from LSP hover)
static struct {
    UINT_PTR timerID = 0;
    int lastLine = -1;
    int lastColumn = -1;
    std::string lastIdentifier;
    bool showingDebugValue = false;
} s_debugHoverState;

const UINT DEBUG_HOVER_TIMER_ID = 0x2002;
const int DEBUG_HOVER_DEBOUNCE_MS = 400;

// ── Extract identifier at column position ──────────────────────────────────
std::string Win32IDE::extractIdentifierAtColumn(const std::string& line, int column) {
    if (column < 0 || column >= (int)line.length())
        return "";

    // Find start of identifier (move left while alphanumeric or _)
    int start = column;
    while (start > 0 && (std::isalnum((unsigned char)line[start - 1]) || line[start - 1] == '_'))
        start--;

    // Find end of identifier (move right while alphanumeric or _)
    int end = column;
    while (end < (int)line.length() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
        end++;

    // If cursor is not on an identifier, try left neighbor
    if (start == end && column > 0) {
        start = column - 1;
        while (start > 0 && (std::isalnum((unsigned char)line[start - 1]) || line[start - 1] == '_'))
            start--;
        if (std::isalnum((unsigned char)line[start]) || line[start] == '_') {
            while (end < (int)line.length() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
                end++;
        } else {
            return "";
        }
    }

    if (start == end)
        return "";

    return line.substr(start, end - start);
}

// ── RichEdit mouse move handler (debugger overlay) ────────────────────────
void Win32IDE::onEditorMouseMoveDebugHover(int x, int y) {
    // Only show hover during active debugging
    if (!m_debuggingActive || !m_debuggerPaused) {
        s_debugHoverState.showingDebugValue = false;
        if (s_debugHoverState.timerID != 0) {
            KillTimer(m_hwndMain, s_debugHoverState.timerID);
            s_debugHoverState.timerID = 0;
        }
        return;
    }

    if (!m_hwndEditor)
        return;

    // Convert pixel position to character position
    POINTL pt = {x, y};
    int charIndex = (int)SendMessageA(m_hwndEditor, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    if (charIndex < 0)
        return;

    int line = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, charIndex, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);
    int column = charIndex - lineStart;

    // Skip if same position (avoid re-triggering)
    if (s_debugHoverState.lastLine == line && s_debugHoverState.lastColumn == column)
        return;

    s_debugHoverState.lastLine = line;
    s_debugHoverState.lastColumn = column;

    // Kill existing timer and start debounce
    if (s_debugHoverState.timerID != 0)
        KillTimer(m_hwndMain, s_debugHoverState.timerID);

    s_debugHoverState.timerID = SetTimer(m_hwndMain, DEBUG_HOVER_TIMER_ID,
                                          DEBUG_HOVER_DEBOUNCE_MS, nullptr);
}

// ── Timer tick: trigger debug hover evaluation ─────────────────────────────
void Win32IDE::onDebugHoverTimer() {
    KillTimer(m_hwndMain, DEBUG_HOVER_TIMER_ID);
    s_debugHoverState.timerID = 0;

    if (!m_debuggingActive || !m_debuggerPaused)
        return;

    int line = s_debugHoverState.lastLine;
    int column = s_debugHoverState.lastColumn;

    if (line < 0 || column < 0)
        return;

    // Get line text directly from the editor control.
    std::string lineText;
    if (!m_hwndEditor)
    {
        return;
    }

    int lineIndex = static_cast<int>(SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0));
    if (lineIndex < 0)
    {
        return;
    }
    int lineLen = static_cast<int>(SendMessageA(m_hwndEditor, EM_LINELENGTH, lineIndex, 0));
    if (lineLen <= 0)
    {
        return;
    }

    std::vector<char> lineBuffer(static_cast<size_t>(lineLen) + 2, '\0');
    *reinterpret_cast<WORD*>(lineBuffer.data()) = static_cast<WORD>(lineLen + 1);
    LRESULT copied = SendMessageA(m_hwndEditor, EM_GETLINE, line, reinterpret_cast<LPARAM>(lineBuffer.data()));
    if (copied <= 0)
    {
        return;
    }
    lineText.assign(lineBuffer.data(), static_cast<size_t>(copied));

    // Extract identifier
    std::string identifier = extractIdentifierAtColumn(lineText, column);
    if (identifier.empty()) {
        s_debugHoverState.showingDebugValue = false;
        dismissHoverTooltip();
        return;
    }

    s_debugHoverState.lastIdentifier = identifier;

    // Evaluate the identifier in current frame context
    std::string value, type;
    int frameId = m_selectedStackFrameIndex >= 0 ? m_selectedStackFrameIndex : 0;

    if (!evaluateWatchExpression(identifier, frameId, value, type)) {
        s_debugHoverState.showingDebugValue = false;
        dismissHoverTooltip();
        return;
    }

    // Build tooltip content
    std::string tooltipContent = identifier + " : " + type + "\n└─ " + value;

    // Truncate long values
    if (tooltipContent.length() > 200)
        tooltipContent = tooltipContent.substr(0, 197) + "...";

    s_debugHoverState.showingDebugValue = true;

    // Get mouse position for tooltip placement
    POINT screenPt;
    GetCursorPos(&screenPt);

    // Show debug value tooltip
    showHoverTooltip(screenPt.x + 10, screenPt.y + 15, tooltipContent);
}

// ── Hide debug hover value ─────────────────────────────────────────────────
void Win32IDE::hideDebugHoverValue() {
    if (s_debugHoverState.timerID != 0) {
        KillTimer(m_hwndMain, s_debugHoverState.timerID);
        s_debugHoverState.timerID = 0;
    }
    s_debugHoverState.showingDebugValue = false;
    s_debugHoverState.lastIdentifier.clear();
}
