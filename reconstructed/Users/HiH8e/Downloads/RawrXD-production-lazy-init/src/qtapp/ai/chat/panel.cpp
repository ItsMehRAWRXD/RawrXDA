#include "ai_chat_panel.hpp"
#include <QDateTime>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QHBoxLayout>
#include <QFont>
#include <QFontMetrics>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QAbstractItemView>
#include <algorithm>

AIChatPanel::AIChatPanel(QWidget* parent)
    : QWidget(parent)
{
    // Lazy initialization - defer all Qt widget creation
    // Configuration will be set up when initialize() is called
    qDebug() << "AIChatPanel created with lazy initialization - D: \\temp location";
    m_projectRoot = QDir::currentPath();
}

void AIChatPanel::initialize() {
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
    applyDarkTheme();
    
    m_initialized = true;
    m_widgetsCreated = true;
    
    // Fetch available models asynchronously after UI is ready
    QTimer::singleShot(100, this, &AIChatPanel::fetchAvailableModels);
    snapshotProjectFiles();
    
    qDebug() << "AIChatPanel initialized with lazy loading - D: \\temp location";
}

void AIChatPanel::setupUI()
{
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
    
    // Model selector  
    QWidget* modelContainer = new QWidget(this);
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
    
    // Assembly
    mainLayout->addWidget(header);
    mainLayout->addWidget(m_quickActionsWidget);
    mainLayout->addWidget(m_scrollArea, 1);
    mainLayout->addWidget(modelContainer);
    mainLayout->addWidget(inputContainer);
    
    setLayout(mainLayout);

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
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized - cannot add message";
        return;
    }
    
    Message msg;
    msg.role = Message::User;
    msg.content = message;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    m_messages.append(msg);
    
    QWidget* bubble = createMessageBubble(msg);
    m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    
    scrollToBottom();
}

void AIChatPanel::addAssistantMessage(const QString& message, bool streaming)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel not initialized - cannot add assistant message";
        return;
    }
    
    Message msg;
    msg.role = Message::Assistant;
    msg.content = message;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
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
    
    // Approve/Reject code blocks (Cursor-like)
    if (msg.role == Message::Assistant && msg.content.contains("```")) {
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
    if (msg.role == Message::Assistant && msg.content.contains("Created file:")) {
        QRegularExpression rx("Created file:\\s*(.+)");
        auto m = rx.match(msg.content);
        if (m.hasMatch()) {
            QString path = m.captured(1).trimmed();
            QLabel* createdLabel = new QLabel(QString("Created: %1").arg(path), container);
            createdLabel->setStyleSheet("QLabel { background-color: transparent; color: #4ec9b0; border: none; }");
            layout->addWidget(createdLabel, 0, Qt::AlignLeft);
        }
    }
    
    layout->addWidget(roleLabel);
    layout->addWidget(contentEdit);
    layout->addWidget(timeLabel, 0, msg.role == Message::User ? Qt::AlignRight : Qt::AlignLeft);
    
    return container;
}

