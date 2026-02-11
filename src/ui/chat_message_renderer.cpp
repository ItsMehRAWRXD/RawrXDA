// ============================================================================
// chat_message_renderer.cpp — Chat Message Rendering Engine Implementation
// ============================================================================
// Markdown-to-HTML conversion, code block extraction, diff parsing,
// action button generation, WebView2 bridge scripting, CSS theming.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "ui/chat_message_renderer.h"

#include <sstream>
#include <regex>
#include <algorithm>
#include <ctime>
#include <atomic>

namespace RawrXD {
namespace UI {

// ============================================================================
// Role names
// ============================================================================

const char* roleToString(MessageRole role) {
    switch (role) {
        case MessageRole::SYSTEM:    return "system";
        case MessageRole::USER:      return "user";
        case MessageRole::ASSISTANT: return "assistant";
        case MessageRole::TOOL:      return "tool";
        case MessageRole::ERROR:     return "error";
        default: return "unknown";
    }
}

// ============================================================================
// DiffBlock::renderAsHtml
// ============================================================================

std::string DiffBlock::renderAsHtml() const {
    std::ostringstream oss;
    oss << "<div class=\"diff-block\">";
    if (!file.empty()) {
        oss << "<div class=\"diff-file\">" << file << "</div>";
    }

    // Split old and new into lines
    auto splitLines = [](const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);
        return lines;
    };

    auto oldLines = splitLines(oldContent);
    auto newLines = splitLines(newContent);

    oss << "<table class=\"diff-table\">";

    // Removed lines
    for (const auto& line : oldLines) {
        oss << "<tr class=\"diff-removed\"><td class=\"diff-sign\">-</td>"
            << "<td class=\"diff-content\">" << line << "</td></tr>";
    }

    // Added lines
    for (const auto& line : newLines) {
        oss << "<tr class=\"diff-added\"><td class=\"diff-sign\">+</td>"
            << "<td class=\"diff-content\">" << line << "</td></tr>";
    }

    oss << "</table></div>";
    return oss.str();
}

// ============================================================================
// ChatMessageRenderer
// ============================================================================

ChatMessageRenderer::ChatMessageRenderer() = default;
ChatMessageRenderer::~ChatMessageRenderer() = default;

ChatMessageRenderer& ChatMessageRenderer::Global() {
    static ChatMessageRenderer instance;
    return instance;
}

// ============================================================================
// Full message rendering
// ============================================================================

RenderResult ChatMessageRenderer::renderMessage(const ChatMessage& msg) const {
    // Extract code blocks first
    auto codeBlocks = extractCodeBlocks(msg.rawContent);
    auto diffBlocks = parseDiffBlocks(codeBlocks);

    // Generate actions for each block
    std::vector<ActionButton> allActions;
    for (int i = 0; i < static_cast<int>(codeBlocks.size()); ++i) {
        auto actions = generateCodeActions(codeBlocks[i], msg.id, i);
        allActions.insert(allActions.end(), actions.begin(), actions.end());
    }
    for (int i = 0; i < static_cast<int>(diffBlocks.size()); ++i) {
        auto actions = generateDiffActions(diffBlocks[i], msg.id, i);
        allActions.insert(allActions.end(), actions.begin(), actions.end());
    }

    // Render full HTML
    std::ostringstream html;
    html << "<div class=\"chat-message role-" << roleToString(msg.role) << "\"";
    html << " data-message-id=\"" << msg.id << "\">";

    // Header
    html << "<div class=\"message-header\">";
    html << "<span class=\"role-badge\">" << roleToString(msg.role) << "</span>";
    if (!msg.modelId.empty()) {
        html << "<span class=\"model-badge\">" << escapeHtml(msg.modelId) << "</span>";
    }
    html << "</div>";

    // Body: convert markdown to HTML, replacing code blocks with rendered versions
    html << "<div class=\"message-body\">";
    html << markdownToHtml(msg.rawContent);
    html << "</div>";

    // Token stats (for assistant messages)
    if (msg.role == MessageRole::ASSISTANT && msg.isFinished) {
        html << "<div class=\"message-stats\">";
        if (msg.promptTokens > 0) {
            html << "<span class=\"stat\">prompt: " << msg.promptTokens << "</span>";
        }
        if (msg.completionTokens > 0) {
            html << "<span class=\"stat\">completion: " << msg.completionTokens << "</span>";
        }
        if (msg.latencyMs > 0) {
            html << "<span class=\"stat\">"
                 << static_cast<int>(msg.latencyMs) << "ms</span>";
        }
        html << "</div>";
    }

    html << "</div>"; // .chat-message

    return RenderResult::ok(html.str(),
                             std::move(codeBlocks),
                             std::move(diffBlocks),
                             std::move(allActions));
}

// ============================================================================
// Streaming
// ============================================================================

RenderResult ChatMessageRenderer::renderStreamChunk(const std::string& messageId,
                                                     const std::string& chunk) const {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    auto it = m_streamBuffers.find(messageId);
    std::string accumulated;
    if (it != m_streamBuffers.end()) {
        accumulated = it->second + chunk;
    } else {
        accumulated = chunk;
    }

    // Render what we have so far
    std::ostringstream html;
    html << "<div class=\"message-body streaming\">";
    html << markdownToHtml(accumulated);
    html << "<span class=\"cursor-blink\">▊</span>";
    html << "</div>";

    return RenderResult::ok(html.str(), {}, {}, {});
}

void ChatMessageRenderer::beginStream(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    m_streamBuffers[messageId] = "";
}

void ChatMessageRenderer::appendStreamChunk(const std::string& messageId,
                                             const std::string& chunk) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    m_streamBuffers[messageId] += chunk;
}

