// ============================================================================
// Win32IDE_MultiCursor.cpp — Multi-Cursor Editing Engine
// ============================================================================
// VS Code-parity multi-cursor features:
//   - Alt+Click: Add cursor at click position
//   - Ctrl+D: Select next occurrence of current word/selection
//   - Ctrl+Alt+Up/Down: Add cursor above/below
//   - Ctrl+Shift+L: Select all occurrences
//   - Esc: Cancel all secondary cursors
//   - Synchronized typing/deletion across all active cursors
//   - Multi-selection rendering with distinct caret draws
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <algorithm>
#include <set>

// ─── Cursor state container ─────────────────────────────────────────
struct MultiCursorState {
    struct Cursor {
        int charPos;        // Character position in the document (absolute)
        int anchorPos;      // Selection anchor (-1 if no selection)
        bool primary;       // Is this the primary (original) cursor?
    };

    std::vector<Cursor> cursors;
    bool active = false;    // true when >1 cursor exists

    void reset() {
        cursors.clear();
        active = false;
    }

    void addCursor(int charPos, int anchorPos = -1, bool isPrimary = false) {
        // Don't add duplicate positions
        for (auto& c : cursors) {
            if (c.charPos == charPos) return;
        }
        Cursor cur;
        cur.charPos  = charPos;
        cur.anchorPos = anchorPos;
        cur.primary   = isPrimary;
        cursors.push_back(cur);
        if (cursors.size() > 1) active = true;
        sortCursors();
    }

    void removeCursor(int charPos) {
        cursors.erase(
            std::remove_if(cursors.begin(), cursors.end(),
                [charPos](const Cursor& c) { return c.charPos == charPos && !c.primary; }),
            cursors.end());
        if (cursors.size() <= 1) active = false;
    }

    void sortCursors() {
        // Sort by charPos descending so text insertions don't invalidate earlier offsets
        std::sort(cursors.begin(), cursors.end(),
            [](const Cursor& a, const Cursor& b) { return a.charPos > b.charPos; });
    }

    int count() const { return (int)cursors.size(); }
};

static MultiCursorState g_multiCursor;

// ─── Initialize multi-cursor system ─────────────────────────────────
void Win32IDE::initMultiCursor() {
    g_multiCursor.reset();
}

// ─── Check if multi-cursor mode is active ───────────────────────────
bool Win32IDE::isMultiCursorActive() const {
    return g_multiCursor.active && g_multiCursor.count() > 1;
}

// ─── Cancel all secondary cursors (Esc) ─────────────────────────────
void Win32IDE::clearSecondaryCursors() {
    if (!g_multiCursor.active) return;

    // Keep only the primary cursor
    MultiCursorState::Cursor primary;
    primary.charPos = -1;
    for (auto& c : g_multiCursor.cursors) {
        if (c.primary) { primary = c; break; }
    }

    g_multiCursor.reset();

    // Restore primary cursor position
    if (primary.charPos >= 0 && m_hwndEditor) {
        CHARRANGE cr;
        if (primary.anchorPos >= 0) {
            cr.cpMin = primary.anchorPos;
            cr.cpMax = primary.charPos;
        } else {
            cr.cpMin = cr.cpMax = primary.charPos;
        }
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
    }

    // Repaint to remove extra caret visuals
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);

    appendToOutput("[MultiCursor] Cleared secondary cursors\n", "Editor");
}

// ─── Add cursor at a specific character position (Alt+Click) ─────────
void Win32IDE::addCursorAtPosition(int charPos) {
    if (!m_hwndEditor) return;

    // If no multi-cursor yet, capture the current primary cursor first
    if (!g_multiCursor.active) {
        CHARRANGE cr;
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);
        g_multiCursor.addCursor(cr.cpMax, (cr.cpMin != cr.cpMax) ? cr.cpMin : -1, true);
    }

    // Check if cursor already exists at this position — if so, remove it
    bool found = false;
    for (auto& c : g_multiCursor.cursors) {
        if (c.charPos == charPos && !c.primary) {
            g_multiCursor.removeCursor(charPos);
            found = true;
            break;
        }
    }

    if (!found) {
        g_multiCursor.addCursor(charPos, -1, false);
    }

    // Update status bar
    updateMultiCursorStatusBar();

    // Repaint to show cursor indicators
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Add cursor above current line (Ctrl+Alt+Up) ────────────────────
void Win32IDE::addCursorAbove() {
    if (!m_hwndEditor) return;

    // Get current cursor position
    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);

    int currentLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMax, 0);
    int lineStart   = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, currentLine, 0);
    int col         = cr.cpMax - lineStart;

    if (currentLine <= 0) return;  // Already at first line

    int targetLine  = currentLine - 1;
    int targetStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, targetLine, 0);
    int targetLen   = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, targetStart, 0);

    // Clamp column to target line length
    int targetCol = (col < targetLen) ? col : targetLen;
    int targetPos = targetStart + targetCol;

    // If no multi-cursor yet, init with primary
    if (!g_multiCursor.active) {
        g_multiCursor.addCursor(cr.cpMax, -1, true);
    }

    g_multiCursor.addCursor(targetPos, -1, false);
    updateMultiCursorStatusBar();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Add cursor below current line (Ctrl+Alt+Down) ──────────────────
