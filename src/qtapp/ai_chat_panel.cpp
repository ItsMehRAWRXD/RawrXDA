#include "ai_chat_panel.hpp"
#include <QDateTime>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QHBoxLayout>
#include <QFont>
#include <QFontMetrics>

AIChatPanel::AIChatPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    applyDarkTheme();
}

void AIChatPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Header
    QLabel* header = new QLabel("  AI Assistant", this);
    QFont headerFont = header->font();
    headerFont.setPointSize(11);
    headerFont.setBold(true);
    header->setFont(headerFont);
    header->setMinimumHeight(35);
    
    // Quick actions
    m_quickActionsWidget = createQuickActions();
    
    // Messages scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    
    m_messagesContainer = new QWidget();
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(10, 10, 10, 10);
    m_messagesLayout->setSpacing(10);
    m_messagesLayout->addStretch();
    
    m_scrollArea->setWidget(m_messagesContainer);
    
    // Input area
    QWidget* inputContainer = new QWidget(this);
    QHBoxLayout* inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(10, 8, 10, 8);
    inputLayout->setSpacing(8);
    
    m_inputField = new QLineEdit(inputContainer);
    m_inputField->setPlaceholderText("Ask AI anything...");
    m_inputField->setMinimumHeight(32);
    
    connect(m_inputField, &QLineEdit::returnPressed,
            this, &AIChatPanel::onSendClicked);
    
    m_sendButton = new QPushButton("Send", inputContainer);
    m_sendButton->setMinimumWidth(70);
    m_sendButton->setMaximumHeight(32);
    
    connect(m_sendButton, &QPushButton::clicked,
            this, &AIChatPanel::onSendClicked);
    
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    
    // Assembly
    mainLayout->addWidget(header);
    mainLayout->addWidget(m_quickActionsWidget);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(inputContainer);
    
    setLayout(mainLayout);
}

QWidget* AIChatPanel::createQuickActions()
{
    QWidget* container = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->setSpacing(5);
    
    QStringList actions = {"Explain", "Fix", "Refactor", "Document", "Test"};
    
    for (const QString& action : actions) {
        QPushButton* btn = new QPushButton(action, container);
        btn->setMaximumHeight(26);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
        
        connect(btn, &QPushButton::clicked, this, [this, action]() {
            onQuickActionClicked(action);
        });
        
        layout->addWidget(btn);
    }
    
    layout->addStretch();
    
    return container;
}

void AIChatPanel::applyDarkTheme()
{
    QString styleSheet = R"(
        AIChatPanel {
            background-color: #1e1e1e;
        }
        QLabel {
            background-color: #252526;
            color: #cccccc;
            border-bottom: 1px solid #3e3e42;
        }
        QScrollArea {
            background-color: #1e1e1e;
            border: none;
        }
        QLineEdit {
            background-color: #3c3c3c;
            color: #cccccc;
            border: 1px solid #3e3e42;
            border-radius: 4px;
            padding: 6px 10px;
            selection-background-color: #094771;
        }
        QLineEdit:focus {
            border: 1px solid #007acc;
        }
        QPushButton {
            background-color: #0e639c;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #1177bb;
        }
        QPushButton:pressed {
            background-color: #0d5a8f;
        }
        QPushButton[flat="true"] {
            background-color: #2d2d30;
            color: #cccccc;
            font-weight: normal;
        }
        QPushButton[flat="true"]:hover {
            background-color: #3e3e42;
        }
        QTextEdit {
            background-color: transparent;
            color: #cccccc;
            border: none;
            selection-background-color: #094771;
        }
    )";
    
    setStyleSheet(styleSheet);
}

void AIChatPanel::addUserMessage(const QString& message)
{
    Message msg;
    msg.role = Message::User;
    msg.content = message;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm");
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    scrollToBottom();
}

void AIChatPanel::addAssistantMessage(const QString& message, bool streaming)
{
    Message msg;
    msg.role = Message::Assistant;
    msg.content = message;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm");
    msg.isStreaming = streaming;
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    if (streaming) {
        m_streamingBubble = bubble;
        m_streamingText = bubble->findChild<QTextEdit*>();
    }
    
    scrollToBottom();
}

void AIChatPanel::updateStreamingMessage(const QString& content)
{
    if (m_streamingText) {
        m_streamingText->setPlainText(content);
        scrollToBottom();
    }
}

void AIChatPanel::finishStreaming()
{
    m_streamingBubble = nullptr;
    m_streamingText = nullptr;
}

QWidget* AIChatPanel::createMessageBubble(const Message& msg)
{
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    // Role label
    QLabel* roleLabel = new QLabel(
        msg.role == Message::User ? "You" : "AI Assistant"
    );
    QFont roleFont = roleLabel->font();
    roleFont.setPointSize(9);
    roleFont.setBold(true);
    roleLabel->setFont(roleFont);
    
    QString roleLabelStyle = QString(
        "QLabel { background-color: transparent; color: %1; border: none; }"
    ).arg(msg.role == Message::User ? "#569cd6" : "#4ec9b0");
    roleLabel->setStyleSheet(roleLabelStyle);
    
    // Message content
    QTextEdit* contentEdit = new QTextEdit();
    contentEdit->setPlainText(msg.content);
    contentEdit->setReadOnly(true);
    contentEdit->setFrameStyle(QFrame::NoFrame);
    contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Calculate height based on content
    QFontMetrics fm(contentEdit->font());
    int lineHeight = fm.lineSpacing();
    int numLines = msg.content.split('\n').count();
    int estimatedHeight = numLines * lineHeight + 20;
    contentEdit->setMaximumHeight(std::min(estimatedHeight, 300));
    
    QString bubbleStyle = QString(
        "QTextEdit { background-color: %1; border-radius: 8px; padding: 8px; }"
    ).arg(msg.role == Message::User ? "#2d2d30" : "#1a1a1a");
    contentEdit->setStyleSheet(bubbleStyle);
    
    // Timestamp
    QLabel* timeLabel = new QLabel(msg.timestamp);
    QFont timeFont = timeLabel->font();
    timeFont.setPointSize(8);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet("QLabel { background-color: transparent; color: #858585; border: none; }");
    
    layout->addWidget(roleLabel);
    layout->addWidget(contentEdit);
    layout->addWidget(timeLabel, 0, msg.role == Message::User ? Qt::AlignRight : Qt::AlignLeft);
    
    return container;
}

void AIChatPanel::onSendClicked()
{
    QString message = m_inputField->text().trimmed();
    if (message.isEmpty()) return;
    
    addUserMessage(message);
    m_inputField->clear();
    
    emit messageSubmitted(message);
}

void AIChatPanel::onQuickActionClicked(const QString& action)
{
    emit quickActionTriggered(action, m_contextCode);
}

void AIChatPanel::setContext(const QString& code, const QString& filePath)
{
    m_contextCode = code;
    m_contextFilePath = filePath;
}

void AIChatPanel::clear()
{
    // Remove all message widgets except the stretch
    while (m_messagesLayout->count() > 1) {
        QLayoutItem* item = m_messagesLayout->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    
    m_messages.clear();
    m_streamingBubble = nullptr;
    m_streamingText = nullptr;
}

void AIChatPanel::scrollToBottom()
{
    QScrollBar* scrollBar = m_scrollArea->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}
