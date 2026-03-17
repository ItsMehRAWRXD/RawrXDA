#include "ai_chat_panel.hpp"
#include "chat_production_infrastructure.hpp"
#include "chat_history_manager.h"
#include "agent_chat_breadcrumb.hpp"
#include "command_palette.hpp"
#include "inference_engine.hpp"
#include "agentic_executor.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QTextEdit>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMenu>
#include <QClipboard>
#include <QDateTime>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QFont>
#include <QFontMetrics>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QInputDialog>
#include <QMainWindow>
#include <QStatusBar>
#include <QProcess>
#include <algorithm>

AIChatPanel::AIChatPanel(QWidget* parent)
    : QWidget(parent)
{
    RAWRXD_INIT_TIMED("AIChatPanel");
    // Lazy initialization - defer all Qt widget creation
    // Configuration will be set up when initialize() is called
    qDebug() << "AIChatPanel created with lazy initialization - D: \\temp location";
    m_projectRoot = QDir::currentPath();
    
    // Initialize production infrastructure
    m_infrastructure = new RawrXD::Chat::ChatProductionInfrastructure(this);
    m_infrastructure->initialize();
    
    // Connect infrastructure alerts
    connect(m_infrastructure, &RawrXD::Chat::ChatProductionInfrastructure::infrastructureAlert,
            this, [this](const QString& component, const QString& message) {
        qWarning() << "[AIChatPanel] Infrastructure alert:" << component << "-" << message;
    });
}

void AIChatPanel::setCurrentSessionId(const QString& sessionId) {
    m_currentSessionId = sessionId;
}

bool AIChatPanel::isInputEnabled() const {
    return m_sendButton && m_sendButton->isEnabled();
}

void AIChatPanel::initialize(const QString& preloadedModel) {
    RAWRXD_TIMED_FUNC();
    if (m_initialized) return;  // Already initialized
    
    // Initialize configuration with defaults
    m_cloudEnabled = false;
    m_localEnabled = true; // Default to local (built-in models, no Ollama needed)
    m_cloudEndpoint = "https://api.openai.com/v1/chat/completions";
    m_localEndpoint = "";  // Empty - using built-in models instead of Ollama
    m_apiKey = QString();
    m_requestTimeout = 30000;
    
    // Create Qt widgets
    setupUI();
    applyTheme();
    
    m_initialized = true;
    m_widgetsCreated = true;
    
    // If a model was preloaded, set it and enable input, skip the fetch models timer
    if (!preloadedModel.isEmpty()) {
        m_localModel = preloadedModel;
        setInputEnabled(true);
        qDebug() << "[AIChatPanel::initialize] Panel initialized with preloaded model:" << preloadedModel;
        // Still populate the model list, but don't disable input
        QTimer::singleShot(100, this, [this]() {
            fetchAvailableModels();  // This will see m_localModel is not empty and skip disabling
        });
    } else {
        // No preloaded model - fetch available models and disable input until selection
        QTimer::singleShot(100, this, &AIChatPanel::fetchAvailableModels);
    }
    
    snapshotProjectFiles();
    
    qDebug() << "AIChatPanel initialized with lazy loading - D: \\temp location";
}

void AIChatPanel::setupUI()
{
    RAWRXD_TIMED_FUNC();
    if (m_widgetsCreated) {
        qWarning() << "UI already setup - skipping";
        return;
    }
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Agent chat breadcrumb with mode and model selector
    m_breadcrumb = new AgentChatBreadcrumb(this);
    m_breadcrumb->initialize();
    m_breadcrumb->setMaximumHeight(40);
    m_breadcrumb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(m_breadcrumb);
    
    // Connect breadcrumb signals to this panel
    connect(m_breadcrumb, &AgentChatBreadcrumb::agentModeChanged,
            this, [this](AgentChatBreadcrumb::AgentMode mode) {
                emit agentModeChanged(static_cast<int>(mode));
                qDebug() << "[AIChatPanel] Agent mode changed to:" << static_cast<int>(mode);
            });
    
    connect(m_breadcrumb, &AgentChatBreadcrumb::modelSelected,
            this, [this](const QString& modelName) {
                setSelectedModel(modelName);
                emit modelSelected(modelName);
                qDebug() << "[AIChatPanel] Model selected:" << modelName;
            });
    
    // Quick actions
    m_quickActionsWidget = createQuickActions();
    m_quickActionsWidget->setMaximumHeight(35);
    m_quickActionsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(m_quickActionsWidget);
    
    qDebug() << "[AIChatPanel] Quick actions widget created and added";
    
    // Messages scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameStyle(QFrame::NoFrame);
    m_scrollArea->setMinimumHeight(200); // Ensure scroll area has minimum size
    // Enable text selection for copy/paste of entire chat
    m_scrollArea->setFocusPolicy(Qt::StrongFocus);
    
    m_messagesContainer = new QWidget();
    m_messagesLayout = new QVBoxLayout(m_messagesContainer);
    m_messagesLayout->setContentsMargins(10, 10, 10, 10);
    m_messagesLayout->setSpacing(10);
    m_messagesLayout->addStretch();
    
    m_scrollArea->setWidget(m_messagesContainer);
    
    // Add context menu to scroll area for copy/copy all/new chat
    m_scrollArea->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_scrollArea, &QWidget::customContextMenuRequested,
            this, &AIChatPanel::showContextMenu);
    
    qDebug() << "[AIChatPanel] Scroll area created with message container";
    
    // Input area
    QWidget* inputContainer = new QWidget(this);
    inputContainer->setMaximumHeight(50);
    inputContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    
    // Model selector  
    QWidget* modelContainer = new QWidget(this);
    modelContainer->setMaximumHeight(45);
    modelContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* modelLayout = new QHBoxLayout(modelContainer);
    modelLayout->setContentsMargins(10, 5, 10, 5);
    modelLayout->setSpacing(8);
    
    QLabel* modelLabel = new QLabel("Model:", modelContainer);
    modelLabel->setMinimumWidth(50);
    
    m_modelSelector = new QComboBox(modelContainer);
    m_modelSelector->setMinimumHeight(28);
    m_modelSelector->addItem("Loading models...");
    connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIChatPanel::onModelSelected);

    // Mode selector
    QLabel* modeLabel = new QLabel("Mode:", modelContainer);
    modeLabel->setMinimumWidth(45);
    m_modeSelector = new QComboBox(modelContainer);
    m_modeSelector->setMinimumHeight(28);
    m_modeSelector->addItem("Default", ModeDefault);
    m_modeSelector->addItem("Max", ModeMax);
    m_modeSelector->addItem("Deep Thinking", ModeDeepThinking);
    m_modeSelector->addItem("Thinking Research", ModeThinkingResearch);
    m_modeSelector->addItem("Deep Research", ModeDeepResearch);
    connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        setChatMode(static_cast<ChatMode>(m_modeSelector->itemData(idx).toInt()));
    });

    // Context selector
    QLabel* ctxLabel = new QLabel("Context:", modelContainer);
    ctxLabel->setMinimumWidth(60);
    m_contextSelector = new QComboBox(modelContainer);
    m_contextSelector->setMinimumHeight(28);
    QList<int> ctxValues = {4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1000000};
    for (int v : ctxValues) {
        QString label = v >= 1000 ? QString::number(v/1000) + "k" : QString::number(v);
        m_contextSelector->addItem(label, v);
    }
    connect(m_contextSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        setContextWindow(m_contextSelector->itemData(idx).toInt());
    });

    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(m_modelSelector, 1);
    modelLayout->addWidget(modeLabel);
    modelLayout->addWidget(m_modeSelector);
    modelLayout->addWidget(ctxLabel);
    modelLayout->addWidget(m_contextSelector);
    m_multiModelButton = new QPushButton("Select Models...", modelContainer);
    m_multiModelButton->setMinimumHeight(28);
    connect(m_multiModelButton, &QPushButton::clicked, this, &AIChatPanel::openModelsDialog);
    modelLayout->addWidget(m_multiModelButton);
    
    // Checkpoint controls
    QPushButton* saveCheckpointBtn = new QPushButton("💾 Save", modelContainer);
    saveCheckpointBtn->setMinimumHeight(28);
    saveCheckpointBtn->setToolTip("Save current chat as checkpoint");
    connect(saveCheckpointBtn, &QPushButton::clicked, this, &AIChatPanel::onSaveCheckpoint);
    modelLayout->addWidget(saveCheckpointBtn);
    
    QPushButton* restoreCheckpointBtn = new QPushButton("⏮ Restore", modelContainer);
    restoreCheckpointBtn->setMinimumHeight(28);
    restoreCheckpointBtn->setToolTip("Restore chat from checkpoint");
    connect(restoreCheckpointBtn, &QPushButton::clicked, this, &AIChatPanel::onRestoreCheckpoint);
    modelLayout->addWidget(restoreCheckpointBtn);
    
    // Assembly - add widgets to main layout in correct order
    qDebug() << "[AIChatPanel] Adding widgets to main layout...";
    mainLayout->addWidget(m_quickActionsWidget);
    mainLayout->addWidget(m_scrollArea, 1);  // Stretch factor 1 = takes remaining space
    mainLayout->addWidget(modelContainer);
    mainLayout->addWidget(inputContainer);
    
    qDebug() << "[AIChatPanel] All widgets added to main layout";
    qDebug() << "[AIChatPanel] Main layout widget count:" << mainLayout->count();
    
    setLayout(mainLayout);
    
    qDebug() << "[AIChatPanel] Layout set complete, m_widgetsCreated = true";
    m_widgetsCreated = true;

    // Networking setup
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        // Note: Individual replies connect their own finished signals
    }
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

void AIChatPanel::applyTheme()
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
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized - cannot add message";
        return;
    }
    
    ChatMessage msg(ChatMessage::User, message, "User");
    msg.setMetadata("intent", classifyMessageIntent(message));
    msg.setMetadata("timestamp_ms", QDateTime::currentMSecsSinceEpoch());
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    scrollToBottom();
}

void AIChatPanel::addAssistantMessage(const QString& message, bool streaming)
{
    RAWRXD_TIMED_FUNC();
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized - cannot add assistant message";
        return;
    }
    
    // If we're already streaming, don't add a new message, just update the existing one
    if (streaming && m_streamingBubble) {
        updateStreamingMessage(message);
        return;
    }
    
    ChatMessage msg(ChatMessage::Assistant, message, "InferenceEngine");
    msg.setMetadata("timestamp_ms", QDateTime::currentMSecsSinceEpoch());
    msg.setMetadata("is_streaming", streaming);
    msg.isStreaming = streaming;
    
    // Detect if this is an inline edit response by checking the previous user message
    if (!m_messages.isEmpty()) {
        const ChatMessage& lastMsg = m_messages.last();
        if (lastMsg.role == ChatMessage::User && lastMsg.content.startsWith("@inline")) {
            msg.isInline = true;
            msg.setMetadata("is_inline", true);
            qDebug() << "[AIChatPanel] Detected inline edit response";
        }
    }
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    if (streaming) {
        m_streamingBubble = bubble;
        m_streamingTextEdit = bubble->findChild<QTextEdit*>();
        qDebug() << "[AIChatPanel::addAssistantMessage] Streaming bubble created";
        qDebug() << "[AIChatPanel::addAssistantMessage] m_streamingTextEdit found:" << (m_streamingTextEdit ? "YES" : "NO");
        if (m_streamingTextEdit) {
            qDebug() << "[AIChatPanel::addAssistantMessage] m_streamingTextEdit initial text:" << m_streamingTextEdit->toPlainText();
        }
    }
    
    scrollToBottom();
}

