#pragma once

#include <Windows.h>
#include <windowsx.h>
#include <Imm.h>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <regex>
#include <chrono>

namespace MiniMonaco {

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

class Editor;
class TextBuffer;
class UndoStack;
class SyntaxHighlighter;
class Minimap;
class FindReplace;

// =============================================================================
// CONFIGURATION
// =============================================================================

struct Config {
    // Appearance
    int fontSize = 14;
    std::wstring fontFamily = L"Consolas";
    int lineHeight = 20;
    int charWidth = 9;
    int tabSize = 4;
    bool showLineNumbers = true;
    bool showMinimap = true;
    bool wordWrap = false;
    bool renderWhitespace = false;
    
    // Colors (32-bit BGRA)
    uint32_t bgColor = 0xFF1E1E1E;
    uint32_t textColor = 0xFFD4D4D4;
    uint32_t selectionBg = 0xFF264F78;
    uint32_t selectionFg = 0xFFFFFFFF;
    uint32_t lineNumberColor = 0xFF858585;
    uint32_t lineNumberBg = 0xFF1E1E1E;
    uint32_t currentLineBg = 0xFF282828;
    uint32_t cursorColor = 0xFFAEAFAD;
    uint32_t searchHighlightBg = 0xFF613214;
    uint32_t searchMatchBg = 0xFF515C6A;
    
    // Behavior
    int cursorBlinkMs = 530;
    int scrollSpeed = 3;
    bool autoIndent = true;
    bool autoBrace = true;
    bool autoClose = true;
    
    // Syntax colors
    std::unordered_map<std::string, uint32_t> syntaxColors = {
        {"keyword", 0xFF569CD6},
        {"string", 0xFFCE9178},
        {"number", 0xFFB5CEA8},
        {"comment", 0xFF6A9955},
        {"function", 0xFFDCDCAA},
        {"type", 0xFF4EC9B0},
        {"operator", 0xFFD4D4D4},
        {"preprocessor", 0xFFC586C0},
        {"variable", 0xFF9CDCFE},
    };
};

// =============================================================================
// TEXT BUFFER (Gap Buffer Implementation)
// =============================================================================

class TextBuffer {
public:
    explicit TextBuffer(size_t initialSize = 1024 * 1024);
    ~TextBuffer();
    
    // Core operations
    void insert(size_t pos, const wchar_t* text, size_t len);
    void erase(size_t pos, size_t len);
    wchar_t at(size_t pos) const;
    std::wstring_view view(size_t pos, size_t len) const;
    std::wstring substr(size_t pos, size_t len) const;
    
    // Bulk operations
    void setText(const std::wstring& text);
    std::wstring text() const;
    
    // Line operations
    size_t lineCount() const;
    size_t lineStart(size_t line) const;
    size_t lineEnd(size_t line) const;
    size_t lineFromPos(size_t pos) const;
    std::wstring lineContent(size_t line) const;
    std::wstring_view lineView(size_t line) const;
    
    // Utilities
    size_t length() const { return length_; }
    bool empty() const { return length_ == 0; }
    
    // Change tracking
    using ChangeCallback = std::function<void(size_t pos, size_t delLen, size_t insLen)>;
    void setChangeCallback(ChangeCallback cb) { onChange_ = std::move(cb); }
    
private:
    void moveGapTo(size_t pos);
    void ensureCapacity(size_t needed);
    
    std::unique_ptr<wchar_t[]> buffer_;
    size_t capacity_;
    size_t length_;
    size_t gapStart_;
    size_t gapEnd_;
    
    // Line cache
    mutable std::vector<size_t> lineStarts_;
    mutable bool linesDirty_ = true;
    void updateLines() const;
    
    ChangeCallback onChange_;
};

// =============================================================================
// UNDO/REDO STACK
// =============================================================================

struct UndoAction {
    enum Type { Insert, Erase, GroupStart, GroupEnd };
    Type type;
    size_t pos;
    std::wstring text;
    size_t group;
};

class UndoStack {
public:
    explicit UndoStack(size_t maxSize = 1000000);
    