void Win32IDE::addCursorBelow() {
    if (!m_hwndEditor) return;

    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);

    int currentLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMax, 0);
    int lineStart   = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, currentLine, 0);
    int col         = cr.cpMax - lineStart;

    int totalLines  = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    int targetLine  = currentLine + 1;
    if (targetLine >= totalLines) return;  // Already at last line

    int targetStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, targetLine, 0);
    int targetLen   = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, targetStart, 0);
    int targetCol   = (col < targetLen) ? col : targetLen;
    int targetPos   = targetStart + targetCol;

    if (!g_multiCursor.active) {
        g_multiCursor.addCursor(cr.cpMax, -1, true);
    }

    g_multiCursor.addCursor(targetPos, -1, false);
    updateMultiCursorStatusBar();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Get word at a character position ────────────────────────────────
static std::string getWordAtPos(HWND hwndEditor, int charPos) {
    int lineIdx = (int)SendMessage(hwndEditor, EM_LINEFROMCHAR, charPos, 0);
    int lineStart = (int)SendMessage(hwndEditor, EM_LINEINDEX, lineIdx, 0);

    char lineBuf[4096] = {0};
    *(WORD*)lineBuf = sizeof(lineBuf) - 2;
    int lineLen = (int)SendMessageA(hwndEditor, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
    lineBuf[lineLen] = '\0';

    int col = charPos - lineStart;
    if (col < 0 || col > lineLen) return "";

    // Find word boundaries
    int wordStart = col;
    while (wordStart > 0 && (std::isalnum(lineBuf[wordStart - 1]) || lineBuf[wordStart - 1] == '_'))
        wordStart--;

    int wordEnd = col;
    while (wordEnd < lineLen && (std::isalnum(lineBuf[wordEnd]) || lineBuf[wordEnd] == '_'))
        wordEnd++;

    if (wordStart >= wordEnd) return "";
    return std::string(lineBuf + wordStart, wordEnd - wordStart);
}

// ─── Select next occurrence of current word/selection (Ctrl+D) ───────
void Win32IDE::selectNextOccurrence() {
    if (!m_hwndEditor) return;

    // Get current selection or word under cursor
    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);

    std::string searchText;

    if (cr.cpMax > cr.cpMin) {
        // Has selection — use selected text
        int len = cr.cpMax - cr.cpMin;
        std::vector<char> buf(len + 1, 0);
        TEXTRANGEA tr;
        tr.chrg.cpMin = cr.cpMin;
        tr.chrg.cpMax = cr.cpMax;
        tr.lpstrText  = buf.data();
        SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        searchText = buf.data();
    } else {
        // No selection — use word under cursor
        searchText = getWordAtPos(m_hwndEditor, cr.cpMax);
        if (searchText.empty()) return;

        // Select the word under cursor first
        int lineIdx   = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, cr.cpMax, 0);
        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int col       = cr.cpMax - lineStart;

        char lineBuf[4096] = {0};
        *(WORD*)lineBuf = sizeof(lineBuf) - 2;
        int lineLen = (int)SendMessageA(m_hwndEditor, EM_GETLINE, lineIdx, (LPARAM)lineBuf);
        lineBuf[lineLen] = '\0';

        int wStart = col;
        while (wStart > 0 && (std::isalnum(lineBuf[wStart - 1]) || lineBuf[wStart - 1] == '_'))
            wStart--;
        int wEnd = col;
        while (wEnd < lineLen && (std::isalnum(lineBuf[wEnd]) || lineBuf[wEnd] == '_'))
            wEnd++;

        int absStart = lineStart + wStart;
        int absEnd   = lineStart + wEnd;

        cr.cpMin = absStart;
        cr.cpMax = absEnd;
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);

        // Init primary cursor with selection
        if (!g_multiCursor.active) {
            g_multiCursor.addCursor(cr.cpMax, cr.cpMin, true);
        }
        return;
    }

    if (searchText.empty()) return;

    // Initialize multi-cursor if needed
    if (!g_multiCursor.active) {
        g_multiCursor.addCursor(cr.cpMax, cr.cpMin, true);
    }

    // Find the positions already occupied by cursors
    std::set<int> occupiedPositions;
    for (auto& c : g_multiCursor.cursors) {
        occupiedPositions.insert(c.charPos);
    }

    // Get full document text for searching
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    std::vector<char> docBuf(textLen + 1, 0);
    GetWindowTextA(m_hwndEditor, docBuf.data(), textLen + 1);
    std::string docText(docBuf.data());

    // Search for next occurrence after the last cursor position
    int searchStart = 0;
    for (auto& c : g_multiCursor.cursors) {
        if (c.charPos > searchStart) searchStart = c.charPos;
    }

    // Search forward from last cursor, wrapping around
    int found = -1;
    for (int pass = 0; pass < 2 && found < 0; pass++) {
        int start = (pass == 0) ? searchStart : 0;
        int limit = (pass == 0) ? textLen : searchStart;

        size_t pos = start;
        while (pos < (size_t)limit) {
            size_t idx = docText.find(searchText, pos);
            if (idx == std::string::npos || (int)idx >= limit) break;

            int matchEnd   = (int)(idx + searchText.size());

            // Check if this occurrence is already selected by an existing cursor
            bool alreadySelected = false;
            for (auto& c : g_multiCursor.cursors) {
                int cStart = (c.anchorPos >= 0) ? std::min(c.anchorPos, c.charPos) : c.charPos;
                int cEnd   = (c.anchorPos >= 0) ? std::max(c.anchorPos, c.charPos) : c.charPos;
                if (cStart == (int)idx && cEnd == matchEnd) {
                    alreadySelected = true;
                    break;
                }
            }

            if (!alreadySelected) {
                found = (int)idx;
                break;
            }

            pos = idx + 1;
        }
    }

    if (found >= 0) {
        int matchEnd = found + (int)searchText.size();
        g_multiCursor.addCursor(matchEnd, found, false);

        // Move primary editor selection to the new occurrence to show it
        CHARRANGE newSel;
        newSel.cpMin = found;
        newSel.cpMax = matchEnd;
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&newSel);
        SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);

        updateMultiCursorStatusBar();
    }

    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Select ALL occurrences of current word/selection (Ctrl+Shift+L) ─