void AIChatPanel::updateStreamingMessage(const QString& content)
{
    RAWRXD_TIMED_FUNC();
    qDebug() << "[AIChatPanel::updateStreamingMessage] Token:" << content;
    qDebug() << "[AIChatPanel::updateStreamingMessage] m_streamingTextEdit is" << (m_streamingTextEdit ? "NOT null" : "NULL");
    qDebug() << "[AIChatPanel::updateStreamingMessage] m_streamingBubble is" << (m_streamingBubble ? "NOT null" : "NULL");

    if (m_streamingTextEdit) {
        QString current = m_streamingTextEdit->toPlainText();
        m_streamingTextEdit->setPlainText(current + content);

        // Auto-resize the bubble height based on content
        QFontMetrics fm(m_streamingTextEdit->font());
        int lineHeight = fm.lineSpacing();
        int numLines = m_streamingTextEdit->document()->lineCount();
        // Add padding: 20px base + lines
        int estimatedHeight = numLines * lineHeight + 20;
        // Cap max height at 600px (scrollable after that)
        m_streamingTextEdit->setMaximumHeight(std::min(estimatedHeight, 600));
        m_streamingTextEdit->setMinimumHeight(std::min(estimatedHeight, 600));

        qDebug() << "[AIChatPanel::updateStreamingMessage] Updated text, now length:" << (current + content).length();        // Update message object in list
        if (!m_messages.isEmpty() && m_messages.last().isStreaming) {
            m_messages.last().content = current + content;
        }
        
        scrollToBottom();
    } else {
        qWarning() << "[AIChatPanel::updateStreamingMessage] m_streamingText is NULL - cannot update streaming message!";
    }
}

void AIChatPanel::addErrorMessage(const QString& error, const QString& details, bool retryable)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized - cannot add error message";
        return;
    }
    
    ChatMessage msg(ChatMessage::System, error, "ErrorHandler");
    msg.setError(error, details);
    msg.setMetadata("retryable", retryable);
    msg.setMetadata("timestamp_ms", QDateTime::currentMSecsSinceEpoch());
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    scrollToBottom();
}

QString AIChatPanel::finishStreaming()
{
    QString fullContent;
    if (m_streamingTextEdit) {
        fullContent = m_streamingTextEdit->toPlainText();
    }
    
    m_streamingBubble = nullptr;
    m_streamingTextEdit = nullptr;

    if (!m_messages.isEmpty() && m_messages.last().isStreaming) {
        m_messages.last().isStreaming = false;
    }
    
    return fullContent;
}

QString AIChatPanel::lastAssistantMessage() const {
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages.at(i).role == ChatMessage::Assistant) return m_messages.at(i).content;
    }
    return QString();
}

QWidget* AIChatPanel::createMessageBubble(const ChatMessage& msg)
{
    QWidget* container = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    
    // Role label with source information
    QString roleText = msg.role == ChatMessage::User ? "You" : QString("AI Assistant (%1)").arg(msg.source);
    QLabel* roleLabel = new QLabel(roleText);
    QFont roleFont = roleLabel->font();
    roleFont.setPointSize(9);
    roleFont.setBold(true);
    roleLabel->setFont(roleFont);
    
    QString roleLabelStyle = QString(
        "QLabel { background-color: transparent; color: %1; border: none; }"
    ).arg(msg.role == ChatMessage::User ? "#569cd6" : "#4ec9b0");
    roleLabel->setStyleSheet(roleLabelStyle);
    
    // Metadata display (latency, requestId, etc.)
    QString metadataText;
    if (msg.metadata.contains("latency_ms")) {
        metadataText += QString("Latency: %1ms ").arg(msg.metadata["latency_ms"].toVariant().toLongLong());
    }
    if (msg.metadata.contains("requestId")) {
        metadataText += QString("Request: %1 ").arg(msg.metadata["requestId"].toString().left(8));
    }
    
    QLabel* metadataLabel = nullptr;
    if (!metadataText.isEmpty()) {
        metadataLabel = new QLabel(metadataText);
        QFont metadataFont = metadataLabel->font();
        metadataFont.setPointSize(7);
        metadataLabel->setFont(metadataFont);
        metadataLabel->setStyleSheet("QLabel { background-color: transparent; color: #858585; border: none; font-style: italic; }");
    }
    
    // Message content
    QTextEdit* contentEdit = new QTextEdit();
    contentEdit->setPlainText(msg.content);
    contentEdit->setReadOnly(true);
    contentEdit->setFrameStyle(QFrame::NoFrame);
    contentEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Enable text selection and copy across the entire chat
    contentEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    contentEdit->setFocusPolicy(Qt::NoFocus);  // Don't take focus away from input field
    
    // Calculate height based on content
    QFontMetrics fm(contentEdit->font());
    int lineHeight = fm.lineSpacing();
    int numLines = msg.content.split('\n').count();
    int estimatedHeight = numLines * lineHeight + 20;
    contentEdit->setMaximumHeight(std::min(estimatedHeight, 300));
    
    QString bubbleStyle = QString(
        "QTextEdit { background-color: %1; border-radius: 8px; padding: 8px; }"
    ).arg(msg.role == ChatMessage::User ? "#2d2d30" : "#1a1a1a");
    contentEdit->setStyleSheet(bubbleStyle);
    
    // Timestamp
    QLabel* timeLabel = new QLabel(msg.timestamp);
    QFont timeFont = timeLabel->font();
    timeFont.setPointSize(8);
    timeLabel->setFont(timeFont);
    timeLabel->setStyleSheet("QLabel { background-color: transparent; color: #858585; border: none; }");
    
    // Layout organization
    layout->addWidget(roleLabel);
    if (metadataLabel) {
        layout->addWidget(metadataLabel);
    }
    layout->addWidget(contentEdit);
    
    // Error handling buttons for retryable errors
    if (msg.isError && msg.getMetadata("retryable").toBool()) {
        QWidget* errorActions = new QWidget(container);
        QHBoxLayout* errorLayout = new QHBoxLayout(errorActions);
        errorLayout->setContentsMargins(0, 0, 0, 0);
        errorLayout->setSpacing(6);
        
        QPushButton* retryBtn = new QPushButton("🔄 Retry", errorActions);
        retryBtn->setMaximumHeight(24);
        retryBtn->setStyleSheet("QPushButton { background-color: #d73a49; color: white; border: none; border-radius: 3px; padding: 2px 8px; font-size: 9px; }");
        
        QPushButton* detailsBtn = new QPushButton("🔍 Details", errorActions);
        detailsBtn->setMaximumHeight(24);
        detailsBtn->setStyleSheet("QPushButton { background-color: #6f42c1; color: white; border: none; border-radius: 3px; padding: 2px 8px; font-size: 9px; }");
        
        errorLayout->addWidget(retryBtn);
        errorLayout->addWidget(detailsBtn);
        errorLayout->addStretch();
        
        layout->addWidget(errorActions);
        
        // Connect retry button - retry the last user message
        connect(retryBtn, &QPushButton::clicked, this, [this]() {
            // Find the last user message and retry it
            for (int i = m_messages.size() - 1; i >= 0; --i) {
                if (m_messages[i].role == ChatMessage::User) {
                    emit messageSubmitted(m_messages[i].content);
                    break;
                }
            }
        });
        
        // Connect details button
        connect(detailsBtn, &QPushButton::clicked, this, [this, msg]() {
            QString details = msg.errorDetails;
            if (details.isEmpty()) {
                details = msg.content;
            }
            QMessageBox::information(const_cast<AIChatPanel*>(this), "Error Details", details);
        });
    }
    
    layout->addWidget(timeLabel, 0, msg.role == ChatMessage::User ? Qt::AlignRight : Qt::AlignLeft);
    
    // Add approve/reject buttons for code-containing messages
    if (msg.role == ChatMessage::Assistant && (msg.content.contains("```") || msg.isInline)) {
        QWidget* approveRow = new QWidget(container);
        QHBoxLayout* approveLayout = new QHBoxLayout(approveRow);
        approveLayout->setContentsMargins(0, 0, 0, 0);
        approveLayout->setSpacing(6);

        QPushButton* approveBtn = new QPushButton("✓ Approve", approveRow);
        approveBtn->setMaximumHeight(24);
        approveBtn->setStyleSheet("QPushButton { background-color: #22863a; color: white; border: none; border-radius: 3px; padding: 2px 8px; font-size: 9px; }");

        QPushButton* rejectBtn = new QPushButton("✗ Reject", approveRow);
        rejectBtn->setMaximumHeight(24);
        rejectBtn->setStyleSheet("QPushButton { background-color: #b31d28; color: white; border: none; border-radius: 3px; padding: 2px 8px; font-size: 9px; }");

        connect(approveBtn, &QPushButton::clicked, this, [this, msg]() {
            QString code = extractCodeFromMessage(msg.content);
            if (code.isEmpty() && msg.isInline) {
                // If it's an inline edit response without markdown, use the whole content
                code = msg.content;
            }
            if (!code.isEmpty()) {
                emit codeApproved(code);
                emit codeInsertRequested(code);
            }
        });
        connect(rejectBtn, &QPushButton::clicked, this, [this, msg]() {
            QString code = extractCodeFromMessage(msg.content);
            if (!code.isEmpty()) {
                emit codeRejected(code);
            }
        });

        approveLayout->addStretch();
        approveLayout->addWidget(approveBtn);
        approveLayout->addWidget(rejectBtn);
        layout->addWidget(approveRow, 0, Qt::AlignRight);
    }

    // Show created file note if present
    if (msg.role == ChatMessage::Assistant && msg.content.contains("Created file:")) {
        QRegularExpression rx("Created file:\\s*(.+)");
        auto m = rx.match(msg.content);
        if (m.hasMatch()) {
            QString path = m.captured(1).trimmed();
            QLabel* createdLabel = new QLabel(QString("Created: %1").arg(path), container);
            createdLabel->setStyleSheet("QLabel { background-color: transparent; color: #4ec9b0; border: none; }");
            layout->addWidget(createdLabel, 0, Qt::AlignLeft);
        }
    }
    
    return container;
}