std::string ChatMessageRenderer::finishStream(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_streamMutex);
    auto it = m_streamBuffers.find(messageId);
    std::string result;
    if (it != m_streamBuffers.end()) {
        result = std::move(it->second);
        m_streamBuffers.erase(it);
    }
    return result;
}

// ============================================================================
// Markdown → HTML (subset)
// ============================================================================

std::string ChatMessageRenderer::markdownToHtml(const std::string& markdown) const {
    std::ostringstream html;
    std::istringstream stream(markdown);
    std::string line;
    bool inCodeBlock = false;
    std::string codeLang;
    std::string codeContent;
    int codeBlockIdx = 0;
    bool inList = false;
    bool inOrderedList = false;

    while (std::getline(stream, line)) {
        // Fenced code blocks
        if (line.substr(0, 3) == "```") {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeLang = (line.size() > 3) ? line.substr(3) : "";
                // Trim whitespace from language
                auto end = codeLang.find_first_of(" \t\r\n");
                if (end != std::string::npos) codeLang = codeLang.substr(0, end);
                codeContent.clear();
            } else {
                // Close code block
                inCodeBlock = false;
                html << "<div class=\"code-block-container\" data-block-idx=\""
                     << codeBlockIdx << "\">";
                if (!codeLang.empty()) {
                    html << "<div class=\"code-lang\">" << escapeHtml(codeLang) << "</div>";
                }
                html << "<div class=\"code-actions\">"
                     << "<button onclick=\"bridgeAction('copy_code',"
                     << codeBlockIdx << ")\">Copy</button>"
                     << "<button onclick=\"bridgeAction('apply_edit',"
                     << codeBlockIdx << ")\">Apply</button>"
                     << "<button onclick=\"bridgeAction('insert_at_cursor',"
                     << codeBlockIdx << ")\">Insert</button>"
                     << "</div>";
                html << "<pre><code class=\"language-" << escapeHtml(codeLang) << "\">"
                     << escapeHtml(codeContent) << "</code></pre>";
                html << "</div>";
                codeBlockIdx++;
            }
            continue;
        }

        if (inCodeBlock) {
            codeContent += line + "\n";
            continue;
        }

        // Close list if non-list line
        if (inList && !line.empty() && line[0] != '-' && line[0] != '*' &&
            line[0] != ' ') {
            html << "</ul>";
            inList = false;
        }
        if (inOrderedList && !line.empty() && !std::isdigit(static_cast<unsigned char>(line[0]))) {
            html << "</ol>";
            inOrderedList = false;
        }

        // Empty line → paragraph break
        if (line.empty()) {
            html << "<br>";
            continue;
        }

        // Headers
        if (line.substr(0, 4) == "### ") {
            html << "<h3>" << escapeHtml(line.substr(4)) << "</h3>";
            continue;
        }
        if (line.substr(0, 3) == "## ") {
            html << "<h2>" << escapeHtml(line.substr(3)) << "</h2>";
            continue;
        }
        if (line.substr(0, 2) == "# ") {
            html << "<h1>" << escapeHtml(line.substr(2)) << "</h1>";
            continue;
        }

        // Horizontal rule
        if (line == "---" || line == "***" || line == "___") {
            html << "<hr>";
            continue;
        }

        // Unordered list
        if ((line[0] == '-' || line[0] == '*') && line.size() > 1 && line[1] == ' ') {
            if (!inList) { html << "<ul>"; inList = true; }
            html << "<li>" << formatInlineMarkdown(line.substr(2)) << "</li>";
            continue;
        }

        // Ordered list
        {
            size_t dotPos = line.find(". ");
            if (dotPos != std::string::npos && dotPos < 4) {
                bool isNum = true;
                for (size_t i = 0; i < dotPos; ++i) {
                    if (!std::isdigit(static_cast<unsigned char>(line[i]))) {
                        isNum = false; break;
                    }
                }
                if (isNum) {
                    if (!inOrderedList) { html << "<ol>"; inOrderedList = true; }
                    html << "<li>" << formatInlineMarkdown(line.substr(dotPos + 2)) << "</li>";
                    continue;
                }
            }
        }

        // Blockquote
        if (line[0] == '>' && line.size() > 1) {
            html << "<blockquote>" << formatInlineMarkdown(line.substr(2)) << "</blockquote>";
            continue;
        }

        // Regular paragraph
        html << "<p>" << formatInlineMarkdown(line) << "</p>";
    }

    // Close open blocks
    if (inCodeBlock) {
        html << "<pre><code>" << escapeHtml(codeContent) << "</code></pre>";
    }
    if (inList) html << "</ul>";
    if (inOrderedList) html << "</ol>";

    return html.str();
}