    void pushInsert(size_t pos, const std::wstring& text);
    void pushErase(size_t pos, const std::wstring& text);
    void beginGroup();
    void endGroup();
    
    bool canUndo() const { return !actions_.empty() && currentGroup_ > 0; }
    bool canRedo() const { return currentGroup_ < maxGroup_; }
    
    std::vector<UndoAction> undo();
    std::vector<UndoAction> redo();
    
    void clear();
    void setMaxSize(size_t size) { maxSize_ = size; }
    
private:
    std::vector<UndoAction> actions_;
    size_t currentGroup_ = 0;
    size_t maxGroup_ = 0;
    size_t maxSize_;
    bool inGroup_ = false;
};

// =============================================================================
// SELECTION AND CURSOR
// =============================================================================

struct Selection {
    size_t anchor = 0;
    size_t cursor = 0;
    
    bool empty() const { return anchor == cursor; }
    size_t start() const { return std::min(anchor, cursor); }
    size_t end() const { return std::max(anchor, cursor); }
    size_t length() const { return end() - start(); }
    
    void clear() { anchor = cursor = 0; }
    void selectAll(size_t docLength) { anchor = 0; cursor = docLength; }
    void move(size_t pos) { anchor = cursor = pos; }
    void extend(size_t pos) { cursor = pos; }
};

struct Cursor {
    size_t position = 0;
    Selection selection;
    bool isPrimary = true;
    
    // For vertical movement (remember column)
    int desiredColumn = 0;
};

// =============================================================================
// SYNTAX HIGHLIGHTING
// =============================================================================

struct Token {
    size_t start;
    size_t end;
    std::string type;
};

class SyntaxHighlighter {
public:
    virtual ~SyntaxHighlighter() = default;
    virtual std::vector<Token> highlight(const std::wstring_view& text) const = 0;
    virtual std::string languageName() const = 0;
};

class GenericHighlighter : public SyntaxHighlighter {
public:
    explicit GenericHighlighter(const std::string& lang = "text");
    
    std::vector<Token> highlight(const std::wstring_view& text) const override;
    std::string languageName() const override { return language_; }
    
    void addKeywords(const std::vector<std::wstring>& keywords);
    void addTypes(const std::vector<std::wstring>& types);
    void addOperators(const std::vector<std::wstring>& ops);
    void addStrings(bool enabled = true);
    void addComments(bool singleLine = true, bool multiLine = true);
    void addNumbers(bool enabled = true);
    void addFunctions(bool enabled = true);
    
private:
    std::string language_;
    std::unordered_set<std::wstring> keywords_;
    std::unordered_set<std::wstring> types_;
    std::unordered_set<std::wstring> operators_;
    bool strings_ = true;
    bool comments_ = true;
    bool multiLineComments_ = true;
    bool numbers_ = true;
    bool functions_ = true;
    
    std::vector<Token> highlightCpp(const std::wstring_view& text) const;
    std::vector<Token> highlightPython(const std::wstring_view& text) const;
    std::vector<Token> highlightGeneric(const std::wstring_view& text) const;
};

// =============================================================================
// RENDER LINE (Virtualized)
// =============================================================================

struct RenderLine {
    size_t lineNumber;
    size_t start;
    size_t end;
    std::vector<Token> tokens;
    std::wstring text;
    bool dirty = true;
    
    int visualHeight = 1;  // For wrapped lines
    int visualStart = 0;   // Y position in screen space
};

// =============================================================================
// FIND AND REPLACE
// =============================================================================

struct SearchResult {
    size_t position;
    size_t length;
    int line;
};

class FindReplace {
public:
    FindReplace();
    