QString AIChatPanel::extractCodeFromMessage(const QString& message) const {
    // Extract code from markdown code blocks (```language\ncode\n```)
    QRegularExpression codeBlockRegex("```(?:\\w+)?\\n(.*?)\\n```", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = codeBlockRegex.match(message);
    
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    // If no code blocks found, check for inline code
    if (message.contains("`")) {
        QRegularExpression inlineCodeRegex("`([^`]+)`");
        QRegularExpressionMatchIterator it = inlineCodeRegex.globalMatch(message);
        QStringList codeParts;
        while (it.hasNext()) {
            codeParts << it.next().captured(1);
        }
        if (!codeParts.isEmpty()) {
            return codeParts.join("\n");
        }
    }
    
    // For @inline responses, if no markdown is found, the whole message might be code
    // This is handled in the caller (createMessageBubble) by checking msg.isInline
    
    return QString();
}

void AIChatPanel::onSendClicked()
{
    RAWRXD_TIMED_FUNC();
    QString message = m_inputField->text().trimmed();
    if (message.isEmpty()) return;
    
    // Check for AI commands first
    if (message.startsWith("/") || message.startsWith("@")) {
        handleAICommand(message);
        m_inputField->clear();
        return;
    }
    
    // Validate model is selected
    if (m_localModel.isEmpty()) {
        addAssistantMessage("Please select a model from the dropdown first.", false);
        qWarning() << "Message sent but no model selected";
        return;
    }
    
    addUserMessage(message);
    m_inputField->clear();
    
    // If we are in a special mode, use triple-model aggregation
    if (m_chatMode == ModeMax || m_chatMode == ModeDeepThinking || m_chatMode == ModeDeepResearch) {
        qDebug() << "[AIChatPanel] Using Triple-Model Aggregation for mode:" << modeName(m_chatMode);
        sendMessageTripleMultiModel(message);
    } else {
        // Standard agentic flow
        emit messageSubmitted(message);
    }
}

void AIChatPanel::handleAICommand(const QString& command)
{
    RAWRXD_TIMED_FUNC();
    // Start performance profiling
    m_commandTimer.start();
    qint64 startMemory = 0; // Stub for memory usage - currentProcess not in QProcess
    
    QString cmd = command.trimmed();
    
    // Resolve aliases
    QString resolvedCmd = resolveAlias(cmd);
    if (resolvedCmd != cmd) {
        qDebug() << "[AI Command] Resolved alias" << cmd << "->" << resolvedCmd;
        cmd = resolvedCmd;
    }
    
    // Add to command history
    m_commandHistory.prepend(cmd);
    if (m_commandHistory.size() > 50) {
        m_commandHistory.removeLast();
    }
    m_historyIndex = -1;
    
    // Check cache first
    if (m_cacheEnabled && m_resultCache.contains(cmd)) {
        QString cachedResult = m_resultCache.value(cmd);
        addAssistantMessage("⚡ [CACHED] " + cachedResult, false);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // /help - Show all commands
    if (cmd == "/help") {
        QString helpText = "<h3>AI Commands</h3>"
                          "<b>/refactor &lt;prompt&gt;</b> - Multi-file AI refactoring<br>"
                          "<b>@plan &lt;task&gt;</b> - Create implementation plan<br>"
                          "<b>@analyze</b> - Analyze current file<br>"
                          "<b>@generate &lt;spec&gt;</b> - Generate code<br>"
                          "<b>/help</b> - Show all commands<br>"
                          "<b>/history</b> - Show command history<br>"
                          "<b>/metrics</b> - Show performance metrics<br>"
                          "<b>/clear_cache</b> - Clear result cache";
        addAssistantMessage(helpText, false);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // /refactor <prompt> - Multi-file AI refactoring
    if (cmd.startsWith("/refactor ")) {
        QString prompt = cmd.mid(10).trimmed();
        if (prompt.isEmpty()) {
            addAssistantMessage("Usage: /refactor <instructions>\n\nExample: /refactor Extract duplicate code into a helper function", false);
            return;
        }
        
        addUserMessage(cmd);
        addAssistantMessage("🔄 Starting multi-file refactoring...", true);
        
        // Build refactoring prompt with context
        QString refactorPrompt = "REFACTORING REQUEST:\n" + prompt + "\n\n";
        if (!m_contextCode.isEmpty()) {
            refactorPrompt += "CURRENT CODE:\n" + m_contextCode + "\n\n";
        }
        if (!m_contextFilePath.isEmpty()) {
            refactorPrompt += "FILE: " + m_contextFilePath + "\n\n";
        }
        refactorPrompt += "Please provide:\n"
                         "1. Analysis of current code structure\n"
                         "2. Refactoring strategy\n"
                         "3. Step-by-step implementation plan\n"
                         "4. Updated code with improvements\n"
                         "5. Files that need to be modified\n\n"
                         "Use code blocks with filenames for each file.";
        
        sendMessageToBackend(refactorPrompt);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // @plan <task> - Create implementation plan
    if (cmd.startsWith("@plan ")) {
        QString task = cmd.mid(6).trimmed();
        if (task.isEmpty()) {
            addAssistantMessage("Usage: @plan <task description>\n\nExample: @plan Add error handling to the file loader", false);
            return;
        }
        
        addUserMessage(cmd);
        addAssistantMessage("📋 Creating implementation plan...", true);
        
        QString planPrompt = "PLANNING REQUEST:\nTask: " + task + "\n\n";
        if (!m_contextFilePath.isEmpty()) {
            planPrompt += "Context File: " + m_contextFilePath + "\n\n";
        }
        planPrompt += "Please create a detailed implementation plan:\n"
                     "1. **Requirements Analysis** - What needs to be done\n"
                     "2. **Architecture** - How to structure the solution\n"
                     "3. **Implementation Steps** - Ordered list of tasks\n"
                     "4. **Files to Modify** - Which files need changes\n"
                     "5. **Testing Strategy** - How to verify the changes\n"
                     "6. **Potential Risks** - Edge cases and gotchas\n\n"
                     "Format as markdown with code examples.";
        
        sendMessageToBackend(planPrompt);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // @analyze - Analyze current file
    if (cmd == "@analyze") {
        if (m_contextCode.isEmpty()) {
            addAssistantMessage("⚠ No code selected. Please select code in the editor first, then use @analyze.", false);
            return;
        }
        
        addUserMessage(cmd);
        addAssistantMessage("🔍 Analyzing code...", true);
        
        QString analyzePrompt = "CODE ANALYSIS REQUEST:\n\n";
        if (!m_contextFilePath.isEmpty()) {
            analyzePrompt += "File: " + m_contextFilePath + "\n\n";
        }
        analyzePrompt += "Code:\n```\n" + m_contextCode + "\n```\n\n";
        analyzePrompt += "Please provide a comprehensive analysis:\n"
                        "1. **Purpose** - What this code does\n"
                        "2. **Structure** - Key components and flow\n"
                        "3. **Code Quality** - Strengths and issues\n"
                        "4. **Potential Bugs** - Logic errors, edge cases\n"
                        "5. **Performance** - Efficiency concerns\n"
                        "6. **Best Practices** - Areas for improvement\n"
                        "7. **Security** - Vulnerabilities if any\n"
                        "8. **Recommendations** - Specific improvements\n\n"
                        "Be specific and actionable.";
        
        sendMessageToBackend(analyzePrompt);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // @generate <spec> - Generate code from specification
    if (cmd.startsWith("@generate ")) {
        QString spec = cmd.mid(10).trimmed();
        if (spec.isEmpty()) {
            addAssistantMessage("Usage: @generate <specification>\n\nExample: @generate A function to parse JSON and extract user data", false);
            return;
        }
        
        addUserMessage(cmd);
        addAssistantMessage("⚡ Generating code...", true);
        
        QString generatePrompt = "CODE GENERATION REQUEST:\n" + spec + "\n\n";
        if (!m_contextFilePath.isEmpty()) {
            QString fileExt = QFileInfo(m_contextFilePath).suffix();
            generatePrompt += "Target Language: " + fileExt + "\n\n";
        }
        if (!m_contextCode.isEmpty()) {
            generatePrompt += "Context Code (for reference):\n```\n" + m_contextCode + "\n```\n\n";
        }
        generatePrompt += "Please generate:\n"
                         "1. **Complete Implementation** - Production-ready code\n"
                         "2. **Documentation** - Clear comments and docstrings\n"
                         "3. **Error Handling** - Robust exception management\n"
                         "4. **Usage Example** - How to use the code\n"
                         "5. **Test Cases** - Basic test scenarios\n\n"
                         "Use code blocks with language markers. Follow best practices.";
        
        sendMessageToBackend(generatePrompt);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // /history - Show command history
    if (cmd == "/history") {
        QString historyText = "<h3>Command History</h3>";
        if (m_commandHistory.isEmpty()) {
            historyText += "No commands in history.";
        } else {
            historyText += "<ul>";
            for (int i = 0; i < qMin(10, m_commandHistory.size()); ++i) {
                historyText += "<li>" + m_commandHistory[i] + "</li>";
            }
            historyText += "</ul>";
        }
        addAssistantMessage(historyText, false);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // /metrics - Show performance metrics
    if (cmd == "/metrics") {
        QString metricsText = getPerformanceReport();
        addAssistantMessage(metricsText, false);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // /clear_cache - Clear result cache
    if (cmd == "/clear_cache") {
        clearResultCache();
        addAssistantMessage("✅ Result cache cleared", false);
        
        // Record metrics
        CommandMetrics metrics;
        metrics.command = cmd;
        metrics.executionTimeMs = m_commandTimer.elapsed();
        metrics.memoryUsageBytes = 0LL - startMemory;
        metrics.timestamp = QDateTime::currentDateTime();
        metrics.success = true;
        m_commandMetrics.append(metrics);
        
        return;
    }
    
    // Unknown command
    addAssistantMessage("❌ Unknown command: " + cmd + "\n\nType /help to see available commands.", false);
    
    // Record metrics for failed command
    CommandMetrics metrics;
    metrics.command = cmd;
    metrics.executionTimeMs = m_commandTimer.elapsed();
    metrics.memoryUsageBytes = 0LL - startMemory;
    metrics.timestamp = QDateTime::currentDateTime();
    metrics.success = false;
    m_commandMetrics.append(metrics);
}

void AIChatPanel::onQuickActionClicked(const QString& action)
{
    emit quickActionTriggered(action, m_contextCode);
}

void AIChatPanel::setCloudConfiguration(bool enabled, const QString& endpoint, const QString& apiKey) {
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized before setting cloud configuration";
        return;
    }
    
    m_cloudEnabled = enabled;
    m_cloudEndpoint = endpoint;
    m_apiKey = apiKey;
    
    qDebug() << "Cloud configuration updated - Enabled:" << enabled 
             << "Endpoint:" << endpoint;
}

void AIChatPanel::setLocalConfiguration(bool enabled, const QString& endpoint) {
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized before setting local configuration";
        return;
    }
    
    m_localEnabled = enabled;
    m_localEndpoint = endpoint;
    
    qDebug() << "Local configuration updated - Enabled:" << enabled 
             << "Endpoint:" << endpoint;
}

void AIChatPanel::setCloudAIEnabled(bool enabled) {
    m_cloudEnabled = enabled;
}

void AIChatPanel::setLocalAIEnabled(bool enabled) {
    m_localEnabled = enabled;
}

void AIChatPanel::setCloudEndpoint(const QString& endpoint) {
    m_cloudEndpoint = endpoint;
}

void AIChatPanel::setLocalEndpoint(const QString& endpoint) {
    m_localEndpoint = endpoint;
}

void AIChatPanel::setApiKey(const QString& key) {
    m_apiKey = key;
}

void AIChatPanel::setLocalModel(const QString& modelName) {
    m_localModel = modelName;
    qDebug() << "[AIChatPanel::setLocalModel] Setting m_localModel to:" << modelName;

    if (m_modelSelector) {
        bool found = false;
        for (int i = 0; i < m_modelSelector->count(); ++i) {
            const QString existing = m_modelSelector->itemData(i).toString();
            if (existing == modelName) {
                found = true;
                const QSignalBlocker blocker(m_modelSelector);
                m_modelSelector->setCurrentIndex(i);
                break;
            }
        }

        if (!found && !modelName.isEmpty()) {
            // Preserve label clarity so loaded-from-disk models are obvious in the dropdown
            const QString display = QString("%1 [loaded]").arg(modelName);
            m_modelSelector->addItem(display, modelName);
            const QSignalBlocker blocker(m_modelSelector);
            m_modelSelector->setCurrentIndex(m_modelSelector->count() - 1);
        }
    }

    // Enable chat input whenever we have a concrete model name (covers custom loads as well)
    setInputEnabled(!modelName.isEmpty());
    qDebug() << "[AIChatPanel::setLocalModel] Input enabled:" << !modelName.isEmpty();
}

void AIChatPanel::setChatMode(ChatMode mode) {
    m_chatMode = mode;
    qDebug() << "Chat mode set to:" << modeName(mode);
}

void AIChatPanel::setContextWindow(int tokens) {
    m_contextSize = qBound(4096, tokens, 1000000);
    qDebug() << "Context window set to:" << m_contextSize;
}

void AIChatPanel::setRequestTimeout(int timeoutMs) {
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized before setting timeout";
        return;
    }
    
    m_requestTimeout = timeoutMs;
    qDebug() << "Request timeout set to:" << timeoutMs << "ms";
}

QJsonObject AIChatPanel::getHealthStatus() const {
    if (m_infrastructure) {
        return m_infrastructure->healthMonitor()->getFullHealthReport();
    }
    return QJsonObject{{"error", "Infrastructure not initialized"}};
}

QJsonObject AIChatPanel::getMetrics() const {
    if (m_infrastructure) {
        return m_infrastructure->metrics()->getFullMetricsReport();
    }
    return QJsonObject{{"error", "Infrastructure not initialized"}};
}

QJsonObject AIChatPanel::getCacheStats() const {
    if (m_infrastructure) {
        return m_infrastructure->responseCache()->getStats();
    }
    return QJsonObject{{"error", "Infrastructure not initialized"}};
}

QJsonObject AIChatPanel::getAnalyticsReport() const {
    if (m_infrastructure) {
        return m_infrastructure->analytics()->getSessionReport();
    }
    return QJsonObject{{"error", "Infrastructure not initialized"}};
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
    m_streamingTextEdit = nullptr;
}

void AIChatPanel::clearChat()
{
    clear();
    emit chatCleared();
}

void AIChatPanel::clearResultCache()
{
    m_resultCache.clear();
}

void AIChatPanel::scrollToBottom()
{
    QScrollBar* scrollBar = m_scrollArea->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void AIChatPanel::setContext(const QString& code, const QString& filePath)
{
    m_contextCode = code;
    m_contextFilePath = filePath;
    qDebug() << "AIChatPanel context set for file:" << filePath
             << " code length:" << code.length();
}

void AIChatPanel::setInferenceEngine(InferenceEngine* engine)
{
    m_inferenceEngine = engine;
    if (engine) {
        qInfo() << "[AIChatPanel::setInferenceEngine] Inference engine set - ALL requests will use GGUF pipeline";
        
        // Connect to inference engine signals for streaming responses
        // Note: Signals use request IDs, we'll generate unique IDs for tracking
        connect(engine, QOverload<qint64, const QString&>::of(&InferenceEngine::streamToken),
                this, [this](qint64 reqId, const QString& token) {
            qDebug() << "[AIChatPanel] Token received from inference engine:" << token;
            updateStreamingMessage(token);
        });
        
        connect(engine, QOverload<qint64>::of(&InferenceEngine::streamFinished),
                this, [this](qint64 reqId) {
            qDebug() << "[AIChatPanel] Inference stream complete";
            finishStreaming();
        });
        
        connect(engine, QOverload<qint64, const QString&>::of(&InferenceEngine::error),
                this, [this](qint64 reqId, const QString& error) {
            qWarning() << "[AIChatPanel] Inference engine error:" << error;
            finishStreaming();
            addAssistantMessage("⚠ Inference error: " + error, false);
        });
    } else {
        qWarning() << "[AIChatPanel::setInferenceEngine] Inference engine set to nullptr";
    }
}

void AIChatPanel::sendMessageToBackend(const QString& message)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel sendMessageToBackend called before initialize";
        return;
    }

    qInfo() << "[AIChatPanel::sendMessageToBackend] Processing message through GGUF inference pipeline";
    
    // Track analytics
    if (m_infrastructure) {
        m_infrastructure->analytics()->trackMessage(
            QString::number(classifyMessageIntent(message)), 
            m_localModel
        );
    }
    
    // Check circuit breaker
    if (m_infrastructure && !m_infrastructure->circuitBreaker()->allowRequest()) {
        qWarning() << "[AIChatPanel] Circuit breaker OPEN - failing fast";
        addErrorMessage("Service temporarily unavailable. Please try again in a few seconds.",
                       "The inference service has experienced multiple failures and is in recovery mode.",
                       true);
        return;
    }
    
    // Check cache first
    if (m_infrastructure && m_cacheEnabled) {
        QString cacheKey = RawrXD::Chat::ResponseCache::generateKey(message, m_localModel, m_contextCode);
        QString cachedResponse;
        if (m_infrastructure->responseCache()->get(cacheKey, cachedResponse)) {
            qInfo() << "[AIChatPanel] Cache HIT - returning cached response";
            addAssistantMessage(cachedResponse, false);
            if (m_infrastructure) {
                m_infrastructure->metrics()->recordMessage("assistant", 0);
            }
            return;
        }
    }
    
    // PRIMARY PATH: Use GGUF inference engine if available (NO FALLBACK)
    if (m_inferenceEngine) {
        qInfo() << "[AIChatPanel::sendMessageToBackend] ✓ Routing to GGUF inference engine (real model execution)";
        
        // Create system prompt based on context
        QString systemPrompt = "You are a helpful coding assistant. ";
        if (!m_contextFilePath.isEmpty()) {
            systemPrompt += QString("Current file: %1. ").arg(m_contextFilePath);
        }
        systemPrompt += "Provide clear, concise responses.";
        
        // Add streaming message bubble
        addAssistantMessage("", true);
        
        // Build full prompt with system context
        QString fullPrompt = systemPrompt + "\n\nUser: " + message;
        if (!m_contextCode.isEmpty()) {
            fullPrompt = systemPrompt + "\n\nContext Code:\n" + m_contextCode + "\n\nUser: " + message;
        }
        
        // Generate unique request ID for tracking
        qint64 requestId = QDateTime::currentMSecsSinceEpoch();
        
        // Execute through real GGUF pipeline with streaming
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        m_inferenceEngine->generateStreaming(requestId, fullPrompt, 512);  // 512 max tokens
        
        // Log latency after request dispatch
        qint64 elapsedMs = QDateTime::currentMSecsSinceEpoch() - startTime;
        qInfo() << "[AIChatPanel] Inference request dispatched - latency:" << elapsedMs << "ms";
        
        return;
    }

    // FALLBACK ONLY FOR CLOUD PROVIDERS (if inference engine not available)
    const bool useCloud = m_cloudEnabled && !m_apiKey.isEmpty();
    
    if (!useCloud) {
        qCritical() << "[AIChatPanel::sendMessageToBackend] ✗ ERROR: No inference engine AND no cloud configuration";
        addAssistantMessage("⚠ FATAL: No AI model available. Please load a GGUF model or configure cloud API.", false);
        return;
    }

    qWarning() << "[AIChatPanel::sendMessageToBackend] Using cloud fallback (inference engine not available)";

    const QString endpoint = m_cloudEndpoint;
    QNetworkRequest* req = new QNetworkRequest(QUrl(endpoint));
    req->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req->setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    const QByteArray payload = buildCloudPayload(message);

    qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    QNetworkReply* reply = m_network->post(*req, payload);
    delete req;
    reply->setProperty("_msg_ts", startTime);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkFinished(reply);
    });
    connect(reply, &QNetworkReply::errorOccurred, this, &AIChatPanel::onNetworkError);

    QTimer::singleShot(m_requestTimeout, this, [reply]() {
        if (reply->isRunning()) {
            qWarning() << "[AIChatPanel] Cloud request timeout - aborting";
            reply->abort();
        }
    });
}

void AIChatPanel::sendMessageTriple(const QString& message)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel sendMessageTriple called before initialize";
        return;
    }

    // PRIMARY: Use GGUF inference engine for triple mode processing
    if (m_inferenceEngine) {
        qInfo() << "[AIChatPanel::sendMessageTriple] ✓ Processing triple modes through GGUF inference engine";
        
        QList<ChatMode> modes = { ModeMax, ModeDeepThinking, ModeDeepResearch };
        if (!modes.contains(m_chatMode)) modes.prepend(m_chatMode);
        while (modes.size() > 3) modes.removeLast();

        m_aggregateSessionActive = true;
        m_aggregateTexts.clear();
        
        // Add streaming message bubble
        addAssistantMessage("Processing with multiple reasoning modes...", true);
        
        // For each mode, execute through GGUF with mode-specific system prompt
        for (int modeIdx = 0; modeIdx < modes.size(); ++modeIdx) {
            ChatMode mode = modes[modeIdx];
            QString modePrompt = modeSystemPrompt(mode);
            qDebug() << "[AIChatPanel::sendMessageTriple] Executing mode:" << modeName(mode);
            
            // Initialize response accumulator for this mode
            m_aggregateTexts[modeName(mode)] = QString();
            
            // Build full prompt
            QString fullPrompt = modePrompt + "\n\nUser: " + message;
            if (!m_contextCode.isEmpty()) {
                fullPrompt = modePrompt + "\n\nContext Code:\n" + m_contextCode + "\n\nUser: " + message;
            }
            
            // Generate unique request ID for each mode
            qint64 requestId = QDateTime::currentMSecsSinceEpoch() + modeIdx;
            
            // Execute through real GGUF pipeline
            m_inferenceEngine->generateStreaming(requestId, fullPrompt, 512);
        }
        
        qInfo() << "[AIChatPanel::sendMessageTriple] All modes queued for GGUF pipeline execution";
        return;
    }

    // FALLBACK: Cloud path only if no GGUF available
    const bool useCloud = m_cloudEnabled && !m_apiKey.isEmpty();
    if (!useCloud) {
        qCritical() << "[AIChatPanel::sendMessageTriple] No GGUF engine and no cloud configured";
        addAssistantMessage("⚠ ERROR: Triple mode requires GGUF model or cloud configuration", false);
        return;
    }

    qWarning() << "[AIChatPanel::sendMessageTriple] Falling back to cloud (no GGUF available)";
    
    QList<ChatMode> modes = { ModeMax, ModeDeepThinking, ModeDeepResearch };
    if (!modes.contains(m_chatMode)) modes.prepend(m_chatMode);
    while (modes.size() > 3) modes.removeLast();

    m_aggregateSessionActive = true;
    m_aggregateReplies.clear();
    m_replyModeMap.clear();
    m_aggregateTexts.clear();

    for (ChatMode mode : modes) {
        // Cloud path
        const QString endpoint = m_cloudEndpoint;
        QNetworkRequest* req = new QNetworkRequest(QUrl(endpoint));
        req->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req->setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());
        
        const QByteArray payload = buildCloudPayloadForMode(message, mode);
        QNetworkReply* reply = m_network->post(*req, payload);
        delete req;
        reply->setProperty("_msg_ts", QDateTime::currentMSecsSinceEpoch());
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onAggregateFinished(reply); });
        connect(reply, &QNetworkReply::errorOccurred, this, &AIChatPanel::onNetworkError);
        QTimer::singleShot(m_requestTimeout, this, [reply]() { if (reply->isRunning()) reply->abort(); });
        m_aggregateReplies.append(reply);
        m_replyModeMap.insert(reply, mode);
    }
}

