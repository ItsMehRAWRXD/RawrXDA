/**
 * @file terminal_emulator.cpp
 * @brief Terminal emulation with ANSI support implementation
 * Batch 5 - Item 64: Terminal emulator
 */

#include "terminal/terminal_emulator.h"
#include <cstring>
#include <algorithm>

namespace RawrXD::Terminal {

TerminalEmulator::TerminalEmulator()
    : m_rows(24)
    , m_cols(80)
    , m_scrollTop(0)
    , m_scrollBottom(23)
    , m_escapeState(EscapeState::Normal)
    , m_currentAttribute(0)
    , m_bold(false)
    , m_italic(false)
    , m_underline(false)
    , m_strikethrough(false)
    , m_fgColor(7)  // White
    , m_bgColor(0)  // Black
    , m_savedRow(0)
    , m_savedCol(0) {
    m_cursor = {0, 0, true, true, 0};
    clear();
}

TerminalEmulator::~TerminalEmulator() {
}

bool TerminalEmulator::initialize(int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    m_scrollTop = 0;
    m_scrollBottom = rows - 1;
    
    resize(rows, cols);
    clear();
    
    return true;
}

void TerminalEmulator::shutdown() {
    m_lines.clear();
    m_scrollback.clear();
}

void TerminalEmulator::resize(int rows, int cols) {
    if (rows <= 0 || cols <= 0) return;
    
    int oldRows = m_rows;
    int oldCols = m_cols;
    
    m_rows = rows;
    m_cols = cols;
    m_scrollBottom = rows - 1;
    
    // Resize existing lines
    for (auto& line : m_lines) {
        line.cells.resize(cols);
    }
    
    // Add or remove lines as needed
    if (rows > oldRows) {
        for (int i = oldRows; i < rows; i++) {
            TerminalLine line;
            line.cells.resize(cols);
            for (auto& cell : line.cells) {
                cell.character = ' ';
                cell.foreground = m_fgColor;
                cell.background = m_bgColor;
                cell.bold = false;
                cell.italic = false;
                cell.underline = false;
                cell.strikethrough = false;
            }
            m_lines.push_back(line);
        }
    } else if (rows < oldRows) {
        m_lines.resize(rows);
    }
    
    // Ensure cursor is within bounds
    if (m_cursor.row >= rows) m_cursor.row = rows - 1;
    if (m_cursor.col >= cols) m_cursor.col = cols - 1;
}

int TerminalEmulator::getRows() const {
    return m_rows;
}

int TerminalEmulator::getCols() const {
    return m_cols;
}

void TerminalEmulator::write(const std::string& data) {
    write(data.c_str(), data.length());
}

void TerminalEmulator::write(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        processByte(data[i]);
    }
}

void TerminalEmulator::processInput(const std::string& input) {
    // Process keyboard input and convert to terminal sequences
    for (char c : input) {
        if (c == '\r') {
            m_cursor.col = 0;
        } else if (c == '\n') {
            m_cursor.row++;
            if (m_cursor.row > m_scrollBottom) {
                scrollUp(1);
                m_cursor.row = m_scrollBottom;
            }
        } else if (c == '\t') {
            // Tab to next 8-column boundary
            int nextTab = ((m_cursor.col / 8) + 1) * 8;
            m_cursor.col = std::min(nextTab, m_cols - 1);
        } else if (c == '\b') {
            if (m_cursor.col > 0) {
                m_cursor.col--;
            }
        } else if (c >= 32 && c < 127) {
            // Printable character
            if (m_cursor.row >= 0 && m_cursor.row < m_rows &&
                m_cursor.col >= 0 && m_cursor.col < m_cols) {
                Cell& cell = m_lines[m_cursor.row].cells[m_cursor.col];
                cell.character = c;
                cell.foreground = m_fgColor;
                cell.background = m_bgColor;
                cell.bold = m_bold;
                cell.italic = m_italic;
                cell.underline = m_underline;
                cell.strikethrough = m_strikethrough;
                
                m_cursor.col++;
                if (m_cursor.col >= m_cols) {
                    m_cursor.col = 0;
                    m_cursor.row++;
                    if (m_cursor.row > m_scrollBottom) {
                        scrollUp(1);
                        m_cursor.row = m_scrollBottom;
                    }
                }
            }
        }
    }
}

void TerminalEmulator::clear() {
    m_lines.clear();
    m_lines.resize(m_rows);
    
    for (auto& line : m_lines) {
        line.cells.resize(m_cols);
        line.wrapped = false;
        for (auto& cell : line.cells) {
            cell.character = ' ';
            cell.foreground = m_fgColor;
            cell.background = m_bgColor;
            cell.bold = false;
            cell.italic = false;
            cell.underline = false;
            cell.strikethrough = false;
        }
    }
    
    m_cursor.row = 0;
    m_cursor.col = 0;
}

void TerminalEmulator::clearLine() {
    if (m_cursor.row >= 0 && m_cursor.row < m_rows) {
        for (auto& cell : m_lines[m_cursor.row].cells) {
            cell.character = ' ';
            cell.foreground = m_fgColor;
            cell.background = m_bgColor;
        }
    }
}

