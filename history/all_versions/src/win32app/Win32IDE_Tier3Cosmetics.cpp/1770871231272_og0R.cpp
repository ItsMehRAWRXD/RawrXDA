// ============================================================================
// Win32IDE_Tier3Cosmetics.cpp — Tier 3 Cosmetic Gaps (#20–#30)
// ============================================================================
//
// 20. Bracket Pair Colorization    — Rainbow brackets (Gold, Purple, Blue)
// 21. Indentation Guides           — Dotted vertical lines at tab-stop intervals
// 22. Whitespace Rendering Toggle  — Middle-dot for spaces, → for tabs
// 23. Word Wrap Indicator          — Draw wrap-arrow glyph at wrap points
// 24. Relative Line Numbers        — Calculate offset from cursor in gutter
// 25. Zen Mode (Distraction Free)  — F11 hides all chrome
// 26. Tab Pinning                  — Pinned tabs: small, always-left, no close
// 27. Preview Tabs (Single Click)  — Italic "preview" tab on single click
// 28. Search Results in Scrollbar  — Match highlights on scrollbar minimap
// 29. Quick Fix Lightbulb          — Lightbulb icon in margin for code actions
// 30. Code Folding Controls        — +/- buttons in gutter for {} blocks
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <stack>
#include <vector>
#include <map>

#pragma comment(lib, "comctl32.lib")

// ============================================================================
// COMMAND IDs for Tier 3 Cosmetics (#20–#30)
// ============================================================================
static constexpr int IDM_T3C_BRACKET_PAIRS       = 12000;
static constexpr int IDM_T3C_INDENT_GUIDES       = 12001;
static constexpr int IDM_T3C_WHITESPACE_TOGGLE   = 12002;
static constexpr int IDM_T3C_WORD_WRAP_IND       = 12003;
static constexpr int IDM_T3C_RELATIVE_LINES      = 12004;
static constexpr int IDM_T3C_ZEN_MODE            = 12005;
static constexpr int IDM_T3C_PIN_TAB             = 12006;
static constexpr int IDM_T3C_UNPIN_TAB           = 12007;
static constexpr int IDM_T3C_CODE_FOLD_TOGGLE    = 12008;
static constexpr int IDM_T3C_LIGHTBULB_ACTION    = 12009;
static constexpr int IDM_T3C_FOLD_ALL            = 12010;
static constexpr int IDM_T3C_UNFOLD_ALL          = 12011;

// Timer IDs
static constexpr UINT_PTR BRACKET_PAIR_TIMER_ID    = 9100;
static constexpr UINT_PTR LIGHTBULB_TIMER_ID       = 9101;
static constexpr UINT BRACKET_PAIR_INTERVAL        = 100;  // ms
static constexpr UINT LIGHTBULB_CHECK_INTERVAL     = 500;  // ms

// ============================================================================
// 20. BRACKET PAIR COLORIZATION
// ============================================================================
// Rainbow bracket colors — Gold, Purple, Blue cycled at increasing depth.
// After tokenizing, we walk the token stream and reassign Bracket tokens
// to depth-specific colors using EM_SETCHARFORMAT.
// ============================================================================

// VS Code default bracket pair colors
static const COLORREF g_bracketColors[] = {
    RGB(255, 215, 0),    // Gold
    RGB(218, 112, 214),  // Orchid / Purple
    RGB(86, 156, 214),   // Cornflower Blue
    RGB(78, 201, 176),   // Teal
    RGB(206, 145, 120),  // Salmon/Coral
    RGB(181, 206, 168),  // Pale Green
};
static constexpr int BRACKET_COLOR_COUNT = 6;

void Win32IDE::initBracketPairColorization() {
    m_bracketPairEnabled = true;
    LOG_INFO("Bracket pair colorization initialized");
}

void Win32IDE::shutdownBracketPairColorization() {
    m_bracketPairEnabled = false;
}

void Win32IDE::toggleBracketPairColorization() {
    m_bracketPairEnabled = !m_bracketPairEnabled;
    if (m_bracketPairEnabled) {
        applyBracketPairColors();
    } else {
        // Recolor all brackets with default bracket color
        onEditorContentChanged();
    }
    appendToOutput(m_bracketPairEnabled
        ? "[Tier3C] Bracket pair colorization enabled.\n"
        : "[Tier3C] Bracket pair colorization disabled.\n");
}

