/**
 * @file editor_agent_integration.cpp
 * @brief Implementation of editor agentic integration
 *
 * Handles ghost text suggestions triggered by TAB key,
 * acceptance via ENTER, and rendering overlays.
 */

#include "editor_agent_integration.hpp"

#include <QPlainTextEdit>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QTimer>
#include <QDebug>

/**
 * @brief Constructor - attach to editor
 */
EditorAgentIntegration::EditorAgentIntegration(QPlainTextEdit* editor, QObject* parent)
    : QObject(parent)
    , m_editor(editor)
{
    m_ghostTextColor = QColor(102, 102, 102);  // Gray
    m_ghostTextFont = m_editor->font();
    m_ghostTextFont.setItalic(true);

    m_autoSuggestionTimer = new QTimer(this);
    connect(m_autoSuggestionTimer, &QTimer::timeout, this, &EditorAgentIntegration::onAutoSuggestionTimer);

    installEventFilter();
    qDebug() << "[EditorAgentIntegration] Initialized with editor";
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

    if (m_agentBridge) {
        connect(m_agentBridge, &IDEAgentBridge::agentCompleted,
                this, &EditorAgentIntegration::onSuggestionGenerated);
    }

    qDebug() << "[EditorAgentIntegration] Agent bridge connected";
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
    qDebug() << "[EditorAgentIntegration] Ghost text:" << (enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Set file type
 */
void EditorAgentIntegration::setFileType(const QString& fileType)
{
    m_fileType = fileType;
    qDebug() << "[EditorAgentIntegration] File type set to:" << fileType;
}

/**
 * @brief Enable auto suggestions
 */
void EditorAgentIntegration::setAutoSuggestions(bool enabled)
{
    m_autoSuggestions = enabled;

    if (enabled) {
        m_autoSuggestionTimer->start(1000);  // Generate suggestion every 1 second
        qDebug() << "[EditorAgentIntegration] Auto-suggestions ENABLED";
    } else {
        m_autoSuggestionTimer->stop();
        qDebug() << "[EditorAgentIntegration] Auto-suggestions DISABLED";
    }
}

/**
 * @brief Trigger suggestion manually
 */
void EditorAgentIntegration::triggerSuggestion(const GhostTextContext& context)
{
    if (!m_ghostTextEnabled || !m_agentBridge) {
        return;
    }

    GhostTextContext ctx = context.currentLine.isEmpty() ? extractContext() : context;

    emit suggestionGenerating();
    generateSuggestion(ctx);
}

/**
 * @brief Accept suggestion
 */
bool EditorAgentIntegration::acceptSuggestion()
{
    if (m_currentSuggestion.text.isEmpty()) {
        qWarning() << "[EditorAgentIntegration] No suggestion to accept";
        return false;
    }

    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(m_currentSuggestion.text);
    m_editor->setTextCursor(cursor);

    QString acceptedText = m_currentSuggestion.text;
    clearGhostText();

    emit suggestionAccepted(acceptedText);
    qDebug() << "[EditorAgentIntegration] Suggestion accepted:" << acceptedText.left(50);

    return true;
}

/**
 * @brief Dismiss suggestion
 */
void EditorAgentIntegration::dismissSuggestion()
{
    clearGhostText();
    emit suggestionDismissed();
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
void EditorAgentIntegration::setGhostTextStyle(const QFont& font, const QColor& color)
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
void EditorAgentIntegration::onEditorKeyPressed(QKeyEvent* event)
{
    if (!m_ghostTextEnabled || !m_agentBridge) {
        return;
    }

    // TAB: Trigger suggestion
    if (event->key() == Qt::Key_Tab) {
        event->accept();
        triggerSuggestion();
        return;
    }

    // ENTER: Accept suggestion
    if (event->key() == Qt::Key_Return && !m_currentSuggestion.text.isEmpty()) {
        if (event->modifiers() & Qt::ControlModifier) {
            event->accept();
            acceptSuggestion();
            return;
        }
    }

    // ESC: Dismiss suggestion
    if (event->key() == Qt::Key_Escape && !m_currentSuggestion.text.isEmpty()) {
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
void EditorAgentIntegration::onSuggestionGenerated(const QJsonObject& result, int elapsedMs)
{
    if (result.value("success").toBool()) {
        GhostTextSuggestion suggestion = parseSuggestion(result);
        m_currentSuggestion = suggestion;

        auto [row, col] = getCursorPosition();
        m_ghostTextRow = row;
        m_ghostTextColumn = col;

        renderGhostText(suggestion.text, row, col);
        emit suggestionAvailable(suggestion);

        qDebug() << "[EditorAgentIntegration] Suggestion generated in" << elapsedMs << "ms";
    } else {
        QString error = result.value("error").toString("Unknown error");
        emit suggestionError(error);
        qWarning() << "[EditorAgentIntegration] Error generating suggestion:" << error;
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
void EditorAgentIntegration::onTextCompleted(const QString& text)
{
    // Called when text is auto-completed
    qDebug() << "[EditorAgentIntegration] Text completed:" << text;
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

    QTextCursor cursor = m_editor->textCursor();
    QTextBlock block = cursor.block();

    // Current line
    context.currentLine = block.text();

    // Previous lines (up to 10 for context)
    QTextBlock prevBlock = block.previous();
    for (int i = 0; i < 10 && prevBlock.isValid(); ++i) {
        context.previousLines.prepend(prevBlock.text() + "\n");
        prevBlock = prevBlock.previous();
    }

    // Cursor position
    context.cursorColumn = cursor.positionInBlock();

    return context;
}

/**
 * @brief Generate suggestion via agent
 */
void EditorAgentIntegration::generateSuggestion(const GhostTextContext& context)
{
    if (!m_agentBridge) {
        emit suggestionError("Agent bridge not set");
        return;
    }

    QString wish = QString("Suggest the next line of code for:\n"
                          "File: %1\n"
                          "Current line: %2\n"
                          "Context: %3")
                      .arg(m_fileType, context.currentLine, context.previousLines.left(200));

    // Use plan mode (preview, not execute)
    m_agentBridge->planWish(wish);
}

/**
 * @brief Parse LLM response into suggestion
 */
GhostTextSuggestion EditorAgentIntegration::parseSuggestion(const QJsonObject& response) const
{
    GhostTextSuggestion suggestion;

    // Extract suggested code from action results
    auto actions = response.value("actions").toArray();
    if (!actions.isEmpty()) {
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
void EditorAgentIntegration::renderGhostText(const QString& text, int row, int column)
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

    qDebug() << "[EditorAgentIntegration] Rendering ghost text at" << row << ":" << column;
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
QPair<int, int> EditorAgentIntegration::getCursorPosition() const
{
    QTextCursor cursor = m_editor->textCursor();
    int row = cursor.blockNumber();
    int column = cursor.positionInBlock();

    return {row, column};
}

/**
 * @brief Get word under cursor
 */
QString EditorAgentIntegration::getWordUnderCursor() const
{
    QTextCursor cursor = m_editor->textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

/**
 * @brief Qt event filter override
 */
bool EditorAgentIntegration::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        onEditorKeyPressed(keyEvent);

        if (keyEvent->isAccepted()) {
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}
