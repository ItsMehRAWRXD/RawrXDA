<<<<<<< HEAD
// ============================================================================
// chat_panel.cpp — Rich Chat Panel with Markdown→RichEdit Rendering
// ============================================================================
// Copilot/Cursor Parity: renders chat messages with bold, italic, inline code,
// fenced code blocks (with language tag), headings, bullet lists, and tool
// action status cards — all via Win32 RichEdit 4.1 (no WebView2 needed).
// ============================================================================

=======
>>>>>>> origin/main
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "ui/chat_panel.h"
#include "ui/tool_action_status.h"
#include <string>
#include <sstream>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
namespace UI {

// ─── RichEdit DLL Loader ─────────────────────────────────────────────────────
static HMODULE s_richEditDll = nullptr;
static void ensureRichEdit() {
    if (!s_richEditDll) {
        s_richEditDll = LoadLibraryW(L"msftedit.dll");
        if (!s_richEditDll) s_richEditDll = LoadLibraryW(L"riched20.dll");
    }
}

// ─── UTF-8 ↔ Wide helpers ───────────────────────────────────────────────────
static std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}

static std::string fromWide(const std::wstring& w) {
    if (w.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), len, nullptr, nullptr);
    return out;
}

// =============================================================================
// CREATE — Build transcript (RichEdit) + input + send button
// =============================================================================
bool ChatPanel::create(HWND parent, int idBase) {
    ensureRichEdit();
    m_idBase = idBase;

    m_container = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, 10, 10, parent, (HMENU)(intptr_t)(idBase), GetModuleHandle(NULL), NULL);
    if (!m_container) return false;

    // Transcript: RichEdit 4.1 (RICHEDIT50W from msftedit.dll) or 2.0 fallback
    const wchar_t* richEditClass = s_richEditDll ? MSFTEDIT_CLASS : RICHEDIT_CLASSW;
    m_transcript = CreateWindowExW(WS_EX_CLIENTEDGE, richEditClass, L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | ES_NOHIDESEL,
        0, 0, 10, 10, m_container, (HMENU)(intptr_t)(idBase + 1), GetModuleHandle(NULL), NULL);

    if (m_transcript) {
        // Configure RichEdit
        SendMessageW(m_transcript, EM_SETBKGNDCOLOR, 0, (LPARAM)m_bgColor);
        SendMessageW(m_transcript, EM_SETEVENTMASK, 0, ENM_LINK | ENM_MOUSEEVENTS);
        SendMessageW(m_transcript, EM_AUTOURLDETECT, TRUE, 0);
        SendMessageW(m_transcript, EM_SETLANGOPTIONS, 0,
            SendMessageW(m_transcript, EM_GETLANGOPTIONS, 0, 0) & ~IMF_AUTOFONT);

        // Default font
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR | CFM_BOLD;
        cf.yHeight = m_fontSize * 20; // twips
        cf.crTextColor = m_fgColor;
        wcscpy_s(cf.szFaceName, L"Cascadia Code");
        SendMessageW(m_transcript, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

        // Paragraph spacing
        PARAFORMAT2 pf = {};
        pf.cbSize = sizeof(pf);
        pf.dwMask = PFM_SPACEAFTER | PFM_LINESPACING;
        pf.dySpaceAfter = 60;
        pf.bLineSpacingRule = 5;
        pf.dyLineSpacing = 22;
        SendMessageW(m_transcript, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    }

    // Input field: also RichEdit for consistent look
    m_input = CreateWindowExW(WS_EX_CLIENTEDGE, richEditClass, L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 10, 10, m_container, (HMENU)(intptr_t)(idBase + 2), GetModuleHandle(NULL), NULL);
    if (m_input) {
        SendMessageW(m_input, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(37, 37, 38));
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
        cf.yHeight = m_fontSize * 20;
        cf.crTextColor = m_fgColor;
        wcscpy_s(cf.szFaceName, L"Cascadia Code");
        SendMessageW(m_input, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }

    m_send = CreateWindowExW(0, L"BUTTON", L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 24, m_container, (HMENU)(intptr_t)(idBase + 3), GetModuleHandle(NULL), NULL);

    return m_transcript && m_input && m_send;
}

// =============================================================================
// RESIZE
// =============================================================================
void ChatPanel::resize(int x, int y, int w, int h) {
    if (!m_container) return;
    MoveWindow(m_container, x, y, w, h, TRUE);
    int pad = 4;
    int inputH = 72;
    int btnW = 64;
    int btnH = 28;
    int transcriptH = h - inputH - pad * 3;
    MoveWindow(m_transcript, pad, pad, w - 2 * pad, transcriptH, TRUE);
    MoveWindow(m_input, pad, pad + transcriptH + pad, w - 2 * pad - btnW - pad, inputH, TRUE);
    MoveWindow(m_send, w - pad - btnW, pad + transcriptH + pad + (inputH - btnH) / 2, btnW, btnH, TRUE);
}

// =============================================================================
// HANDLE WM_COMMAND
// =============================================================================
bool ChatPanel::handleCommand(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (LOWORD(wParam) == (WORD)(m_idBase + 3) && HIWORD(wParam) == BN_CLICKED) {
        std::string text = getInput();
        if (!text.empty()) {
            clearInput();
            appendMessage("You", text);
            if (m_sendCallback) m_sendCallback(text);
        }
        return true;
    }
    return false;
}

// =============================================================================
// MARKDOWN PARSER — tokens stream from markdown text
// =============================================================================
std::vector<MdToken> ChatPanel::parseMarkdown(const std::string& text) const {
    std::vector<MdToken> tokens;
    std::istringstream ss(text);
    std::string line;
    bool inCodeBlock = false;
    std::string codeBlockLang;
    std::string codeBlockContent;

    while (std::getline(ss, line)) {
        if (line.size() >= 3 && line.substr(0, 3) == "```") {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeBlockLang = (line.size() > 3) ? line.substr(3) : "";
                while (!codeBlockLang.empty() && codeBlockLang.back() == ' ') codeBlockLang.pop_back();
                codeBlockContent.clear();
            } else {
                tokens.push_back({MdTokenKind::CodeBlock, codeBlockContent, codeBlockLang});
                inCodeBlock = false;
            }
            continue;
        }
        if (inCodeBlock) {
            if (!codeBlockContent.empty()) codeBlockContent += "\n";
            codeBlockContent += line;
            continue;
        }
        if (!line.empty() && line[0] == '#') {
            int level = 0;
            while (level < (int)line.size() && line[level] == '#') ++level;
            if (level <= 4 && level < (int)line.size() && line[level] == ' ') {
                tokens.push_back({MdTokenKind::Heading, line.substr(level + 1), std::to_string(level)});
                continue;
            }
        }
        if (line.size() >= 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
            tokens.push_back({MdTokenKind::BulletItem, line.substr(2), {}});
            continue;
        }
        // Inline formatting: **bold**, *italic*, `code`
        size_t i = 0;
        std::string cur;
        while (i < line.size()) {
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
                if (!cur.empty()) { tokens.push_back({MdTokenKind::Text, cur, {}}); cur.clear(); }
                size_t end = line.find("**", i + 2);
                if (end != std::string::npos) {
                    tokens.push_back({MdTokenKind::Bold, line.substr(i + 2, end - i - 2), {}});
                    i = end + 2;
                } else { cur += "**"; i += 2; }
                continue;
            }
            if (line[i] == '`') {
                if (!cur.empty()) { tokens.push_back({MdTokenKind::Text, cur, {}}); cur.clear(); }
                size_t end = line.find('`', i + 1);
                if (end != std::string::npos) {
                    tokens.push_back({MdTokenKind::Code, line.substr(i + 1, end - i - 1), {}});
                    i = end + 1;
                } else { cur += '`'; ++i; }
                continue;
            }
            if (line[i] == '*' && (i + 1 >= line.size() || line[i + 1] != '*')) {
                if (!cur.empty()) { tokens.push_back({MdTokenKind::Text, cur, {}}); cur.clear(); }
                size_t end = line.find('*', i + 1);
                if (end != std::string::npos) {
                    tokens.push_back({MdTokenKind::Italic, line.substr(i + 1, end - i - 1), {}});
                    i = end + 1;
                } else { cur += '*'; ++i; }
                continue;
            }
            cur += line[i]; ++i;
        }
        if (!cur.empty()) tokens.push_back({MdTokenKind::Text, cur, {}});
        tokens.push_back({MdTokenKind::Text, "\n", {}});
    }
    if (inCodeBlock && !codeBlockContent.empty())
        tokens.push_back({MdTokenKind::CodeBlock, codeBlockContent, codeBlockLang});
    return tokens;
}