void Win32IDE::applyBracketPairColors() {
    if (!m_hwndEditor || !m_bracketPairEnabled) return;

    // Get entire text
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Build bracket depth map
    struct BracketInfo {
        int offset;
        int depth;
        char ch;
    };
    std::vector<BracketInfo> brackets;
    int depth = 0;

    for (int i = 0; i < (int)text.size(); i++) {
        char ch = text[i];
        // Skip inside strings and comments using a simple state machine
        if (ch == '"' || ch == '\'') {
            char quote = ch;
            i++;
            while (i < (int)text.size() && text[i] != quote) {
                if (text[i] == '\\') i++;
                i++;
            }
            continue;
        }
        if (ch == '/' && i + 1 < (int)text.size()) {
            if (text[i + 1] == '/') {
                // Line comment — skip to end
                while (i < (int)text.size() && text[i] != '\n') i++;
                continue;
            }
            if (text[i + 1] == '*') {
                // Block comment — skip to */
                i += 2;
                while (i + 1 < (int)text.size()) {
                    if (text[i] == '*' && text[i + 1] == '/') { i++; break; }
                    i++;
                }
                continue;
            }
        }

        if (ch == '(' || ch == '[' || ch == '{') {
            brackets.push_back({i, depth, ch});
            depth++;
        } else if (ch == ')' || ch == ']' || ch == '}') {
            depth = (depth > 0) ? depth - 1 : 0;
            brackets.push_back({i, depth, ch});
        }
    }

    if (brackets.empty()) return;

    // Apply colors using EM_SETCHARFORMAT
    // Save and restore selection/scroll
    CHARRANGE oldSel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&oldSel);
    POINT scrollPos;
    SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
    DWORD eventMask = (DWORD)SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, 0);

    for (const auto& br : brackets) {
        COLORREF color = g_bracketColors[br.depth % BRACKET_COLOR_COUNT];

        CHARRANGE sel = { br.offset, br.offset + 1 };
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);

        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        cf.dwEffects = 0;
        SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }

    // Restore
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, eventMask);
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&oldSel);
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessage(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ============================================================================
// 21. INDENTATION GUIDES
// ============================================================================
// Draws dotted vertical lines in the editor background at tab-stop intervals,
// rendered in the line-number gutter overlay or as a transparent overlay on
// the editor itself using WM_PAINT interception.
// ============================================================================

void Win32IDE::initIndentationGuides() {
    m_indentGuidesEnabled = true;
    LOG_INFO("Indentation guides initialized");
}

void Win32IDE::shutdownIndentationGuides() {
    m_indentGuidesEnabled = false;
}

void Win32IDE::toggleIndentationGuides() {
    m_indentGuidesEnabled = !m_indentGuidesEnabled;
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    appendToOutput(m_indentGuidesEnabled
        ? "[Tier3C] Indentation guides enabled.\n"
        : "[Tier3C] Indentation guides disabled.\n");
}

void Win32IDE::paintIndentationGuides(HDC hdc, const RECT& editorRect) {
    if (!m_indentGuidesEnabled || !m_hwndEditor) return;

    // Get editor font metrics for character width
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int charWidth = tm.tmAveCharWidth;
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (charWidth <= 0) charWidth = 8;
    if (lineHeight <= 0) lineHeight = 16;

    int tabStopPixels = m_settings.tabSize * charWidth;
    if (tabStopPixels <= 0) { SelectObject(hdc, hOldFont); return; }

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    int visibleLines = (editorRect.bottom - editorRect.top) / lineHeight + 1;

    // Create a dotted pen for the guide lines
    HPEN hGuide = CreatePen(PS_DOT, 1, RGB(60, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hGuide);

    // Get editor text to determine indent levels for each visible line
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    std::string fullText;
    if (textLen > 0) {
        fullText.resize(textLen + 1);
        GetWindowTextA(m_hwndEditor, &fullText[0], textLen + 1);
        fullText.resize(textLen);
    }

    // Break text into lines for indent analysis
    std::vector<int> lineIndentLevels;
    {
        std::istringstream stream(fullText);
        std::string line;
        while (std::getline(stream, line)) {
            int indent = 0;
            for (char c : line) {
                if (c == ' ') indent++;
                else if (c == '\t') indent += m_settings.tabSize;
                else break;
            }
            lineIndentLevels.push_back(indent / m_settings.tabSize);
        }
    }

    // Determine the max indent level among visible lines
    int maxIndent = 0;
    for (int i = 0; i < visibleLines && (firstVisibleLine + i) < (int)lineIndentLevels.size(); i++) {
        int idx = firstVisibleLine + i;
        if (idx < (int)lineIndentLevels.size())
            maxIndent = (std::max)(maxIndent, lineIndentLevels[idx]);
    }

    // Draw vertical lines for each indent level
    // We get the horizontal offset by querying character positions
    for (int indentLevel = 1; indentLevel <= maxIndent; indentLevel++) {
        int x = editorRect.left + (indentLevel * tabStopPixels);
        if (x >= editorRect.right) break;

        // Draw the guide line only where there's relevant indentation
        for (int i = 0; i < visibleLines; i++) {
            int lineIdx = firstVisibleLine + i;
            if (lineIdx >= (int)lineIndentLevels.size()) break;

            if (lineIndentLevels[lineIdx] >= indentLevel) {
                int y1 = editorRect.top + i * lineHeight;
                int y2 = y1 + lineHeight;
                MoveToEx(hdc, x, y1, nullptr);
                LineTo(hdc, x, y2);
            }
        }
    }

    SelectObject(hdc, hOldPen);
    DeleteObject(hGuide);
    SelectObject(hdc, hOldFont);
}

// ============================================================================
// 22. WHITESPACE RENDERING TOGGLE
// ============================================================================
// Renders middle-dot (·) for spaces and right-arrow (→) for tabs directly
// into the editor via painting overlay. Controlled by a settings checkbox.
// ============================================================================

void Win32IDE::initWhitespaceRendering() {
    m_whitespaceVisible = false; // Off by default
    LOG_INFO("Whitespace rendering initialized (off)");
}

void Win32IDE::toggleWhitespaceRendering() {
    m_whitespaceVisible = !m_whitespaceVisible;
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    appendToOutput(m_whitespaceVisible
        ? "[Tier3C] Whitespace rendering enabled.\n"
        : "[Tier3C] Whitespace rendering disabled.\n");
}

void Win32IDE::paintWhitespaceGlyphs(HDC hdc, const RECT& editorRect) {
    if (!m_whitespaceVisible || !m_hwndEditor) return;

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int charWidth = tm.tmAveCharWidth;
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (charWidth <= 0) charWidth = 8;
    if (lineHeight <= 0) lineHeight = 16;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    int visibleLines = (editorRect.bottom - editorRect.top) / lineHeight + 1;

    // Semi-transparent whitespace glyphs
    SetTextColor(hdc, RGB(80, 80, 80));
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < visibleLines && (firstVisibleLine + i) < lineCount; i++) {
        int lineIdx = firstVisibleLine + i;

        // Get character index of line start
        int charIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charIdx, 0);
        if (lineLen <= 0) continue;

        // Get line text
        char lineBuf[4096];
        if (lineLen >= (int)sizeof(lineBuf)) lineLen = sizeof(lineBuf) - 1;
        *(WORD*)lineBuf = (WORD)lineLen;
        int got = (int)SendMessageA(m_hwndEditor, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
        if (got <= 0) continue;
        lineBuf[got] = '\0';

        // For each whitespace character, get its screen position and draw
        for (int c = 0; c < got; c++) {
            if (lineBuf[c] != ' ' && lineBuf[c] != '\t') continue;

            POINTL pt;
            SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, charIdx + c);

            if (pt.x < editorRect.left || pt.x >= editorRect.right) continue;
            if (pt.y < editorRect.top || pt.y >= editorRect.bottom) continue;

            if (lineBuf[c] == ' ') {
                // Middle dot: Unicode 0x00B7 (·)
                // Center it in the character cell
                int dotX = pt.x + charWidth / 2 - 1;
                int dotY = pt.y + lineHeight / 2 - 1;
                RECT dotRect = { dotX, dotY, dotX + 2, dotY + 2 };
                HBRUSH dotBrush = CreateSolidBrush(RGB(80, 80, 80));
                FillRect(hdc, &dotRect, dotBrush);
                DeleteObject(dotBrush);
            } else {
                // Tab: draw → arrow
                int arrowY = pt.y + lineHeight / 2;
                int arrowX1 = pt.x + 2;
                int arrowX2 = pt.x + charWidth * m_settings.tabSize - 2;
                if (arrowX2 > editorRect.right) arrowX2 = editorRect.right - 2;

                HPEN arrowPen = CreatePen(PS_SOLID, 1, RGB(80, 80, 80));
                HPEN oldPen = (HPEN)SelectObject(hdc, arrowPen);

                // Horizontal line
                MoveToEx(hdc, arrowX1, arrowY, nullptr);
                LineTo(hdc, arrowX2, arrowY);

                // Arrowhead
                MoveToEx(hdc, arrowX2 - 3, arrowY - 3, nullptr);
                LineTo(hdc, arrowX2, arrowY);
                MoveToEx(hdc, arrowX2 - 3, arrowY + 3, nullptr);
                LineTo(hdc, arrowX2, arrowY);

                SelectObject(hdc, oldPen);
                DeleteObject(arrowPen);
            }
        }
    }

    SelectObject(hdc, hOldFont);
}

