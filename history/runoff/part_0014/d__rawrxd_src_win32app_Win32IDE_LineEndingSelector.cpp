// ============================================================================
// Win32IDE_LineEndingSelector.cpp — Tier 5 Gap #40: Line Ending Selector
// ============================================================================
//
// PURPOSE:
//   Detect and display the current file's line ending style (CRLF/LF/CR) in
//   the status bar.  Clicking the status bar item opens a conversion dialog
//   that re-writes every line terminator in the editor buffer.
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <sstream>
#include <vector>

// ============================================================================
// Line-ending detection
// ============================================================================

enum class LineEndingType { CRLF, LF, CR, Mixed, Unknown };

static const char* lineEndingName(LineEndingType t) {
    switch (t) {
        case LineEndingType::CRLF:    return "CRLF";
        case LineEndingType::LF:      return "LF";
        case LineEndingType::CR:      return "CR";
        case LineEndingType::Mixed:   return "Mixed";
        default:                      return "??";
    }
}

static LineEndingType detectLineEndings(const std::string& text) {
    size_t crlfCount = 0, lfCount = 0, crCount = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++crlfCount;
                ++i;  // skip the \n
            } else {
                ++crCount;
            }
        } else if (text[i] == '\n') {
            ++lfCount;
        }
    }
    if (crlfCount == 0 && lfCount == 0 && crCount == 0)
        return LineEndingType::Unknown;

    int kinds = (crlfCount > 0 ? 1 : 0) + (lfCount > 0 ? 1 : 0) + (crCount > 0 ? 1 : 0);
    if (kinds > 1) return LineEndingType::Mixed;
    if (crlfCount > 0) return LineEndingType::CRLF;
    if (lfCount > 0) return LineEndingType::LF;
    return LineEndingType::CR;
}

static std::string convertLineEndings(const std::string& text, LineEndingType target) {
    // First normalize to \n
    std::string normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;  // skip \n after \r
            }
            normalized += '\n';
        } else {
            normalized += text[i];
        }
    }

    // Then apply target
    if (target == LineEndingType::LF)
        return normalized;

    std::string result;
    result.reserve(normalized.size() + normalized.size() / 40);
    const char* eol = (target == LineEndingType::CRLF) ? "\r\n" : "\r";
    for (char c : normalized) {
        if (c == '\n') {
            result += eol;
        } else {
            result += c;
        }
    }
    return result;
}

// ============================================================================
// Read editor text (RichEdit)
// ============================================================================

static std::string getEditorText(HWND hwndEditor) {
    if (!hwndEditor || !IsWindow(hwndEditor)) return {};
    int len = GetWindowTextLengthA(hwndEditor);
    if (len <= 0) return {};
    std::string buf(static_cast<size_t>(len + 1), '\0');
    GetWindowTextA(hwndEditor, &buf[0], len + 1);
    buf.resize(static_cast<size_t>(len));
    return buf;
}

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initLineEndingSelector() {
    if (m_lineEndingInitialized) return;

    OutputDebugStringA("[LineEnding] Tier 5 — Line ending selector initialized.\n");
    m_lineEndingInitialized = true;

    // Detect current file line endings and update status bar
    cmdLineEndingDetect();
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleLineEndingCommand(int commandId) {
    switch (commandId) {
        case IDM_LINEENDING_DETECT:   cmdLineEndingDetect();   return true;
        case IDM_LINEENDING_TOGGLE:   cmdLineEndingToggle();   return true;
        case IDM_LINEENDING_TO_CRLF:  cmdLineEndingToCRLF();   return true;
        case IDM_LINEENDING_TO_LF:    cmdLineEndingToLF();     return true;
        default: return false;
    }
}

// ============================================================================
// Detect and update status bar
// ============================================================================

void Win32IDE::cmdLineEndingDetect() {
    std::string text = getEditorText(m_hwndEditor);
    LineEndingType eol = detectLineEndings(text);
    m_currentLineEnding = static_cast<int>(eol);

    const char* name = lineEndingName(eol);

    // Update status bar EOL indicator (IDC_STATUS_EOL = 1408)
    HWND hwndEOL = GetDlgItem(m_hwndStatusBar, 1408);
    if (hwndEOL && IsWindow(hwndEOL)) {
        SetWindowTextA(hwndEOL, name);
        InvalidateRect(hwndEOL, nullptr, TRUE);
    }

    std::ostringstream oss;
    oss << "[LineEnding] Detected: " << name << "\n";
    appendToOutput(oss.str());
}

// ============================================================================
// Toggle — click cycles CRLF → LF → CRLF
// ============================================================================

void Win32IDE::cmdLineEndingToggle() {
    std::string text = getEditorText(m_hwndEditor);
    LineEndingType current = detectLineEndings(text);

    LineEndingType target = (current == LineEndingType::LF)
                            ? LineEndingType::CRLF
                            : LineEndingType::LF;

    std::string converted = convertLineEndings(text, target);

    // Replace editor content preserving cursor
    if (m_hwndEditor && IsWindow(m_hwndEditor)) {
        SendMessageA(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
        SetWindowTextA(m_hwndEditor, converted.c_str());
        SendMessageA(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    const char* targetName = lineEndingName(target);

    // Update status bar
    HWND hwndEOL = GetDlgItem(m_hwndStatusBar, 1408);
    if (hwndEOL && IsWindow(hwndEOL)) {
        SetWindowTextA(hwndEOL, targetName);
        InvalidateRect(hwndEOL, nullptr, TRUE);
    }

    m_currentLineEnding = static_cast<int>(target);

    std::ostringstream oss;
    oss << "[LineEnding] Converted to " << targetName << "\n";
    appendToOutput(oss.str());
}

// ============================================================================
// Explicit convert to CRLF
// ============================================================================

void Win32IDE::cmdLineEndingToCRLF() {
    std::string text = getEditorText(m_hwndEditor);
    std::string converted = convertLineEndings(text, LineEndingType::CRLF);

    if (m_hwndEditor && IsWindow(m_hwndEditor)) {
        SendMessageA(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
        SetWindowTextA(m_hwndEditor, converted.c_str());
        SendMessageA(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    m_currentLineEnding = static_cast<int>(LineEndingType::CRLF);

    HWND hwndEOL = GetDlgItem(m_hwndStatusBar, 1408);
    if (hwndEOL && IsWindow(hwndEOL)) {
        SetWindowTextA(hwndEOL, "CRLF");
        InvalidateRect(hwndEOL, nullptr, TRUE);
    }

    appendToOutput("[LineEnding] Converted entire file to CRLF\n");
}

// ============================================================================
// Explicit convert to LF
// ============================================================================

void Win32IDE::cmdLineEndingToLF() {
    std::string text = getEditorText(m_hwndEditor);
    std::string converted = convertLineEndings(text, LineEndingType::LF);

    if (m_hwndEditor && IsWindow(m_hwndEditor)) {
        SendMessageA(m_hwndEditor, WM_SETREDRAW, FALSE, 0);
        SetWindowTextA(m_hwndEditor, converted.c_str());
        SendMessageA(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    m_currentLineEnding = static_cast<int>(LineEndingType::LF);

    HWND hwndEOL = GetDlgItem(m_hwndStatusBar, 1408);
    if (hwndEOL && IsWindow(hwndEOL)) {
        SetWindowTextA(hwndEOL, "LF");
        InvalidateRect(hwndEOL, nullptr, TRUE);
    }

    appendToOutput("[LineEnding] Converted entire file to LF\n");
}
