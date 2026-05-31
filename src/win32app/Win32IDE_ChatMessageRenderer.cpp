// =============================================================================
// Win32IDE_ChatMessageRenderer.cpp — Rich Markdown-to-RTF Chat Renderer
// =============================================================================
// Converts markdown-formatted LLM output to RTF for RichEdit display.
// Supports: code blocks (```), inline code (`), bold (**), italic (*),
// headers (#), bullet lists, and role-delimited messages (User/AI).
//
// Architecture: Streaming-compatible. Can be called incrementally per-chunk.
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "Win32IDE.h"
#include <cstring>
#include <richedit.h>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>


// =============================================================================
// Theme Colors (VS Code Dark+ palette, as RGB tuples for RTF color table)
// =============================================================================
namespace
{
/// Cursor / VS Code Copilot–style role headers (internal roles stay lowercase in history).
std::string DisplayRoleForChatUi(const std::string& role)
{
    if (role == "user")
        return "User";
    if (role == "assistant")
        return "Copilot";
    if (role == "system")
        return "System";
    if (role == "tool")
        return "Tool";
    return role.empty() ? std::string("Message") : role;
}
}  // namespace

namespace ChatTheme
{
// RTF uses \red\green\blue in the color table
struct RGBColor
{
    int r, g, b;
};

static constexpr RGBColor kBackground = {30, 30, 30};         // #1e1e1e
static constexpr RGBColor kUserLabel = {86, 156, 214};        // #569cd6 (blue)
static constexpr RGBColor kAssistantLabel = {181, 206, 168};  // #b5cea8 (green)
static constexpr RGBColor kSystemLabel = {206, 145, 88};      // #ce9158 (orange)
static constexpr RGBColor kBodyText = {212, 212, 212};        // #d4d4d4
static constexpr RGBColor kCodeBlock = {206, 145, 120};       // #ce9178 (string color)
static constexpr RGBColor kCodeBlockBg = {37, 37, 38};        // #252526
static constexpr RGBColor kInlineCode = {220, 220, 170};      // #dcdcaa (function color)
static constexpr RGBColor kBold = {212, 212, 212};            // same as body
static constexpr RGBColor kItalic = {156, 220, 254};          // #9cdcfe (variable color)
static constexpr RGBColor kHeader = {197, 134, 192};          // #c586c0 (keyword color)
static constexpr RGBColor kBullet = {106, 153, 85};           // #6a9955 (comment color)
static constexpr RGBColor kToolStatus = {255, 214, 10};       // #ffd60a (gold)
static constexpr RGBColor kToolLabel = {196, 167, 231};       // #c4a7e7 (Iris — tool / agent step headers)
static constexpr RGBColor kError = {244, 71, 71};             // #f44747

static constexpr int kFontSizeBody = 20;    // RTF half-points (10pt)
static constexpr int kFontSizeCode = 18;    // 9pt
static constexpr int kFontSizeHeader = 26;  // 13pt
static constexpr int kFontSizeLabel = 22;   // 11pt
}  // namespace ChatTheme

// =============================================================================
// MarkdownSpan — Parsed inline element
// =============================================================================
enum class SpanType : uint8_t
{
    Text,
    Bold,
    Italic,
    InlineCode,
    CodeBlock,
    Header,
    BulletItem,
    RoleLabel,
    ToolStatus,
    ErrorText,
    LineBreak
};

struct MarkdownSpan
{
    SpanType type;
    std::string text;
    int headerLevel = 0;   // 1..6 for headers
    std::string language;  // for code blocks
};

// =============================================================================
// ChatMessageRenderer — Main renderer class
// =============================================================================
class ChatMessageRenderer
{
  public:
    // Parse markdown text into a list of styled spans
    static std::vector<MarkdownSpan> Parse(const std::string& markdown);

    // Render a list of spans as RTF into a RichEdit control
    static void RenderToRichEdit(HWND hwndRichEdit, const std::vector<MarkdownSpan>& spans);

    // One-shot: parse + render
    static void RenderMarkdown(HWND hwndRichEdit, const std::string& markdown);

    // Append a formatted chat message with role label
    static void AppendChatMessage(HWND hwndRichEdit, const std::string& role, const std::string& content);

    // Apply RTF formatting to a selection range in a RichEdit control
    static void ApplyFormat(HWND hwndRichEdit, const CHARFORMAT2W& cf);

