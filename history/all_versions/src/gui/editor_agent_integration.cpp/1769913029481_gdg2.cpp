/**
 * @file editor_agent_integration.cpp
 * @brief Implementation of editor agentic integration
 *
 * Handles ghost text suggestions and integration with the Direct2D editor.
 */

#include "editor_agent_integration.hpp"
#include <windows.h> // For generic types if needed
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Helper for string conversion
static RawrXD::String toRawrString(const std::string& s) {
    return RawrXD::String::fromUtf8(s.c_str());
}

static std::string toStdString(const RawrXD::String& s) {
    // Convert wide string to UTF-8
    const wchar_t* w = s.constData();
    if (!w) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string str(len - 1, 0); // -1 because len includes null terminator
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &str[0], len, NULL, NULL);
    return str;
}

/**
 * @brief Constructor - attach to editor
 */
EditorAgentIntegration::EditorAgentIntegration(RawrXD::EditorWindow* editor)
    : m_editor(editor)
{
    m_ghostTextColor = 0x666666;  // Gray
    // Font setup skipped - handled by EditorWindow
}

/**
 * @brief Destructor
 */
EditorAgentIntegration::~EditorAgentIntegration() = default;

/**
 * @brief Set agent bridge
 */
void EditorAgentIntegration::setAgentBridge(IDEAgentBridge* bridge)
{
    m_agentBridge = bridge;
}

/**
 * @brief Enable/disable ghost text
 */
void EditorAgentIntegration::setGhostTextEnabled(bool enabled)
{
    m_ghostTextEnabled = enabled;
    if (!enabled) {
        clearGhostText();
    }
}

/**
 * @brief Set file type
 */
void EditorAgentIntegration::setFileType(const std::string& fileType)
{
    m_fileType = fileType;
}

/**
 * @brief Enable auto suggestions
 */
void EditorAgentIntegration::setAutoSuggestions(bool enabled)
{
    m_autoSuggestions = enabled;
    // Auto-suggestion timer requires Window hooks or thread.
    // Marking as TODO for full non-blocking implementation
    // if (enabled) StartTimer(...)
}

/**
 * @brief Trigger suggestion manually
 */
void EditorAgentIntegration::triggerSuggestion(const GhostTextContext& context)
{
    if (!m_ghostTextEnabled || !m_agentBridge) {
        return;
    }

    GhostTextContext ctx = context.currentLine.empty() ? extractContext() : context;

    // suggestionGenerating();
    generateSuggestion(ctx);
}

/**
 * @brief Accept suggestion
 */
bool EditorAgentIntegration::acceptSuggestion()
{
    if (m_currentSuggestion.text.empty()) {
        return false;
    }

    if (m_editor) {
        m_editor->acceptGhostText();
    }

    std::string acceptedText = m_currentSuggestion.text;
    clearGhostText();

    // suggestionAccepted(acceptedText);

    return true;
}

/**
 * @brief Dismiss suggestion
 */
void EditorAgentIntegration::dismissSuggestion()
{
    clearGhostText();
    // suggestionDismissed();
}

/**
 * @brief Clear ghost text
 */
void EditorAgentIntegration::clearGhostText()
{
    m_currentSuggestion.text.clear();
    m_ghostTextRow = -1;
    m_ghostTextColumn = -1;

    if (m_editor) {
        m_editor->setGhostText(RawrXD::String(L""));
    }
}

/**
 * @brief Set ghost text style
 */
void EditorAgentIntegration::setGhostTextStyle(const std::string& font, const uint32_t& color)
{
    m_ghostTextFont = font;
    m_ghostTextColor = color;
}

// Private Slots removed - key handling delegated to EditorWindow

/**
 * @brief Handle agent suggestion completion
 */
void EditorAgentIntegration::onSuggestionGenerated(const void*& result, int elapsedMs)
{
    // TODO: Restore when generic JSON support is added to IDEAgentBridge
    /*
    if (result.value("success").toBool()) {
        GhostTextSuggestion suggestion = parseSuggestion(result);
        m_currentSuggestion = suggestion;

        auto [row, col] = getCursorPosition();
        m_ghostTextRow = row;
        m_ghostTextColumn = col;

        renderGhostText(suggestion.text, row, col);
        // suggestionAvailable(suggestion);

    } else {
        std::string error = result.value("error").toString("Unknown error");
        // suggestionError(error);
    }
    */
}

