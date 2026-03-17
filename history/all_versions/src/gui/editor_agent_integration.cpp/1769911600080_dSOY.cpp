/**
 * @file editor_agent_integration.cpp
 * @brief Implementation of editor agentic integration
 *
 * Handles ghost text suggestions and integration with the Direct2D editor.
 */

#include "editor_agent_integration.hpp"
#include <windows.h> // For generic types if needed

// Helper for string conversion
static RawrXD::String toRawrString(const std::string& s) {
    // Assuming RawrXD::String has a constructor for ASCII or we convert to wstring
    std::wstring ws(s.begin(), s.end());
    return RawrXD::String(ws.c_str());
}

static std::string toStdString(const RawrXD::String& s) {
    // Assuming RawrXD::String has toStdWString or similar, or we act on raw data
    // This is a placeholder for the actual conversion logic specific to RawrXD::String
    // data() likely returns wchar_t*
    const wchar_t* w = s.constData(); // Based on standard Qt/String likeness
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    std::string str(len, 0);
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

    suggestionGenerating();
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

    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(m_currentSuggestion.text);
    m_editor->setTextCursor(cursor);

    std::string acceptedText = m_currentSuggestion.text;
    clearGhostText();

    suggestionAccepted(acceptedText);

    return true;
}

/**
 * @brief Dismiss suggestion
 */
void EditorAgentIntegration::dismissSuggestion()
{
    clearGhostText();
    suggestionDismissed();
}

/**
 * @brief Clear ghost text
 */
void EditorAgentIntegration::clearGhostText()
{
    m_currentSuggestion.text.clear();
    m_ghostTextRow = -1;
    m_ghostTextColumn = -1;

    // In a real implementation, would redraw the editor
    // For now, just clear state
}

/**
 * @brief Set ghost text style
 */
void EditorAgentIntegration::setGhostTextStyle(const std::string& font, const uint32_t& color)
{
    m_ghostTextFont = font;
    m_ghostTextColor = color;
}

// ─────────────────────────────────────────────────────────────────────────
// Private Slots
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Handle key press in editor
 */
void EditorAgentIntegration::onEditorKeyPressed(void*  event)
{
    if (!m_ghostTextEnabled || !m_agentBridge) {
        return;
    }

    // TAB: Trigger suggestion
    if (event->key() == //Key_Tab) {
        event->accept();
        triggerSuggestion();
        return;
    }

    // ENTER: Accept suggestion
    if (event->key() == //Key_Return && !m_currentSuggestion.text.empty()) {
        if (event->modifiers() & //ControlModifier) {
            event->accept();
            acceptSuggestion();
            return;
        }
    }

    // ESC: Dismiss suggestion
    if (event->key() == //Key_Escape && !m_currentSuggestion.text.empty()) {
        event->accept();
        dismissSuggestion();
        return;
    }

    // Other keys: Clear suggestion if typing regular text
    if (event->text().length() > 0 && event->text()[0].isLetterOrNumber()) {
        clearGhostText();
    }
}

/**
 * @brief Handle agent suggestion completion
 */
void EditorAgentIntegration::onSuggestionGenerated(const void*& result, int elapsedMs)
{
    if (result.value("success").toBool()) {
        GhostTextSuggestion suggestion = parseSuggestion(result);
        m_currentSuggestion = suggestion;

        auto [row, col] = getCursorPosition();
        m_ghostTextRow = row;
        m_ghostTextColumn = col;

        renderGhostText(suggestion.text, row, col);
        suggestionAvailable(suggestion);

    } else {
        std::string error = result.value("error").toString("Unknown error");
        suggestionError(error);
    }
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
        suggestionError("Agent bridge not set");
        return;
    }

    std::string wish = std::string("Suggest the next line of code for:\n"
                          "File: %1\n"
                          "Current line: %2\n"
                          "Context: %3")
                      );

    // Use plan mode (preview, not execute)
    m_agentBridge->planWish(wish);
}

/**
 * @brief Parse LLM response into suggestion
 */
GhostTextSuggestion EditorAgentIntegration::parseSuggestion(const void*& response) const
{
    GhostTextSuggestion suggestion;

    // Extract suggested code from action results
    auto actions = response.value("actions").toArray();
    if (!actions.empty()) {
        auto firstAction = actions[0].toObject();
        suggestion.text = firstAction.value("result").toString();
        suggestion.explanation = firstAction.value("description").toString();
        suggestion.confidence = 85;
    }

    // Limit length
    if (suggestion.text.length() > 200) {
        suggestion.text = suggestion.text.left(197) + "...";
    }

    return suggestion;
}

/**
 * @brief Render ghost text
 */
void EditorAgentIntegration::renderGhostText(const std::string& text, int row, int column)
{
    // In a real implementation, would:
    // 1. Create overlay widget or use QPainter to draw ghost text
    // 2. Position at cursor location
    // 3. Use dim color and italic font
    // 4. Update on editor resize
    //
    // For now, just store state

    m_ghostTextRow = row;
    m_ghostTextColumn = column;

}

/**
 * @brief Install event filter
 */
void EditorAgentIntegration::installEventFilter()
{
    if (!m_editor) {
        return;
    }

    // Create an event filter for the editor
    m_editor->installEventFilter(this);
}

/**
 * @brief Get cursor position
 */
std::pair<int, int> EditorAgentIntegration::getCursorPosition() const
{
    QTextCursor cursor = m_editor->textCursor();
    int row = cursor.blockNumber();
    int column = cursor.positionInBlock();

    return {row, column};
}

/**
 * @brief Get word under cursor
 */
std::string EditorAgentIntegration::getWordUnderCursor() const
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

/**
 * @brief Qt event filter override
 */
bool EditorAgentIntegration::eventFilter(void* obj, QEvent* event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        void*  keyEvent = static_cast<void* >(event);
        onEditorKeyPressed(keyEvent);

        if (keyEvent->isAccepted()) {
            return true;
        }
    }

    return void::eventFilter(obj, event);
}