  private:
    static CHARFORMAT2W MakeFormat(const ChatTheme::RGBColor& color, int fontSize, bool bold, bool italic,
                                   bool monospace);
    static void AppendTextWithFormat(HWND hwndRichEdit, const std::wstring& text, const CHARFORMAT2W& cf);
    static std::wstring Utf8ToWide(const std::string& utf8);
};

// =============================================================================
// Parse — Streaming-compatible markdown parser
// =============================================================================
std::vector<MarkdownSpan> ChatMessageRenderer::Parse(const std::string& md)
{
    std::vector<MarkdownSpan> spans;

    size_t i = 0;
    const size_t len = md.size();
    bool inCodeBlock = false;
    std::string codeBlockLang;
    std::string codeBlockAccum;

    while (i < len)
    {
        // Fenced code block detection: ``` at start of line
        if (!inCodeBlock && i + 2 < len && md[i] == '`' && md[i + 1] == '`' && md[i + 2] == '`')
        {
            i += 3;
            // Capture optional language tag
            codeBlockLang.clear();
            while (i < len && md[i] != '\n' && md[i] != '\r')
            {
                codeBlockLang += md[i++];
            }
            if (i < len && md[i] == '\n')
                i++;
            else if (i + 1 < len && md[i] == '\r' && md[i + 1] == '\n')
                i += 2;
            inCodeBlock = true;
            codeBlockAccum.clear();
            continue;
        }

        if (inCodeBlock)
        {
            // Check for closing ```
            if (i + 2 < len && md[i] == '`' && md[i + 1] == '`' && md[i + 2] == '`')
            {
                i += 3;
                // Skip to end of line
                while (i < len && md[i] != '\n' && md[i] != '\r')
                    i++;
                if (i < len && md[i] == '\n')
                    i++;
                else if (i + 1 < len && md[i] == '\r' && md[i + 1] == '\n')
                    i += 2;

                MarkdownSpan span;
                span.type = SpanType::CodeBlock;
                span.text = codeBlockAccum;
                span.language = codeBlockLang;
                spans.push_back(std::move(span));
                inCodeBlock = false;
                continue;
            }
            codeBlockAccum += md[i++];
            continue;
        }

        // Header detection: # at start of line
        if (md[i] == '#')
        {
            int level = 0;
            size_t hStart = i;
            while (i < len && md[i] == '#' && level < 6)
            {
                level++;
                i++;
            }
            if (i < len && md[i] == ' ')
            {
                i++;  // skip the space
                std::string headerText;
                while (i < len && md[i] != '\n' && md[i] != '\r')
                {
                    headerText += md[i++];
                }
                if (i < len && md[i] == '\n')
                    i++;
                else if (i + 1 < len && md[i] == '\r' && md[i + 1] == '\n')
                    i += 2;

                MarkdownSpan span;
                span.type = SpanType::Header;
                span.text = headerText;
                span.headerLevel = level;
                spans.push_back(std::move(span));
                continue;
            }
            // Not a real header, treat as text
            i = hStart;
        }

        // Bullet list: "- " or "* " at start (after possible whitespace)
        if ((md[i] == '-' || md[i] == '*') && i + 1 < len && md[i + 1] == ' ')
        {
            // Check if this is at start of line
            bool atLineStart = (i == 0 || md[i - 1] == '\n');
            if (atLineStart)
            {
                i += 2;
                std::string bulletText;
                while (i < len && md[i] != '\n' && md[i] != '\r')
                {
                    bulletText += md[i++];
                }
                if (i < len && md[i] == '\n')
                    i++;
                else if (i + 1 < len && md[i] == '\r' && md[i + 1] == '\n')
                    i += 2;

                MarkdownSpan span;
                span.type = SpanType::BulletItem;
                span.text = bulletText;
                spans.push_back(std::move(span));
                continue;
            }
        }

        // Bold: **text**
        if (i + 1 < len && md[i] == '*' && md[i + 1] == '*')
        {
            size_t start = i + 2;
            size_t end = md.find("**", start);
            if (end != std::string::npos)
            {
                MarkdownSpan span;
                span.type = SpanType::Bold;
                span.text = md.substr(start, end - start);
                spans.push_back(std::move(span));
                i = end + 2;
                continue;
            }
        }

        // Italic: *text* (single asterisk, not followed by another)
        if (md[i] == '*' && (i + 1 >= len || md[i + 1] != '*'))
        {
            size_t start = i + 1;
            size_t end = md.find('*', start);
            if (end != std::string::npos && end > start)
            {
                MarkdownSpan span;
                span.type = SpanType::Italic;
                span.text = md.substr(start, end - start);
                spans.push_back(std::move(span));
                i = end + 1;
                continue;
            }
        }

        // Inline code: `text`
        if (md[i] == '`')
        {
            size_t start = i + 1;
            size_t end = md.find('`', start);
            if (end != std::string::npos)
            {
                MarkdownSpan span;
                span.type = SpanType::InlineCode;
                span.text = md.substr(start, end - start);
                spans.push_back(std::move(span));
                i = end + 1;
                continue;
            }
        }

        // Line break
        if (md[i] == '\n')
        {
            spans.push_back({SpanType::LineBreak, ""});
            i++;
            continue;
        }
        if (md[i] == '\r')
        {
            if (i + 1 < len && md[i + 1] == '\n')
                i++;
            spans.push_back({SpanType::LineBreak, ""});
            i++;
            continue;
        }

        // Plain text — accumulate until we hit a special character
        std::string plainText;
        while (i < len && md[i] != '`' && md[i] != '*' && md[i] != '#' && md[i] != '\n' && md[i] != '\r')
        {
            // Check for fenced code block
            if (i + 2 < len && md[i] == '`' && md[i + 1] == '`' && md[i + 2] == '`')
                break;
            plainText += md[i++];
        }
        if (!plainText.empty())
        {
            spans.push_back({SpanType::Text, plainText});
        }
    }

    // If we ended inside a code block, flush it
    if (inCodeBlock && !codeBlockAccum.empty())
    {
        MarkdownSpan span;
        span.type = SpanType::CodeBlock;
        span.text = codeBlockAccum;
        span.language = codeBlockLang;
        spans.push_back(std::move(span));
    }

    return spans;
}