void TerminalEmulator::clearScreen() {
    clear();
}

void TerminalEmulator::setCursorPosition(int row, int col) {
    m_cursor.row = std::max(0, std::min(row, m_rows - 1));
    m_cursor.col = std::max(0, std::min(col, m_cols - 1));
}

void TerminalEmulator::getCursorPosition(int& row, int& col) const {
    row = m_cursor.row;
    col = m_cursor.col;
}

void TerminalEmulator::showCursor(bool show) {
    m_cursor.visible = show;
}

void TerminalEmulator::setCursorStyle(int style) {
    m_cursor.style = style;
}

void TerminalEmulator::scrollUp(int lines) {
    for (int i = 0; i < lines && !m_lines.empty(); i++) {
        // Save top line to scrollback if needed
        if (!m_lines.empty()) {
            m_scrollback.push_back(m_lines[0]);
            if (m_scrollback.size() > 1000) {
                m_scrollback.pop_front();
            }
        }
        
        // Remove top line and add new line at bottom
        m_lines.erase(m_lines.begin());
        
        TerminalLine newLine;
        newLine.cells.resize(m_cols);
        for (auto& cell : newLine.cells) {
            cell.character = ' ';
            cell.foreground = m_fgColor;
            cell.background = m_bgColor;
        }
        m_lines.push_back(newLine);
    }
}

void TerminalEmulator::scrollDown(int lines) {
    for (int i = 0; i < lines && !m_lines.empty(); i++) {
        m_lines.pop_back();
        
        TerminalLine newLine;
        newLine.cells.resize(m_cols);
        for (auto& cell : newLine.cells) {
            cell.character = ' ';
            cell.foreground = m_fgColor;
            cell.background = m_bgColor;
        }
        m_lines.insert(m_lines.begin(), newLine);
    }
}

void TerminalEmulator::setScrollRegion(int top, int bottom) {
    m_scrollTop = std::max(0, top);
    m_scrollBottom = std::min(bottom, m_rows - 1);
}

void TerminalEmulator::resetScrollRegion() {
    m_scrollTop = 0;
    m_scrollBottom = m_rows - 1;
}

