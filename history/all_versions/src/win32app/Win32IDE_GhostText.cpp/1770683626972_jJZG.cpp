// ============================================================================
// Win32IDE_GhostText.cpp — Ghost Text / Inline Completion Renderer
// ============================================================================
// Provides GitHub Copilot-style inline code completions:
//   - Grayed-out "ghost text" rendered after the cursor position
//   - Tab to accept, Esc to dismiss
//   - Debounced trigger on typing pause (500ms)
//   - Integration with CompletionServer (local GGUF) or Ollama
//   - Multi-line ghost text support
//   - Cursor movement auto-dismisses
//
// Architecture:
//   - EN_CHANGE fires debounce timer (GHOST_TEXT_TIMER_ID)
//   - Timer fires → background thread requests completion
//   - WM_APP+400 delivers completion to UI thread
//   - Custom paint via editor subclass intercepts WM_PAINT
//   - Ghost text rendered in italic gray after the cursor position
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>

// ============================================================================
// CONSTANTS
// ============================================================================
static const UINT_PTR GHOST_TEXT_TIMER_ID   = 8888;
static const UINT      GHOST_TEXT_DELAY_MS  = 500;   // Debounce: 500ms after last keystroke
static const int       GHOST_TEXT_MAX_CHARS = 512;    // Max ghost text length
static const int       GHOST_TEXT_MAX_LINES = 8;      // Max multi-line completions

// ============================================================================
// INITIALIZATION
// ============================================================================

void Win32IDE::initGhostText() {
    m_ghostTextEnabled        = true;
    m_ghostTextVisible        = false;
    m_ghostTextAccepted       = false;
    m_ghostTextPending        = false;
    m_ghostTextContent.clear();
    m_ghostTextLine           = -1;
    m_ghostTextColumn         = -1;
    m_ghostTextFont           = nullptr;

    // Create ghost text font — italic version of editor font
    LOGFONTA lf = {};
    lf.lfHeight         = -14;    // ~10.5pt
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = TRUE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    LOG_INFO("Ghost text renderer initialized (debounce=" +
             std::to_string(GHOST_TEXT_DELAY_MS) + "ms)");
}

void Win32IDE::shutdownGhostText() {
    dismissGhostText();
    if (m_ghostTextFont) {
        DeleteObject(m_ghostTextFont);
        m_ghostTextFont = nullptr;
    }
}

// ============================================================================
// DEBOUNCE TRIGGER — called from onEditorContentChanged()
// ============================================================================

void Win32IDE::triggerGhostTextCompletion() {
    if (!m_ghostTextEnabled || !m_hwndEditor) return;

    // Kill any existing ghost text timer and dismiss current ghost text
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);
    dismissGhostText();

    // Start a new debounce timer
    SetTimer(m_hwndMain, GHOST_TEXT_TIMER_ID, GHOST_TEXT_DELAY_MS, nullptr);
}

// ============================================================================
// TIMER CALLBACK — fires after debounce period, requests completion
// ============================================================================

void Win32IDE::onGhostTextTimer() {
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);

    if (!m_ghostTextEnabled || !m_hwndEditor) return;
    if (m_ghostTextPending) return;  // Already requesting

    // Gather context: text before cursor
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int cursorPos = sel.cpMin;

    if (cursorPos <= 0) return;

    // Get up to 4KB of text before cursor for context
    int contextStart = (cursorPos > 4096) ? cursorPos - 4096 : 0;
    int contextLen = cursorPos - contextStart;

    TEXTRANGEA tr;
    tr.chrg.cpMin = contextStart;
    tr.chrg.cpMax = cursorPos;

    std::vector<char> buf(contextLen + 1, 0);
    tr.lpstrText = buf.data();
    SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

    std::string context(buf.data());
    if (context.empty()) return;

    // Get current line/column for positioning
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cursorPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = cursorPos - lineStart;

    // Store cursor position for ghost text rendering
    m_ghostTextLine   = lineIndex;
    m_ghostTextColumn = column;
    m_ghostTextPending = true;

    // Detect language for context
    std::string language = getSyntaxLanguageName();

    // Fire background thread for completion
    std::string contextCopy = context;
    std::string langCopy = language;
    int cursorCopy = cursorPos;

    std::thread([this, contextCopy, langCopy, cursorCopy]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        std::string completion = requestGhostTextCompletion(contextCopy, langCopy);

        // Post result to UI thread
        if (isShuttingDown()) return;
        if (!completion.empty()) {
            char* heapText = _strdup(completion.c_str());
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)heapText);
        } else {
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)nullptr);
        }
    }).detach();
}

