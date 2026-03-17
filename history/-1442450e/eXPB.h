#pragma once
#include "RawrXD_Window.h"
#include "RawrXD_TextBuffer.h"
#include "RawrXD_Renderer_D2D.h"
#include "RawrXD_UndoStack.h"
#include "RawrXD_Lexer.h"
#include "RawrXD_StyleManager.h"
#include <algorithm>
#include <memory>

namespace RawrXD {

class Editor : public Window {
public:
    Editor(Window* parent = nullptr);
    ~Editor();
    
    void setLexer(Lexer* l);
    void setStyleManager(StyleManager* sm);

    void setText(const String& text);
    String text() const;
    
    void setFont(const String& family, float size);
    
    // Core Editing
    void insert(const String& text);
    void backspace();
    void deleteKey();
    
    // Clipboard
    void cut();
    void copy();
    void paste();
    
    // Selection
    void selectAll();
    bool hasSelection() const;
    void clearSelection();
    
    // File I/O helpers
    bool loadFile(const String& path);
    bool saveFile(const String& path);
    
    // Undo/Redo
    UndoStack* undoStack() { return &stack; }
    
protected:
    void paintEvent(PAINTSTRUCT& ps) override;
    void resizeEvent(int w, int h) override;
    
    void keyPressEvent(int key, int mods) override;
    void mousePressEvent(int x, int y, int button) override;
    void mouseMoveEvent(int x, int y, int mods) override;
    void mouseReleaseEvent(int x, int y, int button) override;
    
private:
    TextBuffer buffer;
    UndoStack stack;
    
    // Rendering
    Renderer2D renderer;
    Font font;
    Color textColor = Color::Black;
    Color bgColor = Color::White;
    Color selectionColor = Color(0, 120, 215, 100);
    Color constColor = Color(200, 200, 200); // Gutter bg
    
    // State
    int scrollY = 0;
    int scrollX = 0;

    Lexer* lexer = nullptr;
    StyleManager* styleManager = nullptr;
    
    int cursorLine = 0;
    int cursorCol = 0; // Visual column
    
    int anchorLine = 0;
    int anchorCol = 0;
    
    int targetCol = -1; // For up/down navigation
    
    // Layout metrics
    float lineHeight = 20.0f;
    float charWidth = 10.0f; // Monospace assumption for now
    float gutterWidth = 50.0f;
    
    // Helpers
    void updateScrollbars();
    Point screenPosFromLoc(int line, int col);
    void locFromScreenPos(int x, int y, int& line, int& col);
    
    void moveCursor(int dLine, int dCol, bool keepSelection);
    void scrollCursorIntoView();
    
    // Internal commands for UndoStack
    class InsertCmd;
    class DeleteCmd;
    
    void insertInternal(int line, int col, const String& text);
    void removeInternal(int line, int col, int len);
};

} // namespace RawrXD
