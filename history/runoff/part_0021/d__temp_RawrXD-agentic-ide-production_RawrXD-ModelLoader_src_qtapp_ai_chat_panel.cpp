#include "ai_chat_panel.hpp"
#include "agent_chat_breadcrumb.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QFont>
#include <QFontMetrics>
#include <QTextCursor>
#include <QDebug>
#include <QDebug>
#include <QDebug>

AIChatPanel::AIChatPanel(QWidget* parent) : QWidget(parent) {
    setupUI();
}

AIChatPanel::~AIChatPanel() = default;

void AIChatPanel::initialize()
{
    // Nothing additional; kept for backward compatibility
}

void AIChatPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(6);

    // Breadcrumb for modes / model selection
    m_breadcrumb = new AgentChatBreadcrumb(this);
    m_breadcrumb->initialize();
    layout->addWidget(m_breadcrumb);

    connect(m_breadcrumb, &AgentChatBreadcrumb::agentModeChanged,
            this, [this](AgentChatBreadcrumb::AgentMode mode) {
        emit agentModeChanged(static_cast<int>(mode));
    });
    connect(m_breadcrumb, &AgentChatBreadcrumb::modelSelected,
            this, [this](const QString& model) {
        emit modelSelected(model);
    });

    // Quick actions row
    m_quickActions = new QWidget(this);
    auto* qaLayout = new QHBoxLayout(m_quickActions);
    qaLayout->setContentsMargins(0, 0, 0, 0);
    qaLayout->setSpacing(6);
    const QStringList actions = {"Explain", "Fix", "Refactor", "Document", "Test"};
    for (const QString& act : actions) {
        auto* btn = new QPushButton(act, m_quickActions);
        btn->setFlat(true);
        btn->setMaximumHeight(26);
        connect(btn, &QPushButton::clicked, this, [this, act]() {
            onQuickActionClicked(act);
        });
        qaLayout->addWidget(btn);
    }
    qaLayout->addStretch();
    layout->addWidget(m_quickActions);

    // Model status label
    m_modelStatusLabel = new QLabel("Model: Not loaded", this);
    m_modelStatusLabel->setStyleSheet("color: #808080; font-size: 11px; padding: 4px;");
    layout->addWidget(m_modelStatusLabel);

    // Chat display
    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);
    layout->addWidget(m_chatDisplay, 1);

    // Input row
    auto* inputRow = new QWidget(this);
    auto* inputLayout = new QHBoxLayout(inputRow);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(6);

    m_messageInput = new QLineEdit(inputRow);
    m_messageInput->setPlaceholderText("Ask AI anything...");
    m_sendButton = new QPushButton("Send", inputRow);
    m_sendButton->setDefault(true);

    // Optional max mode toggle
    m_maxMode = new QCheckBox("Max", inputRow);
    connect(m_maxMode, &QCheckBox::toggled, this, &AIChatPanel::onMaxModeToggled);

    inputLayout->addWidget(m_messageInput, 1);
    inputLayout->addWidget(m_maxMode);
    inputLayout->addWidget(m_sendButton);
    layout->addWidget(inputRow);

    connect(m_sendButton, &QPushButton::clicked, this, &AIChatPanel::onSendClicked);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &AIChatPanel::onSendClicked);
}

void AIChatPanel::onSendClicked()
{
    const QString message = m_messageInput ? m_messageInput->text().trimmed() : QString();
    if (message.isEmpty()) return;
    if (m_messageInput) m_messageInput->clear();
    addUserMessage(message);
    emit messageSubmitted(message);
}

void AIChatPanel::addUserMessage(const QString& msg)
{
    appendMessage(msg, true, false);
}

void AIChatPanel::addAssistantMessage(const QString& msg, bool streaming)
{
    if (streaming) {
        m_streamingActive = true;
        appendMessage(msg.isEmpty() ? QStringLiteral("Generating…") : msg, false, true);
    } else {
        m_streamingActive = false;
        m_streamingBlock = -1;
        appendMessage(msg, false, false);
    }
}