// =============================================================================
// RICHEDIT FORMATTING HELPERS
// =============================================================================
void ChatPanel::setCharFormat(DWORD effects, COLORREF color, int heightTwips, const wchar_t* face) {
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE;
    if (face) { cf.dwMask |= CFM_FACE; wcscpy_s(cf.szFaceName, face); }
    cf.dwEffects = effects;
    cf.crTextColor = color;
    cf.yHeight = heightTwips;
    SendMessageW(m_transcript, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void ChatPanel::appendPlainText(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(0, m_fgColor, m_fontSize * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void ChatPanel::appendBoldText(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(CFE_BOLD, m_fgColor, m_fontSize * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void ChatPanel::appendItalicText(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(CFE_ITALIC, RGB(180, 180, 180), m_fontSize * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void ChatPanel::appendCodeSpan(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(0, m_codeColor, m_fontSize * 20, L"Consolas");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void ChatPanel::appendCodeBlock(const std::string& lang, const std::wstring& code) {
    appendPlainText(L"\n");
    if (!lang.empty()) {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(CFE_BOLD, RGB(100, 100, 100), (m_fontSize - 1) * 20, L"Cascadia Code");
        std::wstring label = L"  " + toWide(lang) + L"\n";
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)label.c_str());
    }
    int len2 = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len2, (LPARAM)len2);
    setCharFormat(0, m_codeColor, m_fontSize * 20, L"Consolas");
    std::wistringstream ss(code);
    std::wstring wline;
    std::wstring formatted;
    while (std::getline(ss, wline)) formatted += L"    " + wline + L"\n";
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());
    appendPlainText(L"\n");
}

void ChatPanel::appendHeading(const std::wstring& text, int level) {
    appendPlainText(L"\n");
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    int sizes[] = {16, 14, 12, 11};
    int sz = (level >= 1 && level <= 4) ? sizes[level - 1] : 11;
    setCharFormat(CFE_BOLD, m_headingColor, sz * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)(text + L"\n").c_str());
}

void ChatPanel::appendBullet(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(0, m_bulletColor, m_fontSize * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)L"  \x2022 ");
    len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(0, m_fgColor, m_fontSize * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)(text + L"\n").c_str());
}

void ChatPanel::appendSeparator() {
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    setCharFormat(0, RGB(60, 60, 60), (m_fontSize - 2) * 20, L"Cascadia Code");
    SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)L"\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\x2500\n");
}

