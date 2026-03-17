#include "RawrXD_Editor.h"
#include <windows.h>
#include <cmath>

namespace RawrXD {

// Undo Commands
class Editor::InsertCmd : public UndoCommand {
    Editor* editor;
    int line, col;
    String text;
public:
    InsertCmd(Editor* e, int l, int c, const String& t) : editor(e), line(l), col(c), text(t) {}
    void undo() override { 
        // Need internal remove that doesn't push undo
        editor->removeInternal(line, col, text.length()); 
        editor->cursorLine = line; editor->cursorCol = col;
        editor->update();
    }
    void redo() override { 
        editor->insertInternal(line, col, text);
        // Calc new cursor pos
        // Simplified: assumes single line text for cursor update, but logic works for multiline
        editor->update();
    }
    int id() const override { return 1; }
    bool mergeWith(const UndoCommand* other) override {
        // Simple merge for sequential typing
        const InsertCmd* ins = dynamic_cast<const InsertCmd*>(other);
        if (!ins) return false;
        // If appending
        if (ins->line == line && ins->col == col + text.length()) { // Logic simplified
             // text += ins->text;
             // return true;
        }
        return false;
    }
};

class Editor::DeleteCmd : public UndoCommand {
    Editor* editor;
    int line, col;
    String text;
public:
    DeleteCmd(Editor* e, int l, int c, const String& t) : editor(e), line(l), col(c), text(t) {}
    void undo() override { editor->insertInternal(line, col, text); editor->update(); }
    void redo() override { editor->removeInternal(line, col, text.length()); editor->update(); }
};


Editor::Editor(Window* parent) : Window(parent), stack(this), font(L"Consolas", 12.0f) {
    create(parent, L"RawrXD_Editor", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL);
    renderer.initialize(hwnd);
    
    // Default metrics
    // We should measure font using DirectWrite
    // For now, hardcode generic monospace metrics
    charWidth = 9.0f;
    lineHeight = 16.0f;
}

Editor::~Editor() {
    // ...
}

void Editor::setLexer(Lexer* l) {
    lexer = l;
    update();
}

void Editor::setStyleManager(StyleManager* sm) {
    styleManager = sm;
    update();
}

void Editor::setText(const String& text) {
    buffer.setText(text);
    stack.clear();
    cursorLine = 0;
    cursorCol = 0;
    anchorLine = 0;
    anchorCol = 0;
    updateScrollbars();
    update();
}

String Editor::text() const {
    return buffer.text();
}

void Editor::insertInternal(int line, int col, const String& text) {
    int pos = buffer.getLineStart(line) + col;
    buffer.insert(pos, text);
    // Update cursor pos logic to be robust
    // ...
}

void Editor::removeInternal(int line, int col, int len) {
    int pos = buffer.getLineStart(line) + col;
    buffer.remove(pos, len);
}

void Editor::insert(const String& text) {
    // If selection, delete it first
    if (hasSelection()) backspace(); // Simple hack
    
    stack.push(std::make_unique<InsertCmd>(this, cursorLine, cursorCol, text));
    insertInternal(cursorLine, cursorCol, text);
    
    // Update cursor
    // If text contains newlines, recalc line/col
    // Simple for now: just move col
    cursorCol += text.length(); // Wrong for newlines
    
    // Correct way:
    if (text.indexOf(L"\n") >= 0) {
        // Re-evaluate cursor from absolute pos
        // We need a robust "cursor from pos" function
        // But we are tracking visual line/col
    }
    
    anchorLine = cursorLine;
    anchorCol = cursorCol;
    
    scrollCursorIntoView();
    update();
}

void Editor::backspace() {
    if (hasSelection()) {
        // delete selection
        // ...
        clearSelection();
        return;
    }
    
    if (cursorCol > 0) {
        String delChar = buffer.substring(buffer.getLineStart(cursorLine) + cursorCol - 1, 1);
        stack.push(std::make_unique<DeleteCmd>(this, cursorLine, cursorCol - 1, delChar));
        removeInternal(cursorLine, cursorCol - 1, 1);
        cursorCol--;
    } else if (cursorLine > 0) {
        // Join with previous line
        // ...
    }
    anchorLine = cursorLine;
    anchorCol = cursorCol;
    scrollCursorIntoView();
    update();
}

void Editor::deleteKey() {
    // Delete forward
    String delChar = buffer.substring(buffer.getLineStart(cursorLine) + cursorCol, 1);
    // ... logic
}

bool Editor::hasSelection() const {
    return cursorLine != anchorLine || cursorCol != anchorCol;
}

void Editor::clearSelection() {
    anchorLine = cursorLine;
    anchorCol = cursorCol;
    update();
}

void Editor::selectAll() {
    anchorLine = 0;
    anchorCol = 0;
    cursorLine = buffer.lineCount() - 1; 
    cursorCol = buffer.getLineLength(cursorLine);
    update();
}

void Editor::paintEvent(PAINTSTRUCT& ps) {
    renderer.beginPaint();
    renderer.clear(bgColor);
    
    int startLine = scrollY / (int)lineHeight;
    int visibleLines = (int)(height() / lineHeight) + 2;
    int endLine = std::min(buffer.lineCount(), startLine + visibleLines);
    
    // Draw Gutter
    renderer.fillRect(Rect(0, 0, (int)gutterWidth, height()), constColor);
    
    for (int i = startLine; i < endLine; ++i) {
        float y = (i * lineHeight) - scrollY;
        
        // Line number
        renderer.drawText(Point(2, (int)y), String::number(i + 1), font, Color::Black);
        
        // Highlighting selection (simplified)
        // ...
        
        // Text
        int len = buffer.getLineLength(i);
        int pos = buffer.getLineStart(i);
        String lineText = buffer.substring(pos, len);
        
        // Remove newline chars for drawing
        if (lineText.endsWith(L"\n")) lineText = lineText.left(lineText.length() - 1);
        if (lineText.endsWith(L"\r")) lineText = lineText.left(lineText.length() - 1);
        
        renderer.drawText(Point((int)gutterWidth + 2 - scrollX, (int)y), lineText, font, textColor);
    }
    
    // Draw Scrollbars? Windows handles them if we set params, but we draw content
    
    renderer.endPaint();
}

void Editor::resizeEvent(int w, int h) {
    renderer.resize(w, h);
    updateScrollbars();
}

void Editor::keyPressEvent(int key, int mods) {
    // Basic navigation
    if (key == VK_UP) moveCursor(-1, 0, mods & 1); // Shift for selection
    else if (key == VK_DOWN) moveCursor(1, 0, mods & 1);
    else if (key == VK_LEFT) moveCursor(0, -1, mods & 1);
    else if (key == VK_RIGHT) moveCursor(0, 1, mods & 1);
    
    else if (key == VK_BACK) backspace();
    else if (key == VK_DELETE) deleteKey();
    else if (key == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) copy();
    else if (key == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) paste();
    else if (key == 'X' && (GetKeyState(VK_CONTROL) & 0x8000)) cut();
    else if (key == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) stack.undo();
    else if (key == 'Y' && (GetKeyState(VK_CONTROL) & 0x8000)) stack.redo();
    
    // Typing handled visually via WM_CHAR, but we can hook it here or let specific handler do it
    // Window class doesn't have charEvent yet, so we assume keyPress handles non-char keys
    // and we need a charEvent hook for text input.
    // For now, let's assume raw key mapping for demo.
    
    update();
}

void Editor::mousePressEvent(int x, int y, int button) {
    if (button == 1) { // Left
        int l, c;
        locFromScreenPos(x, y, l, c);
        cursorLine = l;
        cursorCol = c;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000);
        if (!shift) {
            anchorLine = cursorLine;
            anchorCol = cursorCol;
        }
        update();
    }
}