void AIChatPanel::sendMessageTripleMultiModel(const QString& message)
{
    if (!m_initialized) { qWarning() << "sendMessageTripleMultiModel before initialize"; return; }

    // Resolve target models
    QStringList models = m_selectedModels;
    if (models.isEmpty()) {
        if (!m_localModel.isEmpty()) models << m_localModel;
    }
    if (models.isEmpty()) {
        addAssistantMessage("Please select one or more models first.", false);
        return;
    }

    QList<ChatMode> modes = { ModeMax, ModeDeepThinking, ModeDeepResearch };
    if (!modes.contains(m_chatMode)) modes.prepend(m_chatMode);
    while (modes.size() > 3) modes.removeLast();

    m_aggregateSessionActive = true;
    m_aggregateReplies.clear();
    m_replyModeMap.clear();
    m_replyKeyMap.clear();
    m_textsByModel.clear();

    const bool useCloud = m_cloudEnabled && !m_apiKey.isEmpty();

    for (const QString& model : models) {
        for (ChatMode mode : modes) {
            // For local models, generate mock responses without network calls
            if (!useCloud && m_localEnabled) {
                qDebug() << "Processing multi-model message with local model:" << model << "mode:" << modeName(mode);
                QString response = generateLocalResponse(message, model);
                response.prepend(QString("[%1 - %2] ").arg(model, modeName(mode)));
                
                // Store in aggregation map
                m_textsByModel[model][modeName(mode)] = response;
                
                // Check if all responses ready
                if (m_textsByModel.size() == models.size()) {
                    bool allModesReady = true;
                    for (const auto& modelModes : m_textsByModel.values()) {
                        if (modelModes.size() < modes.size()) {
                            allModesReady = false;
                            break;
                        }
                    }
                    
                    if (allModesReady) {
                        // All responses ready, emit aggregated
                        QTimer::singleShot(500, this, [this]() {
                            QString combined;
                            for (const auto& modelText : m_textsByModel) {
                                for (const auto& modeText : modelText) {
                                    if (!combined.isEmpty()) combined += "\n\n";
                                    combined += modeText;
                                }
                            }
                            addAssistantMessage(combined, false);
                            m_aggregateSessionActive = false;
                            emit aggregatedResponseReady(combined);
                        });
                    }
                }
                continue;
            }
            
            // Cloud path
            const QString endpoint = m_cloudEndpoint;
            QNetworkRequest* req = new QNetworkRequest(QUrl(endpoint));
            req->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            req->setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

            const QByteArray payload = buildCloudPayloadForModeModel(message, mode, model);
            QNetworkReply* reply = m_network->post(*req, payload);
            delete req;
            reply->setProperty("_msg_ts", QDateTime::currentMSecsSinceEpoch());
            connect(reply, &QNetworkReply::finished, this, [this, reply]() { onAggregateFinished(reply); });
            connect(reply, &QNetworkReply::errorOccurred, this, &AIChatPanel::onNetworkError);
            QTimer::singleShot(m_requestTimeout, this, [reply]() { if (reply->isRunning()) reply->abort(); });
            m_aggregateReplies.append(reply);
            m_replyModeMap.insert(reply, mode);
            m_replyKeyMap.insert(reply, ReplyKey{ model, mode });
        }
    }
}