void Win32IDE::selectAllOccurrences() {
    if (!m_hwndEditor) return;

    CHARRANGE cr;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&cr);

    std::string searchText;
    if (cr.cpMax > cr.cpMin) {
        int len = cr.cpMax - cr.cpMin;
        std::vector<char> buf(len + 1, 0);
        TEXTRANGEA tr;
        tr.chrg.cpMin = cr.cpMin;
        tr.chrg.cpMax = cr.cpMax;
        tr.lpstrText  = buf.data();
        SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        searchText = buf.data();
    } else {
        searchText = getWordAtPos(m_hwndEditor, cr.cpMax);
    }

    if (searchText.empty()) return;

    // Reset and build fresh
    g_multiCursor.reset();

    // Get full document
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    std::vector<char> docBuf(textLen + 1, 0);
    GetWindowTextA(m_hwndEditor, docBuf.data(), textLen + 1);
    std::string docText(docBuf.data());

    // Find all occurrences
    size_t pos = 0;
    bool firstMatch = true;
    int matchCount = 0;
    while (pos < docText.size() && matchCount < 500) {  // Cap at 500 cursors
        size_t idx = docText.find(searchText, pos);
        if (idx == std::string::npos) break;

        int matchStart = (int)idx;
        int matchEnd   = matchStart + (int)searchText.size();

        g_multiCursor.addCursor(matchEnd, matchStart, firstMatch);
        firstMatch = false;
        matchCount++;

        pos = idx + 1;
    }

    updateMultiCursorStatusBar();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);

    if (matchCount > 0) {
        appendToOutput("[MultiCursor] Selected " + std::to_string(matchCount) +
                      " occurrences of '" + searchText + "'\n", "Editor");
    }
}