    // Find
    std::vector<SearchResult> find(const std::wstring& pattern, 
                                    const TextBuffer& buffer,
                                    bool caseSensitive = true,
                                    bool wholeWord = false,
                                    bool regex = false);
    
    // Find next/previous
    SearchResult findNext(const std::wstring& pattern,
                          const TextBuffer& buffer,
                          size_t fromPos,
                          bool caseSensitive = true);
    
    SearchResult findPrev(const std::wstring& pattern,
                          const TextBuffer& buffer,
                          size_t fromPos,
                          bool caseSensitive = true);
    
    // Replace
    size_t replace(TextBuffer& buffer, 
                   size_t pos, 
                   size_t len, 
                   const std::wstring& replacement);
    
    size_t replaceAll(TextBuffer& buffer,
                      const std::wstring& pattern,
                      const std::wstring& replacement,
                      bool caseSensitive = true);
    
private:
    std::wstring lastPattern_;
    std::vector<SearchResult> lastResults_;
};

// =============================================================================
// MINIMAP
// =============================================================================

class Minimap {
public:
    Minimap();
    
    void update(const TextBuffer& buffer, 
                const std::vector<RenderLine>& lines,
                int firstVisibleLine,
                int visibleLines,
                int editorHeight);
    
    void render(HDC hdc, int x, int y, int width, int height, uint32_t bgColor) const;
    
    // Hit testing
    int lineFromY(int y, int height) const;
    
private:
    struct MinimapLine {
        std::vector<std::pair<int, int>> segments;  // x, width pairs for colored spans
        int length;
    };
    
    std::vector<MinimapLine> lines_;
    int firstVisible_ = 0;
    int visibleCount_ = 0;
    int editorHeight_ = 0;
    float scale_ = 0.15f;
};

// =============================================================================
// MAIN EDITOR
// =============================================================================

class Editor {
public:
    Editor(HWND hwnd, Config config = {});
    ~Editor();
    
    // Lifecycle
    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    
    // Text operations
    void setText(const std::wstring& text);
    std::wstring text() const;
    std::wstring selection() const;
    
    void insert(const std::wstring& text);
    void insertNewline();
    void insertTab();
    void backspace();
    void del();
    
    void undo();
    void redo();
    
    // Selection
    void selectAll();
    void clearSelection();
    bool hasSelection() const;
    
    // Navigation
    void moveLeft(bool shift = false);
    void moveRight(bool shift = false);
    void moveUp(bool shift = false);
    void moveDown(bool shift = false);
    void moveHome(bool shift = false);
    void moveEnd(bool shift = false);
    void movePageUp(bool shift = false);
    void movePageDown(bool shift = false);
    void moveWordLeft(bool shift = false);
    void moveWordRight(bool shift = false);
    
    void goToLine(int line);
    void goToPosition(size_t pos);
    
    // Clipboard
    void cut();
    void copy();
    void paste();
    
    // Find/Replace
    void find(const std::wstring& pattern, bool caseSensitive = true);
    void findNext();
    void findPrev();
    void replace(const std::wstring& replacement);
    void replaceAll(const std::wstring& pattern, const std::wstring& replacement);
    
    // Scroll
    void scrollToLine(int line);
    void scrollToPosition(size_t pos);
    
    // Rendering
    void render(HDC hdc);
    void invalidate();
    
    // Input
    void onChar(wchar_t ch);
    void onKeyDown(int vk, bool shift, bool ctrl, bool alt);
    void onMouseDown(int x, int y, bool left, bool right);
    void onMouseMove(int x, int y);
    void onMouseUp(int x, int y);
    void onMouseWheel(int delta, bool shift);
    void onSize(int width, int height);
    
    // Configuration
    void setConfig(const Config& config);
    Config config() const { return config_; }
    
    void setHighlighter(std::unique_ptr<SyntaxHighlighter> highlighter);
    void setLanguage(const std::string& lang);
    
