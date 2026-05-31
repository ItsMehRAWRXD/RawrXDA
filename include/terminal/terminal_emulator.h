#pragma once
/**
 * @file terminal_emulator.h
 * @brief Terminal emulation with ANSI support
 * Batch 5 - Item 64: Terminal emulator
 */

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <windows.h>

namespace RawrXD::Terminal {

enum class TerminalColor {
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite
};

struct Cell {
    char character;
    uint32_t foreground;
    uint32_t background;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
};

struct TerminalLine {
    std::vector<Cell> cells;
    bool wrapped;
};

struct Cursor {
    int row;
    int col;
    bool visible;
    bool blink;
    int style;
};

struct ScrollRegion {
    int top;
    int bottom;
};

class TerminalEmulator {
public:
    TerminalEmulator();
    ~TerminalEmulator();

    // Initialization
    bool initialize(int rows, int cols);
    void shutdown();

    // Size
    void resize(int rows, int cols);
    int getRows() const;
    int getCols() const;

    // Input
    void write(const std::string& data);
    void write(const char* data, size_t len);
    void processInput(const std::string& input);

    // Output
    std::string readOutput();
    void clear();
    void clearLine();
    void clearScreen();

    // Cursor
    void setCursorPosition(int row, int col);
    void getCursorPosition(int& row, int& col) const;
    void showCursor(bool show);
    void setCursorStyle(int style);

    // Scrolling
    void scrollUp(int lines);
    void scrollDown(int lines);
    void setScrollRegion(int top, int bottom);
    void resetScrollRegion();

    // Content
    std::string getLine(int row) const;
    std::string getScreen() const;
    std::vector<TerminalLine> getLines() const;
    std::deque<std::string> getScrollback() const;

    // Selection
    void setSelection(int startRow, int startCol, int endRow, int endCol);
    void clearSelection();
    std::string getSelectedText() const;

    // Attributes
    void setForegroundColor(uint32_t color);
    void setBackgroundColor(uint32_t color);
    void setBold(bool bold);
    void setItalic(bool italic);
    void setUnderline(bool underline);
    void resetAttributes();

    // Events
    using ResizeCallback = std::function<void(int rows, int cols)>;
    using TitleCallback = std::function<void(const std::string& title)>;
    using BellCallback = std::function<void()>;
    void onResize(ResizeCallback callback);
    void onTitleChange(TitleCallback callback);
    void onBell(BellCallback callback);

private:
    int m_rows{24};
    int m_cols{80};
    std::vector<TerminalLine> m_screen;
    std::deque<std::string> m_scrollback;
    Cursor m_cursor;
    ScrollRegion m_scrollRegion;
    Cell m_currentAttributes;
    std::string m_escapeSequence;
    bool m_inEscape{false};

    ResizeCallback m_resizeCallback;
    TitleCallback m_titleCallback;
    BellCallback m_bellCallback;

    void processByte(char c);
    void processEscapeSequence(const std::string& seq);
    void processCSI(const std::string& seq);
    void processOSC(const std::string& seq);
    void ensureLine(int row);
    void scrollUp();
    void scrollDown();
    void insertLine(int row);
    void deleteLine(int row);
    void insertChar(int row, int col);
    void deleteChar(int row, int col);
    void setChar(int row, int col, char c);
    uint32_t ansiColorToRgb(int color);
};

// Global instance
TerminalEmulator& getTerminalEmulator();

} // namespace RawrXD::Terminal
