// ============================================================================
// Win32IDE_TerminalAnsi.cpp — ANSI/VT100 Escape Sequence Parser for Terminal
// ============================================================================
// Parses ANSI SGR (Select Graphic Rendition) escape codes in terminal output
// and renders colored text to RichEdit terminal panes via CHARFORMAT2W.
//
// Supports: reset, bold, dim, italic, underline, 16-color fg/bg,
//           256-color (ESC[38;5;Nm / ESC[48;5;Nm),
//           24-bit RGB (ESC[38;2;R;G;Bm / ESC[48;2;R;G;Bm).
//
// Non-SGR CSI sequences (cursor movement, erase, scrolling) are stripped.
// Partial escape sequences across chunk boundaries are buffered.
//
// Integration: Win32IDE::appendTerminalTextAnsi() replaces raw appendText()
//              in onTerminalOutput() and onTerminalError().
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#ifndef CP_UNICODE
#define CP_UNICODE 1200
#endif
#include <algorithm>
#include <cstdlib>
#include <cstring>


#ifndef EM_GETLIMITTEXT
#define EM_GETLIMITTEXT 0x00D5
#endif

namespace
{

// Drop oldest RichEdit content when approaching EM_EXLIMITTEXT so inserts stay reliable
// and memory stays bounded. Uses UTF-16 code unit boundaries (surrogate pairs).
void trimRichEditHeadIfNeeded(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    LRESULT lim = SendMessage(hwnd, EM_GETLIMITTEXT, 0, 0);
    if (lim <= 0)
        lim = 4 * 1024 * 1024;

    GETTEXTLENGTHEX gtl = {};
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_UNICODE;
    const LONG curLen = static_cast<LONG>(SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0));
    if (curLen <= 0)
        return;

    // Keep at most ~80% of limit; trim from start when over that.
    const LONG softMax = static_cast<LONG>((static_cast<unsigned long long>(lim) * 8ull) / 10ull);
    if (curLen <= softMax)
        return;

    LONG trimEnd = curLen - softMax;
    if (trimEnd <= 0)
        return;

    // Do not split a UTF-16 surrogate pair at the cut index.
    if (trimEnd > 0 && trimEnd < curLen)
    {
        WCHAR w = 0;
        TEXTRANGEW tr = {};
        tr.chrg.cpMin = trimEnd - 1;
        tr.chrg.cpMax = trimEnd;
        tr.lpstrText = &w;
        SendMessage(hwnd, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr));
        if (w >= 0xD800 && w <= 0xDBFF)
            ++trimEnd;
    }

    SendMessage(hwnd, EM_SETSEL, 0, static_cast<LPARAM>(trimEnd));
    SendMessage(hwnd, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));
}

}  // namespace

// ============================================================================
// ANSI Standard 16-color Palette (SGR 30-37, 40-47, 90-97, 100-107)
// Maps to typical dark terminal theme (VS Code Dark+ compatible)
// ============================================================================
static const COLORREF kAnsi16[16] = {
    RGB(0, 0, 0),        // 0  Black
    RGB(205, 49, 49),    // 1  Red
    RGB(13, 188, 121),   // 2  Green
    RGB(229, 229, 16),   // 3  Yellow
    RGB(36, 114, 200),   // 4  Blue
    RGB(188, 63, 188),   // 5  Magenta
    RGB(17, 168, 205),   // 6  Cyan
    RGB(229, 229, 229),  // 7  White
    RGB(102, 102, 102),  // 8  Bright Black
    RGB(241, 76, 76),    // 9  Bright Red
    RGB(35, 209, 139),   // 10 Bright Green
    RGB(245, 245, 67),   // 11 Bright Yellow
    RGB(59, 142, 234),   // 12 Bright Blue
    RGB(214, 112, 214),  // 13 Bright Magenta
    RGB(41, 184, 219),   // 14 Bright Cyan
    RGB(229, 229, 229),  // 15 Bright White
};