// =============================================================================
// APPEND RICH TOKENS
// =============================================================================
void ChatPanel::appendRichTokens(const std::string& who, const std::vector<MdToken>& tokens) {
    if (!m_transcript) return;
    SendMessageW(m_transcript, WM_SETREDRAW, FALSE, 0);

    appendSeparator();
    COLORREF roleColor = (who == "You" || who == "User") ? m_userColor : m_assistantColor;
    {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(CFE_BOLD, roleColor, (m_fontSize + 1) * 20, L"Cascadia Code");
        std::wstring label = toWide(who) + L"\n";
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)label.c_str());
    }

    for (auto& tok : tokens) {
        switch (tok.kind) {
            case MdTokenKind::Text:      appendPlainText(toWide(tok.text)); break;
            case MdTokenKind::Bold:      appendBoldText(toWide(tok.text)); break;
            case MdTokenKind::Italic:    appendItalicText(toWide(tok.text)); break;
            case MdTokenKind::Code:      appendCodeSpan(toWide(tok.text)); break;
            case MdTokenKind::CodeBlock: appendCodeBlock(tok.meta, toWide(tok.text)); break;
            case MdTokenKind::Heading:   appendHeading(toWide(tok.text), tok.meta.empty() ? 1 : std::stoi(tok.meta)); break;
            case MdTokenKind::BulletItem:appendBullet(toWide(tok.text)); break;
            default:                     appendPlainText(toWide(tok.text)); break;
        }
    }
    appendPlainText(L"\n");

    SendMessageW(m_transcript, WM_SETREDRAW, TRUE, 0);
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(m_transcript, EM_SCROLLCARET, 0, 0);
    InvalidateRect(m_transcript, nullptr, TRUE);
}

// =============================================================================
// APPEND MESSAGE — parses markdown, renders rich text
// =============================================================================
void ChatPanel::appendMessage(const std::string& who, const std::string& text) {
    if (!m_transcript) return;
    auto tokens = parseMarkdown(text);
    appendRichTokens(who, tokens);
}