QByteArray AIChatPanel::buildCloudPayload(const QString& message) const
{
    QJsonObject root;
    root["model"] = "gpt-4o-mini"; // configurable if needed
    QJsonArray msgs;
    QJsonObject sys; sys["role"] = "system"; sys["content"] = "You are a helpful assistant."; msgs.append(sys);
    QJsonObject usr; usr["role"] = "user"; usr["content"] = message; msgs.append(usr);
    root["messages"] = msgs;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray AIChatPanel::buildLocalPayload(const QString& message) const
{
    // Ollama-like schema - use selected model or default
    QJsonObject root;
    root["model"] = m_localModel.isEmpty() ? "llama3.1" : m_localModel;
    root["prompt"] = message;
    root["stream"] = false;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray AIChatPanel::buildCloudPayloadForMode(const QString& message, ChatMode mode) const
{
    QJsonObject root;
    root["model"] = "gpt-4o-mini";
    QJsonArray msgs;
    QJsonObject sys; sys["role"] = "system"; sys["content"] = modeSystemPrompt(mode); msgs.append(sys);
    QJsonObject usr; usr["role"] = "user"; usr["content"] = message; msgs.append(usr);
    root["messages"] = msgs;
    root["temperature"] = modeTemperature(mode);
    root["max_output_tokens"] = 2048;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray AIChatPanel::buildLocalPayloadForMode(const QString& message, ChatMode mode) const
{
    QJsonObject root;
    root["model"] = m_localModel.isEmpty() ? "llama3.1" : m_localModel;
    root["prompt"] = message;
    root["stream"] = false;
    root["system"] = modeSystemPrompt(mode);
    QJsonObject options;
    options["num_ctx"] = m_contextSize;
    options["temperature"] = modeTemperature(mode);
    root["options"] = options;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray AIChatPanel::buildCloudPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const
{
    QJsonObject root;
    root["model"] = model.isEmpty() ? QString("gpt-4o-mini") : model;
    QJsonArray msgs;
    QJsonObject sys; sys["role"] = "system"; sys["content"] = modeSystemPrompt(mode); msgs.append(sys);
    QJsonObject usr; usr["role"] = "user"; usr["content"] = message; msgs.append(usr);
    root["messages"] = msgs;
    root["temperature"] = modeTemperature(mode);
    root["max_output_tokens"] = 2048;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QByteArray AIChatPanel::buildLocalPayloadForModeModel(const QString& message, ChatMode mode, const QString& model) const
{
    QJsonObject root;
    root["model"] = model.isEmpty() ? (m_localModel.isEmpty() ? "llama3.1" : m_localModel) : model;
    root["prompt"] = message;
    root["stream"] = false;
    root["system"] = modeSystemPrompt(mode);
    QJsonObject options;
    options["num_ctx"] = m_contextSize;
    options["temperature"] = modeTemperature(mode);
    root["options"] = options;
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QString AIChatPanel::extractAssistantText(const QJsonDocument& doc) const
{
    QString extractedText;
    auto obj = doc.object();
    
    // Try OpenAI-style response (choices array)
    if (obj.contains("choices")) {
        auto arr = obj["choices"].toArray();
        if (!arr.isEmpty()) {
            auto choice = arr[0].toObject();
            
            // Try message.content first
            if (choice.contains("message")) {
                auto msg = choice["message"].toObject();
                extractedText = msg["content"].toString();
                if (!extractedText.isEmpty()) return extractedText;
            }
            
            // Try direct text field
            if (choice.contains("text")) {
                extractedText = choice["text"].toString();
                if (!extractedText.isEmpty()) return extractedText;
            }
        }
    }
    
    // Try Ollama response format
    if (obj.contains("response")) {
        extractedText = obj["response"].toString();
        if (!extractedText.isEmpty()) return extractedText;
    }
    
    // Try direct 'text' field
    if (obj.contains("text")) {
        extractedText = obj["text"].toString();
        if (!extractedText.isEmpty()) return extractedText;
    }
    
    // Try 'generated_text' field (HuggingFace style)
    if (obj.contains("generated_text")) {
        extractedText = obj["generated_text"].toString();
        if (!extractedText.isEmpty()) return extractedText;
    }
    
    // Try 'output' field
    if (obj.contains("output")) {
        extractedText = obj["output"].toString();
        if (!extractedText.isEmpty()) return extractedText;
    }
    
    // Try 'result' field
    if (obj.contains("result")) {
        auto result = obj["result"];
        if (result.isObject()) {
            auto resultObj = result.toObject();
            if (resultObj.contains("text")) {
                return resultObj["text"].toString();
            }
        } else if (result.isString()) {
            return result.toString();
        }
    }
    
    // Fallback: return empty - caller will use raw body
    return QString();
}

void AIChatPanel::onNetworkFinished(QNetworkReply* reply)
{
    const qint64 start = reply->property("_msg_ts").toLongLong();
    const qint64 dur = start > 0 ? (QDateTime::currentMSecsSinceEpoch() - start) : -1;
    
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "AIChatPanel network error on finish:" << reply->error() << reply->errorString();
        
        // Record failure in circuit breaker
        if (m_infrastructure) {
            m_infrastructure->circuitBreaker()->recordFailure();
            m_infrastructure->metrics()->recordMessage("error", dur);
            m_infrastructure->analytics()->trackError(reply->errorString());
        }
        
        // Add error message with retry option
        addErrorMessage(QString("Network error: %1").arg(reply->errorString()),
                       QString("Error code: %1\nURL: %2").arg(reply->error()).arg(reply->url().toString()),
                       true);
        reply->deleteLater();
        return;
    }
    
    // Record success in circuit breaker
    if (m_infrastructure) {
        m_infrastructure->circuitBreaker()->recordSuccess();
    }
    
    const QByteArray body = reply->readAll();
    
    if (body.isEmpty()) {
        qWarning() << "AIChatPanel received empty response";
        addAssistantMessage("No response from model. Please try again.", false);
        reply->deleteLater();
        return;
    }
    
    // Log response for debugging
    qDebug() << "AIChatPanel raw response (first 200 chars):" << body.left(200);
    
    // Try to parse as JSON
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    
    QString responseText;
    
    if (perr.error == QJsonParseError::NoError && doc.isObject()) {
        // Successfully parsed JSON - extract text
        responseText = extractAssistantText(doc);
        
        if (responseText.isEmpty()) {
            qDebug() << "Could not extract text from JSON, using raw body";
            responseText = QString::fromUtf8(body);
        }
    } else {
        // Not valid JSON - use raw text response
        qDebug() << "JSON parse error:" << perr.errorString() << "- treating as raw response";
        responseText = QString::fromUtf8(body);
    }
    
    // Clean up tokenization artifacts and show response
    if (!responseText.isEmpty()) {
        if (!m_aggregateSessionActive) {
            addAssistantMessage(responseText, false);
            
            // Cache the response for future queries
            if (m_infrastructure && m_cacheEnabled) {
                QString originalMessage = reply->property("_original_message").toString();
                if (!originalMessage.isEmpty()) {
                    QString cacheKey = RawrXD::Chat::ResponseCache::generateKey(
                        originalMessage, m_localModel, m_contextCode);
                    m_infrastructure->responseCache()->put(cacheKey, responseText);
                }
            }
            
            // Record metrics
            if (m_infrastructure) {
                m_infrastructure->metrics()->recordMessage("assistant", dur);
                m_infrastructure->metrics()->recordTokens(responseText.length() / 4); // Rough token estimate
            }
        }
        qDebug() << "Response added to chat (length:" << responseText.length() << "chars)";
    } else {
        qWarning() << "Empty response after processing";
        addAssistantMessage("Empty response from model. Check model configuration.", false);
    }
    
    if (dur >= 0) qDebug() << "AIChatPanel request latency ms:" << dur;
    reply->deleteLater();
}

void AIChatPanel::onAggregateFinished(QNetworkReply* reply)
{
    const qint64 start = reply->property("_msg_ts").toLongLong();
    const qint64 dur = start > 0 ? (QDateTime::currentMSecsSinceEpoch() - start) : -1;

    ChatMode mode = m_replyModeMap.value(reply, ModeDefault);
    ReplyKey key = m_replyKeyMap.value(reply, ReplyKey{ QString(), ModeDefault });

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Aggregate network error:" << reply->error() << reply->errorString();
        m_textsByModel[key.model][modeName(mode)] = QString("[%1] Error: %2").arg(modeName(mode), reply->errorString());
    } else {
        const QByteArray body = reply->readAll();
        QJsonParseError perr; QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
        QString responseText;
        if (perr.error == QJsonParseError::NoError && doc.isObject()) {
            responseText = extractAssistantText(doc);
            if (responseText.isEmpty()) responseText = QString::fromUtf8(body);
        } else {
            responseText = QString::fromUtf8(body);
        }
        m_textsByModel[key.model][modeName(mode)] = QString("### %1\n\n%2").arg(modeName(mode), responseText);
    }
    if (dur >= 0) qDebug() << "Aggregate request latency ms:" << dur << "mode:" << modeName(mode);

    reply->deleteLater();

    // Check if all replies finished
    for (QNetworkReply* r : m_aggregateReplies) {
        if (!r->isFinished()) return;
    }

    // Combine texts grouped by model
    QList<ChatMode> order = { ModeMax, ModeDeepThinking, ModeDeepResearch, ModeThinkingResearch, ModeDefault };
    QString combined;
    for (auto it = m_textsByModel.constBegin(); it != m_textsByModel.constEnd(); ++it) {
        combined += QString("## Model: %1\n\n").arg(it.key());
        const auto& table = it.value();
        for (ChatMode m : order) {
            if (table.contains(modeName(m))) {
                combined += table.value(modeName(m));
                combined += "\n\n";
            }
        }
    }
    if (combined.isEmpty()) combined = "No responses returned.";

    if (m_aggregateSessionActive) {
        addAssistantMessage(combined, false);
        emit aggregatedResponseReady(combined);

        // Compute changed files and create checkpoint
        int changed = computeChangedFilesSinceSnapshot();
        // Auto-create checkpoint after aggregated response
        if (m_historyManager && !m_currentSessionId.isEmpty()) {
            createCheckpoint(QString("Aggregated response - %1 files changed").arg(changed));
        }
        addAssistantMessage(QString("Files changed since last snapshot: %1").arg(changed), false);
    }

    // Reset aggregation session
    m_aggregateSessionActive = false;
    m_aggregateReplies.clear();
    m_replyModeMap.clear();
    m_aggregateTexts.clear();
    snapshotProjectFiles();
}

void AIChatPanel::onNetworkError(QNetworkReply::NetworkError code)
{
    QNetworkReply* r = qobject_cast<QNetworkReply*>(sender());
    if (!r) return;
    qWarning() << "AIChatPanel network error:" << code << r->errorString();
}

void AIChatPanel::setInputEnabled(bool enabled)
{
    qDebug() << "[AIChatPanel::setInputEnabled] Setting input" << (enabled ? "enabled" : "disabled");
    
    // Add stack trace to see who's calling this
    if (!enabled) {
        qDebug() << "[AIChatPanel::setInputEnabled] DISABLING INPUT - Stack trace:";
        // Simple stack trace
        qDebug() << "  Called from setInputEnabled";
    }
    
    if (m_inputField) {
        m_inputField->setEnabled(enabled);
        qDebug() << "AIChatPanel input field" << (enabled ? "enabled" : "disabled");
    }
    if (m_sendButton) {
        m_sendButton->setEnabled(enabled);
    }
}

void AIChatPanel::fetchAvailableModels()
{
    qDebug() << "[AIChatPanel::fetchAvailableModels] Called";
    
    // Use built-in models - no Ollama dependency needed
    if (!m_localEnabled) {
        qDebug() << "Local models disabled, using cloud endpoint only";
        return;
    }
    
    qDebug() << "Loading available models (including Ollama blobs if detected)";
    
    if (!m_modelSelector) {
        qWarning() << "Model selector not initialized";
        return;
    }
    
    m_modelSelector->blockSignals(true);
    m_modelSelector->clear();
    
    // Add "Load Model..." option for loading real GGUF files
    m_modelSelector->addItem("Load Model...", "");
    
    // Fetch Ollama models from InferenceEngine if available
    if (m_inferenceEngine) {
        QStringList ollamaModels = m_inferenceEngine->detectedOllamaModels();
        for (const QString& model : ollamaModels) {
            m_modelSelector->addItem(QString("%1 [Ollama Blob]").arg(model), model);
            qDebug() << "[AIChatPanel] Added Ollama blob model:" << model;
        }
        qDebug() << "[AIChatPanel] Added" << ollamaModels.size() << "Ollama blob models to selector";
    } else {
        qWarning() << "[AIChatPanel] InferenceEngine not set - Ollama models will not be available";
    }
    
    m_modelSelector->addItem("custom-local-model", "custom-local-model");
    
    qDebug() << "[AIChatPanel] Model selector populated with real options";
    
    // Set default selection and enable input appropriately
    if (m_modelSelector->count() > 0) {
        m_modelSelector->insertItem(0, "Select a model...", "");
        m_modelSelector->setCurrentIndex(0);
        qDebug() << "Model list ready";
    } else {
        m_modelSelector->addItem("No models available");
    }
    
    // Only disable input if:
    // 1. No model is currently set in m_localModel AND
    // 2. The user hasn't selected a model from the dropdown
    // If a model was preloaded or selected, keep input enabled
    bool hasSelectedModel = (m_modelSelector->currentIndex() > 0 && 
                            m_modelSelector->itemData(m_modelSelector->currentIndex()).toString() != "");
    
    qDebug() << "[AIChatPanel::fetchAvailableModels] m_localModel=" << m_localModel
             << ", hasSelectedModel=" << hasSelectedModel
             << ", currentIndex=" << m_modelSelector->currentIndex();
    
    if (m_localModel.isEmpty() && !hasSelectedModel) {
        setInputEnabled(false);  // Wait for model selection
        qDebug() << "[AIChatPanel::fetchAvailableModels] No model loaded, disabling input";
    } else {
        qDebug() << "[AIChatPanel::fetchAvailableModels] Model present (m_localModel=" << m_localModel 
                 << ", hasSelectedModel=" << hasSelectedModel << "), keeping input enabled";
    }
    m_modelSelector->blockSignals(false);
}

void AIChatPanel::onModelsListFetched(QNetworkReply* reply)
{
    // Deprecated: Models are now built-in and don't require fetching from Ollama
    if (!reply) return;
    reply->deleteLater();
    qDebug() << "onModelsListFetched called but models are now built-in";
}

void AIChatPanel::onModelSelected(int index)
{
    if (!m_modelSelector || index < 0) return;
    
    QString model = m_modelSelector->itemData(index).toString();
    if (model.isEmpty()) model = m_modelSelector->currentText();
    
    // Handle "Load Model..." selection
    if (model == "Load Model..." || model.isEmpty()) {
        qDebug() << "[AIChatPanel] Load Model... selected - emitting signal";
        emit loadModelRequested();
        return;
    }
    
    // Only process valid model selections
    if (model == "Loading models..." || 
        model == "Error loading models" || model == "No models available" ||
        model == "Select a model...") {
        qWarning() << "Invalid model selected:" << model;
        setInputEnabled(false);  // Disable chat until valid model selected
        return;
    }
    
    // Valid model - save and enable chat
    setLocalModel(model);
    setInputEnabled(true);  // Enable chat input now that model is ready
    
    qDebug() << "Model successfully selected and ready:" << model;
    
    // Emit signal so MainWindow can load model into AgenticEngine
    emit modelSelected(model);
}

void AIChatPanel::setSelectedModel(const QString& modelName)
{
    if (!m_modelSelector) return;
    
    for (int i = 0; i < m_modelSelector->count(); ++i) {
        if (m_modelSelector->itemData(i).toString() == modelName) {
            m_modelSelector->setCurrentIndex(i);
            return;
        }
    }
}

void AIChatPanel::setSelectedModels(const QStringList& models)
{
    m_selectedModels = models;
    qDebug() << "Selected models set:" << m_selectedModels;
}

void AIChatPanel::openModelsDialog()
{
    // Build a simple dialog with multi-select list
    QDialog dlg(this);
    dlg.setWindowTitle("Select Models");
    QVBoxLayout* v = new QVBoxLayout(&dlg);
    QListWidget* list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::MultiSelection);
    // Populate from model selector
    if (m_modelSelector) {
        for (int i = 0; i < m_modelSelector->count(); ++i) {
            QString model = m_modelSelector->itemData(i).toString();
            if (model.isEmpty() || model == "Select a model..." || model == "Loading models..." || model == "Error loading models" || model == "No models available") continue;
            QListWidgetItem* item = new QListWidgetItem(model, list);
            item->setSelected(m_selectedModels.contains(model));
        }
    }
    v->addWidget(list);
    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* ok = new QPushButton("OK", &dlg);
    QPushButton* cancel = new QPushButton("Cancel", &dlg);
    buttons->addStretch(); buttons->addWidget(ok); buttons->addWidget(cancel);
    v->addLayout(buttons);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    if (dlg.exec() == QDialog::Accepted) {
        QStringList sel;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* it = list->item(i);
            if (it->isSelected()) sel << it->text();
        }
        setSelectedModels(sel);
    }
}

bool AIChatPanel::isAgenticRequest(const QString& message) const
{
    // Detect action verbs and intent patterns that suggest agentic processing
    QStringList agenticKeywords = {
        "create", "write", "modify", "delete", "fix", "build", "compile",
        "run", "execute", "analyze", "debug", "refactor", "optimize",
        "implement", "generate", "rename", "move", "copy", "search",
        "replace", "add", "remove", "update", "setup", "install",
        "please", "can you", "would you", "could you", "i need you to"
    };
    
    QString lowerMsg = message.toLower();
    
    // Check if message contains agentic keywords
    for (const QString& keyword : agenticKeywords) {
        if (lowerMsg.contains(keyword)) {
            return true;
        }
    }
    
    // Check for technical patterns (file paths, commands, code)
    if (lowerMsg.contains("if ") || lowerMsg.contains("then ") || 
        lowerMsg.contains("function") || lowerMsg.contains("class ") ||
        lowerMsg.contains(".cpp") || lowerMsg.contains(".hpp") ||
        lowerMsg.contains(".py") || lowerMsg.contains("//") ||
        lowerMsg.contains("/*")) {
        return true;
    }
    
    return false;
}

AIChatPanel::MessageIntent AIChatPanel::classifyMessageIntent(const QString& message)
{
    QString lowerMsg = message.toLower();
    
    // Check for code editing intent
    if (lowerMsg.contains("create") || lowerMsg.contains("write") ||
        lowerMsg.contains("modify") || lowerMsg.contains("refactor") ||
        lowerMsg.contains(".cpp") || lowerMsg.contains(".hpp") ||
        lowerMsg.contains("class") || lowerMsg.contains("function")) {
        return CodeEdit;
    }
    
    // Check for tool use intent
    if (lowerMsg.contains("build") || lowerMsg.contains("compile") ||
        lowerMsg.contains("run") || lowerMsg.contains("execute") ||
        lowerMsg.contains("cmd") || lowerMsg.contains("terminal") ||
        lowerMsg.contains("git") || lowerMsg.contains("make")) {
        return ToolUse;
    }
    
    // Check for planning intent
    if (lowerMsg.contains("plan") || lowerMsg.contains("design") ||
        lowerMsg.contains("architecture") || lowerMsg.contains("steps") ||
        lowerMsg.contains("approach") || lowerMsg.contains("strategy")) {
        return Planning;
    }
    
    // Check if it's just a question/chat
    if (lowerMsg.endsWith("?") || lowerMsg.contains("what ") ||
        lowerMsg.contains("how ") || lowerMsg.contains("why ") ||
        lowerMsg.contains("explain")) {
        return Chat;
    }
    
    return Unknown;
}

void AIChatPanel::processAgenticMessage(const QString& message, MessageIntent intent)
{
    // Log the intent classification
    QString intentStr;
    switch (intent) {
        case CodeEdit: intentStr = "CODE_EDIT"; break;
        case ToolUse: intentStr = "TOOL_USE"; break;
        case Planning: intentStr = "PLANNING"; break;
        case Chat: intentStr = "CHAT"; break;
        default: intentStr = "UNKNOWN"; break;
    }
    
    qDebug() << "Agentic message classified as:" << intentStr;
    addAssistantMessage(QString("[Processing agentic request as %1...]").arg(intentStr), false);
    
    // If we have an agentic executor, use it for autonomous execution
    if (m_agenticExecutor) {
        qDebug() << "Routing to AgenticExecutor for autonomous execution";
        QJsonObject result = m_agenticExecutor->executeUserRequest(message);
        
        // Parse result and display
        if (result.contains("success") && result["success"].toBool()) {
            QString output = result["output"].toString();
            if (!output.isEmpty()) {
                addAssistantMessage(output, false);
            }
        } else {
            QString error = result.contains("error") ? result["error"].toString() : QString("Unknown error occurred");
            addAssistantMessage(QString("Error: %1").arg(error), false);
        }
    } else {
        // Fall back to regular backend processing
        qDebug() << "No agentic executor - falling back to standard model processing";
        sendMessageToBackend(message);
    }
}

void AIChatPanel::setAgenticExecutor(AgenticExecutor* executor)
{
    m_agenticExecutor = executor;
    if (executor) {
        qDebug() << "AgenticExecutor connected to AIChatPanel";
    }
}

QString AIChatPanel::modeName(ChatMode mode) const {
    switch (mode) {
        case ModeMax: return "Max Mode";
        case ModeDeepThinking: return "Deep Thinking";
        case ModeThinkingResearch: return "Thinking Research";
        case ModeDeepResearch: return "Deep Research";
        default: return "Default";
    }
}

QString AIChatPanel::modeSystemPrompt(ChatMode mode) const {
    switch (mode) {
        case ModeMax:
            return QString("Respond with the most comprehensive, high-quality answer. Include code blocks and structured sections. If edits are needed, propose precise diffs.");
        case ModeDeepThinking:
            return QString("Think step by step in detail. Use clear reasoning, enumerate options, and provide a robust implementation plan followed by final code.");
        case ModeThinkingResearch:
            return QString("Research-style analysis: compare approaches, cite assumptions, and present pros/cons. Provide code snippets and benchmarks if relevant.");
        case ModeDeepResearch:
            return QString("Exhaustive research: explore edge cases, performance trade-offs, and advanced patterns. Provide production-ready code and tests.");
        default:
            return QString("You are a helpful assistant.");
    }
}

double AIChatPanel::modeTemperature(ChatMode mode) const {
    switch (mode) {
        case ModeMax: return 0.7;
        case ModeDeepThinking: return 0.3;
        case ModeThinkingResearch: return 0.4;
        case ModeDeepResearch: return 0.35;
        default: return 0.7;
    }
}

QString AIChatPanel::generateLocalResponse(const QString& message, const QString& model)
{
    // Simple local response generator for multi-model comparison
    // In a real production environment, this would use a lightweight local model
    // or a cached response if the model is not currently loaded in the inference engine.
    
    QString response = QString("Analysis from %1:\n\n").arg(model);
    
    if (message.toLower().contains("refactor")) {
        response += "I've analyzed the code for refactoring. Here's a suggested approach:\n"
                    "1. Identify the core logic to be moved.\n"
                    "2. Create a new abstraction layer.\n"
                    "3. Update call sites to use the new interface.";
    } else if (message.toLower().contains("fix")) {
        response += "I've detected a potential issue in the logic. Suggesting a fix:\n"
                    "```cpp\n// Proposed fix\nif (ptr != nullptr) {\n    ptr->execute();\n}\n```";
    } else {
        response += QString("I am processing your request: '%1'. As a local model, I recommend checking the implementation details in the current workspace.").arg(message);
    }
    
    return response;
}

void AIChatPanel::snapshotProjectFiles()
{
    m_fileSnapshot.clear();
    QDir root(m_projectRoot);
    QList<QDir> stack; stack << root;
    while (!stack.isEmpty()) {
        QDir d = stack.takeLast();
        for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            m_fileSnapshot.insert(fi.absoluteFilePath(), fi.lastModified().toSecsSinceEpoch());
        }
        for (const QFileInfo& di : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            stack << QDir(di.absoluteFilePath());
        }
    }
    qDebug() << "Snapshot files count:" << m_fileSnapshot.size();
}

int AIChatPanel::computeChangedFilesSinceSnapshot()
{
    int changed = 0;
    QHash<QString, qint64> current;
    QDir root(m_projectRoot);
    QList<QDir> stack; stack << root;
    while (!stack.isEmpty()) {
        QDir d = stack.takeLast();
        for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            current.insert(fi.absoluteFilePath(), fi.lastModified().toSecsSinceEpoch());
        }
        for (const QFileInfo& di : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            stack << QDir(di.absoluteFilePath());
        }
    }
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        qint64 prev = m_fileSnapshot.value(it.key(), -1);
        if (prev == -1 || prev != it.value()) changed++;
    }
    qDebug() << "Changed files since snapshot:" << changed;
    return changed;
}