// ============================================================================
// COMPLETION REQUEST — runs on background thread
// ============================================================================

std::string Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                   const std::string& language) {
    // Strategy 1: Use native engine if model is loaded
    if (m_nativeEngine && m_nativeEngine->IsModelLoaded()) {
        auto tokens = m_nativeEngine->Tokenize(
            "Complete the following " + language + " code. Output ONLY the completion, "
            "no explanation, no markdown:\n\n" + context);

        auto generated = m_nativeEngine->Generate(tokens, 64);  // Short completions
        std::string result = m_nativeEngine->Detokenize(generated);

        // Trim to reasonable length and line count
        result = trimGhostText(result);
        return result;
    }

    // Strategy 2: Try Ollama if connected
    if (!m_ollamaBaseUrl.empty()) {
        std::string response;
        // Build a minimal completion prompt
        std::string prompt = "Complete the following " + language + " code. "
                             "Output ONLY the completion, no explanation, no markdown. "
                             "Maximum 3 lines:\n\n" + context;

        // Try Ollama — reuse existing trySendToOllama
        if (trySendToOllama(prompt, response)) {
            return trimGhostText(response);
        }
    }

    return "";  // No completion source available
}

// ============================================================================
// GHOST TEXT DELIVERY — WM_GHOST_TEXT_READY handler (UI thread)
// ============================================================================

void Win32IDE::onGhostTextReady(int requestedCursorPos, const char* completionText) {
    m_ghostTextPending = false;

    if (!completionText || !m_hwndEditor) return;

    // Verify cursor hasn't moved since request
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin != requestedCursorPos) {
        // Cursor moved — discard stale completion
        return;
    }

    m_ghostTextContent = completionText;
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;

    // Record event
    recordEvent(AgentEventType::GhostTextRequested, "",
                m_ghostTextContent.substr(0, 128), "", 0, true);

    // Force editor repaint to show ghost text
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

// ============================================================================
// DISMISS — clears ghost text
// ============================================================================

void Win32IDE::dismissGhostText() {
    if (!m_ghostTextVisible) return;

    m_ghostTextVisible  = false;
    m_ghostTextContent.clear();
    m_ghostTextLine     = -1;
    m_ghostTextColumn   = -1;
    m_ghostTextAccepted = false;

    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
}

// ============================================================================
// ACCEPT — Tab key inserts the ghost text into the editor
// ============================================================================

void Win32IDE::acceptGhostText() {
    if (!m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor) return;

    std::string textToInsert = m_ghostTextContent;
    m_ghostTextAccepted = true;

    // Dismiss first to avoid re-rendering ghost during insert
    dismissGhostText();

    // Insert text at current cursor position
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)textToInsert.c_str());

    // Record acceptance event
    recordEvent(AgentEventType::GhostTextAccepted, "",
                textToInsert.substr(0, 128), "", 0, true);

    LOG_INFO("Ghost text accepted (" + std::to_string(textToInsert.size()) + " chars)");
}

// ============================================================================
// RENDER — paints ghost text onto the editor surface
// ============================================================================