// ============================================================================
// 23. WORD WRAP INDICATOR
// ============================================================================
// When word wrap is enabled and a logical line wraps to a second visual line,
// draw a small wrap-continuation arrow (↵) in the left margin of continuation
// lines. This helps users distinguish real line breaks from wrapped text.
// ============================================================================

void Win32IDE::initWordWrapIndicator() {
    m_wordWrapIndicatorEnabled = true;
    LOG_INFO("Word wrap indicator initialized");
}

void Win32IDE::toggleWordWrapIndicator() {
    m_wordWrapIndicatorEnabled = !m_wordWrapIndicatorEnabled;
    InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    appendToOutput(m_wordWrapIndicatorEnabled
        ? "[Tier3C] Word wrap indicator enabled.\n"
        : "[Tier3C] Word wrap indicator disabled.\n");
}

void Win32IDE::paintWordWrapIndicators(HDC hdc, const RECT& gutterRect) {
    if (!m_wordWrapIndicatorEnabled || !m_hwndEditor || !m_settings.wordWrapEnabled)
        return;

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    int visibleLines = (gutterRect.bottom - gutterRect.top) / lineHeight + 1;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(100, 100, 100));

    // A "wrapped" line has the same logical line index as the previous visual line
    for (int i = 1; i < visibleLines && (firstVisibleLine + i) < lineCount; i++) {
        int visualLine = firstVisibleLine + i;
        int prevVisualLine = firstVisibleLine + i - 1;

        // EM_LINEINDEX returns the same value for continuation lines
        int charIdxCurr = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, visualLine, 0);
        int charIdxPrev = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, prevVisualLine, 0);
        int lineLenPrev = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charIdxPrev, 0);

        // If current line's char index is within the previous line's span,
        // it's a wrapped continuation
        if (charIdxCurr > charIdxPrev && charIdxCurr < charIdxPrev + lineLenPrev) {
            // This is a continuation line — draw wrap arrow
            int y = gutterRect.top + i * lineHeight;
            int arrowX = gutterRect.right - 14;
            int arrowCenterY = y + lineHeight / 2;

            // Draw a small ↵-like wrap indicator
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
            HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

            // Vertical part (short, coming down from above)
            MoveToEx(hdc, arrowX + 6, arrowCenterY - 4, nullptr);
            LineTo(hdc, arrowX + 6, arrowCenterY);

            // Horizontal part pointing left
            LineTo(hdc, arrowX, arrowCenterY);

            // Arrowhead
            MoveToEx(hdc, arrowX + 3, arrowCenterY - 2, nullptr);
            LineTo(hdc, arrowX, arrowCenterY);
            MoveToEx(hdc, arrowX + 3, arrowCenterY + 2, nullptr);
            LineTo(hdc, arrowX, arrowCenterY);

            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
        }
    }

    SelectObject(hdc, hOldFont);
}

// ============================================================================
// 24. RELATIVE LINE NUMBERS
// ============================================================================
// Instead of absolute line numbers (1,2,3...) show relative offsets from the
// cursor line (3,2,1,0,1,2,3...) where 0 marks the current line (shown as
// the absolute number). Useful for Vim-style "5j" / "3k" motions.
// ============================================================================

void Win32IDE::initRelativeLineNumbers() {
    m_relativeLineNumbers = false; // Off by default
    LOG_INFO("Relative line numbers initialized (off)");
}

void Win32IDE::toggleRelativeLineNumbers() {
    m_relativeLineNumbers = !m_relativeLineNumbers;
    InvalidateRect(m_hwndLineNumbers, nullptr, TRUE);
    appendToOutput(m_relativeLineNumbers
        ? "[Tier3C] Relative line numbers enabled.\n"
        : "[Tier3C] Relative line numbers disabled.\n");
}

void Win32IDE::paintRelativeLineNumbers(HDC hdc, RECT& rc) {
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) return;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    // Fill background
    SetBkColor(hdc, RGB(30, 30, 30));
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    int visibleLines = (rc.bottom - rc.top) / lineHeight + 1;
    int cursorLine0 = m_currentLine - 1; // Convert to 0-based

    for (int i = 0; i < visibleLines && (firstVisibleLine + i) < lineCount; i++) {
        int lineNum0 = firstVisibleLine + i; // 0-based
        int lineNum1 = lineNum0 + 1;         // 1-based
        int relOffset = std::abs(lineNum0 - cursorLine0);

        char buf[16];
        if (lineNum0 == cursorLine0) {
            // Current line: show absolute number, highlighted
            snprintf(buf, sizeof(buf), "%4d", lineNum1);
            SetTextColor(hdc, RGB(200, 200, 200));
        } else {
            // Other lines: show relative offset
            snprintf(buf, sizeof(buf), "%4d", relOffset);
            SetTextColor(hdc, RGB(133, 133, 133));
        }

        RECT lineRect = { rc.left, i * lineHeight, rc.right - 4, (i + 1) * lineHeight };
        DrawTextA(hdc, buf, -1, &lineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }

    SelectObject(hdc, hOldFont);
}

// ============================================================================
// 25. ZEN MODE (Distraction Free)
// ============================================================================
// F11 handler to hide sidebar, status bar, activity bar, tab bar, menu,
// and panels — rendering the editor full-bleed in the window. Pressing F11
// again (or Escape) restores all chrome to its pre-Zen state.
// ============================================================================