// ============================================================================
// 256-color palette builder (indices 0-15 = kAnsi16, 16-231 = 6x6x6 cube,
// 232-255 = greyscale ramp)
// ============================================================================
static COLORREF ansi256Color(int idx)
{
    if (idx < 0 || idx > 255)
        return RGB(229, 229, 229);  // default white
    if (idx < 16)
        return kAnsi16[idx];
    if (idx < 232)
    {
        // 6x6x6 color cube: index = 16 + 36*r + 6*g + b  (r,g,b in 0..5)
        int ci = idx - 16;
        int b = ci % 6;
        int g = (ci / 6) % 6;
        int r = ci / 36;
        auto s = [](int v) -> int { return v ? 55 + 40 * v : 0; };
        return RGB(s(r), s(g), s(b));
    }
    // Greyscale ramp: 232..255 → grey 8..238
    int grey = 8 + 10 * (idx - 232);
    return RGB(grey, grey, grey);
}

// ============================================================================
// Per-terminal ANSI state — tracks the current SGR attributes between chunks
// ============================================================================
struct AnsiState
{
    COLORREF fg = RGB(204, 204, 204);  // default terminal fg
    COLORREF bg = RGB(30, 30, 30);     // default terminal bg
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool fgSet = false;  // explicit fg was set
    bool bgSet = false;  // explicit bg was set

    void reset()
    {
        fg = RGB(204, 204, 204);
        bg = RGB(30, 30, 30);
        bold = false;
        dim = false;
        italic = false;
        underline = false;
        fgSet = false;
        bgSet = false;
    }
};

// ============================================================================
// Segment — a run of text with uniform SGR attributes
// ============================================================================
struct AnsiSegment
{
    std::wstring text;
    COLORREF fg;
    COLORREF bg;
    bool bold;
    bool italic;
    bool underline;
};

// ============================================================================
// SGR parameter parser — process one SGR sequence parameter list
// ============================================================================
static void applySgrParams(const int* params, int count, AnsiState& st)
{
    for (int i = 0; i < count; ++i)
    {
        int p = params[i];
        switch (p)
        {
            case 0:
                st.reset();
                break;

            case 1:
                st.bold = true;
                break;
            case 2:
                st.dim = true;
                break;
            case 3:
                st.italic = true;
                break;
            case 4:
                st.underline = true;
                break;

            case 21:
                st.bold = false;
                break;
            case 22:
                st.bold = false;
                st.dim = false;
                break;
            case 23:
                st.italic = false;
                break;
            case 24:
                st.underline = false;
                break;

            // Standard foreground (30-37)
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
                st.fg = kAnsi16[p - 30];
                st.fgSet = true;
                break;

            // Default foreground
            case 39:
                st.fg = RGB(204, 204, 204);
                st.fgSet = false;
                break;

            // Standard background (40-47)
            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
                st.bg = kAnsi16[p - 40];
                st.bgSet = true;
                break;

            // Default background
            case 49:
                st.bg = RGB(30, 30, 30);
                st.bgSet = false;
                break;

            // Bright foreground (90-97)
            case 90:
            case 91:
            case 92:
            case 93:
            case 94:
            case 95:
            case 96:
            case 97:
                st.fg = kAnsi16[p - 90 + 8];
                st.fgSet = true;
                break;

            // Bright background (100-107)
            case 100:
            case 101:
            case 102:
            case 103:
            case 104:
            case 105:
            case 106:
            case 107:
                st.bg = kAnsi16[p - 100 + 8];
                st.bgSet = true;
                break;

            // Extended color: 38;5;N (256-color) or 38;2;R;G;B (truecolor)
            case 38:
                if (i + 1 < count && params[i + 1] == 5 && i + 2 < count)
                {
                    st.fg = ansi256Color(params[i + 2]);
                    st.fgSet = true;
                    i += 2;
                }
                else if (i + 1 < count && params[i + 1] == 2 && i + 4 < count)
                {
                    int r = std::clamp(params[i + 2], 0, 255);
                    int g = std::clamp(params[i + 3], 0, 255);
                    int b = std::clamp(params[i + 4], 0, 255);
                    st.fg = RGB(r, g, b);
                    st.fgSet = true;
                    i += 4;
                }
                break;

            // Extended background: 48;5;N or 48;2;R;G;B
            case 48:
                if (i + 1 < count && params[i + 1] == 5 && i + 2 < count)
                {
                    st.bg = ansi256Color(params[i + 2]);
                    st.bgSet = true;
                    i += 2;
                }
                else if (i + 1 < count && params[i + 1] == 2 && i + 4 < count)
                {
                    int r = std::clamp(params[i + 2], 0, 255);
                    int g = std::clamp(params[i + 3], 0, 255);
                    int b = std::clamp(params[i + 4], 0, 255);
                    st.bg = RGB(r, g, b);
                    st.bgSet = true;
                    i += 4;
                }
                break;

            default:
                break;  // Ignore unknown SGR codes
        }
    }
}