void Win32IDE::renderGhostText(HDC hdc) {
    if (!m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor) return;

    // Get current cursor position to know where to draw
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Get the pixel position of the cursor
    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, sel.cpMin);

    if (pt.x < 0 || pt.y < 0) return;  // Cursor not visible

    // Get the text after cursor on the same line to know rendering offset
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen   = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);
    int lineEnd   = lineStart + lineLen;

    // If cursor is not at end of line, render after end of line text
    // (ghost text appears after existing text)
    POINTL endPt;
    if (lineEnd > sel.cpMin) {
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&endPt, lineEnd);
    } else {
        endPt = pt;
    }

    // Setup ghost text rendering
    HFONT oldFont = (HFONT)SelectObject(hdc, m_ghostTextFont ? m_ghostTextFont : GetStockObject(SYSTEM_FONT));
    
    // Ghost text color: muted/grayed version of text color
    COLORREF ghostColor = RGB(
        (GetRValue(m_currentTheme.textColor) + GetRValue(m_currentTheme.backgroundColor)) / 2,
        (GetGValue(m_currentTheme.textColor) + GetGValue(m_currentTheme.backgroundColor)) / 2,
        (GetBValue(m_currentTheme.textColor) + GetBValue(m_currentTheme.backgroundColor)) / 2
    );
    SetTextColor(hdc, ghostColor);
    SetBkMode(hdc, TRANSPARENT);

    // Split ghost text into lines
    std::vector<std::string> lines;
    std::istringstream stream(m_ghostTextContent);
    std::string line;
    int lineCount = 0;
    while (std::getline(stream, line) && lineCount < GHOST_TEXT_MAX_LINES) {
        lines.push_back(line);
        lineCount++;
    }

    if (lines.empty()) {
        SelectObject(hdc, oldFont);
        return;
    }

    // Get line height
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    // Draw first line at cursor/end-of-line position
    int drawX = endPt.x + 2;  // Small gap after existing text
    int drawY = endPt.y;

    // Get editor client rect for clipping
    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);

    for (size_t i = 0; i < lines.size(); i++) {
        if (drawY + lineHeight > editorRC.bottom) break;  // Don't draw below editor

        if (i == 0) {
            // First line: render after cursor
            TextOutA(hdc, drawX, drawY, lines[i].c_str(), (int)lines[i].size());
        } else {
            // Subsequent lines: render at left margin (indented to match cursor column)
            // Get x position of the start of the line (respect indentation)
            POINTL lineStartPt;
            SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&lineStartPt, lineStart);
            int indentX = lineStartPt.x;

            drawY += lineHeight;
            TextOutA(hdc, indentX, drawY, lines[i].c_str(), (int)lines[i].size());
        }
    }

    SelectObject(hdc, oldFont);
}

// ============================================================================
// KEY HANDLER — intercepts Tab/Esc in editor subclass
// ============================================================================

bool Win32IDE::handleGhostTextKey(UINT vk) {
    if (!m_ghostTextVisible) return false;

    if (vk == VK_TAB) {
        acceptGhostText();
        return true;  // Consumed
    }

    if (vk == VK_ESCAPE) {
        dismissGhostText();
        return true;  // Consumed
    }

    // Any other key dismisses ghost text (typing, arrow keys, etc.)
    // Exception: don't dismiss on Shift/Ctrl/Alt alone
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU) {
        dismissGhostText();
    }

    return false;  // Not consumed — pass to editor
}

// ============================================================================
// TOGGLE — enable/disable ghost text
// ============================================================================

void Win32IDE::toggleGhostText() {
    m_ghostTextEnabled = !m_ghostTextEnabled;
    if (!m_ghostTextEnabled) {
        dismissGhostText();
    }
    appendToOutput(std::string("[Ghost Text] ") +
                   (m_ghostTextEnabled ? "Enabled" : "Disabled"),
                   "General", OutputSeverity::Info);
}

// ============================================================================
// TRIM HELPER — truncates completion to reasonable size
// ============================================================================

std::string Win32IDE::trimGhostText(const std::string& raw) {
    std::string result;
    int lineCount = 0;
    int charCount = 0;

    for (char c : raw) {
        if (charCount >= GHOST_TEXT_MAX_CHARS) break;
        if (c == '\n') {
            lineCount++;
            if (lineCount >= GHOST_TEXT_MAX_LINES) break;
        }
        result += c;
        charCount++;
    }

    // Strip trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}