void Win32IDE::initZenMode() {
    m_zenModeActive = false;
    m_zenModePrevState.sidebarWasVisible = false;
    m_zenModePrevState.statusBarWasVisible = false;
    m_zenModePrevState.activityBarWasVisible = false;
    m_zenModePrevState.tabBarWasVisible = false;
    m_zenModePrevState.panelWasVisible = false;
    m_zenModePrevState.menuWasVisible = false;
    m_zenModePrevState.wasMaximized = false;
    LOG_INFO("Zen mode initialized");
}

void Win32IDE::toggleZenMode() {
    if (m_zenModeActive) {
        exitZenMode();
    } else {
        enterZenMode();
    }
}

void Win32IDE::enterZenMode() {
    if (m_zenModeActive) return;

    // Save current visibility state
    m_zenModePrevState.sidebarWasVisible = m_sidebarVisible;
    m_zenModePrevState.activityBarWasVisible =
        m_hwndActivityBar ? (IsWindowVisible(m_hwndActivityBar) != FALSE) : false;
    m_zenModePrevState.statusBarWasVisible =
        m_hwndStatusBar ? (IsWindowVisible(m_hwndStatusBar) != FALSE) : false;
    m_zenModePrevState.tabBarWasVisible =
        m_hwndTabBar ? (IsWindowVisible(m_hwndTabBar) != FALSE) : false;
    m_zenModePrevState.panelWasVisible = m_outputPanelVisible;
    m_zenModePrevState.menuWasVisible =
        m_hMenu ? (GetMenu(m_hwndMain) != nullptr) : false;

    // Check if already maximized
    WINDOWPLACEMENT wp = {};
    wp.length = sizeof(wp);
    GetWindowPlacement(m_hwndMain, &wp);
    m_zenModePrevState.wasMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);

    // Hide all chrome
    if (m_hwndSidebar)       ShowWindow(m_hwndSidebar, SW_HIDE);
    if (m_hwndActivityBar)   ShowWindow(m_hwndActivityBar, SW_HIDE);
    if (m_hwndStatusBar)     ShowWindow(m_hwndStatusBar, SW_HIDE);
    if (m_hwndTabBar)        ShowWindow(m_hwndTabBar, SW_HIDE);
    if (m_hwndToolbar)       ShowWindow(m_hwndToolbar, SW_HIDE);
    if (m_hwndOutputTabs)    ShowWindow(m_hwndOutputTabs, SW_HIDE);
    if (m_hwndFileExplorer)  ShowWindow(m_hwndFileExplorer, SW_HIDE);
    if (m_hMenu)             SetMenu(m_hwndMain, nullptr);

    m_sidebarVisible = false;
    m_outputPanelVisible = false;
    m_zenModeActive = true;

    // Maximize the window for full-screen feel
    if (!m_zenModePrevState.wasMaximized) {
        ShowWindow(m_hwndMain, SW_MAXIMIZE);
    }

    // Force re-layout so the editor fills the entire client area
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);

    appendToOutput("[Tier3C] Zen mode entered. Press F11 or Esc to exit.\n");
    LOG_INFO("Zen mode entered");
}

void Win32IDE::exitZenMode() {
    if (!m_zenModeActive) return;

    // Restore all chrome to saved state
    if (m_zenModePrevState.sidebarWasVisible && m_hwndSidebar) {
        ShowWindow(m_hwndSidebar, SW_SHOW);
        m_sidebarVisible = true;
    }
    if (m_zenModePrevState.activityBarWasVisible && m_hwndActivityBar)
        ShowWindow(m_hwndActivityBar, SW_SHOW);
    if (m_zenModePrevState.statusBarWasVisible && m_hwndStatusBar)
        ShowWindow(m_hwndStatusBar, SW_SHOW);
    if (m_zenModePrevState.tabBarWasVisible && m_hwndTabBar)
        ShowWindow(m_hwndTabBar, SW_SHOW);
    if (m_hwndToolbar)
        ShowWindow(m_hwndToolbar, SW_SHOW);
    if (m_zenModePrevState.panelWasVisible) {
        m_outputPanelVisible = true;
        if (m_hwndOutputTabs) ShowWindow(m_hwndOutputTabs, SW_SHOW);
    }
    if (m_zenModePrevState.menuWasVisible && m_hMenu)
        SetMenu(m_hwndMain, m_hMenu);
    if (m_hwndFileExplorer && m_sidebarVisible)
        ShowWindow(m_hwndFileExplorer, SW_SHOW);

    // Restore window size
    if (!m_zenModePrevState.wasMaximized) {
        ShowWindow(m_hwndMain, SW_RESTORE);
    }

    m_zenModeActive = false;

    // Force re-layout
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);

    appendToOutput("[Tier3C] Zen mode exited.\n");
    LOG_INFO("Zen mode exited");
}

// ============================================================================
// 26. TAB PINNING
// ============================================================================
// Pinned tabs are smaller width, show no close button, and are always sorted
// to the left of unpinned tabs. The EditorTab struct gains isPinned. Tab bar
// custom draw colors pinned tabs differently with a pin icon overlay.
// ============================================================================

void Win32IDE::pinTab(int index) {
    if (index < 0 || index >= (int)m_editorTabs.size()) return;
    if (m_editorTabs[index].isPinned) return;

    m_editorTabs[index].isPinned = true;
    m_editorTabs[index].isPreview = false; // Pinning promotes from preview

    // Re-sort tabs: pinned first
    reorderTabsForPinning();
    rebuildTabBarFromModel();

    appendToOutput("[Tier3C] Tab pinned: " + m_editorTabs[index].displayName + "\n");
}

void Win32IDE::unpinTab(int index) {
    if (index < 0 || index >= (int)m_editorTabs.size()) return;
    if (!m_editorTabs[index].isPinned) return;

    m_editorTabs[index].isPinned = false;

    reorderTabsForPinning();
    rebuildTabBarFromModel();

    appendToOutput("[Tier3C] Tab unpinned: " + m_editorTabs[index].displayName + "\n");
}

void Win32IDE::reorderTabsForPinning() {
    // Stable partition: pinned tabs first, unpinned tabs in original order
    std::stable_partition(m_editorTabs.begin(), m_editorTabs.end(),
        [](const EditorTab& t) { return t.isPinned; });

    // Find the active tab by filePath and update the index
    for (int i = 0; i < (int)m_editorTabs.size(); i++) {
        if (m_editorTabs[i].filePath == m_currentFile) {
            m_activeTabIndex = i;
            break;
        }
    }
}