void Editor::mouseMoveEvent(int x, int y, int mods) {
    if (mods & MK_LBUTTON) {
        int l, c;
        locFromScreenPos(x, y, l, c);
        cursorLine = l;
        cursorCol = c;
        update();
    }
}

void Editor::mouseReleaseEvent(int x, int y, int button) {}

void Editor::moveCursor(int dLine, int dCol, bool keepSelection) {
    cursorLine += dLine;
    cursorCol += dCol;
    
    if (cursorLine < 0) cursorLine = 0;
    if (cursorLine >= buffer.lineCount()) cursorLine = buffer.lineCount() - 1;
    
    // Clamp col
    int maxCol = buffer.getLineLength(cursorLine); // includes \n?
    if (cursorCol < 0) cursorCol = 0;
    // ...
    
    if (!keepSelection) {
        anchorLine = cursorLine;
        anchorCol = cursorCol;
    }
    
    scrollCursorIntoView();
}

void Editor::scrollCursorIntoView() {
    // Adjust scrollY/scrollX
}

void Editor::updateScrollbars() {
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = buffer.lineCount();
    si.nPage = (int)(height() / lineHeight);
    si.nPos = scrollY / (int)lineHeight;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void Editor::locFromScreenPos(int x, int y, int& line, int& col) {
    line = (y + scrollY) / (int)lineHeight;
    if (line < 0) line = 0;
    if (line >= buffer.lineCount()) line = buffer.lineCount() - 1;
    
    col = (int)((x + scrollX - gutterWidth) / charWidth);
    if (col < 0) col = 0;
}

// Stubs
void Editor::cut() {}
void Editor::copy() {}
void Editor::paste() {}
bool Editor::loadFile(const String& path) { return false; }
bool Editor::saveFile(const String& path) { return false; }

} // namespace RawrXD