std::string TerminalEmulator::getLine(int row) const {
    if (row < 0 || row >= m_rows) return "";
    
    std::string result;
    for (const auto& cell : m_lines[row].cells) {
        result += cell.character;
    }
    
    // Trim trailing spaces
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

std::string TerminalEmulator::getSelectedText(int startRow, int startCol, int endRow, int endCol) const {
    std::string result;
    
    for (int row = startRow; row <= endRow && row < m_rows; row++) {
        int colStart = (row == startRow) ? startCol : 0;
        int colEnd = (row == endRow) ? endCol : m_cols - 1;
        
        for (int col = colStart; col <= colEnd && col < m_cols; col++) {
            result += m_lines[row].cells[col].character;
        }
        
        if (row < endRow) {
            result += '\n';
        }
    }
    
    return result;
}

std::vector<TerminalLine> TerminalEmulator::getVisibleLines() const {
    return m_lines;
}

void TerminalEmulator::processByte(char c) {
    switch (m_escapeState) {
        case EscapeState::Normal:
            if (c == '\x1b') {
                m_escapeState = EscapeState::Escape;
                m_escapeSequence.clear();
            } else {
                processInput(std::string(1, c));
            }
            break;
            
        case EscapeState::Escape:
            if (c == '[') {
                m_escapeState = EscapeState::CSI;
                m_escapeSequence.clear();
            } else if (c == ']') {
                m_escapeState = EscapeState::OSC;
                m_escapeSequence.clear();
            } else {
                // Other escape sequences
                m_escapeState = EscapeState::Normal;
            }
            break;
            
        case EscapeState::CSI:
            if ((c >= '0' && c <= '9') || c == ';' || c == '?') {
                m_escapeSequence += c;
            } else {
                // CSI command
                processCSISequence(c);
                m_escapeState = EscapeState::Normal;
            }
            break;
            
        case EscapeState::OSC:
            if (c == '\x07' || c == '\x1b') {
                // OSC end
                m_escapeState = EscapeState::Normal;
            } else {
                m_escapeSequence += c;
            }
            break;
    }
}

void TerminalEmulator::processCSISequence(char command) {
    std::vector<int> params;
    std::string currentParam;
    
    for (char c : m_escapeSequence) {
        if (c == ';') {
            params.push_back(currentParam.empty() ? 0 : std::stoi(currentParam));
            currentParam.clear();
        } else if (c >= '0' && c <= '9') {
            currentParam += c;
        }
    }
    
    if (!currentParam.empty()) {
        params.push_back(std::stoi(currentParam));
    }
    
    switch (command) {
        case 'H': // Cursor position
        case 'f':
            setCursorPosition(
                params.size() > 0 ? params[0] - 1 : 0,
                params.size() > 1 ? params[1] - 1 : 0
            );
            break;
            
        case 'J': // Erase display
            if (params.empty() || params[0] == 0) {
                // Clear from cursor to end
                clearFromCursor();
            } else if (params[0] == 1) {
                // Clear from beginning to cursor
                clearToCursor();
            } else if (params[0] == 2) {
                clearScreen();
            }
            break;
            
        case 'K': // Erase line
            if (params.empty() || params[0] == 0) {
                clearLineFromCursor();
            } else if (params[0] == 1) {
                clearLineToCursor();
            } else if (params[0] == 2) {
                clearLine();
            }
            break;
            
        case 'm': // SGR - Select Graphic Rendition
            processSGR(params);
            break;
            
        case 'A': // Cursor up
            m_cursor.row = std::max(0, m_cursor.row - (params.empty() ? 1 : params[0]));
            break;
            
        case 'B': // Cursor down
            m_cursor.row = std::min(m_rows - 1, m_cursor.row + (params.empty() ? 1 : params[0]));
            break;
            
        case 'C': // Cursor forward
            m_cursor.col = std::min(m_cols - 1, m_cursor.col + (params.empty() ? 1 : params[0]));
            break;
            
        case 'D': // Cursor backward
            m_cursor.col = std::max(0, m_cursor.col - (params.empty() ? 1 : params[0]));
            break;
            
        case 's': // Save cursor position
            m_savedRow = m_cursor.row;
            m_savedCol = m_cursor.col;
            break;
            
        case 'u': // Restore cursor position
            m_cursor.row = m_savedRow;
            m_cursor.col = m_savedCol;
            break;
    }
}

void TerminalEmulator::processSGR(const std::vector<int>& params) {
    if (params.empty()) {
        // Reset
        m_bold = false;
        m_italic = false;
        m_underline = false;
        m_strikethrough = false;
        m_fgColor = 7;
        m_bgColor = 0;
        return;
    }
    
    for (size_t i = 0; i < params.size(); i++) {
        int param = params[i];
        
        switch (param) {
            case 0: // Reset
                m_bold = false;
                m_italic = false;
                m_underline = false;
                m_strikethrough = false;
                m_fgColor = 7;
                m_bgColor = 0;
                break;
            case 1: // Bold
                m_bold = true;
                break;
            case 3: // Italic
                m_italic = true;
                break;
            case 4: // Underline
                m_underline = true;
                break;
            case 9: // Strikethrough
                m_strikethrough = true;
                break;
            case 22: // Normal intensity
                m_bold = false;
                break;
            case 23: // Not italic
                m_italic = false;
                break;
            case 24: // Not underlined
                m_underline = false;
                break;
            case 29: // Not strikethrough
                m_strikethrough = false;
                break;
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                // Foreground colors 0-7
                m_fgColor = param - 30;
                break;
            case 38: // Extended foreground color
                if (i + 2 < params.size() && params[i + 1] == 5) {
                    m_fgColor = params[i + 2];
                    i += 2;
                }
                break;
            case 39: // Default foreground
                m_fgColor = 7;
                break;
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                // Background colors 0-7
                m_bgColor = param - 40;
                break;
            case 48: // Extended background color
                if (i + 2 < params.size() && params[i + 1] == 5) {
                    m_bgColor = params[i + 2];
                    i += 2;
                }
                break;
            case 49: // Default background
                m_bgColor = 0;
                break;
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                // Bright foreground colors
                m_fgColor = param - 90 + 8;
                break;
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                // Bright background colors
                m_bgColor = param - 100 + 8;
                break;
        }
    }
}

void TerminalEmulator::clearFromCursor() {
    if (m_cursor.row >= 0 && m_cursor.row < m_rows) {
        // Clear from cursor to end of line
        for (int col = m_cursor.col; col < m_cols; col++) {
            m_lines[m_cursor.row].cells[col].character = ' ';
        }
        
        // Clear lines below
        for (int row = m_cursor.row + 1; row < m_rows; row++) {
            for (auto& cell : m_lines[row].cells) {
                cell.character = ' ';
            }
        }
    }
}

void TerminalEmulator::clearToCursor() {
    // Clear from beginning to cursor
    for (int row = 0; row <= m_cursor.row && row < m_rows; row++) {
        int endCol = (row == m_cursor.row) ? m_cursor.col : m_cols - 1;
        for (int col = 0; col <= endCol; col++) {
            m_lines[row].cells[col].character = ' ';
        }
    }
}

void TerminalEmulator::clearLineFromCursor() {
    if (m_cursor.row >= 0 && m_cursor.row < m_rows) {
        for (int col = m_cursor.col; col < m_cols; col++) {
            m_lines[m_cursor.row].cells[col].character = ' ';
        }
    }
}

void TerminalEmulator::clearLineToCursor() {
    if (m_cursor.row >= 0 && m_cursor.row < m_rows) {
        for (int col = 0; col <= m_cursor.col; col++) {
            m_lines[m_cursor.row].cells[col].character = ' ';
        }
    }
}

} // namespace RawrXD::Terminal