// ─── Type text at all cursor positions ───────────────────────────────
// This is the core synchronized editing function.
// text insertion offsets adjust backward through the sorted cursor list.
void Win32IDE::multiCursorInsertText(const std::string& text) {
    if (!m_hwndEditor || !g_multiCursor.active) return;

    // Cursors are sorted descending by charPos, so inserting at later
    // positions first preserves the offsets of earlier cursors
    g_multiCursor.sortCursors();

    int totalDelta = 0;  // Running offset adjustment for each cursor

    // Send a single undo-batch begin
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, 0);
    // We'll do explicit EM_REPLACESEL for each cursor position

    for (int ci = 0; ci < g_multiCursor.count(); ci++) {
        auto& c = g_multiCursor.cursors[ci];

        // If cursor has selection, replace it; otherwise insert at point
        CHARRANGE sel;
        if (c.anchorPos >= 0 && c.anchorPos != c.charPos) {
            sel.cpMin = std::min(c.anchorPos, c.charPos);
            sel.cpMax = std::max(c.anchorPos, c.charPos);
        } else {
            sel.cpMin = sel.cpMax = c.charPos;
        }

        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);
        SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());

        int oldLen = sel.cpMax - sel.cpMin;
        int newLen = (int)text.size();
        int delta  = newLen - oldLen;

        // Update this cursor's position
        c.charPos   = sel.cpMin + newLen;
        c.anchorPos = -1;

        // Adjust all earlier cursors (lower positions) by the delta
        for (int j = ci + 1; j < g_multiCursor.count(); j++) {
            // Since sorted descending, j > ci means LOWER positions
            // But wait — sorted descending means cursors[0] has highest charPos
            // ci = 0 is the HIGHEST position, so ci+1..end are lower
            // Inserting at higher positions doesn't affect lower positions
            // So we actually need to do nothing for descending order!
        }
    }

    // Revalidate positions — re-sort
    g_multiCursor.sortCursors();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Delete one char at all cursor positions (Backspace) ─────────────
void Win32IDE::multiCursorBackspace() {
    if (!m_hwndEditor || !g_multiCursor.active) return;

    g_multiCursor.sortCursors();

    for (int ci = 0; ci < g_multiCursor.count(); ci++) {
        auto& c = g_multiCursor.cursors[ci];

        CHARRANGE sel;
        if (c.anchorPos >= 0 && c.anchorPos != c.charPos) {
            // Has selection — delete selected text
            sel.cpMin = std::min(c.anchorPos, c.charPos);
            sel.cpMax = std::max(c.anchorPos, c.charPos);
        } else if (c.charPos > 0) {
            // Delete one char before cursor
            sel.cpMin = c.charPos - 1;
            sel.cpMax = c.charPos;
        } else {
            continue;
        }

        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);
        SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)"");

        c.charPos   = sel.cpMin;
        c.anchorPos = -1;
    }

    g_multiCursor.sortCursors();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Delete character to the right (Delete key) ─────────────────────
void Win32IDE::multiCursorDelete() {
    if (!m_hwndEditor || !g_multiCursor.active) return;

    g_multiCursor.sortCursors();

    for (int ci = 0; ci < g_multiCursor.count(); ci++) {
        auto& c = g_multiCursor.cursors[ci];

        CHARRANGE sel;
        if (c.anchorPos >= 0 && c.anchorPos != c.charPos) {
            sel.cpMin = std::min(c.anchorPos, c.charPos);
            sel.cpMax = std::max(c.anchorPos, c.charPos);
        } else {
            sel.cpMin = c.charPos;
            sel.cpMax = c.charPos + 1;
        }

        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);
        SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)"");

        c.charPos   = sel.cpMin;
        c.anchorPos = -1;
    }

    g_multiCursor.sortCursors();
    if (m_hwndEditor) InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ─── Paint multi-cursor indicators ──────────────────────────────────