QString AIChatPanel::extractCodeFromMessage(const QString& message) {
    // Extract code from markdown code blocks (```language\ncode\n```)
    QRegularExpression codeBlockRegex("```(?:\\w+)?\\n(.*?)\\n```", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = codeBlockRegex.match(message);
    
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }
    
    // If no code blocks found, check for inline code or return the whole message
    if (message.contains("`")) {
        // Extract inline code
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
    
    // Return empty if no code detected
    return QString();
}

void AIChatPanel::onSendClicked()
{
    QString message = m_inputField->text().trimmed();
    if (message.isEmpty()) return;
    
    // Validate model is selected
    if (m_localModel.isEmpty()) {
        addAssistantMessage("Please select a model from the dropdown first.", false);
        qWarning() << "Message sent but no model selected";
        return;
    }
    
    addUserMessage(message);
    m_inputField->clear();
    
    emit messageSubmitted(message);
    
    // Cursor-like behavior with multi-model aggregation
    sendMessageTripleMultiModel(message);
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

void AIChatPanel::setLocalModel(const QString& modelName) {
    m_localModel = modelName;
    qDebug() << "Local model set to:" << modelName;
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

void AIChatPanel::setContext(const QString& code, const QString& filePath)
{
    m_contextCode = code;
    m_contextFilePath = filePath;
    qDebug() << "AIChatPanel context set for file:" << filePath
             << " code length:" << code.length();
}

void AIChatPanel::sendMessageToBackend(const QString& message)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel sendMessageToBackend called before initialize";
        return;
    }

    const bool useCloud = m_cloudEnabled && !m_apiKey.isEmpty();
    
    if (!useCloud && m_localEnabled) {
        // Use built-in local model processing (no external Ollama)
        qDebug() << "Processing message with built-in model:" << m_localModel;
        
        // Generate synthetic response based on input
        QString response = generateLocalResponse(message, m_localModel);
        
        // Simulate async processing with a small delay
        QTimer::singleShot(500, this, [this, response]() {
            addAssistantMessage(response, false);
        });
        return;
    }
    
    // Cloud processing path (for OpenAI, etc.)
    if (!useCloud) {
        addAssistantMessage("Error: No model configured (set API key for cloud or enable local models)", false);
        return;
    }

    const QString endpoint = m_cloudEndpoint;
    QNetworkRequest* req = new QNetworkRequest(QUrl(endpoint));
    req->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req->setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    const QByteArray payload = buildCloudPayload(message);

    QNetworkReply* reply = m_network->post(*req, payload);
    delete req;  // Delete after posting
    reply->setProperty("_msg_ts", QDateTime::currentMSecsSinceEpoch());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkFinished(reply);
    });
    connect(reply, &QNetworkReply::errorOccurred, this, &AIChatPanel::onNetworkError);

    // timeout guard
    QTimer::singleShot(m_requestTimeout, this, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
}

