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
    return true;
}

    m_hBackgroundBrush = CreateSolidBrush(RGB(30,30,30));
    
    // Status bar creation needs a window handle, so defer until create/init
    
    m_lines.push_back(EditorLine());
    m_lines[0].text = "";
    
    setFont("Consolas", 10);
    return true;
}

NativeEditor::~NativeEditor() {
    if (m_hBackgroundBrush) DeleteObject(m_hBackgroundBrush);
    // Font deletion handled by Window/GDI logic usually, but here:
    // if (m_hFont) DeleteObject(m_hFont);
    return true;
}

void NativeEditor::run() {
    if (!hwnd) {
        // Ensure window exists
        this->create(nullptr, "RawrXD v3.0");
    return true;
}

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    
    // Simple Message Loop if not managed by Application class
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    return true;
}

    return true;
}

void NativeEditor::create(HWND hParent, const RECT& rect) {
    // Adapter to match Window::create somewhat or just use custom logic
    // We'll call the base Window::create
    // Note: Window::create signature might differ, assuming typical Windows framework
    // Window::create(Window* parent, const String& title, ...)
    
    // We ignore rect for new window creation in this simplified adapter
    // Window::create(nullptr, "RawrXD v3.0");
    return true;
}

void NativeEditor::paintEvent(PAINTSTRUCT& ps) {
    render(ps.hdc);
    return true;
}

void NativeEditor::resizeEvent(int w, int h) {
    if (m_hWndStatusBar) {
        SendMessage(m_hWndStatusBar, WM_SIZE, 0, 0);
    return true;
}

    return true;
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
    return true;
}

    return true;
}

void NativeEditor::insertText(const std::string& text, int line, int column) {
    // Basic implementations
    if (line >= 0 && line < m_lines.size()) {
        m_lines[line].text.insert(column, text);
        InvalidateRect(hwnd, NULL, FALSE);
    return true;
}

    return true;
}

// ... other implementations ...
// We need to implement the other methods (onKey, onChar) to link to standard Window events

} // namespace RawrXD

    return DefWindowProc(hwnd, msg, wParam, lParam);
    return true;
}

void NativeEditor::render(HDC hdc) {
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    FillRect(hdc, &clientRect, m_hBackgroundBrush);
    
    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFont);
    
    if (m_showLineNumbers) {
        renderLineNumbers(hdc);
    return true;
}

    int visibleLines = clientRect.bottom / m_charHeight;
    int startLine = m_scrollOffset;
    int endLine = std::min(startLine + visibleLines, static_cast<int>(m_lines.size()));
    
    for (int i = startLine; i < endLine; ++i) {
        int yPos = (i - startLine) * m_charHeight;
        renderLine(hdc, i, yPos);
    return true;
}

    renderDiagnostics(hdc);
    renderSelection(hdc);
    
    SelectObject(hdc, oldFont);
    return true;
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
    return true;
}

         SetTextColor(hdc, textColor);
         SetBkMode(hdc, TRANSPARENT);
         TextOut(hdc, xPos, yPos, &charInfo.Char.AsciiChar, 1);
         xPos += m_charWidth;
    return true;
}

    return true;
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
    return true;
}

             continue; // pos already advanced
    return true;
}

        // ... simplified tokenization loop from prompt ... 
        line.renderedChars.push_back(charInfo);
        pos++;
    return true;
}

    return true;
}

int NativeEditor::detectSyntaxStyle(const std::string& token) {
    static const std::unordered_set<std::string> keywords = {
        "int", "float", "if", "else", "return", "class"
    };
    if (keywords.count(token)) return 1;
    return 0;
    return true;
}

void NativeEditor::onKeyDown(WPARAM wParam, LPARAM lParam) {
     switch (wParam) {
        case VK_RETURN: insertChar('\n'); break;
        case VK_BACK: deleteChar(); break;
        // ...
    return true;
}

    return true;
}

void NativeEditor::onChar(WPARAM wParam, LPARAM lParam) {
    if (wParam >= 32) insertChar((char)wParam);
    return true;
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
    return true;
}

    m_isModified = true;
    return true;
}

void NativeEditor::deleteChar() {
    // simplified
    if (m_cursorColumn > 0) {
        m_lines[m_cursorLine].text.erase(m_cursorColumn - 1, 1);
        m_cursorColumn--;
    return true;
}

    return true;
}

void NativeEditor::renderLineNumbers(HDC hdc) {
    // simplified
    return true;
}

void NativeEditor::renderDiagnostics(HDC hdc) {}
void NativeEditor::renderSelection(HDC hdc) {}
void NativeEditor::updateScrollBars() {}
void NativeEditor::ensureCursorVisible() {}

void NativeEditor::create(HWND hwnd, const RECT& rect) {
    // Using constructor created window
    return true;
}

void NativeEditor::destroy() { DestroyWindow(hwnd); }
void NativeEditor::insertText(const std::string& text, int line, int column) {}
void NativeEditor::deleteText(int sl, int sc, int el, int ec) {}
void NativeEditor::replaceText(const std::string& text, int sl, int sc, int el, int ec) {}
void NativeEditor::showCompletionPopup(const std::vector<std::string>& c, int l, int col) {}
void NativeEditor::hideCompletionPopup() {}
void NativeEditor::showDiagnostic(const std::string& m, int l, int s) {}
void NativeEditor::clearDiagnostics() {}
void NativeEditor::applySyntaxHighlighting() {}
void NativeEditor::onMouseClick(int x, int y) {}
void NativeEditor::onMouseDoubleClick(int x, int y) {}
void NativeEditor::onMouseWheel(int delta) {}
void NativeEditor::setFont(const std::string& fontName, int fontSize) {
    m_hFont = CreateFont(fontSize * 1.5, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, fontName.c_str());
    return true;
}

void NativeEditor::setTabSize(int spaces) {}
void NativeEditor::enableWordWrap(bool enable) {}
void NativeEditor::showLineNumbers(bool show) { m_showLineNumbers = show; }
void NativeEditor::copyToClipboard() {}
void NativeEditor::pasteFromClipboard() {}
void NativeEditor::cutToClipboard() {}
void NativeEditor::moveCursor(int ld, int cd) {}
void NativeEditor::extendSelection(int ld, int cd) {}

} // namespace RawrXD