// Called from editor's WM_PAINT handler to draw additional carets
void Win32IDE::paintMultiCursorIndicators(HDC hdc) {
    if (!m_hwndEditor || !g_multiCursor.active) return;

    HBRUSH caretBrush = CreateSolidBrush(RGB(220, 220, 170));   // Yellow caret
    HBRUSH selBrush   = CreateSolidBrush(RGB(38, 79, 120));     // VS Code selection blue (translucent feel)

    for (auto& c : g_multiCursor.cursors) {
        if (c.primary) continue;  // Primary is drawn by RichEdit itself

        // Get the screen position of this cursor via EM_POSFROMCHAR
        POINTL pt;
        SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, c.charPos);

        // Get font metrics for caret height
        int lineIdx = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, c.charPos, 0);
        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);

        // Estimate line height from font
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        int lineHeight = tm.tmHeight + tm.tmExternalLeading;

        // Draw selection highlight if cursor has anchor
        if (c.anchorPos >= 0 && c.anchorPos != c.charPos) {
            int selStart = std::min(c.anchorPos, c.charPos);
            int selEnd   = std::max(c.anchorPos, c.charPos);

            POINTL selStartPt, selEndPt;
            SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&selStartPt, selStart);
            SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&selEndPt, selEnd);

            // Simple single-line selection rectangle
            if (selStartPt.y == selEndPt.y) {
                RECT selRect;
                selRect.left   = selStartPt.x;
                selRect.top    = selStartPt.y;
                selRect.right  = selEndPt.x;
                selRect.bottom = selStartPt.y + lineHeight;
                FillRect(hdc, &selRect, selBrush);
            }
        }

        // Draw caret (2px wide bar)
        RECT caretRect;
        caretRect.left   = pt.x;
        caretRect.top    = pt.y;
        caretRect.right  = pt.x + 2;
        caretRect.bottom = pt.y + lineHeight;
        FillRect(hdc, &caretRect, caretBrush);
    }

    DeleteObject(caretBrush);
    DeleteObject(selBrush);
}

// ─── Handle keyboard input for multi-cursor mode ────────────────────
// Returns true if the key was handled (caller should not process further)
bool Win32IDE::handleMultiCursorKeyDown(WPARAM vk, bool ctrl, bool shift, bool alt) {
    // Esc — cancel multi-cursor
    if (vk == VK_ESCAPE && g_multiCursor.active) {
        clearSecondaryCursors();
        return true;
    }

    // Ctrl+Alt+Up — add cursor above
    if (vk == VK_UP && ctrl && alt) {
        addCursorAbove();
        return true;
    }

    // Ctrl+Alt+Down — add cursor below
    if (vk == VK_DOWN && ctrl && alt) {
        addCursorBelow();
        return true;
    }

    // Ctrl+D — select next occurrence
    if (vk == 'D' && ctrl && !shift && !alt) {
        selectNextOccurrence();
        return true;
    }

    // Ctrl+Shift+L — select all occurrences
    if (vk == 'L' && ctrl && shift && !alt) {
        selectAllOccurrences();
        return true;
    }

    // If multi-cursor is active, intercept typing keys
    if (!g_multiCursor.active) return false;

    // Backspace
    if (vk == VK_BACK) {
        multiCursorBackspace();
        return true;
    }

    // Delete
    if (vk == VK_DELETE) {
        multiCursorDelete();
        return true;
    }

    // Enter
    if (vk == VK_RETURN) {
        multiCursorInsertText("\r\n");
        return true;
    }

    // Tab
    if (vk == VK_TAB && !ctrl && !alt) {
        multiCursorInsertText("\t");
        return true;
    }

    return false;
}

// ─── Handle WM_CHAR for multi-cursor synchronized typing ────────────
bool Win32IDE::handleMultiCursorChar(WPARAM ch) {
    if (!g_multiCursor.active) return false;

    // Printable ASCII characters
    if (ch >= 32 && ch < 127) {
        std::string text(1, (char)ch);
        multiCursorInsertText(text);
        return true;
    }

    return false;
}

// ─── Handle Alt+Click from editor mouse handler ─────────────────────
bool Win32IDE::handleMultiCursorClick(int charPos, bool altHeld) {
    if (!altHeld) {
        // Regular click — if multi-cursor is active, end it
        if (g_multiCursor.active) {
            clearSecondaryCursors();
        }
        return false;
    }

    // Alt+Click — add/toggle cursor at click position
    addCursorAtPosition(charPos);
    return true;
}

// ─── Update status bar with cursor count ─────────────────────────────
void Win32IDE::updateMultiCursorStatusBar() {
    if (!m_hwndStatusBar) return;

    if (g_multiCursor.active && g_multiCursor.count() > 1) {
        std::string status = std::to_string(g_multiCursor.count()) + " cursors";
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 3, (LPARAM)status.c_str());
    } else {
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 3, (LPARAM)"");
    }
}

// ─── Get the number of active cursors ────────────────────────────────
int Win32IDE::getMultiCursorCount() const {
    return g_multiCursor.count();
}
