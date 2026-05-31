// ============================================================================
// Win32IDE_StatusBarActions.cpp — Interactive Status Bar Segment Handlers
// ============================================================================
// Extends the status bar from a passive display to an interactive VS Code-like
// experience.  Each clickable segment triggers a popup or action:
//
//   Part 6  (Ln:Col)     → opens Go To Line dialog
//   Part 7  (Spaces/Tab) → toggle spaces ↔ tabs + tab-size cycle (2→4→8)
//   Part 9  (EOL)        → cycle CRLF → LF → CR
//   Part 11 (Backend)    → quick backend-switch (rotates available backends)
//
// Also provides live status updates: cursor movement → Part 6,
// selection count → Part 5 (spacer), file-modified indicator, etc.
//
// Architecture:
//   - Wires into existing handleStatusBarClick(int partIndex)
//   - Tab size / EOL / indentation state stored in m_statusBarInfo
//   - Each handler is a small focused function
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <string>

// ============================================================================
// handleStatusBarAction — extended click dispatcher
// ============================================================================
// Called from handleStatusBarClick() for parts not yet handled.
// Returns true if the part was handled, false otherwise.

bool Win32IDE::handleStatusBarAction(int partIndex)
{
    switch (partIndex) {

    // ── Part 6: Ln:Col → Go To Line (Ctrl+G equivalent) ────────────────────
    case 6:
        showGoToLineDialog();
        return true;

    // ── Part 7: Spaces/Tab → cycle indent mode ─────────────────────────────
    case 7:
    {
        if (m_statusBarInfo.useSpaces) {
            // Cycle tab width: 2 → 4 → 8 → then switch to Tabs
            if (m_statusBarInfo.spacesOrTabWidth == 2)
                m_statusBarInfo.spacesOrTabWidth = 4;
            else if (m_statusBarInfo.spacesOrTabWidth == 4)
                m_statusBarInfo.spacesOrTabWidth = 8;
            else {
                // Switch to tabs mode, start at 4
                m_statusBarInfo.useSpaces = false;
                m_statusBarInfo.spacesOrTabWidth = 4;
            }
        } else {
            // Switch back to spaces, start at 2
            m_statusBarInfo.useSpaces = true;
            m_statusBarInfo.spacesOrTabWidth = 2;
        }
        updateEnhancedStatusBar();
        return true;
    }

    // ── Part 9: EOL → cycle CRLF / LF / CR ─────────────────────────────────
    case 9:
    {
        if (m_statusBarInfo.eolSequence == "CRLF")
            m_statusBarInfo.eolSequence = "LF";
        else if (m_statusBarInfo.eolSequence == "LF")
            m_statusBarInfo.eolSequence = "CR";
        else
            m_statusBarInfo.eolSequence = "CRLF";
        updateEnhancedStatusBar();
        return true;
    }

    // ── Part 3: Errors → open Problems Panel ────────────────────────────────
    case 3:
    case 4:
    {
        // Toggle problems panel visibility
        if (m_hwndProblemsListView && IsWindowVisible(m_hwndProblemsListView))
            ShowWindow(m_hwndProblemsListView, SW_HIDE);
        else if (m_hwndProblemsListView)
            ShowWindow(m_hwndProblemsListView, SW_SHOW);
        return true;
    }

    default:
        return false;
    }
}

// ============================================================================
// updateStatusBarCursorPosition — call on EN_SELCHANGE or caret movement
// ============================================================================
void Win32IDE::updateStatusBarCursorPosition()
{
    if (!m_hwndEditor || !m_hwndStatusBar) return;

    // Get current caret position
    DWORD selStart = 0, selEnd = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    // Convert char offset to line number
    int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selStart, 0) + 1;
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line - 1, 0);
    int col = (int)(selStart - lineStart) + 1;

    m_statusBarInfo.line = line;
    m_statusBarInfo.column = col;
    m_currentLine = line;

    // Update part 6 directly for speed (avoid full updateEnhancedStatusBar)
    std::ostringstream oss;
    oss << "Ln " << line << ", Col " << col;

    // If there's a selection, show count
    if (selEnd > selStart) {
        int selLen = (int)(selEnd - selStart);
        // Count lines in selection
        int startLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selStart, 0);
        int endLine   = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selEnd, 0);
        int selLines  = endLine - startLine;
        if (selLines > 0)
            oss << " (" << selLen << " chars, " << selLines << " lines)";
        else
            oss << " (" << selLen << " selected)";
    }

    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 6, (LPARAM)oss.str().c_str());
}

// ============================================================================
// updateStatusBarFileInfo — update after file open/switch
// ============================================================================
void Win32IDE::updateStatusBarFileInfo()
{
    if (!m_hwndStatusBar) return;

    // Detect language from file extension
    if (!m_currentFile.empty()) {
        std::string ext;
        auto dot = m_currentFile.rfind('.');
        if (dot != std::string::npos)
            ext = m_currentFile.substr(dot);

        // Map extension to language name
        std::string lang = "Plain Text";
        if (ext == ".cpp" || ext == ".cxx" || ext == ".cc")  lang = "C++";
        else if (ext == ".c")    lang = "C";
        else if (ext == ".h" || ext == ".hpp" || ext == ".hxx") lang = "C/C++ Header";
        else if (ext == ".py")   lang = "Python";
        else if (ext == ".js")   lang = "JavaScript";
        else if (ext == ".ts")   lang = "TypeScript";
        else if (ext == ".json") lang = "JSON";
        else if (ext == ".xml")  lang = "XML";
        else if (ext == ".html" || ext == ".htm") lang = "HTML";
        else if (ext == ".css")  lang = "CSS";
        else if (ext == ".asm" || ext == ".masm") lang = "x86 Assembly";
        else if (ext == ".rs")   lang = "Rust";
        else if (ext == ".go")   lang = "Go";
        else if (ext == ".java") lang = "Java";
        else if (ext == ".cs")   lang = "C#";
        else if (ext == ".md")   lang = "Markdown";
        else if (ext == ".cmake") lang = "CMake";
        else if (ext == ".ps1")  lang = "PowerShell";
        else if (ext == ".sh" || ext == ".bash") lang = "Shell Script";

        m_statusBarInfo.languageMode = lang;
    }

    updateEnhancedStatusBar();
}