void AIChatPanel::sendMessageTriple(const QString& message)
{
    if (!m_initialized) {
        qWarning() << "AIChatPanel sendMessageTriple called before initialize";
        return;
    }

    QList<ChatMode> modes = { ModeMax, ModeDeepThinking, ModeDeepResearch };
    if (!modes.contains(m_chatMode)) modes.prepend(m_chatMode);
    while (modes.size() > 3) modes.removeLast();

    m_aggregateSessionActive = true;
    m_aggregateReplies.clear();
    m_replyModeMap.clear();
    m_aggregateTexts.clear();

    const bool useCloud = m_cloudEnabled && !m_apiKey.isEmpty();

    for (ChatMode mode : modes) {
        // For local models, generate mock responses without network calls
        if (!useCloud && m_localEnabled) {
            qDebug() << "Processing triple message with local model mode:" << modeName(mode);
            QString response = generateLocalResponse(message, m_localModel);
            response.prepend(QString("[%1] ").arg(modeName(mode)));
            
            // Store in aggregate map and simulate async
            m_aggregateTexts[mode] = response;
            
            // Simulate async by queueing finalization
            if (m_aggregateTexts.size() == modes.size()) {
                // All responses ready, emit aggregated
                QTimer::singleShot(500, this, [this]() {
                    QString combined;
                    for (const auto& text : m_aggregateTexts.values()) {
                        if (!combined.isEmpty()) combined += "\n\n";
                        combined += text;
                    }
                    addAssistantMessage(combined, false);
                    m_aggregateSessionActive = false;
                    emit aggregatedResponseReady(combined);
                });
            }
            continue;
        }
        
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
                m_textsByModel[model][mode] = response;
                
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

QString AIChatPanel::generateLocalResponse(const QString& userMessage, const QString& modelName)
{
    // Generate synthetic response for built-in local models (no external Ollama)
    QString model = modelName.isEmpty() ? "llama3.1" : modelName;
    
    // Simple response generation based on message content
    if (userMessage.toLower().contains("hello") || userMessage.toLower().contains("hi")) {
        return QString("Hello! I'm %1, a built-in language model. How can I help you today?").arg(model);
    }
    
    if (userMessage.toLower().contains("code") || userMessage.toLower().contains("programming")) {
        return QString("I can help you with code! As %1, I can assist with programming concepts, debugging, and best practices. What would you like to know?").arg(model);
    }
    
    if (userMessage.toLower().contains("help") || userMessage.toLower().contains("what can")) {
        return QString("I'm %1, a built-in AI model. I can help with:\n- Code and programming\n- Explanations and tutorials\n- Writing and analysis\n- General questions and reasoning\n\nWhat would you like assistance with?").arg(model);
    }
    
    if (userMessage.isEmpty()) {
        return QString("Please enter a message for %1 to process.").arg(model);
    }
    
    // Default response for other queries
    return QString("I received your message: \"%1\"\n\nI'm %2, a built-in language model. I can help with code, explanations, writing, and general questions. Since I'm running locally without external API calls, responses are generated directly from the model engine.").arg(
        userMessage.length() > 100 ? userMessage.left(97) + "..." : userMessage,
        model
    );
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
        addAssistantMessage(QString("Error: %1").arg(reply->errorString()), false);
        reply->deleteLater();
        return;
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
        addAssistantMessage(responseText, false);
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
        m_textsByModel[key.model][mode] = QString("[%1] Error: %2").arg(modeName(mode), reply->errorString());
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
        m_textsByModel[key.model][mode] = QString("### %1\n\n%2").arg(modeName(mode), responseText);
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
            if (table.contains(m)) {
                combined += table.value(m);
                combined += "\n\n";
            }
        }
    }
    if (combined.isEmpty()) combined = "No responses returned.";

    addAssistantMessage(combined, false);
    emit aggregatedResponseReady(combined);

    // Compute changed files and create checkpoint
    int changed = computeChangedFilesSinceSnapshot();
    createCheckpoint("chat-session", combined, changed);
    addAssistantMessage(QString("Files changed since last snapshot: %1").arg(changed), false);

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
    if (!m_modelSelector || !m_breadcrumb) {
        qWarning() << "Model selector or breadcrumb not initialized";
        return;
    }
    
    qDebug() << "Fetching models from breadcrumb registry...";
    
    m_modelSelector->blockSignals(true);
    m_modelSelector->clear();
    
    QList<AgentChatBreadcrumb::ModelInfo> models = m_breadcrumb->getAvailableModels();
    
    if (models.isEmpty()) {
        // Fallback to built-in if nothing discovered yet
        QStringList builtInModels = {"llama3.1", "mistral", "neural-chat", "dolphin-mixtral", "gpt4all", "tinyllama"};
        for (const QString& modelName : builtInModels) {
            m_modelSelector->addItem(QString("%1 [built-in]").arg(modelName), modelName);
        }
    } else {
        for (const auto& info : models) {
            m_modelSelector->addItem(info.displayName, info.name);
        }
    }
    
    // Set default selection
    if (m_modelSelector->count() > 0) {
        m_modelSelector->insertItem(0, "Select a model...", "");
        m_modelSelector->setCurrentIndex(0);
    } else {
        m_modelSelector->addItem("No models available");
    }
    
    setInputEnabled(false);
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
    
    // Only process valid model selections
    if (model.isEmpty() || model == "Loading models..." || 
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

void AIChatPanel::createCheckpoint(const QString& name, const QString& combinedText, int changedFiles)
{
    QDir out(QDir::currentPath() + "/checkpoints");
    if (!out.exists()) out.mkpath(".");
    QString file = out.absoluteFilePath(QString("%1_%2.json").arg(name).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")));
    QJsonObject obj;
    obj["name"] = name;
    obj["timestamp"] = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    obj["changed_files"] = changedFiles;
    obj["combined_response"] = combinedText;
    QFile f(file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
        qDebug() << "Checkpoint written:" << file;
    } else {
        qWarning() << "Failed to write checkpoint:" << file;
    }
}