// ============================================================================
// Inline markdown formatting
// ============================================================================

std::string ChatMessageRenderer::formatInlineMarkdown(const std::string& text) const {
    std::string result = escapeHtml(text);

    // Bold: **text** → <strong>text</strong>
    static const std::regex boldRegex(R"(\*\*(.+?)\*\*)", std::regex::optimize);
    result = std::regex_replace(result, boldRegex, "<strong>$1</strong>");

    // Italic: *text* → <em>text</em>
    static const std::regex italicRegex(R"(\*(.+?)\*)", std::regex::optimize);
    result = std::regex_replace(result, italicRegex, "<em>$1</em>");

    // Inline code: `code` → <code>code</code>
    static const std::regex codeRegex(R"(`([^`]+)`)", std::regex::optimize);
    result = std::regex_replace(result, codeRegex, "<code class=\"inline\">$1</code>");

    // Links: [text](url) → <a href="url">text</a>
    static const std::regex linkRegex(R"(\[([^\]]+)\]\(([^)]+)\))", std::regex::optimize);
    result = std::regex_replace(result, linkRegex, "<a href=\"$2\">$1</a>");

    return result;
}

// ============================================================================
// Code block extraction
// ============================================================================

std::vector<CodeBlock> ChatMessageRenderer::extractCodeBlocks(const std::string& markdown) const {
    std::vector<CodeBlock> blocks;
    std::istringstream stream(markdown);
    std::string line;
    int lineNum = 0;
    bool inBlock = false;
    CodeBlock current;

    while (std::getline(stream, line)) {
        lineNum++;

        if (line.substr(0, 3) == "```") {
            if (!inBlock) {
                inBlock = true;
                current = {};
                current.startLine = lineNum;
                current.language = (line.size() > 3) ? line.substr(3) : "";
                auto end = current.language.find_first_of(" \t\r\n");
                if (end != std::string::npos) current.language = current.language.substr(0, end);
                current.isDiff = (current.language == "diff");
            } else {
                current.endLine = lineNum;
                blocks.push_back(std::move(current));
                inBlock = false;
            }
            continue;
        }

        if (inBlock) {
            current.code += line + "\n";
            // Check if content looks like a diff
            if (!current.isDiff && !line.empty() &&
                (line[0] == '+' || line[0] == '-') &&
                line.substr(0, 3) != "---" && line.substr(0, 3) != "+++") {
                // Could be a diff, but only mark if language is already diff
            }
        }
    }

    return blocks;
}