// =============================================================================
// APPEND TOOL ACTION — formatted action card
// =============================================================================
void ChatPanel::appendToolAction(const ToolActionStatus& action) {
    if (!m_transcript) return;
    SendMessageW(m_transcript, WM_SETREDRAW, FALSE, 0);

    std::string icon = ToolActionStatusFormatter::iconForKind(action.kind);
    std::string name = ToolActionStatusFormatter::nameForKind(action.kind);
    std::string stateIcon;
    switch (action.state) {
        case ToolActionState::Running:   stateIcon = "\xE2\x8F\xB3"; break;
        case ToolActionState::Completed: stateIcon = "\xE2\x9C\x85"; break;
        case ToolActionState::Failed:    stateIcon = "\xE2\x9D\x8C"; break;
        default:                         stateIcon = "\xE2\x96\xB6\xEF\xB8\x8F"; break;
    }
    {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(0, m_toolIconColor, m_fontSize * 20, L"Cascadia Code");
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)toWide("  " + stateIcon + " " + icon + " ").c_str());
    }
    {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(CFE_BOLD, m_assistantColor, m_fontSize * 20, L"Cascadia Code");
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)toWide(name).c_str());
    }
    if (!action.summary.empty()) {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(0, RGB(150, 150, 150), (m_fontSize - 1) * 20, L"Cascadia Code");
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)toWide("  " + action.summary).c_str());
    }
    if (action.durationMs > 0) {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(0, RGB(100, 100, 100), (m_fontSize - 1) * 20, L"Cascadia Code");
        std::wstring dur = L"  (" + std::to_wstring(action.durationMs) + L"ms)";
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)dur.c_str());
    }
    appendPlainText(L"\n");

    SendMessageW(m_transcript, WM_SETREDRAW, TRUE, 0);
    int endLen = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)endLen, (LPARAM)endLen);
    SendMessageW(m_transcript, EM_SCROLLCARET, 0, 0);
    InvalidateRect(m_transcript, nullptr, TRUE);
}

// =============================================================================
// APPEND WORKING BUBBLE
// =============================================================================
void ChatPanel::appendWorkingBubble(const WorkingBubble& bubble) {
    if (!m_transcript) return;
    std::string text = ToolActionStatusFormatter::formatWorkingBubblePlainText(bubble);
    SendMessageW(m_transcript, WM_SETREDRAW, FALSE, 0);
    {
        int len = GetWindowTextLengthW(m_transcript);
        SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        setCharFormat(CFE_ITALIC, RGB(140, 140, 140), m_fontSize * 20, L"Cascadia Code");
        SendMessageW(m_transcript, EM_REPLACESEL, FALSE, (LPARAM)toWide("  " + text).c_str());
    }
    SendMessageW(m_transcript, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_transcript, nullptr, TRUE);
}

// =============================================================================
// INPUT / CLEAR / SETTINGS
// =============================================================================
std::string ChatPanel::getInput() const {
    if (!m_input) return {};
    int len = GetWindowTextLengthW(m_input);
    if (len <= 0) return {};
    std::wstring buf(len + 1, L'\0');
    GetWindowTextW(m_input, buf.data(), (int)buf.size());
<<<<<<< HEAD
    return fromWide(buf.substr(0, len));
=======
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return {};
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, buf.c_str(), -1, result.data(), utf8Len, nullptr, nullptr);
    result.resize(utf8Len - 1);  // Exclude null terminator
    return result;
>>>>>>> origin/main
}

void ChatPanel::clearInput() {
    if (m_input) SetWindowTextW(m_input, L"");
}

<<<<<<< HEAD
void ChatPanel::setDarkMode(bool dark) {
    m_darkMode = dark;
    if (dark) {
        m_bgColor = RGB(30, 30, 30); m_fgColor = RGB(212, 212, 212);
        m_userColor = RGB(86, 156, 214); m_assistantColor = RGB(78, 201, 176);
        m_codeColor = RGB(206, 145, 120); m_headingColor = RGB(220, 220, 170);
    } else {
        m_bgColor = RGB(255, 255, 255); m_fgColor = RGB(30, 30, 30);
        m_userColor = RGB(0, 0, 180); m_assistantColor = RGB(0, 128, 64);
        m_codeColor = RGB(163, 21, 21); m_headingColor = RGB(50, 50, 50);
    }
    if (m_transcript) {
        SendMessageW(m_transcript, EM_SETBKGNDCOLOR, 0, (LPARAM)m_bgColor);
        InvalidateRect(m_transcript, nullptr, TRUE);
    }
=======
void ChatPanel::appendToolAction(const ToolActionStatus& action) {
    if (!m_transcript) return;
    std::string text = ToolActionStatusFormatter::formatPlainText(action);
    std::wstring wtext(text.begin(), text.end());
    int len = GetWindowTextLengthW(m_transcript);
    SendMessageW(m_transcript, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    std::wstring line = L"  " + wtext;
    SendMessageW(m_transcript, EM_REPLACESEL, TRUE, (LPARAM)line.c_str());
>>>>>>> origin/main
}

void ChatPanel::setFontSize(int points) {
    m_fontSize = (points > 6 && points < 30) ? points : 10;
}

} // namespace UI
} // namespace RawrXD