    // Events
    using ChangeCallback = std::function<void()>;
    void setChangeCallback(ChangeCallback cb) { onChange_ = std::move(cb); }
    
    using CursorCallback = std::function<void(int line, int col)>;
    void setCursorCallback(CursorCallback cb) { onCursorChange_ = std::move(cb); }
    
    // Info
    int currentLine() const;
    int currentColumn() const;
    size_t currentPosition() const { return cursors_.empty() ? 0 : cursors_.front().position; }
    int lineCount() const;
    
private:
    // Core
    HWND hwnd_ = nullptr;
    Config config_;
    std::unique_ptr<TextBuffer> buffer_;
    std::unique_ptr<UndoStack> undoStack_;
    std::unique_ptr<SyntaxHighlighter> highlighter_;
    std::unique_ptr<FindReplace> findReplace_;
    std::unique_ptr<Minimap> minimap_;
    
    // Cursors
    std::vector<Cursor> cursors_;
    Cursor& primaryCursor() { return cursors_.front(); }
    
    // View state
    int firstVisibleLine_ = 0;
    int visibleLineCount_ = 0;
    std::vector<RenderLine> renderLines_;
    bool needsFullRebuild_ = true;
    
    // Font
    HFONT hFont_ = nullptr;
    int charWidth_ = 9;
    int lineHeight_ = 20;
    
    // Input state
    bool isSelecting_ = false;
    int selectionStartX_ = 0;
    int selectionStartY_ = 0;
    
    // Blink
    bool cursorVisible_ = true;
    std::chrono::steady_clock::time_point lastBlink_;
    
    // Find state
    std::wstring findPattern_;
    std::vector<SearchResult> findResults_;
    size_t currentFindResult_ = 0;
    
    // Callbacks
    ChangeCallback onChange_;
    CursorCallback onCursorChange_;
    
    // Internal methods
    void rebuildRenderLines();
    void updateRenderLine(size_t line);
    void updateVisibleRange();
    
    void drawLine(HDC hdc, const RenderLine& line, int y, bool isCurrentLine);
    void drawSelection(HDC hdc, size_t start, size_t end);
    void drawCursor(HDC hdc);
    void drawMinimap(HDC hdc);
    void drawLineNumber(HDC hdc, int line, int y, bool isCurrent);
    void drawFindHighlights(HDC hdc);
    
    void createFont();
    void measureFont();
    
    size_t posFromPoint(int x, int y) const;
    int lineFromY(int y) const;
    int colFromX(int x, int line) const;
    int xFromCol(int col, int line) const;
    int yFromLine(int line) const;
    
    bool isWordChar(wchar_t ch) const;
    size_t wordStart(size_t pos) const;
    size_t wordEnd(size_t pos) const;
    
    void clampCursor();
    void updateDesiredColumn();
    
    // Auto pairs
    bool autoClose(wchar_t ch);
    wchar_t getCloseChar(wchar_t open) const;
};

// =============================================================================
// WINDOW CLASS (Win32)
// =============================================================================

class EditorWindow {
public:
    static const wchar_t* ClassName() { return L"MiniMonacoEditor"; }
    
    static bool Register(HINSTANCE hInstance);
    static Editor* Create(HWND parent, int x, int y, int width, int height, const Config& config = {});
    static Editor* FromHwnd(HWND hwnd);
    
private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static std::unordered_map<HWND, std::unique_ptr<Editor>> editors_;
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

namespace Utils {
    std::wstring utf8ToWide(const std::string& utf8);
    std::string wideToUtf8(const std::wstring& wide);
    std::vector<std::wstring> split(const std::wstring& text, wchar_t delim);
    std::wstring join(const std::vector<std::wstring>& parts, const std::wstring& delim);
    std::wstring trim(const std::wstring& text);
    bool isWhitespace(wchar_t ch);
    bool isAlphaNumeric(wchar_t ch);
}

} // namespace MiniMonaco
