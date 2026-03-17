/**
 * @file agentic_text_edit.cpp
 * @brief Implementation of AgenticTextEdit
 */

#include "agentic_text_edit.h"
#include <QDebug>

namespace RawrXD {

AgenticTextEdit::AgenticTextEdit(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_lspClient(nullptr)
    , m_aiProvider(nullptr)
    , m_ghostRenderer(nullptr)
    , m_aiCompletionsEnabled(true)
    , m_autoCompletionsEnabled(true)
{
}

void AgenticTextEdit::initialize()
{
    // Initialize ghost text renderer
    m_ghostRenderer = new GhostTextRenderer(this);
    
    // Setup timers, etc.
}

void AgenticTextEdit::setLSPClient(LSPClient* client)
{
    m_lspClient = client;
}

void AgenticTextEdit::setAICompletionProvider(AICompletionProvider* provider)
{
    m_aiProvider = provider;
}

void AgenticTextEdit::setAICompletionsEnabled(bool enabled)
{
    m_aiCompletionsEnabled = enabled;
}

void AgenticTextEdit::setDocumentUri(const QString& uri)
{
    m_documentUri = uri;
}

void AgenticTextEdit::setLineNumbersVisible(bool visible)
{
    // This would typically show/hide line numbers in the editor margin
    // For now, just log the setting change
    qDebug() << "[AgenticTextEdit] Line numbers visibility:" << visible;
}

void AgenticTextEdit::setAutoCompletionsEnabled(bool enabled)
{
    m_autoCompletionsEnabled = enabled;
}

} // namespace RawrXD
