// ============================================================================
// CustomSyntaxEditor.h — Pure Win32 Custom Syntax Highlighting Editor
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "../include/RawrXD_ColorSpace.h"

using namespace RawrXD::ColorSpace;

// Helper to convert AdobeRGBa to COLORREF for Win32 GDI
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255),
               static_cast<int>(srgb.g * 255),
               static_cast<int>(srgb.b * 255));
}

// Syntax token types
enum class SyntaxTokenType {
    Unknown,
    Keyword,
    Identifier,
    String,
    Number,
    Comment,
    Preprocessor,
    Operator,
    Type,
    Function,
    Variable,
    Constant,
    Whitespace
};

// Token information
struct SyntaxToken {
    SyntaxTokenType type;
    size_t start;
    size_t length;
    COLORREF color;
    
    SyntaxToken(SyntaxTokenType t = SyntaxTokenType::Unknown, size_t s = 0, size_t l = 0, COLORREF c = 0)
        : type(t), start(s), length(l), color(c) {}
};

// Language definition
struct LanguageDefinition {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::string> keywords;
    std::vector<std::string> types;
    std::vector<std::string> functions;
    std::vector<std::string> constants;
    std::string singleLineComment;
    std::string multiLineCommentStart;
    std::string multiLineCommentEnd;
    char stringDelimiter;
    char charDelimiter;
};

// Custom editor class
class CustomSyntaxEditor {
public:
    CustomSyntaxEditor();
    ~CustomSyntaxEditor();

    // Initialization
    bool Create(HWND parent, int x, int y, int width, int height);
    void Destroy();
    
    // Text operations
    void SetText(const std::string& text);
    std::string GetText() const;
    void InsertText(int line, int col, const std::string& text);
    void DeleteRange(int startLine, int startCol, int endLine, int endCol);
    
    // Syntax highlighting
    void SetLanguage(const std::string& language);
    void Rehighlight();
    
    // Theme
    void SetTheme(const AdobeRGBa& textColor, const AdobeRGBa& backgroundColor,
                  const AdobeRGBa& selectionColor, const AdobeRGBa& lineNumberColor);
    
    // Rendering
    void Render(HDC hdc, const RECT& clientRect);
    void OnPaint();
    
    // Scrolling
    void SetScrollPos(int x, int y);
    void GetScrollPos(int& x, int& y) const;
    void UpdateScrollBars();
    
    // Cursor and selection
    void SetCursorPos(int line, int col);
    void GetCursorPos(int& line, int& col) const;
    void SetSelection(int startLine, int startCol, int endLine, int endCol);
    void GetSelection(int& startLine, int& startCol, int& endLine, int& endCol) const;
    
    // Font
    void SetFont(const std::wstring& fontName, int fontSize);
    
    // Line numbers
    void ShowLineNumbers(bool show);
    
    // Visibility
    void Show();
    void Hide();
    bool IsVisible() const;
    
    // Focus
    void SetFocus();
    bool HasFocus() const;
    
    // Event handlers
    void SetOnTextChangedCallback(std::function<void()> callback);
    void SetOnSelectionChangedCallback(std::function<void()> callback);
    
    // Utility functions
    int GetLineCount() const;
    int GetLineLength(int line) const;
    std::string GetLineText(int line) const;
    
private:
    // Internal structures
    struct EditorLine {
        std::string text;
        std::vector<SyntaxToken> tokens;
        int yPos;
        int height;
    };
    
    // Parsing and highlighting
    void ParseSyntax();
    void ParseLineSyntax(EditorLine& line);
    void ApplySyntaxHighlighting(HDC hdc, const EditorLine& line, int yPos);
    
    // Token recognition
    bool IsKeyword(const std::string& word) const;
    bool IsType(const std::string& word) const;
    bool IsFunction(const std::string& word) const;
    bool IsConstant(const std::string& word) const;
    SyntaxTokenType IdentifyToken(const std::string& text, size_t pos) const;
    
    // Position conversion
    size_t LineColToPos(int line, int col) const;
    void PosToLineCol(size_t pos, int& line, int& col) const;
    
    // Drawing helpers
    void DrawLineNumbers(HDC hdc, const RECT& clientRect);
    void DrawTextContent(HDC hdc, const RECT& clientRect);
    void DrawSelection(HDC hdc, const RECT& clientRect);
    void DrawCursor(HDC hdc, const RECT& clientRect);
    
    // Measurement helpers
    void CalculateLineMetrics();
    void MeasureLine(EditorLine& line);
    SIZE MeasureText(const std::string& text) const;
    
    // Scroll helpers
    void EnsureCursorVisible();
    
    // Member variables
    HWND m_hWnd;
    HWND m_parent;
    bool m_visible;
    
    // Text data
    std::vector<EditorLine> m_lines;
    std::string m_text;
    
    // Language and syntax
    LanguageDefinition m_language;
    std::map<std::string, LanguageDefinition> m_languages;
    
    // Theme
    AdobeRGBa m_textColor;
    AdobeRGBa m_backgroundColor;
    AdobeRGBa m_selectionColor;
    AdobeRGBa m_lineNumberColor;
    COLORREF m_textColorRef;
    COLORREF m_backgroundColorRef;
    COLORREF m_selectionColorRef;
    COLORREF m_lineNumberColorRef;
    
    // Font
    HFONT m_font;
    std::wstring m_fontName;
    int m_fontSize;
    int m_charWidth;
    int m_charHeight;
    int m_lineHeight;
    
    // Cursor and selection
    int m_cursorLine;
    int m_cursorCol;
    int m_selectionStartLine;
    int m_selectionStartCol;
    int m_selectionEndLine;
    int m_selectionEndCol;
    bool m_hasSelection;
    
    // Scrolling
    int m_scrollX;
    int m_scrollY;
    int m_maxScrollX;
    int m_maxScrollY;
    
    // UI state
    bool m_showLineNumbers;
    int m_lineNumberWidth;
    
    // Callbacks
    std::function<void()> m_onTextChanged;
    std::function<void()> m_onSelectionChanged;
    
    // Double buffering
    HDC m_memDC;
    HBITMAP m_memBitmap;
    int m_bufferWidth;
    int m_bufferHeight;
};