/**
 * @brief Auto-suggestion timer
 */
void EditorAgentIntegration::onAutoSuggestionTimer()
{
    if (m_autoSuggestions && m_ghostTextEnabled && m_agentBridge) {
        triggerSuggestion();
    }
}

/**
 * @brief Text completed
 */
void EditorAgentIntegration::onTextCompleted(const std::string& text)
{
    // Called when text is auto-completed
}

// ─────────────────────────────────────────────────────────────────────────
// Private Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Extract context from editor
 */
GhostTextContext EditorAgentIntegration::extractContext() const
{
    GhostTextContext context;
    context.fileType = m_fileType;
    if (!m_editor) return context;

    RawrXD::Point p = m_editor->getCursorPosition();
    context.cursorColumn = p.x;
    
    // Get current line
    RawrXD::String line = m_editor->getLine(p.y);
    context.currentLine = toStdString(line);
    
    // Get prev lines
    int start = (p.y > 10) ? (p.y - 10) : 0;
    for (int i = start; i < p.y; i++) {
        RawrXD::String pl = m_editor->getLine(i);
        context.previousLines += toStdString(pl) + "\n";
    }

    return context;
}

/**
 * @brief Generate suggestion via agent
 */
void EditorAgentIntegration::generateSuggestion(const GhostTextContext& context)
{
    if (!m_agentBridge) {
        // suggestionError("Agent bridge not set");
        return;
    }

    std::string wish = "Suggest the next line of code for:\nFile: " + m_fileType + 
                       "\nCurrent line: " + context.currentLine + 
                       "\nContext: " + context.previousLines;

    // Use plan mode (preview, not execute)
    m_agentBridge->planWish(wish);
}

/**
 * @brief Parse LLM response into suggestion
 */
GhostTextSuggestion EditorAgentIntegration::parseSuggestion(const void*& response) const
{
    GhostTextSuggestion suggestion;

    // TODO: Parse response object when type is known
    /*
    // Extract suggested code from action results
    auto actions = response.value("actions").toArray();
    if (!actions.empty()) {
        auto firstAction = actions[0].toObject();
        suggestion.text = firstAction.value("result").toString();
        suggestion.explanation = firstAction.value("description").toString();
        suggestion.confidence = 85;
    }
    */
    
    // Mock for now
    suggestion.text = "// Ghost Text Suggestion Mock"; 

    // Limit length
    if (suggestion.text.length() > 200) {
        suggestion.text = suggestion.text.substr(0, 197) + "...";
    }

    return suggestion;
}

/**
 * @brief Render ghost text
 */
void EditorAgentIntegration::renderGhostText(const std::string& text, int row, int column)
{
    m_ghostTextRow = row;
    m_ghostTextColumn = column;

    if (m_editor) {
        m_editor->setGhostText(toRawrString(text));
    }
}

// Event hooks removed - utilizing RawrXD::EditorWindow internal handling

/**
 * @brief Get cursor position
 */
std::pair<int, int> EditorAgentIntegration::getCursorPosition() const
{
    if (!m_editor) return {0,0};
    RawrXD::Point p = m_editor->getCursorPosition();
    return {p.y, p.x};
}

/**
 * @brief Get word under cursor
 */
std::string EditorAgentIntegration::getWordUnderCursor() const
{
    // Simplified implementation
    if (!m_editor) return "";
    RawrXD::Point p = m_editor->getCursorPosition();
    RawrXD::String line = m_editor->getLine(p.y);
    std::string s = toStdString(line);
    
    if (p.x >= s.length()) return "";

    int start = p.x;
    while(start > 0 && isalnum(s[start-1])) start--;
    int end = p.x;
    while(end < s.length() && isalnum(s[end])) end++;
    
    return s.substr(start, end-start);
}