// ============================================================================
// Diff block parsing
// ============================================================================

std::vector<DiffBlock> ChatMessageRenderer::parseDiffBlocks(
    const std::vector<CodeBlock>& codeBlocks) const {

    std::vector<DiffBlock> diffs;

    for (const auto& block : codeBlocks) {
        if (!block.isDiff && block.language != "diff") continue;

        DiffBlock diff;
        diff.isPartial = true;

        std::istringstream stream(block.code);
        std::string line;

        while (std::getline(stream, line)) {
            if (line.substr(0, 4) == "+++ ") {
                diff.file = line.substr(4);
                if (diff.file.substr(0, 2) == "b/") diff.file = diff.file.substr(2);
            } else if (line.substr(0, 3) == "@@ ") {
                // Parse hunk header for start line
                static const std::regex hunkRe(R"(@@ -(\d+))");
                std::smatch m;
                if (std::regex_search(line, m, hunkRe)) {
                    diff.startLine = std::stoi(m[1].str());
                }
            } else if (!line.empty() && line[0] == '-' && line.substr(0, 3) != "---") {
                diff.oldContent += line.substr(1) + "\n";
            } else if (!line.empty() && line[0] == '+') {
                diff.newContent += line.substr(1) + "\n";
            }
        }

        if (!diff.oldContent.empty() || !diff.newContent.empty()) {
            diffs.push_back(std::move(diff));
        }
    }

    return diffs;
}

// ============================================================================
// Action generation
// ============================================================================

std::vector<ActionButton> ChatMessageRenderer::generateCodeActions(
    const CodeBlock& block, const std::string& messageId, int blockIndex) const {

    std::vector<ActionButton> actions;

    // Copy button (always)
    actions.push_back({
        messageId + "_copy_" + std::to_string(blockIndex),
        "Copy",
        "copy_code",
        block.code
    });

    // Apply button (for code blocks with a language)
    if (!block.language.empty() && !block.isDiff) {
        actions.push_back({
            messageId + "_apply_" + std::to_string(blockIndex),
            "Apply",
            "apply_edit",
            block.code
        });
    }

    // Insert at cursor
    actions.push_back({
        messageId + "_insert_" + std::to_string(blockIndex),
        "Insert",
        "insert_at_cursor",
        block.code
    });

    // Run in terminal (for shell/bash blocks)
    if (block.language == "bash" || block.language == "sh" ||
        block.language == "cmd" || block.language == "powershell" ||
        block.language == "ps1") {
        actions.push_back({
            messageId + "_run_" + std::to_string(blockIndex),
            "Run",
            "run_terminal",
            block.code
        });
    }

    return actions;
}

std::vector<ActionButton> ChatMessageRenderer::generateDiffActions(
    const DiffBlock& diff, const std::string& messageId, int blockIndex) const {

    std::vector<ActionButton> actions;

    // Apply diff
    actions.push_back({
        messageId + "_applydiff_" + std::to_string(blockIndex),
        "Apply Changes",
        "apply_edit",
        diff.newContent
    });

    // Reject diff
    actions.push_back({
        messageId + "_rejectdiff_" + std::to_string(blockIndex),
        "Reject",
        "reject_edit",
        ""
    });

    return actions;
}

// ============================================================================
// WebView2 Bridge
// ============================================================================

