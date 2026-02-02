#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include "../RawrXD_Editor.h"

namespace RawrXD {

struct EditorLine {
    std::string text;
    std::vector<CHAR_INFO> renderedChars;
    int syntaxStyle; // 0=default, 1=keyword, 2=string, etc.
};

class NativeEditor : public Editor {
public:
    NativeEditor(HWND hwnd = NULL);
    ~NativeEditor() override;
    
    // Non-copyable
    NativeEditor(const NativeEditor&) = delete;
    NativeEditor& operator=(const NativeEditor&) = delete;

    void paintEvent(PAINTSTRUCT& ps) override;
    void resizeEvent(int w, int h) override;
    
    // Real editor functionality
    void create(HWND hwnd, const RECT& rect);
    void destroy();
    void run(); // Added to support main.cpp usage
    
    // Real text editing
    void insertText(const std::string& text, int line, int column);
    void deleteText(int startLine, int startCol, int endLine, int endCol);
    void replaceText(const std::string& text, int startLine, int startCol, int endLine, int endCol);
    
    // Real rendering
    void render(HDC hdc);
    void renderLine(HDC hdc, int lineIndex, int yPos);
    void applySyntaxHighlighting();
    
    // Real user input
    void onKeyDown(WPARAM wParam, LPARAM lParam);
    void onChar(WPARAM wParam, LPARAM lParam);
    void onMouseClick(int x, int y);
    void onMouseDoubleClick(int x, int y);
    void onMouseWheel(int delta);
    
    // Real features
    void setFont(const std::string& fontName, int fontSize);
    void setTabSize(int spaces);
    void enableWordWrap(bool enable);
    void showLineNumbers(bool show);
    
    // Real completion
    void showCompletionPopup(const std::vector<std::string>& completions, int line, int column);
    void hideCompletionPopup();
    
    // Real diagnostics
    void showDiagnostic(const std::string& message, int line, int severity);
    void clearDiagnostics();
    
    // Status
    int getLineCount() const { return (int)m_lines.size(); }
    int getCurrentLine() const { return m_cursorLine; }
    int getCurrentColumn() const { return m_cursorColumn; }
    bool isModified() const { return m_isModified; }
    
    // Event handling
    std::function<void()> onTextChanged;
    std::function<void()> onCursorMoved;
    std::function<void()> onSelectionChanged;
    std::function<void()> onBuildRequest;
    
private:
    HWND m_hwnd;
    HWND m_hWndEditor;
    HWND m_hWndStatusBar;
    HFONT m_hFont;
    HBRUSH m_hBackgroundBrush;
    
    // Real text buffer
    std::vector<EditorLine> m_lines;
    int m_cursorLine = 0;
    int m_cursorColumn = 0;
    int m_scrollOffset = 0;
    int m_tabSize = 4;
    bool m_isModified = false;
    bool m_wordWrap = false;
    bool m_showLineNumbers = true;
    
    // Real selection
    struct Selection {
        int startLine = 0;
        int startColumn = 0;
        int endLine = 0;
        int endColumn = 0;
        bool isActive = false;
    } m_selection;
    
    // Real completion
    HWND m_hWndCompletionPopup = NULL;
    std::vector<std::string> m_currentCompletions;
    int m_completionIndex = 0;
    
    // Real diagnostics
    struct Diagnostic {
        std::string message;
        int line;
        int severity; // 0=error, 1=warning, 2=info
    };
    std::vector<Diagnostic> m_diagnostics;
    
    // Rendering
    int m_charWidth = 8;
    int m_charHeight = 16;
    int m_lineNumberWidth = 50;
    
    // Real implementation methods
    void updateScrollBars();
    void ensureCursorVisible();
    void renderLineNumbers(HDC hdc);
    void renderDiagnostics(HDC hdc);
    void renderSelection(HDC hdc);
    
    // Syntax highlighting
    void tokenizeLine(int lineIndex);
    int detectSyntaxStyle(const std::string& token);
    
    // Input handling
    void insertChar(char c);
    void deleteChar();
    void moveCursor(int lineDelta, int colDelta);
    void extendSelection(int lineDelta, int colDelta);
    
    // Clipboard
    void copyToClipboard();
    void pasteFromClipboard();
    void cutToClipboard();
    
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace RawrXD