// ============================================================================
// Parse ANSI escape sequences from raw terminal output.
// Returns a vector of text segments with resolved color attributes.
// `carry` is the partial sequence buffer from the previous chunk.
// ============================================================================
static std::vector<AnsiSegment> parseAnsiOutput(const std::string& input, AnsiState& state, std::string& carry)
{
    std::vector<AnsiSegment> segments;

    // Prepend any leftover partial escape from previous chunk
    std::string data = carry + input;
    carry.clear();

    std::wstring currentText;
    const size_t len = data.size();
    size_t pos = 0;

    auto flushText = [&]()
    {
        if (!currentText.empty())
        {
            AnsiSegment seg;
            seg.text = std::move(currentText);
            seg.fg = state.fg;
            seg.bg = state.bg;
            seg.bold = state.bold;
            seg.italic = state.italic;
            seg.underline = state.underline;
            segments.push_back(std::move(seg));
            currentText.clear();
        }
    };

    while (pos < len)
    {
        char ch = data[pos];

        // ESC detected — start of escape sequence
        if (ch == '\x1b')
        {
            // Check if we have enough data to determine sequence type
            if (pos + 1 >= len)
            {
                // Partial: ESC at end of chunk, carry it
                carry = data.substr(pos);
                break;
            }

            char next = data[pos + 1];

            if (next == '[')
            {
                // CSI sequence: ESC [ <params> <final byte>
                size_t csiStart = pos + 2;
                size_t csiEnd = csiStart;

                // Scan for the final byte (0x40-0x7E range)
                while (csiEnd < len && (data[csiEnd] < 0x40 || data[csiEnd] > 0x7E))
                    ++csiEnd;

                if (csiEnd >= len)
                {
                    // Partial CSI — carry the rest
                    carry = data.substr(pos);
                    break;
                }

                char finalByte = data[csiEnd];

                if (finalByte == 'm')
                {
                    // SGR sequence — parse parameters
                    flushText();

                    std::string paramStr(data.data() + csiStart, csiEnd - csiStart);
                    int params[32];
                    int paramCount = 0;

                    if (paramStr.empty())
                    {
                        // ESC[m is equivalent to ESC[0m
                        params[0] = 0;
                        paramCount = 1;
                    }
                    else
                    {
                        // Parse semicolon-separated parameters
                        const char* p = paramStr.c_str();
                        while (*p && paramCount < 32)
                        {
                            char* end = nullptr;
                            long val = strtol(p, &end, 10);
                            if (end == p)
                            {
                                ++p;  // skip separator
                                continue;
                            }
                            params[paramCount++] = (int)val;
                            p = end;
                            if (*p == ';' || *p == ':')
                                ++p;
                        }
                    }

                    applySgrParams(params, paramCount, state);
                }
                else
                {
                    // Non-SGR CSI (cursor movement, erase, etc.) — strip it
                }

                pos = csiEnd + 1;
                continue;
            }

            if (next == ']')
            {
                // OSC sequence: ESC ] ... ST (string terminator)
                // ST is either ESC \ or BEL (\x07)
                size_t oscEnd = pos + 2;
                while (oscEnd < len)
                {
                    if (data[oscEnd] == '\x07')
                    {
                        ++oscEnd;
                        break;
                    }
                    if (data[oscEnd] == '\x1b' && oscEnd + 1 < len && data[oscEnd + 1] == '\\')
                    {
                        oscEnd += 2;
                        break;
                    }
                    ++oscEnd;
                }
                if (oscEnd >= len && data[len - 1] != '\x07')
                {
                    // Partial OSC — carry
                    carry = data.substr(pos);
                    break;
                }
                pos = oscEnd;
                continue;
            }

            // Simple 2-byte escape (ESC followed by letter) — skip
            pos += 2;
            continue;
        }

        // Regular UTF-8 character → convert to wide and accumulate
        // Handle multi-byte UTF-8 properly
        unsigned char uch = (unsigned char)ch;
        int byteCount = 1;
        if (uch >= 0xC0 && uch < 0xE0)
            byteCount = 2;
        else if (uch >= 0xE0 && uch < 0xF0)
            byteCount = 3;
        else if (uch >= 0xF0 && uch < 0xF8)
            byteCount = 4;

        if (pos + byteCount > len)
        {
            carry = data.substr(pos);
            break;
        }

        // Convert this UTF-8 codepoint to wchar_t
        if (byteCount == 1)
        {
            currentText += (wchar_t)uch;
        }
        else
        {
            std::string mb(data.data() + pos, byteCount);
            int needed = MultiByteToWideChar(CP_UTF8, 0, mb.c_str(), (int)mb.size(), nullptr, 0);
            if (needed > 0)
            {
                wchar_t wbuf[4] = {};
                MultiByteToWideChar(CP_UTF8, 0, mb.c_str(), (int)mb.size(), wbuf, 4);
                for (int w = 0; w < needed; ++w)
                    currentText += wbuf[w];
            }
        }
        pos += byteCount;
    }

    flushText();
    return segments;
}