void ChatMessageRenderer::registerBridgeCallback(BridgeCallback callback, void* userData) {
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    m_bridgeCallback = callback;
    m_bridgeUserData = userData;
}

void ChatMessageRenderer::dispatchBridgeAction(const BridgeAction& action) {
    std::lock_guard<std::mutex> lock(m_bridgeMutex);
    if (m_bridgeCallback) {
        m_bridgeCallback(action, m_bridgeUserData);
    }
}

std::string ChatMessageRenderer::generateBridgeScript() const {
    return R"JS(
// RawrXD Chat Bridge — injected into WebView2
(function() {
    'use strict';

    window.bridgeAction = function(actionType, blockIndex, payload) {
        var msg = {
            type: 'bridge_action',
            actionType: actionType,
            blockIndex: blockIndex,
            payload: payload || ''
        };
        window.chrome.webview.postMessage(JSON.stringify(msg));
    };

    // Copy code to clipboard
    window.copyCodeBlock = function(idx) {
        var block = document.querySelector('[data-block-idx="' + idx + '"] code');
        if (block) {
            navigator.clipboard.writeText(block.textContent).then(function() {
                // Flash feedback
                block.parentElement.classList.add('copied');
                setTimeout(function() {
                    block.parentElement.classList.remove('copied');
                }, 1000);
            });
        }
    };

    // Apply edit via bridge
    window.applyEdit = function(idx) {
        bridgeAction('apply_edit', idx);
    };

    // Reject edit via bridge
    window.rejectEdit = function(idx) {
        bridgeAction('reject_edit', idx);
    };

    // Insert at cursor
    window.insertAtCursor = function(idx) {
        bridgeAction('insert_at_cursor', idx);
    };

    // Run in terminal
    window.runInTerminal = function(idx) {
        bridgeAction('run_terminal', idx);
    };

    // Show symbol definition
    window.showSymbolDefinition = function(symbol) {
        bridgeAction('show_definition', -1, symbol);
    };

    // Handle streaming updates
    window.updateStreamingMessage = function(messageId, html) {
        var el = document.querySelector('[data-message-id="' + messageId + '"] .message-body');
        if (el) el.innerHTML = html;
    };

    // Scroll to bottom
    window.scrollToBottom = function() {
        window.scrollTo(0, document.body.scrollHeight);
    };

    console.log('[RawrXD] Chat bridge initialized');
})();
)JS";
}

// ============================================================================
// Theme / CSS
// ============================================================================

void ChatMessageRenderer::setDarkMode(bool dark) {
    m_darkMode = dark;
}