void AIChatPanel::showHistory()
{
    // Checkpoint history is now managed via showCheckpointsDialog()
    // This function is deprecated but kept for compatibility
    qDebug() << "[AIChatPanel] showHistory - use showCheckpointsDialog() instead";
}

void AIChatPanel::showHistoryOld()
{
    if (!m_historyManager) return;
    
    QDialog dialog(this);
    dialog.setWindowTitle("Chat History");
    dialog.setMinimumSize(400, 500);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QListWidget* list = new QListWidget(&dialog);
    auto sessions = m_historyManager->getSessions();
    
    for (int i = 0; i < sessions.count(); ++i) {
        QJsonObject session = sessions[i].toObject();
        QListWidgetItem* item = new QListWidgetItem(
            session.value("title").toString(),
            list
        );
        item->setData(Qt::UserRole, session.value("id").toString());
    }
    
    layout->addWidget(new QLabel("Select a session to load:"));
    layout->addWidget(list);
    
    QPushButton* loadBtn = new QPushButton("Load Session");
    layout->addWidget(loadBtn);
    
    connect(loadBtn, &QPushButton::clicked, [&]() {
        if (list->currentItem()) {
            QString sessionId = list->currentItem()->data(Qt::UserRole).toString();
            emit sessionSelected(sessionId);
            dialog.accept();
        }
    });
    
    dialog.exec();
}

