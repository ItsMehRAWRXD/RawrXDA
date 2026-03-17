// ============================================================================
// chat_message_renderer.h — Chat Message Rendering Engine
// ============================================================================
// Structured chat message types with code block extraction, diff rendering,
// Markdown-to-HTML conversion, and WebView2 bridge function dispatch.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <chrono>

#include "tool_action_status.h"

#ifdef _WIN32
#include <windows.h>
// windows.h defines ERROR macro which conflicts with our enum
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace RawrXD {
namespace UI {

// ============================================================================
// Message Role
// ============================================================================

enum class MessageRole : uint8_t {
    SYSTEM    = 0,
    USER      = 1,
    ASSISTANT = 2,
    TOOL      = 3,
    ERROR     = 4
};

const char* roleToString(MessageRole role);

// ============================================================================
// Code Block — extracted fenced code block from Markdown
// ============================================================================

struct CodeBlock {
    std::string language;       // "cpp", "python", "masm", ""
    std::string code;           // Raw code content (no fences)
    int         startLine;      // Line offset in original message
    int         endLine;
    bool        isDiff;         // true if language == "diff" or starts with +-
};

// ============================================================================
// Diff Block — structured diff for inline apply/reject
// ============================================================================

struct DiffBlock {
    std::string file;           // Target file path
    int         startLine;      // Original file line
    std::string oldContent;     // Lines removed
    std::string newContent;     // Lines added
    bool        isPartial;      // true if this is a partial (hunk) diff

    // Pre-computed HTML for rendering
    std::string renderAsHtml() const;
};

// ============================================================================
// Action Button — clickable action in rendered message
// ============================================================================

struct ActionButton {
    std::string id;             // Unique button ID
    std::string label;          // Display text
    std::string action;         // "apply_edit", "reject_edit", "copy_code",
                                // "show_definition", "insert_at_cursor",
                                // "run_terminal"
    std::string payload;        // JSON or raw data for the action
};

// ============================================================================
// Chat Message — full message with parsed content
// ============================================================================

struct ChatMessage {
    std::string       id;              // Unique message ID
    MessageRole       role;
    std::string       rawContent;      // Original Markdown text
    std::string       renderedHtml;    // Converted HTML for display

    std::vector<CodeBlock>    codeBlocks;
    std::vector<DiffBlock>    diffBlocks;
    std::vector<ActionButton> actions;

    uint64_t          timestampMs;     // Epoch ms
    bool              isStreaming;      // true during streaming
    bool              isFinished;

    // Token stats
    int               promptTokens;
    int               completionTokens;
    float             latencyMs;

    // Model info
    std::string       modelId;

    // Tool action status entries (rendered inline before message body)
    std::vector<ToolActionStatus> toolActions;
};

// ============================================================================
// Render Result
// ============================================================================

struct RenderResult {
    bool        success;
    const char* detail;
    int         errorCode;

    std::string html;
    std::vector<CodeBlock> codeBlocks;
    std::vector<DiffBlock> diffBlocks;
    std::vector<ActionButton> actions;

    static RenderResult ok(std::string html,
                           std::vector<CodeBlock> blocks,
                           std::vector<DiffBlock> diffs,
                           std::vector<ActionButton> acts) {
        RenderResult r;
        r.success        = true;
        r.detail          = "Rendered";
        r.errorCode       = 0;
        r.html            = std::move(html);
        r.codeBlocks      = std::move(blocks);
        r.diffBlocks      = std::move(diffs);
        r.actions         = std::move(acts);
        return r;
    }

    static RenderResult error(const char* msg, int code = -1) {
        RenderResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// WebView2 Bridge Action — dispatched from JS to C++
// ============================================================================

struct BridgeAction {
    std::string actionType;    // "apply_edit", "reject_edit", "copy_code", etc.
    std::string messageId;
    int         blockIndex;
    std::string payload;       // JSON data from JS
};

using BridgeCallback = void(*)(const BridgeAction&, void* userData);

// ============================================================================
// Chat Message Renderer
// ============================================================================

class ChatMessageRenderer {
public:
    ChatMessageRenderer();
    ~ChatMessageRenderer();

    // Singleton
    static ChatMessageRenderer& Global();

    // ---- Rendering ----

    // Full render: markdown → HTML with code blocks, diffs, actions extracted
    RenderResult renderMessage(const ChatMessage& msg) const;

    // Incremental render: append streaming chunk to existing message
    RenderResult renderStreamChunk(const std::string& messageId,
                                    const std::string& chunk) const;

    // ---- Markdown Processing ----

    // Convert Markdown to HTML (subset: bold, italic, code, headers, lists, links)
    std::string markdownToHtml(const std::string& markdown) const;

    // Extract all fenced code blocks from Markdown
    std::vector<CodeBlock> extractCodeBlocks(const std::string& markdown) const;

    // Parse diff blocks from code blocks with language=="diff"
    std::vector<DiffBlock> parseDiffBlocks(const std::vector<CodeBlock>& codeBlocks) const;

    // ---- Action Generation ----

    // Generate action buttons for a code block
    std::vector<ActionButton> generateCodeActions(const CodeBlock& block,
                                                   const std::string& messageId,
                                                   int blockIndex) const;

    // Generate action buttons for a diff block
    std::vector<ActionButton> generateDiffActions(const DiffBlock& diff,
                                                   const std::string& messageId,
                                                   int blockIndex) const;

    // ---- WebView2 Bridge ----

    // Register callback for bridge actions from JS
    void registerBridgeCallback(BridgeCallback callback, void* userData);

    // Dispatch a bridge action (called from WebView2 message handler)
    void dispatchBridgeAction(const BridgeAction& action);

    // Generate the JavaScript bridge code for WebView2 injection
    std::string generateBridgeScript() const;

    // ---- Theme / Styling ----

    void setDarkMode(bool dark);
    bool isDarkMode() const { return m_darkMode; }

    // CSS for the chat panel (includes tool-action-status CSS)
    std::string generateCSS() const;

    // Render tool action status block (HTML)
    std::string renderToolActions(const std::vector<ToolActionStatus>& actions) const;

    // Render tool action status block (plain text, for Win32 EDIT controls)
    std::string renderToolActionsPlainText(const std::vector<ToolActionStatus>& actions) const;

    // ---- Streaming State ----

    void beginStream(const std::string& messageId);
    void appendStreamChunk(const std::string& messageId, const std::string& chunk);
    std::string finishStream(const std::string& messageId);

private:
    // Escape HTML entities
    std::string escapeHtml(const std::string& text) const;

    // Format inline markdown (bold, italic, code, links)
    std::string formatInlineMarkdown(const std::string& text) const;

    // Apply syntax highlighting hints (class annotations for JS highlighter)
    std::string applySyntaxClasses(const std::string& code,
                                    const std::string& language) const;

    // Generate unique ID
    std::string generateId() const;

    bool m_darkMode = true;

    // Bridge callbacks
    BridgeCallback m_bridgeCallback = nullptr;
    void*          m_bridgeUserData = nullptr;
    mutable std::mutex m_bridgeMutex;

    // Streaming state
    mutable std::mutex m_streamMutex;
    std::unordered_map<std::string, std::string> m_streamBuffers;
};

} // namespace UI
} // namespace RawrXD