void AIChatPanel::addSystemMessage(const QString& msg)
{
    if (!m_chatDisplay) return;
    QString html = QString("<div style='margin:4px 0; color:#808080;'><i>%1</i></div>")
                       .arg(msg.toHtmlEscaped());
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->insertHtml(html);
    m_chatDisplay->insertHtml("<br/>");
    m_chatDisplay->ensureCursorVisible();
}

void AIChatPanel::setPlaceholderText(const QString& text)
{
    if (m_messageInput) {
        m_messageInput->setPlaceholderText(text);
    }
}

void AIChatPanel::startStreaming()
{
    m_streamingActive = true;
    addAssistantMessage("", true);
}

void AIChatPanel::updateStreamingMessage(const QString& partial)
{
    if (!m_streamingActive || m_streamingBlock < 0 || !m_chatDisplay) return;
    QTextBlock block = m_chatDisplay->document()->findBlockByNumber(m_streamingBlock);
    if (!block.isValid()) return;
    QTextCursor cursor(block);
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.insertText(QString("AI: %1").arg(partial));
}

void AIChatPanel::finishStreaming()
{
    m_streamingActive = false;
    m_streamingBlock = -1;
}

void AIChatPanel::clearChat()
{
    if (m_chatDisplay) m_chatDisplay->clear();
}

void AIChatPanel::setInputEnabled(bool enabled)
{
    m_inputEnabled = enabled;
    if (m_messageInput) m_messageInput->setEnabled(enabled);
    if (m_sendButton) m_sendButton->setEnabled(enabled);
}

void AIChatPanel::setContext(const QString& code, const QString& filePath)
{
    m_contextCode = code;
    m_contextFilePath = filePath;
}

void AIChatPanel::setAgenticExecutor(AgenticExecutor* executor)
{
    m_agenticExecutor = executor;
    Q_UNUSED(m_agenticExecutor);
}

void AIChatPanel::setModelStatus(const QString& modelName, bool ready)
{
    if (!m_modelStatusLabel) return;
    
    if (ready && !modelName.isEmpty()) {
        m_modelStatusLabel->setText(QString("✓ Model: %1").arg(modelName));
        m_modelStatusLabel->setStyleSheet("color: #4ec9b0; font-size: 11px; padding: 4px;");
        setInputEnabled(true);
        setPlaceholderText("Ask AI anything...");
    } else {
        m_modelStatusLabel->setText("⚠ Model: Not loaded");
        m_modelStatusLabel->setStyleSheet("color: #f48771; font-size: 11px; padding: 4px;");
        setInputEnabled(false);
        setPlaceholderText("Load a model to start chatting...");
    }
}

void AIChatPanel::onQuickActionClicked(const QString& action)
{
    const QString context = m_contextCode.isEmpty() ? QString() : m_contextCode;
    emit quickActionTriggered(action, context);
    // Show in chat for transparency
    addUserMessage(QString("/%1").arg(action.toLower()));
}

void AIChatPanel::onMaxModeToggled(bool enabled)
{
    emit maxModeChanged(enabled);
}

void AIChatPanel::appendMessage(const QString& message, bool isUser, bool isStreaming)
{
    if (!m_chatDisplay) return;

    QString prefix = isUser ? "You: " : "AI: ";
    QString html = QString("<div style='margin:4px 0; color:%1;'><b>%2</b>%3</div>")
                        .arg(isUser ? "#4ec9b0" : "#d4d4d4")
                        .arg(prefix)
                        .arg(message.toHtmlEscaped());

    // Append and remember block for streaming updates
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_chatDisplay->setTextCursor(cursor);
    m_chatDisplay->insertHtml(html);
    m_chatDisplay->insertHtml("<br/>");
    m_chatDisplay->ensureCursorVisible();

    if (isStreaming) {
        QTextDocument* doc = m_chatDisplay->document();
        m_streamingBlock = doc->blockCount() - 1;
    }
}