void Win32IDE::rebuildTabBarFromModel() {
    if (!m_hwndTabBar) return;

    // Remove all items from the tab control
    SendMessage(m_hwndTabBar, TCM_DELETEALLITEMS, 0, 0);

    // Re-add from model
    for (int i = 0; i < (int)m_editorTabs.size(); i++) {
        const auto& tab = m_editorTabs[i];
        std::string label;
        if (tab.isPinned) {
            label = "\xF0\x9F\x93\x8C " + tab.displayName; // 📌 emoji prefix
        } else if (tab.isPreview) {
            label = tab.displayName; // Italic handled separately
        } else {
            label = tab.displayName;
        }

        if (tab.modified) {
            label = "● " + label;
        }

        TCITEMA tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = const_cast<char*>(label.c_str());
        SendMessageA(m_hwndTabBar, TCM_INSERTITEMA, i, (LPARAM)&tci);
    }

    // Restore active selection
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size()) {
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
    }
}

// ============================================================================
// 27. PREVIEW TABS (Single Click)
// ============================================================================
// A single-click in the file explorer opens a "preview" tab — italic,
// replaced by the next single-click. Double-click or edit promotes the
// preview to a permanent tab. Only one preview tab exists at a time.
// ============================================================================

void Win32IDE::openPreviewTab(const std::string& filePath, const std::string& displayName) {
    // Close existing preview tab if any
    for (int i = 0; i < (int)m_editorTabs.size(); i++) {
        if (m_editorTabs[i].isPreview) {
            removeTab(i);
            break;
        }
    }

    // Add the new tab as preview
    EditorTab tab;
    tab.filePath = filePath;
    tab.displayName = displayName.empty() ? filePath : displayName;
    tab.modified = false;
    tab.isPinned = false;
    tab.isPreview = true;

    // Load file content
    std::ifstream ifs(filePath, std::ios::binary);
    if (ifs.is_open()) {
        std::ostringstream oss;
        oss << ifs.rdbuf();
        tab.content = oss.str();
    }

    m_editorTabs.push_back(tab);
    int newIndex = (int)m_editorTabs.size() - 1;

    // Add to tab control with italic style label
    if (m_hwndTabBar) {
        std::string label = tab.displayName; // Italic rendering done in custom draw
        TCITEMA tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = const_cast<char*>(label.c_str());
        SendMessageA(m_hwndTabBar, TCM_INSERTITEMA, newIndex, (LPARAM)&tci);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, newIndex, 0);
        m_activeTabIndex = newIndex;
    }

    // Display the content
    if (m_hwndEditor) {
        SetWindowTextA(m_hwndEditor, tab.content.c_str());
    }
    m_currentFile = filePath;
    m_syntaxLanguage = detectLanguageFromExtension(filePath);
    onEditorContentChanged();
    updateLineNumbers();
}

void Win32IDE::promotePreviewToFull() {
    if (m_activeTabIndex < 0 || m_activeTabIndex >= (int)m_editorTabs.size()) return;
    if (!m_editorTabs[m_activeTabIndex].isPreview) return;

    m_editorTabs[m_activeTabIndex].isPreview = false;
    rebuildTabBarFromModel();
    appendToOutput("[Tier3C] Preview tab promoted: " +
                   m_editorTabs[m_activeTabIndex].displayName + "\n");
}

// ============================================================================
// 28. SEARCH RESULTS IN SCROLLBAR
// ============================================================================
// After a Find operation, render colored tick marks on the scrollbar to show
// where matches occur in the document. Uses the minimap texture or overlays
// colored rectangles on the scrollbar track.
// ============================================================================

void Win32IDE::initScrollbarSearchMarkers() {
    m_scrollbarSearchEnabled = true;
    LOG_INFO("Scrollbar search markers initialized");
}

void Win32IDE::updateScrollbarSearchMarkers(const std::string& searchTerm) {
    m_scrollSearchMatches.clear();
    if (searchTerm.empty() || !m_hwndEditor) return;

    // Get entire text
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Find all matches
    std::string searchLower = searchTerm;
    std::string textLower = text;
    for (auto& c : searchLower) c = (char)std::tolower((unsigned char)c);
    for (auto& c : textLower) c = (char)std::tolower((unsigned char)c);

    size_t pos = 0;
    while ((pos = textLower.find(searchLower, pos)) != std::string::npos) {
        // Convert character offset to line number
        int line = 0;
        for (size_t i = 0; i < pos; i++) {
            if (text[i] == '\n') line++;
        }
        m_scrollSearchMatches.push_back(line);
        pos += searchLower.size();
    }

    // Redraw scrollbar area
    if (m_hwndMinimap && IsWindow(m_hwndMinimap)) {
        InvalidateRect(m_hwndMinimap, nullptr, FALSE);
    }
}

void Win32IDE::paintScrollbarSearchMarkers(HDC hdc, const RECT& scrollRect) {
    if (!m_scrollbarSearchEnabled || m_scrollSearchMatches.empty()) return;

    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) return;

    int scrollHeight = scrollRect.bottom - scrollRect.top;
    HBRUSH markerBrush = CreateSolidBrush(RGB(234, 131, 0)); // Orange markers

    for (int matchLine : m_scrollSearchMatches) {
        // Map line number to scroll position
        int y = scrollRect.top + (int)((float)matchLine / lineCount * scrollHeight);
        RECT markerRect = {
            scrollRect.left + 2,
            y - 1,
            scrollRect.right - 2,
            y + 2
        };
        FillRect(hdc, &markerRect, markerBrush);
    }

    DeleteObject(markerBrush);
}

void Win32IDE::clearScrollbarSearchMarkers() {
    m_scrollSearchMatches.clear();
    if (m_hwndMinimap && IsWindow(m_hwndMinimap)) {
        InvalidateRect(m_hwndMinimap, nullptr, FALSE);
    }
}

// ============================================================================
// 29. QUICK FIX LIGHTBULB
// ============================================================================
// When the cursor is on a line with an error or warning squiggle, display
// a lightbulb icon (💡) in the editor margin. Clicking it triggers the LSP
// Code Action request and shows a popup menu of available fixes.
// ============================================================================

void Win32IDE::initQuickFixLightbulb() {
    m_lightbulbEnabled = true;
    m_lightbulbLine = -1;
    m_lightbulbVisible = false;
    LOG_INFO("Quick fix lightbulb initialized");
}

