#include "native_editor.h"
#include <commctrl.h>
#include <richedit.h>
#include <algorithm>

namespace RawrXD {

NativeEditor::NativeEditor(HWND h) : Editor(nullptr) {
    if (h) {
        // If wrapping existing HWND, set it.
        // Assuming Window base allows setting hwnd directly or we just wrap it.
        // For now, we prefer creating our own managed window.
        // But if provided, we can't easily hijack the WndProc without subclassing.
        this->hwnd = h;
    }
    
    m_hBackgroundBrush = CreateSolidBrush(RGB(30,30,30));
    
    // Status bar creation needs a window handle, so defer until create/init
    
    m_lines.push_back(EditorLine());
    m_lines[0].text = "";
    
    setFont("Consolas", 10);
}

NativeEditor::~NativeEditor() {
    if (m_hBackgroundBrush) DeleteObject(m_hBackgroundBrush);
    // Font deletion handled by Window/GDI logic usually, but here:
    // if (m_hFont) DeleteObject(m_hFont); 
}

void NativeEditor::run() {
    if (!hwnd) {
        // Ensure window exists
        this->create(nullptr, "RawrXD v3.0");
    }
    
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    
    // Simple Message Loop if not managed by Application class
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void NativeEditor::create(HWND hParent, const RECT& rect) {
    // Adapter to match Window::create somewhat or just use custom logic
    // We'll call the base Window::create
    // Note: Window::create signature might differ, assuming typical Windows framework
    // Window::create(Window* parent, const String& title, ...)
    
    // We ignore rect for new window creation in this simplified adapter
    // Window::create(nullptr, "RawrXD v3.0");
}

void NativeEditor::paintEvent(PAINTSTRUCT& ps) {
    render(ps.hdc);
}

void NativeEditor::resizeEvent(int w, int h) {
    if (m_hWndStatusBar) {
        SendMessage(m_hWndStatusBar, WM_SIZE, 0, 0);
    }
}

// Map custom render to paintEvent
void NativeEditor::render(HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    FillRect(hdc, &rect, m_hBackgroundBrush);
    
    // Draw text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(220, 220, 220));
    
    if (m_hFont) SelectObject(hdc, m_hFont);
    
    int y = 0;
    int lineHeight = 16; // Hardcoded or calculated from font
    
    for (const auto& line : m_lines) {
        TextOutA(hdc, 0, y, line.text.c_str(), line.text.length());
        y += lineHeight;
    }
}

void NativeEditor::insertText(const std::string& text, int line, int column) {
    // Basic implementations
    if (line >= 0 && line < m_lines.size()) {
        m_lines[line].text.insert(column, text);
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

// ... other implementations ...
// We need to implement the other methods (onKey, onChar) to link to standard Window events

} // namespace RawrXD

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void NativeEditor::render(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    FillRect(hdc, &clientRect, m_hBackgroundBrush);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);
    
    if (m_showLineNumbers) {
        renderLineNumbers(hdc);
    }
    
    int visibleLines = clientRect.bottom / m_charHeight;
    int startLine = m_scrollOffset;
    int endLine = std::min(startLine + visibleLines, static_cast<int>(m_lines.size()));
    
    for (int i = startLine; i < endLine; ++i) {
        int yPos = (i - startLine) * m_charHeight;
        renderLine(hdc, i, yPos);
    }
    
    renderDiagnostics(hdc);
    renderSelection(hdc);
    
    SelectObject(hdc, oldFont);
}

void NativeEditor::renderLine(HDC hdc, int lineIndex, int yPos) {
    if (lineIndex >= m_lines.size()) return;
    auto& line = m_lines[lineIndex];
    tokenizeLine(lineIndex);
    
    int xPos = m_showLineNumbers ? m_lineNumberWidth : 0;
    
    for (size_t i = 0; i < line.renderedChars.size(); ++i) {
         const auto& charInfo = line.renderedChars[i];
         COLORREF textColor = RGB(200, 200, 200);
         switch (charInfo.Attributes) {
             case 1: textColor = RGB(86, 156, 214); break;
             case 2: textColor = RGB(206, 145, 120); break;
             case 3: textColor = RGB(96, 139, 78); break;
             case 4: textColor = RGB(181, 206, 168); break;
         }
         SetTextColor(hdc, textColor);
         SetBkMode(hdc, TRANSPARENT);
         TextOut(hdc, xPos, yPos, &charInfo.Char.AsciiChar, 1);
         xPos += m_charWidth;
    }
}

void NativeEditor::tokenizeLine(int lineIndex) {
    if (lineIndex >= m_lines.size()) return;
    auto& line = m_lines[lineIndex];
    line.renderedChars.clear();
    std::string text = line.text;
    size_t pos = 0;
    
    while (pos < text.length()) {
        CHAR_INFO charInfo;
        charInfo.Char.AsciiChar = text[pos];
        charInfo.Attributes = 0;
        
        if (isalpha(text[pos])) {
             size_t start = pos;
             while (pos < text.length() && (isalnum(text[pos]) || text[pos] == '_')) pos++;
             std::string token = text.substr(start, pos - start);
             int style = detectSyntaxStyle(token);
             for(size_t i=start; i<pos; ++i) {
                 CHAR_INFO c; c.Char.AsciiChar = text[i]; c.Attributes = style;
                 line.renderedChars.push_back(c);
             }
             continue; // pos already advanced
        }
        // ... simplified tokenization loop from prompt ... 
        line.renderedChars.push_back(charInfo);
        pos++;
    }
}

int NativeEditor::detectSyntaxStyle(const std::string& token) {
    static const std::unordered_set<std::string> keywords = {
        "int", "float", "if", "else", "return", "class"
    };
    if (keywords.count(token)) return 1;
    return 0;
}

void NativeEditor::onKeyDown(WPARAM wParam, LPARAM lParam) {
     switch (wParam) {
        case VK_RETURN: insertChar('\n'); break;
        case VK_BACK: deleteChar(); break;
        // ...
     }
}

void NativeEditor::onChar(WPARAM wParam, LPARAM lParam) {
    if (wParam >= 32) insertChar((char)wParam);
}

void NativeEditor::insertChar(char c) {
    if (m_cursorLine >= m_lines.size()) m_lines.resize(m_cursorLine + 1);
    
    if (c == '\n') {
        // split line
        std::string current = m_lines[m_cursorLine].text;
        std::string next = current.substr(m_cursorColumn);
        m_lines[m_cursorLine].text = current.substr(0, m_cursorColumn);
        
        EditorLine newLine;
        newLine.text = next;
        m_lines.insert(m_lines.begin() + m_cursorLine + 1, newLine);
        m_cursorLine++;
        m_cursorColumn = 0;
    } else {
        m_lines[m_cursorLine].text.insert(m_cursorColumn, 1, c);
        m_cursorColumn++;
    }
    m_isModified = true;
}

void NativeEditor::deleteChar() {
    // simplified
    if (m_cursorColumn > 0) {
        m_lines[m_cursorLine].text.erase(m_cursorColumn - 1, 1);
        m_cursorColumn--;
    }
}

void NativeEditor::renderLineNumbers(HDC hdc) {
    // simplified
}

void NativeEditor::renderDiagnostics(HDC hdc) {
    // Render diagnostic markers in gutter
    if (m_diagnostics.empty()) return;
    
    for (const auto& diag : m_diagnostics) {
        int y = diag.line * m_lineHeight;
        if (y >= 0 && y < m_height) {
            // Draw colored indicator based on severity
            COLORREF color = RGB(255, 200, 0); // Warning
            if (diag.severity == "error") color = RGB(255, 50, 50);
            else if (diag.severity == "info") color = RGB(50, 150, 255);
            
            HBRUSH brush = CreateSolidBrush(color);
            RECT markerRect = {2, y + 2, 8, y + m_lineHeight - 2};
            FillRect(hdc, &markerRect, brush);
            DeleteObject(brush);
        }
    }
}

void NativeEditor::renderSelection(HDC hdc) {
    // Highlight selected text region
    if (m_selectionStart.line == m_selectionEnd.line && 
        m_selectionStart.column == m_selectionEnd.column) return;
    
    HBRUSH selBrush = CreateSolidBrush(RGB(0, 100, 200));
    
    int startLine = std::min(m_selectionStart.line, m_selectionEnd.line);
    int endLine = std::max(m_selectionStart.line, m_selectionEnd.line);
    
    for (int line = startLine; line <= endLine; ++line) {
        int y = line * m_lineHeight;
        int startCol = (line == startLine) ? m_selectionStart.column : 0;
        int endCol = (line == endLine) ? m_selectionEnd.column : m_lines[line].length();
        
        int x1 = startCol * m_charWidth + m_marginLeft;
        int x2 = endCol * m_charWidth + m_marginLeft;
        
        RECT selRect = {x1, y, x2, y + m_lineHeight};
        FillRect(hdc, &selRect, selBrush);
    }
    
    DeleteObject(selBrush);
}

void NativeEditor::updateScrollBars() {
    // Update scrollbar ranges based on content size
    if (!m_hwnd) return;
    
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    
    // Vertical scrollbar
    si.nMin = 0;
    si.nMax = static_cast<int>(m_lines.size());
    si.nPage = m_visibleLines;
    si.nPos = m_scrollOffsetY;
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
    
    // Horizontal scrollbar
    int maxLineWidth = 0;
    for (const auto& line : m_lines) {
        maxLineWidth = std::max(maxLineWidth, static_cast<int>(line.length()));
    }
    si.nMax = maxLineWidth;
    si.nPage = m_visibleColumns;
    si.nPos = m_scrollOffsetX;
    SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
}

void NativeEditor::ensureCursorVisible() {
    // Scroll to keep cursor in view
    if (m_cursorLine < m_scrollOffsetY) {
        m_scrollOffsetY = m_cursorLine;
    } else if (m_cursorLine >= m_scrollOffsetY + m_visibleLines) {
        m_scrollOffsetY = m_cursorLine - m_visibleLines + 1;
    }
    
    if (m_cursorColumn < m_scrollOffsetX) {
        m_scrollOffsetX = m_cursorColumn;
    } else if (m_cursorColumn >= m_scrollOffsetX + m_visibleColumns) {
        m_scrollOffsetX = m_cursorColumn - m_visibleColumns + 1;
    }
    
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::insertText(const std::string& text, int line, int column) {
    if (line < 0 || line >= static_cast<int>(m_lines.size())) return;
    if (column < 0 || column > static_cast<int>(m_lines[line].length())) return;
    
    m_lines[line].insert(column, text);
    m_modified = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::deleteText(int sl, int sc, int el, int ec) {
    if (sl < 0 || sl >= static_cast<int>(m_lines.size())) return;
    if (el < 0 || el >= static_cast<int>(m_lines.size())) return;
    
    if (sl == el) {
        // Single line deletion
        m_lines[sl].erase(sc, ec - sc);
    } else {
        // Multi-line deletion
        std::string remaining = m_lines[sl].substr(0, sc) + m_lines[el].substr(ec);
        m_lines.erase(m_lines.begin() + sl, m_lines.begin() + el + 1);
        m_lines.insert(m_lines.begin() + sl, remaining);
    }
    m_modified = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::replaceText(const std::string& text, int sl, int sc, int el, int ec) {
    deleteText(sl, sc, el, ec);
    insertText(text, sl, sc);
}

void NativeEditor::showCompletionPopup(const std::vector<std::string>& c, int l, int col) {
    m_completionItems = c;
    m_completionLine = l;
    m_completionColumn = col;
    m_completionVisible = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::hideCompletionPopup() {
    m_completionVisible = false;
    m_completionItems.clear();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::showDiagnostic(const std::string& m, int l, int s) {
    Diagnostic diag;
    diag.message = m;
    diag.line = l;
    diag.severity = (s >= 2) ? "error" : (s >= 1) ? "warning" : "info";
    m_diagnostics.push_back(diag);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::clearDiagnostics() {
    m_diagnostics.clear();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::applySyntaxHighlighting() {
    // Tokenize and apply syntax highlighting
    // In production: use proper lexer/parser for the language
    m_highlightCache.clear();
    
    for (size_t i = 0; i < m_lines.size(); ++i) {
        std::vector<Token> tokens;
        // Simple keyword highlighting
        const std::string& line = m_lines[i];
        size_t pos = 0;
        while (pos < line.length()) {
            // Skip whitespace
            while (pos < line.length() && std::isspace(line[pos])) pos++;
            if (pos >= line.length()) break;
            
            // Check for keywords
            static const std::set<std::string> keywords = {
                "if", "else", "for", "while", "return", "void", "int", "float", 
                "double", "class", "struct", "namespace", "public", "private", "protected"
            };
            
            size_t wordEnd = pos;
            while (wordEnd < line.length() && (std::isalnum(line[wordEnd]) || line[wordEnd] == '_')) wordEnd++;
            
            if (wordEnd > pos) {
                std::string word = line.substr(pos, wordEnd - pos);
                Token token;
                token.start = pos;
                token.length = wordEnd - pos;
                token.type = (keywords.count(word) > 0) ? TokenType::Keyword : TokenType::Identifier;
                tokens.push_back(token);
                pos = wordEnd;
            } else {
                pos++;
            }
        }
        m_highlightCache[i] = tokens;
    }
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::onMouseClick(int x, int y) {
    int line = (y / m_lineHeight) + m_scrollOffsetY;
    int col = ((x - m_marginLeft) / m_charWidth) + m_scrollOffsetX;
    
    if (line >= 0 && line < static_cast<int>(m_lines.size())) {
        m_cursorLine = line;
        m_cursorColumn = std::min(col, static_cast<int>(m_lines[line].length()));
        m_selectionStart = {m_cursorLine, m_cursorColumn};
        m_selectionEnd = m_selectionStart;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void NativeEditor::onMouseDoubleClick(int x, int y) {
    int line = (y / m_lineHeight) + m_scrollOffsetY;
    if (line >= 0 && line < static_cast<int>(m_lines.size())) {
        // Select word under cursor
        const std::string& text = m_lines[line];
        int col = ((x - m_marginLeft) / m_charWidth) + m_scrollOffsetX;
        col = std::min(col, static_cast<int>(text.length()));
        
        int start = col;
        while (start > 0 && (std::isalnum(text[start - 1]) || text[start - 1] == '_')) start--;
        
        int end = col;
        while (end < static_cast<int>(text.length()) && (std::isalnum(text[end]) || text[end] == '_')) end++;
        
        m_selectionStart = {line, start};
        m_selectionEnd = {line, end};
        m_cursorLine = line;
        m_cursorColumn = end;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void NativeEditor::onMouseWheel(int delta) {
    int linesToScroll = delta / WHEEL_DELTA;
    m_scrollOffsetY = std::max(0, m_scrollOffsetY - linesToScroll);
    m_scrollOffsetY = std::min(m_scrollOffsetY, static_cast<int>(m_lines.size()) - m_visibleLines);
    updateScrollBars();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::setTabSize(int spaces) {
    m_tabSize = spaces;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::enableWordWrap(bool enable) {
    m_wordWrap = enable;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativeEditor::copyToClipboard() {
    if (m_selectionStart.line == m_selectionEnd.line && 
        m_selectionStart.column == m_selectionEnd.column) return;
    
    std::string selectedText;
    int startLine = std::min(m_selectionStart.line, m_selectionEnd.line);
    int endLine = std::max(m_selectionStart.line, m_selectionEnd.line);
    
    for (int line = startLine; line <= endLine; ++line) {
        int startCol = (line == startLine) ? m_selectionStart.column : 0;
        int endCol = (line == endLine) ? m_selectionEnd.column : m_lines[line].length();
        
        if (line > startLine) selectedText += "\n";
        selectedText += m_lines[line].substr(startCol, endCol - startCol);
    }
    
    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, selectedText.size() + 1);
        if (hMem) {
            char* pMem = static_cast<char*>(GlobalLock(hMem));
            std::memcpy(pMem, selectedText.c_str(), selectedText.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void NativeEditor::pasteFromClipboard() {
    if (!OpenClipboard(m_hwnd)) return;
    
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* pText = static_cast<char*>(GlobalLock(hData));
        if (pText) {
            insertText(pText, m_cursorLine, m_cursorColumn);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

void NativeEditor::cutToClipboard() {
    copyToClipboard();
    deleteText(m_selectionStart.line, m_selectionStart.column, m_selectionEnd.line, m_selectionEnd.column);
}

void NativeEditor::moveCursor(int ld, int cd) {
    m_cursorLine = std::max(0, std::min(m_cursorLine + ld, static_cast<int>(m_lines.size()) - 1));
    m_cursorColumn = std::max(0, std::min(m_cursorColumn + cd, static_cast<int>(m_lines[m_cursorLine].length())));
    ensureCursorVisible();
}

void NativeEditor::extendSelection(int ld, int cd) {
    m_cursorLine = std::max(0, std::min(m_cursorLine + ld, static_cast<int>(m_lines.size()) - 1));
    m_cursorColumn = std::max(0, std::min(m_cursorColumn + cd, static_cast<int>(m_lines[m_cursorLine].length())));
    m_selectionEnd = {m_cursorLine, m_cursorColumn};
    ensureCursorVisible();
}

} // namespace RawrXD