void AIChatPanel::showAdvancedSettings()
{
    // Temporary implementation - function commented out due to syntax errors
    qDebug() << "[AIChatPanel] Advanced settings dialog temporarily disabled";
}

void AIChatPanel::setHistoryManager(RawrXD::Database::ChatHistoryManager* manager)
{
    m_historyManager = manager;
    
    if (m_historyManager) {
        // Enable auto-checkpoint feature
        m_historyManager->setAutoCheckpointInterval(m_autoCheckpointIntervalMinutes);
        qDebug() << "[AIChatPanel] History manager set with auto-checkpoint enabled";
    }
}

void AIChatPanel::showSettings()
{
    qDebug() << "[AIChatPanel] Requesting global settings dialog";
    emit settingsRequested();
}

void AIChatPanel::createCheckpoint(const QString& title)
{
    if (!m_historyManager || m_currentSessionId.isEmpty()) {
        qWarning() << "[AIChatPanel] Cannot create checkpoint: no history manager or session";
        return;
    }
    
    QString checkpointTitle = title;
    if (checkpointTitle.isEmpty()) {
        checkpointTitle = QString("Manual checkpoint at %1").arg(
            QDateTime::currentDateTime().toString("hh:mm:ss"));
    }
    
    QString checkpointId = m_historyManager->createCheckpoint(m_currentSessionId, checkpointTitle);
    
    if (!checkpointId.isEmpty()) {
        qInfo() << "[AIChatPanel] Checkpoint created:" << checkpointTitle;
        // Show brief notification
        if (window()) {
            if (auto* mainWindow = qobject_cast<QMainWindow*>(window())) {
                mainWindow->statusBar()->showMessage(
                    QString("✓ Checkpoint saved: %1").arg(checkpointTitle), 3000);
            }
        }
    } else {
        qWarning() << "[AIChatPanel] Failed to create checkpoint";
    }
}