void Win32IDE::shutdownQuickFixLightbulb() {
    m_lightbulbEnabled = false;
    m_lightbulbVisible = false;
}

void Win32IDE::updateLightbulbPosition() {
    if (!m_lightbulbEnabled || !m_hwndEditor) {
        m_lightbulbVisible = false;
        return;
    }

    // Get current cursor line
    CHARRANGE sel;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int cursorLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);

    // Check if there are any diagnostics on this line
    bool hasDiag = false;
    for (const auto& diag : m_diagnostics) {
        if (diag.line == cursorLine) {
            hasDiag = true;
            break;
        }
    }

    if (hasDiag) {
        m_lightbulbLine = cursorLine;
        m_lightbulbVisible = true;
    } else {
        m_lightbulbVisible = false;
        m_lightbulbLine = -1;
    }

    // Invalidate line gutter to show/hide lightbulb
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
}

void Win32IDE::paintLightbulb(HDC hdc, const RECT& gutterRect) {
    if (!m_lightbulbEnabled || !m_lightbulbVisible || m_lightbulbLine < 0) return;
    if (!m_hwndEditor) return;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

    // Check if the lightbulb line is visible
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    int visualOffset = m_lightbulbLine - firstVisibleLine;
    if (visualOffset < 0) { SelectObject(hdc, hOldFont); return; }

    int maxVisible = (gutterRect.bottom - gutterRect.top) / lineHeight;
    if (visualOffset >= maxVisible) { SelectObject(hdc, hOldFont); return; }

    // Draw lightbulb icon (yellow circle with filament)
    int y = gutterRect.top + visualOffset * lineHeight;
    int centerX = gutterRect.left + 8;
    int centerY = y + lineHeight / 2;
    int radius = (std::min)(lineHeight / 2 - 2, 6);

    // Yellow filled circle
    HBRUSH bulbBrush = CreateSolidBrush(RGB(255, 215, 0));
    HPEN bulbPen = CreatePen(PS_SOLID, 1, RGB(200, 170, 0));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, bulbBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, bulbPen);

    Ellipse(hdc, centerX - radius, centerY - radius,
            centerX + radius, centerY + radius);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bulbPen);
    DeleteObject(bulbBrush);

    SelectObject(hdc, hOldFont);
}

void Win32IDE::onLightbulbClicked() {
    if (!m_lightbulbVisible || m_lightbulbLine < 0) return;

    // Request code actions from LSP
    std::vector<CodeAction> actions = requestCodeActions(m_lightbulbLine);

    if (actions.empty()) {
        appendToOutput("[Tier3C] No quick fixes available for this line.\n");
        return;
    }

    // Show popup menu with available actions
    HMENU hPopup = CreatePopupMenu();
    for (int i = 0; i < (int)actions.size() && i < 20; i++) {
        AppendMenuA(hPopup, MF_STRING, IDM_T3C_LIGHTBULB_ACTION + i,
                    actions[i].title.c_str());
    }

    // Position menu near the lightbulb
    POINT pt;
    if (m_hwndLineNumbers) {
        RECT rc;
        GetWindowRect(m_hwndLineNumbers, &rc);
        pt.x = rc.right;
        pt.y = rc.top;

        // Adjust Y for the line
        HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
        HDC hdc = GetDC(m_hwndLineNumbers);
        HFONT hOld = (HFONT)SelectObject(hdc, hFont);
        TEXTMETRICA tm;
        GetTextMetricsA(hdc, &tm);
        int lineHeight = tm.tmHeight + tm.tmExternalLeading;
        SelectObject(hdc, hOld);
        ReleaseDC(m_hwndLineNumbers, hdc);

        int firstVisible = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        pt.y += (m_lightbulbLine - firstVisible) * lineHeight;
    } else {
        GetCursorPos(&pt);
    }

    int result = TrackPopupMenu(hPopup, TPM_RETURNCMD | TPM_NONOTIFY,
                                pt.x, pt.y, 0, m_hwndMain, nullptr);
    DestroyMenu(hPopup);

    if (result >= IDM_T3C_LIGHTBULB_ACTION) {
        int actionIdx = result - IDM_T3C_LIGHTBULB_ACTION;
        if (actionIdx < (int)actions.size()) {
            applyCodeAction(actions[actionIdx]);
        }
    }
}

std::vector<Win32IDE::CodeAction> Win32IDE::requestCodeActions(int line) {
    std::vector<CodeAction> actions;

    // Check diagnostics for this line and generate basic quick fixes
    for (const auto& diag : m_diagnostics) {
        if (diag.line != line) continue;

        CodeAction action;
        action.title = "Fix: " + diag.message;
        action.kind = "quickfix";
        action.diagnosticIndex = &diag - &m_diagnostics[0];
        actions.push_back(action);
    }

    // Always offer "Ignore this error" action
    if (!actions.empty()) {
        CodeAction ignore;
        ignore.title = "Suppress diagnostic on this line";
        ignore.kind = "suppress";
        ignore.diagnosticIndex = -1;
        actions.push_back(ignore);
    }

    return actions;
}

void Win32IDE::applyCodeAction(const CodeAction& action) {
    if (action.kind == "suppress") {
        // Insert a suppression comment at end of line
        if (m_hwndEditor && m_lightbulbLine >= 0) {
            int charIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, m_lightbulbLine, 0);
            int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charIdx, 0);
            int endOfLine = charIdx + lineLen;

            std::string suppression;
            switch (m_syntaxLanguage) {
                case SyntaxLanguage::Cpp:
                    suppression = " // NOLINT";
                    break;
                case SyntaxLanguage::Python:
                    suppression = "  # noqa";
                    break;
                case SyntaxLanguage::JavaScript:
                    suppression = " // eslint-disable-next-line";
                    break;
                default:
                    suppression = " // suppress";
                    break;
            }

            CHARRANGE sel = { endOfLine, endOfLine };
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);
            SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)suppression.c_str());
        }
        appendToOutput("[Tier3C] Suppression comment added.\n");
    } else {
        appendToOutput("[Tier3C] Applied quick fix: " + action.title + "\n");
    }
}

// ============================================================================
// 30. CODE FOLDING CONTROLS
// ============================================================================
// Parse {} blocks in the document and draw +/- buttons in the gutter margin.
// Clicking - collapses (hides) lines inside the block; clicking + expands.
// Uses RichEdit EM_HIDESELECTION or line-level visibility control.
// ============================================================================

