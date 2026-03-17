#pragma once

#include <string>
#include <vector>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <richedit.h>
#endif

// Forward declarations
namespace RawrXD { namespace UI {
    struct ToolActionStatus;
    struct WorkingBubble;
}}

namespace RawrXD {
namespace UI {

// ─── Markdown Token Types for RichEdit Rendering ─────────────────────────────
enum class MdTokenKind { Text, Bold, Italic, Code, CodeBlock, Heading, BulletItem, Link };

struct MdToken {
    MdTokenKind kind = MdTokenKind::Text;
    std::string text;
    std::string meta; // language for CodeBlock, URL for Link
};

class ChatPanel {
public:
    bool create(HWND parent, int idBase);
    void resize(int x, int y, int w, int h);
    void appendMessage(const std::string& who, const std::string& text);

    // Append a tool action status line (rich formatted)
    void appendToolAction(const ToolActionStatus& action);

    // Append a working bubble summary (rich formatted)
    void appendWorkingBubble(const WorkingBubble& bubble);

    std::string getInput() const;
    void clearInput();
    HWND hwnd() const { return m_container; }

    // Rich-text formatting controls
    void setDarkMode(bool dark);
    void setFontSize(int points);

    // Callback for send button click
    using SendCallback = std::function<void(const std::string&)>;
    void setSendCallback(SendCallback cb) { m_sendCallback = std::move(cb); }

    // Handle WM_COMMAND from parent
    bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
    // Markdown parser: converts markdown text to token stream
    std::vector<MdToken> parseMarkdown(const std::string& text) const;

    // Apply tokens to RichEdit control with formatting
    void appendRichTokens(const std::string& who, const std::vector<MdToken>& tokens);

    // RichEdit formatting helpers
    void appendPlainText(const std::wstring& text);
    void appendBoldText(const std::wstring& text);
    void appendItalicText(const std::wstring& text);
    void appendCodeSpan(const std::wstring& text);
    void appendCodeBlock(const std::string& lang, const std::wstring& code);
    void appendHeading(const std::wstring& text, int level);
    void appendBullet(const std::wstring& text);
    void appendSeparator();
    void setCharFormat(DWORD effects, COLORREF color, int heightTwips, const wchar_t* face);

    HWND m_container{nullptr};
    HWND m_transcript{nullptr};  // RichEdit control
    HWND m_input{nullptr};
    HWND m_send{nullptr};
    int  m_idBase{0};
    bool m_darkMode{true};
    int  m_fontSize{10};
    SendCallback m_sendCallback;

    // Colors
    COLORREF m_bgColor{RGB(30, 30, 30)};
    COLORREF m_fgColor{RGB(212, 212, 212)};
    COLORREF m_userColor{RGB(86, 156, 214)};
    COLORREF m_assistantColor{RGB(78, 201, 176)};
    COLORREF m_codeColor{RGB(206, 145, 120)};
    COLORREF m_codeBgColor{RGB(45, 45, 45)};
    COLORREF m_headingColor{RGB(220, 220, 170)};
    COLORREF m_bulletColor{RGB(150, 150, 150)};
    COLORREF m_toolIconColor{RGB(255, 200, 50)};
};

} // namespace UI
} // namespace RawrXD