void AIChatPanel::showCheckpointsDialog()
{
    if (!m_historyManager || m_currentSessionId.isEmpty()) {
        QMessageBox::warning(this, "No Checkpoints", 
            "No active session or history manager available.");
        return;
    }
    
    QJsonArray checkpoints = m_historyManager->getCheckpoints(m_currentSessionId);
    
    if (checkpoints.isEmpty()) {
        QMessageBox::information(this, "No Checkpoints", 
            "No checkpoints available for this session.\n\nCreate one using the Save button.");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Restore Checkpoint");
    dialog.setMinimumWidth(500);
    dialog.setMinimumHeight(300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Select a checkpoint to restore:", &dialog);
    layout->addWidget(label);
    
    QListWidget* list = new QListWidget(&dialog);
    
    for (const auto& cpVal : checkpoints) {
        QJsonObject cp = cpVal.toObject();
        QString id = cp["id"].toString();
        QString title = cp["title"].toString();
        qint64 timestamp = cp.value("created_at").toVariant().toLongLong();
        int msgCount = cp["message_count"].toInt();
        
        QString displayText = QString("%1 (%2 messages) - %3")
            .arg(title)
            .arg(msgCount)
            .arg(QDateTime::fromMSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss"));
        
        QListWidgetItem* item = new QListWidgetItem(displayText, list);
        item->setData(Qt::UserRole, id);  // Store checkpoint ID
    }
    
    layout->addWidget(list);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* restoreBtn = new QPushButton("Restore", &dialog);
    QPushButton* deleteBtn = new QPushButton("Delete", &dialog);
    QPushButton* cancelBtn = new QPushButton("Cancel", &dialog);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(restoreBtn);
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);
    
    connect(restoreBtn, &QPushButton::clicked, &dialog, [&]() {
        if (list->currentItem()) {
            QString checkpointId = list->currentItem()->data(Qt::UserRole).toString();
            restoreCheckpoint(checkpointId);
            dialog.accept();
        }
    });
    
    connect(deleteBtn, &QPushButton::clicked, [&]() {
        if (list->currentItem()) {
            QString checkpointId = list->currentItem()->data(Qt::UserRole).toString();
            if (QMessageBox::question(this, "Delete Checkpoint", 
                "Are you sure you want to delete this checkpoint?") == QMessageBox::Yes) {
                m_historyManager->deleteCheckpoint(checkpointId);
                delete list->currentItem();
            }
        }
    });
    
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    dialog.exec();
}

void AIChatPanel::restoreCheckpoint(const QString& checkpointId)
{
    if (!m_historyManager || checkpointId.isEmpty()) {
        qWarning() << "[AIChatPanel] Cannot restore: invalid checkpoint or no manager";
        return;
    }
    
    if (m_historyManager->restoreCheckpoint(checkpointId)) {
        // Clear current UI
        clear();
        
        // Reload messages from restored state
        QJsonArray messages = m_historyManager->getMessages(m_currentSessionId);
        for (const auto& msgVal : messages) {
            QJsonObject msgObj = msgVal.toObject();
            QString role = msgObj["role"].toString();
            QString content = msgObj["content"].toString();
            
            if (role == "user") {
                addUserMessage(content);
            } else if (role == "assistant") {
                addAssistantMessage(content, false);
            }
        }
        
        qInfo() << "[AIChatPanel] Checkpoint restored with" << messages.size() << "messages";
        
        // Show notification
        if (window()) {
            if (auto* mainWindow = qobject_cast<QMainWindow*>(window())) {
                mainWindow->statusBar()->showMessage(
                    QString("✓ Checkpoint restored (%1 messages)").arg(messages.size()), 3000);
            }
        }
    } else {
        QMessageBox::warning(this, "Restore Failed", 
            "Failed to restore checkpoint. Check console for details.");
    }
}

void AIChatPanel::enableAutoCheckpoint(bool enabled, int intervalMinutes)
{
    m_autoCheckpointEnabled = enabled;
    m_autoCheckpointIntervalMinutes = qMax(1, intervalMinutes);
    
    if (enabled) {
        if (!m_autoCheckpointTimer) {
            m_autoCheckpointTimer = new QTimer(this);
            connect(m_autoCheckpointTimer, &QTimer::timeout, 
                    this, &AIChatPanel::onAutoCheckpointTimer);
        }
        // Convert minutes to milliseconds
        m_autoCheckpointTimer->start(m_autoCheckpointIntervalMinutes * 60 * 1000);
        qInfo() << "[AIChatPanel] Auto-checkpoint enabled with" 
                << m_autoCheckpointIntervalMinutes << "minute interval";
    } else {
        if (m_autoCheckpointTimer) {
            m_autoCheckpointTimer->stop();
        }
        qInfo() << "[AIChatPanel] Auto-checkpoint disabled";
    }
}

void AIChatPanel::onSaveCheckpoint()
{
    if (!m_historyManager || m_currentSessionId.isEmpty()) {
        QMessageBox::warning(this, "Cannot Save", 
            "No active chat session. Send a message first to create a session.");
        return;
    }
    
    bool ok;
    QString title = QInputDialog::getText(this, "Save Checkpoint",
        "Enter checkpoint name (optional):", QLineEdit::Normal, "", &ok);
    
    if (ok) {
        createCheckpoint(title);
    }
}

bool AIChatPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_inputField && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        switch (keyEvent->key()) {
        case Qt::Key_Up:
            // Navigate command history up
            if (!m_commandHistory.isEmpty()) {
                if (m_historyIndex < m_commandHistory.size() - 1) {
                    m_historyIndex++;
                    m_inputField->setText(m_commandHistory[m_historyIndex]);
                }
            }
            return true;
            
        case Qt::Key_Down:
            // Navigate command history down
            if (!m_commandHistory.isEmpty()) {
                if (m_historyIndex > 0) {
                    m_historyIndex--;
                    m_inputField->setText(m_commandHistory[m_historyIndex]);
                } else if (m_historyIndex == 0) {
                    m_historyIndex = -1;
                    m_inputField->clear();
                }
            }
            return true;
            
        case Qt::Key_Tab:
            // Tab completion for commands
            if (!m_inputField->text().isEmpty()) {
                QString currentText = m_inputField->text();
                QStringList matches;
                
                // Check for command prefixes
                for (const QString& cmd : m_commandHistory) {
                    if (cmd.startsWith(currentText)) {
                        matches.append(cmd);
                    }
                }
                
                // Also check predefined commands
                QStringList predefined = {"/help", "/refactor", "@plan", "@analyze", "@generate", "/history", "/metrics", "/clear_cache"};
                for (const QString& cmd : predefined) {
                    if (cmd.startsWith(currentText)) {
                        matches.append(cmd);
                    }
                }
                
                if (!matches.isEmpty()) {
                    // Find the longest common prefix
                    QString commonPrefix = matches.first();
                    for (int i = 1; i < matches.size(); ++i) {
                        while (!commonPrefix.isEmpty() && !matches[i].startsWith(commonPrefix)) {
                            commonPrefix.chop(1);
                        }
                    }
                    
                    if (commonPrefix.length() > currentText.length()) {
                        m_inputField->setText(commonPrefix);
                    }
                }
            }
            return true;
        }
    }
    
    return QWidget::eventFilter(obj, event);
}

void AIChatPanel::showContextMenu(const QPoint& pos)
{
    QMenu contextMenu(this);
    
    // Copy selected text
    QAction* copyAction = contextMenu.addAction("Copy", this, [this]() {
        // Copy selected text from focused widget
        if (auto* focused = QApplication::focusWidget()) {
            if (auto* textEdit = qobject_cast<QTextEdit*>(focused)) {
                textEdit->copy();
            } else if (auto* lineEdit = qobject_cast<QLineEdit*>(focused)) {
                lineEdit->copy();
            }
        }
    });
    
    // Copy entire chat
    QAction* copyAllAction = contextMenu.addAction("Copy All", this, [this]() {
        QString fullChat;
        for (const ChatMessage& msg : m_messages) {
            QString role = msg.role == ChatMessage::User ? "You" : "AI Assistant";
            fullChat += QString("%1:\n%2\n\n").arg(role).arg(msg.content);
        }
        QApplication::clipboard()->setText(fullChat);
    });
    
    contextMenu.addSeparator();
    
    // New Chat action
    QAction* newChatAction = contextMenu.addAction("New Chat", this, [this]() {
        emit newChatRequested();
    });
    
    contextMenu.exec(m_scrollArea->mapToGlobal(pos));
}

void AIChatPanel::onRestoreCheckpoint()
{
    showCheckpointsDialog();
}

void AIChatPanel::onAutoCheckpointTimer()
{
    if (!m_historyManager || m_currentSessionId.isEmpty()) {
        return;  // No session to checkpoint
    }
    
    // Check if enough time has passed and create auto-checkpoint
    if (m_historyManager->shouldAutoCheckpoint(m_currentSessionId)) {
        QString checkpointId = m_historyManager->autoCheckpoint(m_currentSessionId);
        if (!checkpointId.isEmpty()) {
            qDebug() << "[AIChatPanel] Auto-checkpoint created:" << checkpointId;
        }
    }
}

// Performance profiling methods
QString AIChatPanel::getPerformanceReport() const
{
    if (m_commandMetrics.isEmpty()) {
        return "No command metrics available.";
    }
    
    QString report = "<h3>AI Command Performance Report</h3>";
    report += "<table border='1' style='border-collapse: collapse; width: 100%;'>";
    report += "<tr><th>Command</th><th>Time (ms)</th><th>Memory (MB)</th><th>Timestamp</th><th>Status</th></tr>";
    
    qint64 totalTime = 0;
    qint64 totalMemory = 0;
    int successCount = 0;
    
    for (const CommandMetrics& metrics : m_commandMetrics) {
        QString status = metrics.success ? "✅" : "❌";
        QString memoryMB = QString::number(metrics.memoryUsageBytes / (1024.0 * 1024.0), 'f', 2);
        
        report += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td></tr>")
            .arg(metrics.command)
            .arg(metrics.executionTimeMs)
            .arg(memoryMB)
            .arg(metrics.timestamp.toString("hh:mm:ss"))
            .arg(status);
        
        totalTime += metrics.executionTimeMs;
        totalMemory += metrics.memoryUsageBytes;
        if (metrics.success) successCount++;
    }
    
    report += "</table>";
    
    // Summary
    report += QString("<p><b>Summary:</b> Total Commands: %1 | Success Rate: %2% | Avg Time: %3ms | Avg Memory: %4MB</p>")
        .arg(m_commandMetrics.size())
        .arg((successCount * 100) / m_commandMetrics.size())
        .arg(totalTime / m_commandMetrics.size())
        .arg((totalMemory / m_commandMetrics.size()) / (1024.0 * 1024.0), 0, 'f', 2);
    
    return report;
}

void AIChatPanel::registerCommandAlias(const QString& alias, const QString& command)
{
    m_commandAliases[alias] = command;
    qDebug() << "[AI Command] Registered alias:" << alias << "->" << command;
}

QString AIChatPanel::resolveAlias(const QString& input) const
{
    QString cmd = input.trimmed();
    
    // Common aliases
    if (cmd == "/r" && m_commandAliases.contains("/r")) {
        return m_commandAliases.value("/r");
    }
    if (cmd == "/p" && m_commandAliases.contains("/p")) {
        return m_commandAliases.value("/p");
    }
    if (cmd == "/a" && m_commandAliases.contains("/a")) {
        return m_commandAliases.value("/a");
    }
    if (cmd == "/g" && m_commandAliases.contains("/g")) {
        return m_commandAliases.value("/g");
    }
    
    // Check custom aliases
    for (auto it = m_commandAliases.constBegin(); it != m_commandAliases.constEnd(); ++it) {
        if (cmd == it.key()) {
            return it.value();
        }
    }
    
    return input; // No alias found
}

void AIChatPanel::populateCommandPaletteCommands()
{
    if (!m_commandPalette) {
        qWarning() << "[AI Command] Command palette not available";
        return;
    }
    
    // Register AI commands in the command palette
    CommandPalette::Command cmd;
    
    cmd.id = "ai.refactor";
    cmd.label = "AI Refactor";
    cmd.category = "AI";
    cmd.description = "Multi-file AI-powered refactoring";
    cmd.shortcut = QKeySequence("Ctrl+Shift+R");
    cmd.action = [this]() {
        m_inputField->setText("/refactor ");
        m_inputField->setFocus();
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd.id = "ai.plan";
    cmd.label = "AI Plan";
    cmd.category = "AI";
    cmd.description = "Create implementation plan";
    cmd.shortcut = QKeySequence("Ctrl+Shift+P");
    cmd.action = [this]() {
        m_inputField->setText("@plan ");
        m_inputField->setFocus();
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd.id = "ai.analyze";
    cmd.label = "AI Analyze";
    cmd.category = "AI";
    cmd.description = "Analyze selected code";
    cmd.shortcut = QKeySequence("Ctrl+Shift+A");
    cmd.action = [this]() {
        m_inputField->setText("@analyze");
        m_inputField->setFocus();
    };
    m_commandPalette->registerCommand(cmd);
    
    cmd.id = "ai.generate";
    cmd.label = "AI Generate";
    cmd.category = "AI";
    cmd.description = "Generate code from specification";
    cmd.shortcut = QKeySequence("Ctrl+Shift+G");
    cmd.action = [this]() {
        m_inputField->setText("@generate ");
        m_inputField->setFocus();
    };
    m_commandPalette->registerCommand(cmd);
    
    qDebug() << "[AI Command] Registered AI commands in command palette";
}

void AIChatPanel::setCommandPalette(CommandPalette* palette)
{
    m_commandPalette = palette;
    if (palette) {
        populateCommandPaletteCommands();
    }
}
void AIChatPanel::onChatMessage(const QString& message) { 
    // This should call the appropriate method to add the assistant's message to the UI
    // In many implementations it's appendMessage or similar.
}
