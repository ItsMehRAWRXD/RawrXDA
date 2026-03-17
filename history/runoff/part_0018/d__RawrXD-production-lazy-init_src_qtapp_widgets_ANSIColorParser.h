// ANSIColorParser.h - ANSI Escape Sequence Parser
// Part of RawrXD Agentic IDE - Phase 6 (Terminal Integration)
// Parses and applies ANSI color codes to terminal output

#ifndef ANSICOLORPARSER_H
#define ANSICOLORPARSER_H

#include <QString>
#include <QList>
#include <QColor>
#include <QTextCharFormat>
#include <QMap>

// ============================================================================
// ANSI Attribute Definitions
// ============================================================================

enum class ANSIForeground {
    Default = 39,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

enum class ANSIBackground {
    Default = 49,
    Black = 40,
    Red = 41,
    Green = 42,
    Yellow = 43,
    Blue = 44,
    Magenta = 45,
    Cyan = 46,
    White = 47,
    BrightBlack = 100,
    BrightRed = 101,
    BrightGreen = 102,
    BrightYellow = 103,
    BrightBlue = 104,
    BrightMagenta = 105,
    BrightCyan = 106,
    BrightWhite = 107
};

enum class ANSIAttribute {
    Reset = 0,
    Bold = 1,
    Dim = 2,
    Italic = 3,
    Underline = 4,
    Blink = 5,
    Reverse = 7,
    Hidden = 8,
    Strikethrough = 9
};

// ============================================================================
// ANSI State Structure
// ============================================================================

struct ANSIState {
    ANSIForeground foreground = ANSIForeground::Default;
    ANSIBackground background = ANSIBackground::Default;
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool blink = false;
    bool reverse = false;
    bool hidden = false;
    bool strikethrough = false;

    // Convert state to QTextCharFormat
    QTextCharFormat toFormat() const;

    // Reset all attributes
    void reset();
};

// ============================================================================
// Segment Structure
// ============================================================================

struct ANSISegment {
    QString text;
    ANSIState state;
};

// ============================================================================
// ANSI Color Parser Class
// ============================================================================

class ANSIColorParser
{
public:
    ANSIColorParser();
    ~ANSIColorParser();

    // Parse ANSI escape sequences
    QList<ANSISegment> parse(const QString& text);
    
    // Parse single line (faster for streaming)
    QList<ANSISegment> parseLine(const QString& line);

    // Convert ANSI code to state change
    void applyANSICode(int code, ANSIState& state);

    // Get current state
    ANSIState getCurrentState() const { return m_currentState; }
    void setCurrentState(const ANSIState& state) { m_currentState = state; }
    void resetState() { m_currentState.reset(); }

    // Extract plain text (remove ANSI codes)
    QString stripANSICodes(const QString& text);

    // Check if string contains ANSI codes
    bool hasANSICodes(const QString& text) const;

    // Get color from code
    static QColor getANSIColor(ANSIForeground fg, bool isBright = false);
    static QColor getANSIColor(ANSIBackground bg, bool isBright = false);

private:
    // Helper methods
    QList<int> parseANSISequence(const QString& sequence);
    QColor foregroundToColor(ANSIForeground fg) const;
    QColor backgroundToColor(ANSIBackground bg) const;
    QString extractText(const QString& text, int& pos);

    // State
    ANSIState m_currentState;
    static const QString ANSI_ESCAPE;
    static const QMap<int, ANSIForeground> FOREGROUND_MAP;
    static const QMap<int, ANSIBackground> BACKGROUND_MAP;
};

#endif // ANSICOLORPARSER_H
