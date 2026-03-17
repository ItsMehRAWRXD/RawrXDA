#include "AgentChatPane.h"
#include <QScrollBar>
#include <QDateTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>

namespace RawrXD {

AgentChatPane::AgentChatPane(QWidget* parent) : QWidget(parent) {
    setupUi();
}

AgentChatPane::~AgentChatPane() {
}

void AgentChatPane::setupUi() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout = new QVBoxLayout(this);

    // Chat Area
    m_chatScrollArea = new QScrollArea(this);
    m_chatScrollArea->setWidgetResizable(true);
    m_chatScrollArea->setFrameShape(QFrame::NoFrame);
    
    m_chatContainer = new QWidget();
    m_chatLayout = new QVBoxLayout(m_chatContainer);
    m_chatLayout->addStretch();
    
    m_chatScrollArea->setWidget(m_chatContainer);
    m_mainLayout->addWidget(m_chatScrollArea);

    // Status & Progress
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Agent Ready", this);
    m_statusLabel->setStyleSheet("color: #888; font-size: 10px;");
    
    m_thinkingProgress = new QProgressBar(this);
    m_thinkingProgress->setRange(0, 0);
    m_thinkingProgress->setFixedHeight(4);
    m_thinkingProgress->setTextVisible(false);
    m_thinkingProgress->hide();
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    m_mainLayout->addLayout(statusLayout);
    m_mainLayout->addWidget(m_thinkingProgress);

    // Input Area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText("Ask the agent anything...");
    
    m_sendButton = new QPushButton("Send", this);
    m_sendButton->setStyleSheet("background-color: #007acc; color: white; border-radius: 4px; padding: 5px 15px;");
    
    inputLayout->addWidget(m_inputField);
    inputLayout->addWidget(m_sendButton);
    m_mainLayout->addLayout(inputLayout);

    // Agentic Actions
    QHBoxLayout* actionLayout = new QHBoxLayout();
    m_planButton = new QPushButton("Plan Task", this);
    m_analyzeButton = new QPushButton("Analyze Code", this);
    
    QString actionStyle = "font-size: 10px; padding: 2px 8px;";
    m_planButton->setStyleSheet(actionStyle);
    m_analyzeButton->setStyleSheet(actionStyle);
    
    actionLayout->addWidget(m_planButton);
    actionLayout->addWidget(m_analyzeButton);
    actionLayout->addStretch();
    m_mainLayout->addLayout(actionLayout);

    // Connections
    connect(m_sendButton, &QPushButton::clicked, this, &AgentChatPane::onSendClicked);
    connect(m_inputField, &QLineEdit::returnPressed, this, &AgentChatPane::onSendClicked);
    connect(m_planButton, &QPushButton::clicked, this, &AgentChatPane::onPlanTaskClicked);
    connect(m_analyzeButton, &QPushButton::clicked, this, [this](){ emit analyzeCodeRequest(); });
}

void AgentChatPane::addMessage(const QString& sender, const QString& text, bool isAi) {
    QFrame* bubble = new QFrame(m_chatContainer);
    bubble->setFrameShape(QFrame::StyledPanel);
    bubble->setStyleSheet(isAi ? 
        "background-color: #2d2d2d; border-radius: 8px; margin: 5px;" : 
        "background-color: #3e3e3e; border-radius: 8px; margin: 5px;");
    
    QVBoxLayout* bubbleLayout = new QVBoxLayout(bubble);
    
    QLabel* senderLabel = new QLabel(QString("<b>%1</b>").arg(sender), bubble);
    senderLabel->setStyleSheet("color: #007acc; font-size: 10px;");
    
    QLabel* textLabel = new QLabel(text, bubble);
    textLabel->setWordWrap(true);
    textLabel->setTextFormat(Qt::MarkdownText);
    
    bubbleLayout->addWidget(senderLabel);
    bubbleLayout->addWidget(textLabel);
    
    m_chatLayout->insertWidget(m_chatLayout->count() - 1, bubble);
    
    QTimer::singleShot(50, [this](){
        m_chatScrollArea->verticalScrollBar()->setValue(m_chatScrollArea->verticalScrollBar()->maximum());
    });
}

void AgentChatPane::setThinking(bool thinking) {
    m_thinkingProgress->setVisible(thinking);
    m_sendButton->setEnabled(!thinking);
    m_statusLabel->setText(thinking ? "Agent is thinking..." : "Agent Ready");
}

void AgentChatPane::updateModelInfo(const QString& modelName, const QString& tier) {
    m_statusLabel->setText(QString("Model: %1 | Tier: %2").arg(modelName, tier));
}

void AgentChatPane::onSendClicked() {
    QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) return;
    
    addMessage("User", text, false);
    m_inputField->clear();
    emit sendMessage(text);
}

void AgentChatPane::onPlanTaskClicked() {
    QString text = m_inputField->text().trimmed();
    if (text.isEmpty()) {
        addMessage("System", "Please enter a goal in the input field first.", false);
        return;
    }
    emit planTaskRequest(text);
}

} // namespace RawrXD