std::string ChatMessageRenderer::generateCSS() const {
    if (m_darkMode) {
        return R"CSS(
:root {
    --bg: #1e1e1e;
    --fg: #d4d4d4;
    --bg-message-user: #2d2d30;
    --bg-message-assistant: #252526;
    --bg-message-system: #1b1b2f;
    --bg-message-error: #3b1c1c;
    --bg-code: #1a1a2e;
    --fg-code: #e0e0e0;
    --border: #3c3c3c;
    --accent: #569cd6;
    --diff-add-bg: #1e3a1e;
    --diff-del-bg: #3a1e1e;
    --diff-add-fg: #4ec9b0;
    --diff-del-fg: #f44747;
    --button-bg: #0e639c;
    --button-fg: #ffffff;
    --badge-bg: #404040;
}

body {
    background: var(--bg);
    color: var(--fg);
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    font-size: 13px;
    line-height: 1.5;
    margin: 0;
    padding: 8px;
}

.chat-message {
    margin: 8px 0;
    padding: 12px;
    border-radius: 6px;
    border-left: 3px solid var(--accent);
}
.role-user { background: var(--bg-message-user); }
.role-assistant { background: var(--bg-message-assistant); }
.role-system { background: var(--bg-message-system); }
.role-error { background: var(--bg-message-error); border-left-color: #f44747; }

.message-header {
    display: flex;
    gap: 8px;
    margin-bottom: 6px;
    font-size: 11px;
}
.role-badge {
    background: var(--badge-bg);
    padding: 2px 6px;
    border-radius: 3px;
    text-transform: uppercase;
    font-weight: 600;
}
.model-badge {
    color: var(--accent);
    font-style: italic;
}

.code-block-container {
    position: relative;
    margin: 8px 0;
    border-radius: 4px;
    overflow: hidden;
}
.code-lang {
    background: #333;
    padding: 2px 8px;
    font-size: 11px;
    color: #888;
}
.code-actions {
    position: absolute;
    top: 4px;
    right: 4px;
    display: flex;
    gap: 4px;
}
.code-actions button {
    background: var(--button-bg);
    color: var(--button-fg);
    border: none;
    padding: 2px 8px;
    border-radius: 3px;
    cursor: pointer;
    font-size: 11px;
}
.code-actions button:hover { opacity: 0.8; }

pre {
    background: var(--bg-code);
    padding: 12px;
    margin: 0;
    overflow-x: auto;
}
code {
    font-family: 'Cascadia Code', 'Fira Code', Consolas, monospace;
    font-size: 12px;
}
code.inline {
    background: var(--bg-code);
    padding: 1px 4px;
    border-radius: 3px;
}

.diff-block { margin: 8px 0; }
.diff-table { width: 100%; border-collapse: collapse; font-family: monospace; font-size: 12px; }
.diff-removed { background: var(--diff-del-bg); }
.diff-added { background: var(--diff-add-bg); }
.diff-removed .diff-content { color: var(--diff-del-fg); }
.diff-added .diff-content { color: var(--diff-add-fg); }
.diff-sign { width: 16px; text-align: center; user-select: none; }

.message-stats {
    margin-top: 6px;
    font-size: 11px;
    color: #888;
    display: flex;
    gap: 12px;
}

.cursor-blink {
    animation: blink 1s step-start infinite;
}
@keyframes blink {
    50% { opacity: 0; }
}

.copied pre { outline: 2px solid var(--accent); transition: outline 0.3s; }

blockquote {
    border-left: 3px solid var(--border);
    margin: 4px 0;
    padding: 4px 12px;
    color: #999;
}
)CSS";
    } else {
        return R"CSS(
:root {
    --bg: #ffffff;
    --fg: #333333;
    --bg-message-user: #f3f3f3;
    --bg-message-assistant: #f8f8f8;
    --bg-message-system: #e8e8f0;
    --bg-message-error: #fde8e8;
    --bg-code: #f5f5f5;
    --fg-code: #333;
    --border: #e0e0e0;
    --accent: #0066b8;
    --diff-add-bg: #e6ffe6;
    --diff-del-bg: #ffe6e6;
    --diff-add-fg: #22863a;
    --diff-del-fg: #cb2431;
    --button-bg: #0066b8;
    --button-fg: #ffffff;
    --badge-bg: #e0e0e0;
}
/* ... same structure, light colors ... */
body { background: var(--bg); color: var(--fg); font-family: 'Segoe UI', sans-serif; font-size: 13px; }
.chat-message { margin: 8px 0; padding: 12px; border-radius: 6px; border-left: 3px solid var(--accent); }
.role-user { background: var(--bg-message-user); }
.role-assistant { background: var(--bg-message-assistant); }
pre { background: var(--bg-code); padding: 12px; }
code { font-family: Consolas, monospace; font-size: 12px; }
)CSS";
    }
}

// ============================================================================
// Helpers
// ============================================================================

std::string ChatMessageRenderer::escapeHtml(const std::string& text) const {
    std::string result;
    result.reserve(text.size() + 32);
    for (char c : text) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c;
        }
    }
    return result;
}

std::string ChatMessageRenderer::applySyntaxClasses(const std::string& code,
                                                     const std::string& language) const {
    // Lightweight: wrap in <code class="language-X"> for JS highlighter
    return "<code class=\"language-" + escapeHtml(language) + "\">" +
           escapeHtml(code) + "</code>";
}

std::string ChatMessageRenderer::generateId() const {
    static std::atomic<uint64_t> counter{0};
    return "msg_" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

} // namespace UI
} // namespace RawrXD