// =============================================================================
// Helpers
// =============================================================================
std::wstring ChatMessageRenderer::Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (needed <= 0)
        return {};
    std::wstring wide(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), needed);
    return wide;
}

CHARFORMAT2W ChatMessageRenderer::MakeFormat(const ChatTheme::RGBColor& color, int fontSize, bool bold, bool italic,
                                             bool monospace)
{
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_FACE;
    cf.crTextColor = RGB(color.r, color.g, color.b);
    cf.yHeight = fontSize * 10;  // twips (1/20 of a point * 10)
    if (bold)
        cf.dwEffects |= CFE_BOLD;
    if (italic)
        cf.dwEffects |= CFE_ITALIC;

    if (monospace)
    {
        wcscpy_s(cf.szFaceName, LF_FACESIZE, L"Cascadia Code");
    }
    else
    {
        wcscpy_s(cf.szFaceName, LF_FACESIZE, L"Segoe UI");
    }

    return cf;
}

void ChatMessageRenderer::ApplyFormat(HWND hwndRichEdit, const CHARFORMAT2W& cf)
{
    SendMessageW(hwndRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void ChatMessageRenderer::AppendTextWithFormat(HWND hwndRichEdit, const std::wstring& text, const CHARFORMAT2W& cf)
{
    if (text.empty() || !hwndRichEdit)
        return;

    // Move cursor to end
    SendMessageW(hwndRichEdit, EM_SETSEL, -1, -1);
    // Apply format to selection (insertion point)
    SendMessageW(hwndRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    // Insert text
    SendMessageW(hwndRichEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

// =============================================================================
// RenderToRichEdit — Render parsed spans into a RichEdit control
// =============================================================================
void ChatMessageRenderer::RenderToRichEdit(HWND hwndRichEdit, const std::vector<MarkdownSpan>& spans)
{
    if (!hwndRichEdit || spans.empty())
        return;

    // Suspend redraws for batch update
    SendMessageW(hwndRichEdit, WM_SETREDRAW, FALSE, 0);

    for (const auto& span : spans)
    {
        switch (span.type)
        {
            case SpanType::Text:
            {
                auto cf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeBody, false, false, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                break;
            }

            case SpanType::Bold:
            {
                auto cf = MakeFormat(ChatTheme::kBold, ChatTheme::kFontSizeBody, true, false, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                break;
            }

            case SpanType::Italic:
            {
                auto cf = MakeFormat(ChatTheme::kItalic, ChatTheme::kFontSizeBody, false, true, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                break;
            }

            case SpanType::InlineCode:
            {
                auto cf = MakeFormat(ChatTheme::kInlineCode, ChatTheme::kFontSizeCode, false, false, true);
                // Add background effect via paraformat would be ideal,
                // but RichEdit doesn't support per-char background easily.
                // Use highlight color instead.
                cf.dwMask |= CFM_BACKCOLOR;
                cf.crBackColor = RGB(ChatTheme::kCodeBlockBg.r, ChatTheme::kCodeBlockBg.g, ChatTheme::kCodeBlockBg.b);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(" " + span.text + " "), cf);
                break;
            }

            case SpanType::CodeBlock:
            {
                auto cf = MakeFormat(ChatTheme::kCodeBlock, ChatTheme::kFontSizeCode, false, false, true);
                cf.dwMask |= CFM_BACKCOLOR;
                cf.crBackColor = RGB(ChatTheme::kCodeBlockBg.r, ChatTheme::kCodeBlockBg.g, ChatTheme::kCodeBlockBg.b);

                // Language label header
                if (!span.language.empty())
                {
                    auto langCf = MakeFormat(ChatTheme::kBullet, ChatTheme::kFontSizeCode, true, false, true);
                    langCf.dwMask |= CFM_BACKCOLOR;
                    langCf.crBackColor = cf.crBackColor;
                    std::wstring langLabel = L"\n  " + Utf8ToWide(span.language) + L"\n";
                    AppendTextWithFormat(hwndRichEdit, langLabel, langCf);
                }
                else
                {
                    auto nlCf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeCode, false, false, false);
                    AppendTextWithFormat(hwndRichEdit, L"\n", nlCf);
                }

                // Code body — prepend each line with indent
                std::istringstream stream(span.text);
                std::string line;
                while (std::getline(stream, line))
                {
                    std::wstring wline = L"  " + Utf8ToWide(line) + L"\n";
                    AppendTextWithFormat(hwndRichEdit, wline, cf);
                }

                // Trailing separator
                auto sepCf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeBody, false, false, false);
                AppendTextWithFormat(hwndRichEdit, L"\n", sepCf);
                break;
            }

            case SpanType::Header:
            {
                int fontSize = ChatTheme::kFontSizeHeader;
                if (span.headerLevel >= 3)
                    fontSize = ChatTheme::kFontSizeBody + 2;

                auto cf = MakeFormat(ChatTheme::kHeader, fontSize, true, false, false);
                AppendTextWithFormat(hwndRichEdit, L"\n", cf);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                AppendTextWithFormat(hwndRichEdit, L"\n", cf);
                break;
            }

            case SpanType::BulletItem:
            {
                auto bulletCf = MakeFormat(ChatTheme::kBullet, ChatTheme::kFontSizeBody, false, false, false);
                AppendTextWithFormat(hwndRichEdit, L"  \x2022 ", bulletCf);

                auto textCf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeBody, false, false, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), textCf);
                AppendTextWithFormat(hwndRichEdit, L"\n", textCf);
                break;
            }

            case SpanType::RoleLabel:
            {
                ChatTheme::RGBColor labelColor = ChatTheme::kAssistantLabel;
                if (span.text == "User" || span.text == "user")
                    labelColor = ChatTheme::kUserLabel;
                else if (span.text == "System" || span.text == "system")
                    labelColor = ChatTheme::kSystemLabel;
                else if (span.text == "Tool" || span.text == "tool")
                    labelColor = ChatTheme::kToolLabel;
                else if (span.text == "Copilot" || span.text == "assistant")
                    labelColor = ChatTheme::kAssistantLabel;

                auto cf = MakeFormat(labelColor, ChatTheme::kFontSizeLabel, true, false, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text + ": "), cf);
                break;
            }

            case SpanType::ToolStatus:
            {
                auto cf = MakeFormat(ChatTheme::kToolStatus, ChatTheme::kFontSizeCode, true, true, true);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                break;
            }

            case SpanType::ErrorText:
            {
                auto cf = MakeFormat(ChatTheme::kError, ChatTheme::kFontSizeBody, true, false, false);
                AppendTextWithFormat(hwndRichEdit, Utf8ToWide(span.text), cf);
                break;
            }

            case SpanType::LineBreak:
            {
                auto cf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeBody, false, false, false);
                AppendTextWithFormat(hwndRichEdit, L"\n", cf);
                break;
            }
        }
    }

    // Re-enable redraws
    SendMessageW(hwndRichEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndRichEdit, nullptr, TRUE);

    // Scroll to bottom
    SendMessageW(hwndRichEdit, EM_SETSEL, -1, -1);
    SendMessageW(hwndRichEdit, EM_SCROLLCARET, 0, 0);
}