// ============================================================================
// File-static ANSI state per terminal pane (keyed by pane ID)
// ============================================================================
static std::unordered_map<int, AnsiState> s_ansiStates;
static std::unordered_map<int, std::string> s_ansiCarry;

// ============================================================================
// Win32IDE::appendTerminalTextAnsi — Append ANSI-colored text to a
// RichEdit terminal pane.  Replaces raw appendText() in terminal callbacks.
// ============================================================================
void Win32IDE::appendTerminalTextAnsi(int paneId, HWND hwnd, const std::string& rawOutput)
{
    if (!hwnd || !IsWindow(hwnd))
        return;

    trimRichEditHeadIfNeeded(hwnd);

    AnsiState& state = s_ansiStates[paneId];
    std::string& carry = s_ansiCarry[paneId];

    auto segments = parseAnsiOutput(rawOutput, state, carry);
    if (segments.empty())
        return;

    // Suspend redraw for batch operation
    SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);

    for (const auto& seg : segments)
    {
        if (seg.text.empty())
            continue;

        // Move caret to end
        GETTEXTLENGTHEX gtl = {};
        gtl.flags = GTL_DEFAULT;
        gtl.codepage = CP_UNICODE;
        LONG length = (LONG)SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
        SendMessage(hwnd, EM_SETSEL, length, length);

        // Build CHARFORMAT2W for this segment
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_FACE | CFM_SIZE;

        cf.crTextColor = seg.fg;
        cf.crBackColor = seg.bg;

        cf.dwEffects = 0;
        if (seg.bold)
            cf.dwEffects |= CFE_BOLD;
        if (seg.italic)
            cf.dwEffects |= CFE_ITALIC;
        if (seg.underline)
            cf.dwEffects |= CFE_UNDERLINE;

        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 200;  // 10pt in twips (10 * 20)

        // Apply character format for the text we're about to insert
        SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

        // Insert text
        SETTEXTEX st = {};
        st.flags = ST_SELECTION;
        st.codepage = 1200;  // UTF-16
        SendMessageW(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)seg.text.c_str());
    }

    // Resume redraw and scroll to bottom
    SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwnd, nullptr, FALSE);

    // Auto-scroll to bottom
    SendMessage(hwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

// ============================================================================
// Win32IDE::resetTerminalAnsiState — Clear ANSI state for a pane
// (call when terminal is cleared or pane is destroyed)
// ============================================================================
void Win32IDE::resetTerminalAnsiState(int paneId)
{
    s_ansiStates.erase(paneId);
    s_ansiCarry.erase(paneId);
}