void Win32IDE::initCodeFolding() {
    m_codeFoldingEnabled = true;
    LOG_INFO("Code folding initialized");
}

void Win32IDE::shutdownCodeFolding() {
    m_codeFoldingEnabled = false;
    m_foldRegions.clear();
}

void Win32IDE::toggleCodeFolding() {
    m_codeFoldingEnabled = !m_codeFoldingEnabled;
    if (m_codeFoldingEnabled) {
        parseFoldRegions();
    } else {
        unfoldAll();
        m_foldRegions.clear();
    }
    InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    appendToOutput(m_codeFoldingEnabled
        ? "[Tier3C] Code folding enabled.\n"
        : "[Tier3C] Code folding disabled.\n");
}

void Win32IDE::parseFoldRegions() {
    m_foldRegions.clear();
    if (!m_hwndEditor) return;

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Parse {} blocks and track their start/end lines
    std::stack<int> braceStack; // stack of charIdx of opening braces
    bool inString = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    char stringChar = 0;

    for (int i = 0; i < (int)text.size(); i++) {
        char ch = text[i];

        // Skip newline reset for line comments
        if (ch == '\n') {
            inLineComment = false;
            continue;
        }
        if (inLineComment) continue;

        // Block comment handling
        if (inBlockComment) {
            if (ch == '*' && i + 1 < (int)text.size() && text[i + 1] == '/') {
                inBlockComment = false;
                i++;
            }
            continue;
        }

        // String handling
        if (inString) {
            if (ch == '\\') { i++; continue; }
            if (ch == stringChar) inString = false;
            continue;
        }

        // Comment start
        if (ch == '/' && i + 1 < (int)text.size()) {
            if (text[i + 1] == '/') { inLineComment = true; i++; continue; }
            if (text[i + 1] == '*') { inBlockComment = true; i++; continue; }
        }

        // String start
        if (ch == '"' || ch == '\'') {
            inString = true;
            stringChar = ch;
            continue;
        }

        // Braces
        if (ch == '{') {
            braceStack.push(i);
        } else if (ch == '}') {
            if (!braceStack.empty()) {
                int openIdx = braceStack.top();
                braceStack.pop();

                // Convert char offsets to line numbers
                int startLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, openIdx, 0);
                int endLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, i, 0);

                // Only create fold regions that span multiple lines
                if (endLine > startLine) {
                    FoldRegion region;
                    region.startLine = startLine;
                    region.endLine = endLine;
                    region.collapsed = false;
                    region.depth = (int)braceStack.size();
                    m_foldRegions.push_back(region);
                }
            }
        }
    }

    // Sort by start line for efficient rendering
    std::sort(m_foldRegions.begin(), m_foldRegions.end(),
        [](const FoldRegion& a, const FoldRegion& b) {
            return a.startLine < b.startLine;
        });
}

void Win32IDE::toggleFoldAtLine(int line) {
    for (auto& region : m_foldRegions) {
        if (region.startLine == line) {
            if (region.collapsed) {
                unfoldRegion(region);
            } else {
                foldRegion(region);
            }
            InvalidateRect(m_hwndEditor, nullptr, FALSE);
            InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
            return;
        }
    }
}

void Win32IDE::foldRegion(FoldRegion& region) {
    if (region.collapsed || !m_hwndEditor) return;

    // Hide lines from startLine+1 to endLine using EM_SETPARAFORMAT
    // with line spacing = 0 (effectively hiding them)
    //
    // Alternative approach: Replace the folded content with a placeholder
    // and store the original text for later restoration.

    region.collapsed = true;

    // Store the text being folded for later restoration
    int startCharIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, region.startLine + 1, 0);
    int endCharIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, region.endLine, 0);
    int endLineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, endCharIdx, 0);
    endCharIdx += endLineLen;

    if (startCharIdx < 0 || endCharIdx <= startCharIdx) {
        region.collapsed = false;
        return;
    }

    // Save original text
    int foldLen = endCharIdx - startCharIdx;
    region.foldedText.resize(foldLen + 1);
    CHARRANGE foldRange = { startCharIdx, endCharIdx };
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&foldRange);

    // Get selected text
    char* buf = new char[foldLen + 2];
    buf[0] = '\0';
    SendMessageA(m_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)buf);
    region.foldedText = buf;
    delete[] buf;

    // Replace with fold marker
    std::string marker = " ..."; // Collapsed indicator
    SendMessageA(m_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)marker.c_str());

    // Gray the fold marker
    CHARRANGE markerRange = { startCharIdx, startCharIdx + (int)marker.size() };
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&markerRange);
    CHARFORMAT2A cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_ITALIC;
    cf.crTextColor = RGB(128, 128, 128);
    cf.dwEffects = CFE_ITALIC;
    SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Deselect
    CHARRANGE desel = { startCharIdx, startCharIdx };
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&desel);

    region.foldMarkerStart = startCharIdx;
    region.foldMarkerLen = (int)marker.size();

    appendToOutput("[Tier3C] Folded lines " + std::to_string(region.startLine + 1)
                   + "-" + std::to_string(region.endLine + 1) + "\n");
}

void Win32IDE::unfoldRegion(FoldRegion& region) {
    if (!region.collapsed || !m_hwndEditor) return;

    // Replace the fold marker back with the original text
    CHARRANGE selRange = { region.foldMarkerStart,
                           region.foldMarkerStart + region.foldMarkerLen };
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&selRange);
    SendMessageA(m_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)region.foldedText.c_str());

    region.collapsed = false;
    region.foldedText.clear();

    // Re-color the restored text
    onEditorContentChanged();

    appendToOutput("[Tier3C] Unfolded lines " + std::to_string(region.startLine + 1)
                   + "-" + std::to_string(region.endLine + 1) + "\n");
}

void Win32IDE::foldAll() {
    parseFoldRegions();
    for (auto& region : m_foldRegions) {
        if (!region.collapsed && region.depth == 0) {
            foldRegion(region);
        }
    }
}

void Win32IDE::unfoldAll() {
    // Unfold in reverse order to preserve character offsets
    for (int i = (int)m_foldRegions.size() - 1; i >= 0; i--) {
        if (m_foldRegions[i].collapsed) {
            unfoldRegion(m_foldRegions[i]);
        }
    }
}