// =============================================================================
// RenderMarkdown — One-shot parse + render
// =============================================================================
void ChatMessageRenderer::RenderMarkdown(HWND hwndRichEdit, const std::string& markdown)
{
    auto spans = Parse(markdown);
    RenderToRichEdit(hwndRichEdit, spans);
}

// =============================================================================
// AppendChatMessage — Format a role-tagged message
// =============================================================================
void ChatMessageRenderer::AppendChatMessage(HWND hwndRichEdit, const std::string& role, const std::string& content)
{
    if (!hwndRichEdit)
        return;

    SendMessageW(hwndRichEdit, WM_SETREDRAW, FALSE, 0);

    // Role label
    MarkdownSpan roleSpan;
    roleSpan.type = SpanType::RoleLabel;
    roleSpan.text = DisplayRoleForChatUi(role);
    RenderToRichEdit(hwndRichEdit, {roleSpan});

    // Message body with markdown rendering
    auto bodySpans = Parse(content);
    RenderToRichEdit(hwndRichEdit, bodySpans);

    // Trailing newline separator
    auto sepCf = MakeFormat(ChatTheme::kBodyText, ChatTheme::kFontSizeBody, false, false, false);
    AppendTextWithFormat(hwndRichEdit, L"\n\n", sepCf);

    SendMessageW(hwndRichEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndRichEdit, nullptr, TRUE);
    SendMessageW(hwndRichEdit, EM_SETSEL, -1, -1);
    SendMessageW(hwndRichEdit, EM_SCROLLCARET, 0, 0);
}

