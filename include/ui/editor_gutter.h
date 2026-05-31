#pragma once
/**
 * @file editor_gutter.h
 * @brief Line numbers and folding indicators
 * Batch 4 - Item 59: Editor gutter
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class FoldState {
    None,
    Collapsed,
    Expanded,
    Collapsible
};

struct FoldRegion {
    int startLine;
    int endLine;
    FoldState state;
    int level;
};

struct GutterLine {
    int lineNumber;
    bool isCurrentLine;
    bool hasBreakpoint;
    bool hasBookmark;
    FoldState foldState;
    int foldLevel;
    std::string glyph;
};

class EditorGutter {
public:
    EditorGutter();
    ~EditorGutter();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);
    void setPosition(int x, int y);

    // Line numbers
    void setLineCount(int count);
    void setCurrentLine(int line);
    int getCurrentLine() const;

    // Line info
    void setLineInfo(int line, const GutterLine& info);
    void clearLineInfo(int line);
    void clearAllLineInfo();

    // Folding
    void setFoldRegions(const std::vector<FoldRegion>& regions);
    void setFoldState(int line, FoldState state);
    void toggleFold(int line);
    void expandAll();
    void collapseAll();
    void expandLevel(int level);

    // Breakpoints
    void setBreakpoint(int line, bool set);
    void toggleBreakpoint(int line);
    bool hasBreakpoint(int line) const;
    void clearAllBreakpoints();
    std::vector<int> getBreakpoints() const;

    // Bookmarks
    void setBookmark(int line, bool set);
    void toggleBookmark(int line);
    bool hasBookmark(int line) const;
    void clearAllBookmarks();
    std::vector<int> getBookmarks() const;
    void nextBookmark();
    void previousBookmark();

    // Rendering
    void render();
    void invalidate();
    void invalidateLine(int line);

    // Configuration
    void setLineNumbersVisible(bool visible);
    bool areLineNumbersVisible() const;
    void setFoldingVisible(bool visible);
    bool isFoldingVisible() const;
    void setWidth(int width);
    int getWidth() const;
    void setFont(HFONT font);

    // Events
    using LineClickCallback = std::function<void(int line, int x)>;
    using FoldToggleCallback = std::function<void(int line)>;
    void onLineClick(LineClickCallback callback);
    void onFoldToggle(FoldToggleCallback callback);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    int m_width{60};
    int m_height{0};
    int m_lineCount{0};
    int m_currentLine{0};
    int m_lineHeight{20};
    bool m_showLineNumbers{true};
    bool m_showFolding{true};
    HFONT m_font{nullptr};
    std::map<int, GutterLine> m_lineInfo;
    std::vector<FoldRegion> m_foldRegions;

    LineClickCallback m_lineClickCallback;
    FoldToggleCallback m_foldToggleCallback;

    void layout();
    void draw(HDC hdc);
    void drawLineNumber(HDC hdc, int line, RECT& rect);
    void drawFoldIndicator(HDC hdc, int line, RECT& rect);
    void drawGlyph(HDC hdc, const std::string& glyph, RECT& rect);
    int getLineAtY(int y) const;
    int getFoldIndicatorWidth() const;
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
EditorGutter& getEditorGutter();

} // namespace RawrXD::UI