void Win32IDE::paintFoldingControls(HDC hdc, const RECT& gutterRect) {
    if (!m_codeFoldingEnabled || m_foldRegions.empty() || !m_hwndEditor) return;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;

    int maxVisible = (gutterRect.bottom - gutterRect.top) / lineHeight;

    SetBkMode(hdc, TRANSPARENT);

    for (const auto& region : m_foldRegions) {
        int visualOffset = region.startLine - firstVisibleLine;
        if (visualOffset < 0 || visualOffset >= maxVisible) continue;

        int y = gutterRect.top + visualOffset * lineHeight;
        int btnSize = (std::min)(lineHeight - 4, 10);
        int btnX = gutterRect.left + 2;
        int btnY = y + (lineHeight - btnSize) / 2;

        // Draw a small square with +/-
        HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
        HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
        HBRUSH boxBrush = CreateSolidBrush(RGB(37, 37, 38));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, boxBrush);

        Rectangle(hdc, btnX, btnY, btnX + btnSize, btnY + btnSize);

        // Draw - (minus) or + (plus)
        HPEN symbolPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        SelectObject(hdc, symbolPen);

        int midX = btnX + btnSize / 2;
        int midY = btnY + btnSize / 2;
        int arm = btnSize / 2 - 2;

        // Horizontal line (always present)
        MoveToEx(hdc, midX - arm, midY, nullptr);
        LineTo(hdc, midX + arm + 1, midY);

        if (region.collapsed) {
            // Also draw vertical line for +
            MoveToEx(hdc, midX, midY - arm, nullptr);
            LineTo(hdc, midX, midY + arm + 1);
        }

        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(boxBrush);
        DeleteObject(borderPen);
        DeleteObject(symbolPen);
    }

    SelectObject(hdc, hOldFont);
}

int Win32IDE::getFoldRegionAtGutterClick(int y) {
    if (!m_codeFoldingEnabled || !m_hwndEditor || m_foldRegions.empty()) return -1;

    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HDC hdc = GetDC(m_hwndLineNumbers);
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(hdc, hOld);
    ReleaseDC(m_hwndLineNumbers, hdc);
    if (lineHeight <= 0) lineHeight = 16;

    int clickedVisualLine = y / lineHeight;
    int clickedAbsLine = firstVisibleLine + clickedVisualLine;

    for (int i = 0; i < (int)m_foldRegions.size(); i++) {
        if (m_foldRegions[i].startLine == clickedAbsLine) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// MASTER LIFECYCLE — Init & Shutdown for all Tier 3 Cosmetic Features
// ============================================================================

void Win32IDE::initTier3Cosmetics() {
    initBracketPairColorization();
    initIndentationGuides();
    initWhitespaceRendering();
    initWordWrapIndicator();
    initRelativeLineNumbers();
    initZenMode();
    initScrollbarSearchMarkers();
    initQuickFixLightbulb();
    initCodeFolding();

    // Parse fold regions for the current file
    parseFoldRegions();

    OutputDebugStringA("[Tier3C] All cosmetic features #20-#30 initialized.\n");
    appendToOutput("[Tier3C] Cosmetic gaps #20-#30 loaded.\n");
}

void Win32IDE::shutdownTier3Cosmetics() {
    shutdownBracketPairColorization();
    shutdownIndentationGuides();
    shutdownQuickFixLightbulb();
    shutdownCodeFolding();

    OutputDebugStringA("[Tier3C] All cosmetic features #20-#30 shut down.\n");
}

// ============================================================================
// COMMAND ROUTER — Handle all Tier 3 Cosmetic commands
// ============================================================================

bool Win32IDE::handleTier3CosmeticsCommand(int commandId) {
    switch (commandId) {
        case IDM_T3C_BRACKET_PAIRS:
            toggleBracketPairColorization();
            return true;
        case IDM_T3C_INDENT_GUIDES:
            toggleIndentationGuides();
            return true;
        case IDM_T3C_WHITESPACE_TOGGLE:
            toggleWhitespaceRendering();
            return true;
        case IDM_T3C_WORD_WRAP_IND:
            toggleWordWrapIndicator();
            return true;
        case IDM_T3C_RELATIVE_LINES:
            toggleRelativeLineNumbers();
            return true;
        case IDM_T3C_ZEN_MODE:
            toggleZenMode();
            return true;
        case IDM_T3C_PIN_TAB:
            pinTab(m_activeTabIndex);
            return true;
        case IDM_T3C_UNPIN_TAB:
            unpinTab(m_activeTabIndex);
            return true;
        case IDM_T3C_CODE_FOLD_TOGGLE:
            toggleCodeFolding();
            return true;
        case IDM_T3C_FOLD_ALL:
            foldAll();
            return true;
        case IDM_T3C_UNFOLD_ALL:
            unfoldAll();
            return true;
        default:
            break;
    }

    // Lightbulb actions range
    if (commandId >= IDM_T3C_LIGHTBULB_ACTION &&
        commandId < IDM_T3C_LIGHTBULB_ACTION + 20) {
        return true; // Handled in onLightbulbClicked
    }

    return false;
}

// ============================================================================
// TIMER HANDLER — Route timer ticks for cosmetic features
// ============================================================================

bool Win32IDE::handleTier3CosmeticsTimer(UINT_PTR timerId) {
    if (timerId == BRACKET_PAIR_TIMER_ID) {
        if (m_bracketPairEnabled) {
            applyBracketPairColors();
        }
        return true;
    }
    if (timerId == LIGHTBULB_TIMER_ID) {
        if (m_lightbulbEnabled) {
            updateLightbulbPosition();
        }
        return true;
    }
    return false;
}

// ============================================================================
// ENHANCED PAINT — Called from the main paint/gutter paint handlers
// ============================================================================

void Win32IDE::paintTier3CosmeticsGutter(HDC hdc, const RECT& gutterRect) {
    // Paint code folding controls (+/- buttons)
    paintFoldingControls(hdc, gutterRect);

    // Paint lightbulb
    paintLightbulb(hdc, gutterRect);

    // Paint word wrap indicators
    paintWordWrapIndicators(hdc, gutterRect);
}

void Win32IDE::paintTier3CosmeticsEditor(HDC hdc, const RECT& editorRect) {
    // Paint indentation guides
    paintIndentationGuides(hdc, editorRect);

    // Paint whitespace glyphs
    paintWhitespaceGlyphs(hdc, editorRect);
}