// =============================================================================
// HandleChatMessageRenderer — Feature manifest entry point (backwards compat)
// =============================================================================
void HandleChatMessageRenderer(void* idePtr)
{
    // The renderer is now a class-based API used directly by the chat panel.
    // This handler is retained for feature manifest compatibility.
    (void)idePtr;
}

// =============================================================================
// Win32IDE bridge methods — Called from other translation units via Win32IDE.h
// =============================================================================

void Win32IDE::renderMarkdownToChat(const std::string& markdown)
{
    if (!m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput) || markdown.empty())
        return;
    std::lock_guard<std::mutex> outLock(m_outputMutex);
    ChatMessageRenderer::RenderMarkdown(m_hwndCopilotChatOutput, markdown);
    SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::appendFormattedChatMessage(const std::string& role, const std::string& content)
{
    if (!m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput) || content.empty())
        return;
    std::lock_guard<std::mutex> outLock(m_outputMutex);
    ChatMessageRenderer::AppendChatMessage(m_hwndCopilotChatOutput, role, content);
    SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::replaceLastStreamingBlockWithMarkdown(const std::string& markdown)
{
    if (!m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput) || markdown.empty())
        return;
    std::lock_guard<std::mutex> outLock(m_outputMutex);

    // Determine how many characters of raw streaming text to replace.
    // Use the tracked wide-char write count so UTF-8 multi-byte tokens and the "Copilot: "
    // prefix don't cause an off-by-N selection.
    int totalLen = GetWindowTextLengthW(m_hwndCopilotChatOutput);
    int streamLen = m_streamingWrittenWchars;

    // Safety: don't allow out-of-bounds selection or negative stream lengths
    if (streamLen < 0)
        streamLen = 0;
    if (streamLen > totalLen)
    {
        OutputDebugStringA("[RawrXD] STREAM_DESYNC_DETECTED: streamLen > totalLen — stale streaming state, clamping.\n");
        streamLen = totalLen;
    }

    int accumulatorWideLen = 0;
    if (!m_streamingTokenAccumulator.empty())
    {
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, m_streamingTokenAccumulator.data(),
                                             static_cast<int>(m_streamingTokenAccumulator.size()), nullptr, 0);
        accumulatorWideLen = (wlen > 0) ? wlen : 0;
    }

    OutputDebugStringA(("[replaceLastStreaming] totalLen=" + std::to_string(totalLen) +
                        " accumulatorWide=" + std::to_string(accumulatorWideLen) +
                        " writtenWchars=" + std::to_string(m_streamingWrittenWchars) +
                        " using streamLen=" + std::to_string(streamLen) + "\n").c_str());

    // Select the trailing block that was raw-streamed tokens
    int selStart = totalLen - streamLen;
    if (selStart < 0)
        selStart = 0;
    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, selStart, totalLen);
    SendMessageW(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)L"");

    // Now render the same content as formatted markdown
    ChatMessageRenderer::AppendChatMessage(m_hwndCopilotChatOutput, "assistant", markdown);
    SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}
