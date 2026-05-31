/**
 * \file agentic_text_edit.cpp
 * \brief Production implementation of AgenticTextEdit with LSP and ghost text
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "agentic_text_edit.h"
#include "lsp_client.h"
#include "ghost_text_renderer.h"
#include "ai_completion_provider.h"
#include "core/rust_parser.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
#include <regex>

namespace RawrXD {

AgenticTextEdit::AgenticTextEdit(void* parent)
    : m_parent(parent)
    , m_lspClient(nullptr)
    , m_aiProvider(nullptr)
    , m_ghostRenderer(nullptr)
    , m_completionDelay(300)
    , m_autoCompletionsEnabled(true)
    , m_aiCompletionsEnabled(true)
    , m_documentOpened(false)
    , m_documentVersion(0)
    , m_cursorPos(0)
    , m_completionTimer(nullptr)
{
}

AgenticTextEdit::~AgenticTextEdit() {
    if (m_completionTimer) {
        m_completionTimer = nullptr;
    }
}

void AgenticTextEdit::initialize() {
    if (!m_ghostRenderer) {
        m_ghostRenderer = new GhostTextRenderer();
    }
}

void AgenticTextEdit::setLSPClient(LSPClient* client) {
    if (m_lspClient == client) return;
    m_lspClient = client;
    if (m_lspClient && !m_documentUri.empty()) {
        syncDocumentToLSP();
    }
}

LSPClient* AgenticTextEdit::lspClient() const {
    return m_lspClient;
}

void AgenticTextEdit::setAICompletionProvider(AICompletionProvider* provider) {
    m_aiProvider = provider;
}

AICompletionProvider* AgenticTextEdit::aiCompletionProvider() const {
    return m_aiProvider;
}

void AgenticTextEdit::setAICompletionsEnabled(bool enabled) {
    m_aiCompletionsEnabled = enabled;
}

bool AgenticTextEdit::aiCompletionsEnabled() const {
    return m_aiCompletionsEnabled;
}

GhostTextRenderer* AgenticTextEdit::ghostRenderer() const {
    return m_ghostRenderer;
}

void AgenticTextEdit::setDocumentUri(const std::string& uri) {
    if (m_documentUri == uri) return;
    if (m_documentOpened && m_lspClient && !m_documentUri.empty()) {
        m_lspClient->closeDocument(m_documentUri);
        m_documentOpened = false;
    }
    m_documentUri = uri;
    // Auto-detect language from extension
    size_t dot = uri.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = uri.substr(dot + 1);
        if (ext == "rs") m_languageId = "rust";
        else if (ext == "cpp" || ext == "cc" || ext == "cxx") m_languageId = "cpp";
        else if (ext == "c") m_languageId = "c";
        else if (ext == "h" || ext == "hpp") m_languageId = "cpp";
        else if (ext == "py") m_languageId = "python";
        else if (ext == "js") m_languageId = "javascript";
        else if (ext == "ts") m_languageId = "typescript";
        else if (ext == "go") m_languageId = "go";
        else if (ext == "java") m_languageId = "java";
        else if (ext == "cs") m_languageId = "csharp";
    }
    if (m_lspClient && !m_documentUri.empty()) {
        syncDocumentToLSP();
    }
}

void AgenticTextEdit::setLanguageId(const std::string& languageId) {
    m_languageId = languageId;
}

std::string AgenticTextEdit::documentUri() const {
    return m_documentUri;
}

void AgenticTextEdit::setAutoCompletionsEnabled(bool enabled) {
    m_autoCompletionsEnabled = enabled;
}

bool AgenticTextEdit::autoCompletionsEnabled() const {
    return m_autoCompletionsEnabled;
}

void AgenticTextEdit::setCompletionDelay(int ms) {
    m_completionDelay = std::max(50, std::min(ms, 5000));
}

void AgenticTextEdit::setText(const std::string& text) {
    m_buffer = text;
    m_cursorPos = static_cast<int>(m_buffer.length());
    m_documentVersion++;
    if (onTextChanged) onTextChanged(m_buffer);
    syncDocumentToLSP();
}

std::string AgenticTextEdit::text() const {
    return m_buffer;
}

void AgenticTextEdit::insertPlainText(const std::string& text) {
    m_buffer.insert(m_cursorPos, text);
    m_cursorPos += static_cast<int>(text.length());
    m_documentVersion++;
    if (onTextChanged) onTextChanged(m_buffer);
    if (m_autoCompletionsEnabled) {
        triggerCompletion();
    }
    syncDocumentToLSP();
}

void AgenticTextEdit::setCursorPosition(int pos) {
    m_cursorPos = std::max(0, std::min(pos, static_cast<int>(m_buffer.length())));
}

int AgenticTextEdit::cursorPosition() const {
    return m_cursorPos;
}

void AgenticTextEdit::keyPressEvent(void* event) {
    // Handle key events for completion acceptance/dismissal
    if (!event) return;
    // Cast to Win32 MSG structure for key processing
    MSG* msg = static_cast<MSG*>(event);
    if (msg->message == WM_KEYDOWN) {
        switch (msg->wParam) {
            case VK_TAB:
                acceptCompletion();
                break;
            case VK_ESCAPE:
                dismissCompletion();
                break;
            case VK_UP:
            case VK_DOWN:
                navigateCompletion(msg->wParam == VK_UP ? -1 : 1);
                break;
        }
    }
}

void AgenticTextEdit::onTextChanged() {
    m_documentVersion++;
    if (m_autoCompletionsEnabled) {
        triggerCompletion();
    }
    syncDocumentToLSP();
}

void AgenticTextEdit::onCursorPositionChanged() {
    if (m_ghostRenderer) {
        m_ghostRenderer->updatePosition(m_cursorPos);
    }
}

void AgenticTextEdit::onCompletionTimeout() {
    if (!m_autoCompletionsEnabled) return;
    std::string lineText = getCurrentLineText();
    if (!shouldTriggerCompletion(lineText)) return;
    
    if (m_lspClient && !m_documentUri.empty()) {
        int line = 0, character = 0;
        size_t pos = 0;
        for (size_t i = 0; i < m_buffer.length() && i < m_cursorPos; ++i) {
            if (m_buffer[i] == '\n') {
                line++;
                character = 0;
            } else {
                character++;
            }
        }
        m_lspClient->requestCompletion(m_documentUri, line, character);
    }
    
    if (m_aiCompletionsEnabled && m_aiProvider) {
        std::string prefix = m_buffer.substr(0, m_cursorPos);
        std::string suffix = m_buffer.substr(m_cursorPos);
        m_aiProvider->requestCompletion(prefix, suffix, m_languageId);
    }
    
    // Rust: use native parser for scope-aware symbol completions
    if (m_languageId == "rust" && !m_buffer.empty()) {
        using namespace rawrxd::ast::rust;
        RustParser parser;
        auto result = parser.parse(m_buffer, m_documentUri.empty() ? "untitled.rs" : m_documentUri);
        if (result.success && !result.nodes.empty()) {
            std::vector<CompletionItem> rustItems;
            for (const auto& node : result.nodes) {
                CompletionItem item;
                item.label = node->name;
                switch (node->type) {
                    case RustASTNode::Function: item.kind = 3; item.detail = "fn"; break;
                    case RustASTNode::Struct: item.kind = 23; item.detail = "struct"; break;
                    case RustASTNode::Trait: item.kind = 8; item.detail = "trait"; break;
                    case RustASTNode::Impl: item.kind = 3; item.detail = "impl"; break;
                    case RustASTNode::Module: item.kind = 9; item.detail = "mod"; break;
                    case RustASTNode::Enum: item.kind = 13; item.detail = "enum"; break;
                    case RustASTNode::TypeAlias: item.kind = 22; item.detail = "type"; break;
                    case RustASTNode::Const: item.kind = 21; item.detail = "const"; break;
                    case RustASTNode::Static: item.kind = 14; item.detail = "static"; break;
                    case RustASTNode::Macro: item.kind = 20; item.detail = "macro"; break;
                    default: item.kind = 1; item.detail = "symbol"; break;
                }
                rustItems.push_back(item);
            }
            if (!rustItems.empty()) {
                onCompletionsReceived(m_documentUri, 0, 0, rustItems);
            }
        }
    }
}

void AgenticTextEdit::onCompletionsReceived(const std::string& uri, int line, int character, 
                                            const std::vector<CompletionItem>& items) {
    if (items.empty()) return;
    if (m_ghostRenderer) {
        m_ghostRenderer->showCompletions(items);
    }
}

void AgenticTextEdit::onAICompletionsReceived(const std::vector<AICompletion>& completions) {
    if (completions.empty()) return;
    if (m_ghostRenderer) {
        m_ghostRenderer->showAICompletions(completions);
    }
}

void AgenticTextEdit::onAICompletionError(const std::string& error) {
    fprintf(stderr, "[AgenticTextEdit] AI Completion Error: %s\n", error.c_str());
    if (m_ghostRenderer) {
        m_ghostRenderer->showError(error);
    }
}

void AgenticTextEdit::onGhostTextAccepted(const std::string& text) {
    insertPlainText(text);
    if (m_onCompletionAccepted) {
        m_onCompletionAccepted(text);
    }
    if (m_ghostRenderer) {
        m_ghostRenderer->clear();
    }
}

void AgenticTextEdit::onGhostTextDismissed() {
    if (m_ghostRenderer) {
        m_ghostRenderer->clear();
    }
}

void AgenticTextEdit::triggerCompletion() {
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_completionDelay));
        onCompletionTimeout();
    }).detach();
}

void AgenticTextEdit::syncDocumentToLSP() {
    if (!m_lspClient || m_documentUri.empty()) return;
    if (!m_documentOpened) {
        m_lspClient->openDocument(m_documentUri, m_languageId, m_documentVersion, m_buffer);
        m_documentOpened = true;
    } else {
        m_lspClient->updateDocument(m_documentUri, m_documentVersion, m_buffer);
    }
}

std::string AgenticTextEdit::getCurrentLineText() const {
    size_t lineStart = m_buffer.rfind('\n', m_cursorPos - 1);
    if (lineStart == std::string::npos) {
        lineStart = 0;
    } else {
        lineStart++;
    }
    size_t lineEnd = m_buffer.find('\n', m_cursorPos);
    if (lineEnd == std::string::npos) {
        lineEnd = m_buffer.length();
    }
    return m_buffer.substr(lineStart, lineEnd - lineStart);
}

bool AgenticTextEdit::shouldTriggerCompletion(const std::string& lineText) const {
    if (lineText.empty()) return false;
    bool hasNonWhitespace = false;
    for (char c : lineText) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            hasNonWhitespace = true;
            break;
        }
    }
    if (!hasNonWhitespace) return false;
    
    static const std::vector<std::string> triggerPatterns = {
        ".", "::", "->", "(", "[", "{", ",", " "
    };
    for (const auto& pattern : triggerPatterns) {
        if (lineText.length() >= pattern.length()) {
            std::string end = lineText.substr(lineText.length() - pattern.length());
            if (end == pattern) {
                return true;
            }
        }
    }
    
    if (lineText.length() >= 3) {
        size_t lastSpace = lineText.find_last_of(" \t");
        if (lastSpace != std::string::npos && lastSpace + 3 < lineText.length()) {
            return true;
        }
    }
    return false;
}

void AgenticTextEdit::completionAccepted(const std::string& text) {
    onGhostTextAccepted(text);
}

void AgenticTextEdit::completionDismissed() {
    onGhostTextDismissed();
}

} // namespace RawrXD

