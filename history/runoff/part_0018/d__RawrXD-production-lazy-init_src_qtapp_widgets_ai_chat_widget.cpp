/**
 * @file ai_chat_widget.cpp
 * @brief Implementation of AIChatWidget - AI-powered chat interface
 */

#include "ai_chat_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QComboBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QTimer>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QDebug>

// ============================================================================
// AIChatWorker Implementation
// ============================================================================

AIChatWorker::AIChatWorker(QObject* parent)
    : QObject(parent)
{
}

void AIChatWorker::sendMessage(const QString& message, const QList<ChatMessage>& context, const QJsonObject& options)
{
    try {
        QJsonObject response = callAIAPI(message, context, options);
        QString content = response["content"].toString();
        QJsonObject metadata = response["metadata"].toObject();

        emit responseReceived(content, metadata);
    } catch (const std::exception& e) {
        emit error(QString("AI API call failed: %1").arg(e.what()));
    }
}

QJsonObject AIChatWorker::callAIAPI(const QString& prompt, const QList<ChatMessage>& context, const QJsonObject& options)
{
    // Simulate AI API call
    // In production, this would call actual AI services like OpenAI, Anthropic, etc.

    QThread::msleep(1000 + (qrand() % 2000)); // Simulate network delay

    // Mock response based on input
    QString response;
    if (prompt.toLower().contains("hello") || prompt.toLower().contains("hi")) {
        response = "Hello! How can I help you today?";
    } else if (prompt.toLower().contains("code") || prompt.toLower().contains("programming")) {
        response = "I'd be happy to help you with coding! What programming language or framework are you working with?";
    } else if (prompt.toLower().contains("weather")) {
        response = "I'm sorry, but I don't have access to real-time weather data. However, I can help you find weather APIs or suggest ways to implement weather functionality in your application.";
    } else if (prompt.toLower().contains("debug") || prompt.toLower().contains("error")) {
        response = "Debugging can be tricky! Could you share the error message or describe the issue you're experiencing? I'll do my best to help you troubleshoot it.";
    } else {
        response = QString("I understand you're asking about: \"%1\". This is an interesting topic. Could you provide more details so I can give you a more specific and helpful response?").arg(prompt.left(50));
    }

    QJsonObject result;
    result["content"] = response;

    QJsonObject metadata;
    metadata["model"] = model_;
    metadata["tokens_used"] = 150 + (qrand() % 100);
    metadata["finish_reason"] = "stop";
    metadata["response_time"] = 1.5 + (qrand() % 100) / 100.0;

    result["metadata"] = metadata;

    return result;
}

// ============================================================================
// AIChatWidget Implementation
// ============================================================================

AIChatWidget::AIChatWidget(QWidget* parent)
    : QWidget(parent)
    , mainLayout_(nullptr)
    , toolbarWidget_(nullptr)
    , mainSplitter_(nullptr)
    , newChatBtn_(nullptr)
    , loadChatBtn_(nullptr)
    , saveChatBtn_(nullptr)
    , deleteChatBtn_(nullptr)
    , exportChatBtn_(nullptr)
    , clearChatBtn_(nullptr)
    , modelCombo_(nullptr)
    , chatSplitter_(nullptr)
    , sessionList_(nullptr)
    , chatScrollArea_(nullptr)
    , chatContainer_(nullptr)
    , chatLayout_(nullptr)
    , inputWidget_(nullptr)
    , messageInput_(nullptr)
    , sendBtn_(nullptr)
    , stopBtn_(nullptr)
    , statusLabel_(nullptr)
    , settingsGroup_(nullptr)
    , modelSelector_(nullptr)
    , temperatureInput_(nullptr)
    , maxTokensInput_(nullptr)
    , streamResponsesCheck_(nullptr)
    , progressBar_(nullptr)
    , chatWorker_(nullptr)
    , workerThread_(nullptr)
    , isGenerating_(false)
    , typingTimer_(nullptr)
{
    RAWRXD_INIT_TIMED("AIChatWidget");
    setupUI();
    setupConnections();

    restoreState();

    // Create initial session
    createNewSession("New Chat");

    qDebug() << "AIChatWidget initialized";
}

AIChatWidget::~AIChatWidget()
{
    saveState();

    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
    }
}

void AIChatWidget::setupUI()
{
    RAWRXD_TIMED_FUNC();
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);

    setupToolbar();
    setupChatArea();
    setupSidebar();

    // Set initial splitter sizes
    mainSplitter_->setSizes({200, 600});
    chatSplitter_->setSizes({150, 450});
}

void AIChatWidget::setupToolbar()
{
    toolbarWidget_ = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget_);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);

    newChatBtn_ = new QPushButton(tr("New Chat"), toolbarWidget_);
    loadChatBtn_ = new QPushButton(tr("Load Chat"), toolbarWidget_);
    saveChatBtn_ = new QPushButton(tr("Save Chat"), toolbarWidget_);
    deleteChatBtn_ = new QPushButton(tr("Delete Chat"), toolbarWidget_);
    exportChatBtn_ = new QPushButton(tr("Export"), toolbarWidget_);
    clearChatBtn_ = new QPushButton(tr("Clear"), toolbarWidget_);

    toolbarLayout->addWidget(newChatBtn_);
    toolbarLayout->addWidget(loadChatBtn_);
    toolbarLayout->addWidget(saveChatBtn_);
    toolbarLayout->addWidget(deleteChatBtn_);
    toolbarLayout->addWidget(exportChatBtn_);
    toolbarLayout->addWidget(clearChatBtn_);

    toolbarLayout->addStretch();

    toolbarLayout->addWidget(new QLabel(tr("Model:"), toolbarWidget_));
    modelCombo_ = new QComboBox(toolbarWidget_);
    modelCombo_->addItems({"GPT-4", "GPT-3.5", "Claude-3", "Claude-2", "Gemini Pro", "Local Model"});
    modelCombo_->setCurrentText("GPT-4");
    toolbarLayout->addWidget(modelCombo_);

    mainLayout_->addWidget(toolbarWidget_);
}

void AIChatWidget::setupChatArea()
{
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainLayout_->addWidget(mainSplitter_);

    // Session list
    sessionList_ = new QListWidget(this);
    sessionList_->setMaximumWidth(200);
    sessionList_->setContextMenuPolicy(Qt::CustomContextMenu);
    mainSplitter_->addWidget(sessionList_);

    // Chat area
    chatSplitter_ = new QSplitter(Qt::Vertical, this);
    mainSplitter_->addWidget(chatSplitter_);

    // Chat display
    chatScrollArea_ = new QScrollArea(this);
    chatScrollArea_->setWidgetResizable(true);
    chatScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chatScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    chatContainer_ = new QWidget();
    chatLayout_ = new QVBoxLayout(chatContainer_);
    chatLayout_->setSpacing(10);
    chatLayout_->addStretch();

    chatScrollArea_->setWidget(chatContainer_);
    chatSplitter_->addWidget(chatScrollArea_);

    // Message input area
    inputWidget_ = new QWidget(this);
    QVBoxLayout* inputLayout = new QVBoxLayout(inputWidget_);

    QHBoxLayout* messageLayout = new QHBoxLayout();
    messageInput_ = new QLineEdit(inputWidget_);
    messageInput_->setPlaceholderText(tr("Type your message here..."));
    sendBtn_ = new QPushButton(tr("Send"), inputWidget_);
    stopBtn_ = new QPushButton(tr("Stop"), inputWidget_);
    stopBtn_->setVisible(false);

    messageLayout->addWidget(messageInput_);
    messageLayout->addWidget(sendBtn_);
    messageLayout->addWidget(stopBtn_);

    inputLayout->addLayout(messageLayout);

    statusLabel_ = new QLabel(tr("Ready"), inputWidget_);
    statusLabel_->setStyleSheet("color: gray; font-size: 11px;");
    inputLayout->addWidget(statusLabel_);

    chatSplitter_->addWidget(inputWidget_);

    // Set input area to fixed height
    inputWidget_->setMaximumHeight(80);
}

void AIChatWidget::setupSidebar()
{
    QWidget* sidebarWidget = new QWidget(this);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);

    // Settings group
    settingsGroup_ = new QGroupBox(tr("AI Settings"), sidebarWidget);
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsGroup_);

    settingsLayout->addWidget(new QLabel(tr("Model:"), settingsGroup_));
    modelSelector_ = new QComboBox(settingsGroup_);
    modelSelector_->addItems({"GPT-4", "GPT-3.5-Turbo", "Claude-3-Opus", "Claude-3-Sonnet", "Gemini Pro", "Local Llama-2-7B", "Local CodeLlama"});
    modelSelector_->setCurrentText("GPT-4");
    settingsLayout->addWidget(modelSelector_);

    settingsLayout->addWidget(new QLabel(tr("Temperature:"), settingsGroup_));
    temperatureInput_ = new QLineEdit("0.7", settingsGroup_);
    temperatureInput_->setValidator(new QDoubleValidator(0.0, 2.0, 2, this));
    settingsLayout->addWidget(temperatureInput_);

    settingsLayout->addWidget(new QLabel(tr("Max Tokens:"), settingsGroup_));
    maxTokensInput_ = new QLineEdit("2048", settingsGroup_);
    maxTokensInput_->setValidator(new QIntValidator(1, 32768, this));
    settingsLayout->addWidget(maxTokensInput_);

    streamResponsesCheck_ = new QCheckBox(tr("Stream Responses"), settingsGroup_);
    streamResponsesCheck_->setChecked(true);
    settingsLayout->addWidget(streamResponsesCheck_);

    QPushButton* applySettingsBtn = new QPushButton(tr("Apply Settings"), settingsGroup_);
    settingsLayout->addWidget(applySettingsBtn);

    sidebarLayout->addWidget(settingsGroup_);

    // Progress bar
    progressBar_ = new QProgressBar(sidebarWidget);
    progressBar_->setVisible(false);
    sidebarLayout->addWidget(progressBar_);

    sidebarLayout->addStretch();

    mainSplitter_->addWidget(sidebarWidget);

    connect(applySettingsBtn, &QPushButton::clicked, this, &AIChatWidget::onSettingsChanged);
}

void AIChatWidget::setupConnections()
{
    // Toolbar actions
    connect(newChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onNewChat);
    connect(loadChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onLoadChat);
    connect(saveChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onSaveChat);
    connect(deleteChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onDeleteChat);
    connect(exportChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onExportChat);
    connect(clearChatBtn_, &QPushButton::clicked, this, &AIChatWidget::onClearChat);
    connect(modelCombo_, QOverload<const QString&>::of(&QComboBox::currentTextChanged), this, &AIChatWidget::onModelChanged);

    // Chat actions
    connect(sendBtn_, &QPushButton::clicked, this, &AIChatWidget::onSendMessage);
    connect(stopBtn_, &QPushButton::clicked, this, &AIChatWidget::onStopGeneration);
    connect(messageInput_, &QLineEdit::returnPressed, this, &AIChatWidget::onSendMessage);
    connect(messageInput_, &QLineEdit::textChanged, this, &AIChatWidget::onMessageTextChanged);

    // Session list
    connect(sessionList_, &QListWidget::itemClicked, this, &AIChatWidget::onSessionSelected);
    connect(sessionList_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = sessionList_->itemAt(pos);
        if (item) {
            showSessionContextMenu(pos);
        }
    });

    // Chat worker
    if (!workerThread_) {
        workerThread_ = new QThread(this);
        chatWorker_ = new AIChatWorker();
        chatWorker_->moveToThread(workerThread_);
        workerThread_->start();
    }

    connect(chatWorker_, &AIChatWorker::responseReceived, this, &AIChatWidget::onResponseReceived);
    connect(chatWorker_, &AIChatWorker::error, this, &AIChatWidget::onError);

    // Typing timer for status updates
    typingTimer_ = new QTimer(this);
    typingTimer_->setSingleShot(true);
    connect(typingTimer_, &QTimer::timeout, this, [this]() {
        if (!isGenerating_) {
            statusLabel_->setText(tr("Ready"));
        }
    });
}

bool AIChatWidget::createNewSession(const QString& title)
{
    ChatSession session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.title = title.isEmpty() ? tr("New Chat") : title;
    session.created = QDateTime::currentDateTime();
    session.lastModified = session.created;
    session.model = modelCombo_->currentText();
    session.settings["temperature"] = 0.7;
    session.settings["max_tokens"] = 2048;

    sessions_.prepend(session); // Add to beginning
    currentSessionId_ = session.id;

    updateSessionList();
    updateChatDisplay();

    emit sessionCreated(session.id);
    return true;
}

bool AIChatWidget::loadSession(const QString& sessionId)
{
    for (const ChatSession& session : sessions_) {
        if (session.id == sessionId) {
            currentSessionId_ = sessionId;
            updateChatDisplay();
            emit sessionLoaded(sessionId);
            return true;
        }
    }
    return false;
}

bool AIChatWidget::saveSession(const QString& sessionId)
{
    QString saveId = sessionId.isEmpty() ? currentSessionId_ : sessionId;
    if (saveId.isEmpty()) return false;

    // Find and update the session
    for (ChatSession& session : sessions_) {
        if (session.id == saveId) {
            session.lastModified = QDateTime::currentDateTime();
            // In a real implementation, would save to file/database
            return true;
        }
    }
    return false;
}

void AIChatWidget::deleteSession(const QString& sessionId)
{
    for (int i = 0; i < sessions_.size(); ++i) {
        if (sessions_[i].id == sessionId) {
            sessions_.removeAt(i);

            if (currentSessionId_ == sessionId) {
                currentSessionId_ = sessions_.isEmpty() ? QString() : sessions_.first().id;
                updateChatDisplay();
            }

            updateSessionList();
            break;
        }
    }
}

void AIChatWidget::sendMessage(const QString& message)
{
    if (message.trimmed().isEmpty() || currentSessionId_.isEmpty()) {
        return;
    }

    // Find current session
    ChatSession* currentSession = nullptr;
    for (ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            currentSession = &session;
            break;
        }
    }

    if (!currentSession) return;

    // Create user message
    ChatMessage userMessage;
    userMessage.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    userMessage.role = "user";
    userMessage.content = message;
    userMessage.timestamp = QDateTime::currentDateTime();

    // Add to session
    currentSession->messages.append(userMessage);
    currentSession->lastModified = QDateTime::currentDateTime();

    // Update title if it's the first message
    if (currentSession->messages.size() == 1) {
        currentSession->title = generateSessionTitle(message);
        updateSessionList();
    }

    // Display message
    addMessageToChat(userMessage);
    messageInput_->clear();

    // Update status
    isGenerating_ = true;
    statusLabel_->setText(tr("Generating response..."));
    sendBtn_->setEnabled(false);
    stopBtn_->setVisible(true);
    progressBar_->setVisible(true);
    progressBar_->setRange(0, 0); // Indeterminate

    // Prepare context (last few messages)
    QList<ChatMessage> context;
    int contextSize = qMin(10, currentSession->messages.size()); // Last 10 messages
    for (int i = currentSession->messages.size() - contextSize; i < currentSession->messages.size(); ++i) {
        context.append(currentSession->messages[i]);
    }

    // Prepare options
    QJsonObject options;
    options["temperature"] = temperatureInput_->text().toDouble();
    options["max_tokens"] = maxTokensInput_->text().toInt();
    options["stream"] = streamResponsesCheck_->isChecked();

    // Send to worker
    QMetaObject::invokeMethod(chatWorker_, "sendMessage", 
                            Qt::QueuedConnection,
                            Q_ARG(QString, message),
                            Q_ARG(QList<ChatMessage>, context),
                            Q_ARG(QJsonObject, options));

    emit messageSent(message);
}

void AIChatWidget::clearChat()
{
    if (currentSessionId_.isEmpty()) return;

    for (ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            session.messages.clear();
            session.lastModified = QDateTime::currentDateTime();
            updateChatDisplay();
            break;
        }
    }
}

void AIChatWidget::exportChat(const QString& format)
{
    if (currentSessionId_.isEmpty()) return;

    ChatSession* currentSession = nullptr;
    for (ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            currentSession = &session;
            break;
        }
    }

    if (!currentSession || currentSession->messages.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No messages to export."));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Chat"), 
                                                  QString("%1.%2").arg(currentSession->title, format == "json" ? "json" : "txt"), 
                                                  format == "json" ? tr("JSON files (*.json)") : tr("Text files (*.txt)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Error"), tr("Could not open file for writing."));
        return;
    }

    QTextStream out(&file);

    if (format == "json") {
        QJsonObject sessionObj;
        sessionObj["id"] = currentSession->id;
        sessionObj["title"] = currentSession->title;
        sessionObj["created"] = currentSession->created.toString(Qt::ISODate);
        sessionObj["model"] = currentSession->model;

        QJsonArray messagesArray;
        for (const ChatMessage& msg : currentSession->messages) {
            QJsonObject msgObj;
            msgObj["id"] = msg.id;
            msgObj["role"] = msg.role;
            msgObj["content"] = msg.content;
            msgObj["timestamp"] = msg.timestamp.toString(Qt::ISODate);
            msgObj["metadata"] = msg.metadata;
            messagesArray.append(msgObj);
        }
        sessionObj["messages"] = messagesArray;

        QJsonDocument doc(sessionObj);
        out << doc.toJson(QJsonDocument::Indented);
    } else {
        // Text format
        out << QString("Chat Session: %1\n").arg(currentSession->title);
        out << QString("Created: %1\n").arg(currentSession->created.toString());
        out << QString("Model: %1\n").arg(currentSession->model);
        out << QString("Messages: %1\n\n").arg(currentSession->messages.size());

        for (const ChatMessage& msg : currentSession->messages) {
            out << QString("[%1] %2: %3\n\n")
                   .arg(msg.timestamp.toString("yyyy-MM-dd hh:mm:ss"),
                        msg.role.toUpper(),
                        msg.content);
        }
    }

    file.close();
    QMessageBox::information(this, tr("Export Complete"), tr("Chat exported successfully."));
}

void AIChatWidget::setApiKey(const QString& key)
{
    apiKey_ = key;
    if (chatWorker_) {
        chatWorker_->setApiKey(key);
    }
}

void AIChatWidget::setModel(const QString& model)
{
    modelCombo_->setCurrentText(model);
    modelSelector_->setCurrentText(model);
    if (chatWorker_) {
        chatWorker_->setModel(model);
    }
}

void AIChatWidget::setTemperature(double temperature)
{
    temperatureInput_->setText(QString::number(temperature));
}

void AIChatWidget::setMaxTokens(int tokens)
{
    maxTokensInput_->setText(QString::number(tokens));
}

void AIChatWidget::refresh()
{
    updateSessionList();
    updateChatDisplay();
}

void AIChatWidget::onResponseReceived(const QString& response, const QJsonObject& metadata)
{
    if (currentSessionId_.isEmpty()) return;

    // Find current session
    ChatSession* currentSession = nullptr;
    for (ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            currentSession = &session;
            break;
        }
    }

    if (!currentSession) return;

    // Create assistant message
    ChatMessage assistantMessage;
    assistantMessage.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    assistantMessage.role = "assistant";
    assistantMessage.content = response;
    assistantMessage.timestamp = QDateTime::currentDateTime();
    assistantMessage.metadata = metadata;

    // Add to session
    currentSession->messages.append(assistantMessage);
    currentSession->lastModified = QDateTime::currentDateTime();

    // Display message
    addMessageToChat(assistantMessage);

    // Reset UI state
    isGenerating_ = false;
    sendBtn_->setEnabled(true);
    stopBtn_->setVisible(false);
    progressBar_->setVisible(false);
    statusLabel_->setText(tr("Ready"));

    // Update metadata display
    if (metadata.contains("tokens_used")) {
        statusLabel_->setText(tr("Response complete (%1 tokens)").arg(metadata["tokens_used"].toInt()));
    }

    emit responseReceived(response);
}

void AIChatWidget::onError(const QString& error)
{
    QMessageBox::warning(this, tr("AI Error"), error);

    isGenerating_ = false;
    sendBtn_->setEnabled(true);
    stopBtn_->setVisible(false);
    progressBar_->setVisible(false);
    statusLabel_->setText(tr("Error occurred"));
}

void AIChatWidget::onNewChat()
{
    bool ok;
    QString title = QInputDialog::getText(this, tr("New Chat Session"), 
                                        tr("Session title:"), QLineEdit::Normal, tr("New Chat"), &ok);
    if (ok) {
        createNewSession(title);
    }
}

void AIChatWidget::onLoadChat()
{
    // In a real implementation, would show file dialog to load saved sessions
    QMessageBox::information(this, tr("Load Chat"), tr("Load chat functionality not implemented yet"));
}

void AIChatWidget::onSaveChat()
{
    saveCurrentSession();
}

void AIChatWidget::onDeleteChat()
{
    if (currentSessionId_.isEmpty()) return;

    ChatSession* currentSession = nullptr;
    for (const ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            currentSession = const_cast<ChatSession*>(&session);
            break;
        }
    }

    if (!currentSession) return;

    if (QMessageBox::question(this, tr("Delete Chat"), 
                            tr("Delete chat session '%1'?").arg(currentSession->title)) == QMessageBox::Yes) {
        deleteSession(currentSessionId_);
    }
}

void AIChatWidget::onExportChat()
{
    exportChat("txt");
}

void AIChatWidget::onSendMessage()
{
    QString message = messageInput_->text().trimmed();
    if (!message.isEmpty()) {
        sendMessage(message);
    }
}

void AIChatWidget::onMessageTextChanged()
{
    updateSendButton();

    if (typingTimer_->isActive()) {
        typingTimer_->stop();
    }
    typingTimer_->start(1000); // Reset status after 1 second of no typing
}

void AIChatWidget::onSessionSelected(QListWidgetItem* item)
{
    if (!item) return;

    QString sessionId = item->data(Qt::UserRole).toString();
    loadSession(sessionId);
}

void AIChatWidget::onModelChanged(const QString& model)
{
    modelSelector_->setCurrentText(model);
    if (chatWorker_) {
        chatWorker_->setModel(model);
    }
}

void AIChatWidget::onSettingsChanged()
{
    // Update current session settings
    if (!currentSessionId_.isEmpty()) {
        for (ChatSession& session : sessions_) {
            if (session.id == currentSessionId_) {
                session.model = modelSelector_->currentText();
                session.settings["temperature"] = temperatureInput_->text().toDouble();
                session.settings["max_tokens"] = maxTokensInput_->text().toInt();
                break;
            }
        }
    }

    // Update worker settings
    if (chatWorker_) {
        chatWorker_->setModel(modelSelector_->currentText());
    }
}

void AIChatWidget::onStopGeneration()
{
    // In a real implementation, would signal the worker to stop
    isGenerating_ = false;
    sendBtn_->setEnabled(true);
    stopBtn_->setVisible(false);
    progressBar_->setVisible(false);
    statusLabel_->setText(tr("Generation stopped"));
}

void AIChatWidget::onClearChat()
{
    clearChat();
}

void AIChatWidget::onCopyMessage()
{
    // Implementation for copying selected message
}

void AIChatWidget::onRegenerateResponse()
{
    // Implementation for regenerating last response
}

void AIChatWidget::updateSessionList()
{
    sessionList_->clear();

    for (const ChatSession& session : sessions_) {
        QListWidgetItem* item = new QListWidgetItem(sessionList_);
        item->setText(session.title);
        item->setData(Qt::UserRole, session.id);

        // Add message count
        QString tooltip = tr("Created: %1\nMessages: %2\nModel: %3")
                         .arg(session.created.toString(), QString::number(session.messages.size()), session.model);
        item->setToolTip(tooltip);

        // Highlight current session
        if (session.id == currentSessionId_) {
            item->setBackground(QBrush(QColor(100, 150, 200, 50)));
        }
    }
}

void AIChatWidget::updateChatDisplay()
{
    // Clear existing messages
    QLayoutItem* child;
    while ((child = chatLayout_->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    if (currentSessionId_.isEmpty()) {
        QLabel* emptyLabel = new QLabel(tr("No chat session selected"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: gray; font-size: 14px;");
        chatLayout_->addWidget(emptyLabel);
        chatLayout_->addStretch();
        return;
    }

    // Find current session
    const ChatSession* currentSession = nullptr;
    for (const ChatSession& session : sessions_) {
        if (session.id == currentSessionId_) {
            currentSession = &session;
            break;
        }
    }

    if (!currentSession) return;

    // Add messages
    for (const ChatMessage& message : currentSession->messages) {
        addMessageToChat(message);
    }

    chatLayout_->addStretch();
    scrollToBottom();
}

void AIChatWidget::addMessageToChat(const ChatMessage& message)
{
    QWidget* messageWidget = new QWidget();
    QVBoxLayout* messageLayout = new QVBoxLayout(messageWidget);
    messageLayout->setContentsMargins(10, 5, 10, 5);

    // Header with role and timestamp
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* roleLabel = new QLabel(message.role == "user" ? tr("You") : tr("Assistant"));
    roleLabel->setStyleSheet(QString("font-weight: bold; color: %1;").arg(getRoleColor(message.role).name()));
    roleLabel->setFixedWidth(80);

    QLabel* timeLabel = new QLabel(message.timestamp.toString("hh:mm"));
    timeLabel->setStyleSheet("color: gray; font-size: 10px;");

    headerLayout->addWidget(roleLabel);
    headerLayout->addWidget(timeLabel);
    headerLayout->addStretch();

    messageLayout->addLayout(headerLayout);

    // Message content
    QLabel* contentLabel = new QLabel(message.content);
    contentLabel->setWordWrap(true);
    contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLabel->setStyleSheet("padding: 5px; background-color: rgba(0,0,0,0.05); border-radius: 5px;");
    messageLayout->addWidget(contentLabel);

    // Metadata (if available)
    if (!message.metadata.isEmpty()) {
        QHBoxLayout* metaLayout = new QHBoxLayout();
        metaLayout->addStretch();

        if (message.metadata.contains("tokens_used")) {
            QLabel* tokensLabel = new QLabel(tr("%1 tokens").arg(message.metadata["tokens_used"].toInt()));
            tokensLabel->setStyleSheet("color: gray; font-size: 9px;");
            metaLayout->addWidget(tokensLabel);
        }

        if (message.metadata.contains("response_time")) {
            QLabel* timeLabel = new QLabel(tr("%.1fs").arg(message.metadata["response_time"].toDouble()));
            timeLabel->setStyleSheet("color: gray; font-size: 9px;");
            metaLayout->addWidget(timeLabel);
        }

        messageLayout->addLayout(metaLayout);
    }

    chatLayout_->insertWidget(chatLayout_->count() - 1, messageWidget);
}

void AIChatWidget::scrollToBottom()
{
    QTimer::singleShot(100, [this]() {
        QScrollBar* scrollbar = chatScrollArea_->verticalScrollBar();
        scrollbar->setValue(scrollbar->maximum());
    });
}

void AIChatWidget::updateSendButton()
{
    QString text = messageInput_->text().trimmed();
    sendBtn_->setEnabled(!text.isEmpty() && !isGenerating_);
}

QString AIChatWidget::generateSessionTitle(const QString& firstMessage) const
{
    // Generate a title based on the first message
    QString title = firstMessage.left(50);
    if (firstMessage.length() > 50) {
        title += "...";
    }
    return title;
}

QString AIChatWidget::formatMessageForDisplay(const ChatMessage& message) const
{
    return QString("[%1] %2: %3")
           .arg(message.timestamp.toString("hh:mm:ss"),
                message.role,
                message.content);
}

QColor AIChatWidget::getRoleColor(const QString& role) const
{
    if (role == "user") return QColor(100, 150, 200);
    if (role == "assistant") return QColor(100, 200, 150);
    if (role == "system") return QColor(200, 150, 100);
    return QColor(150, 150, 150);
}

void AIChatWidget::saveCurrentSession()
{
    saveSession(currentSessionId_);
}

void AIChatWidget::loadCurrentSession()
{
    loadSession(currentSessionId_);
}

void AIChatWidget::saveState()
{
    RAWRXD_TIMED_FUNC();
    QSettings settings;
    settings.beginGroup("AIChatWidget");
    settings.setValue("mainSplitterSizes", mainSplitter_->saveState());
    settings.setValue("chatSplitterSizes", chatSplitter_->saveState());
    settings.setValue("currentModel", modelCombo_->currentText());
    settings.setValue("temperature", temperatureInput_->text());
    settings.setValue("maxTokens", maxTokensInput_->text());
    settings.setValue("streamResponses", streamResponsesCheck_->isChecked());
    settings.setValue("apiKey", apiKey_);
    settings.endGroup();

    // Save sessions (simplified - in production would save to files)
    QJsonArray sessionsArray;
    for (const ChatSession& session : sessions_) {
        RAWRXD_TIMED_NAMED("serializeSession");
        QJsonObject sessionObj;
        sessionObj["id"] = session.id;
        sessionObj["title"] = session.title;
        sessionObj["created"] = session.created.toString(Qt::ISODate);
        sessionObj["lastModified"] = session.lastModified.toString(Qt::ISODate);
        sessionObj["model"] = session.model;
        sessionObj["settings"] = session.settings;

        QJsonArray messagesArray;
        for (const ChatMessage& msg : session.messages) {
            QJsonObject msgObj;
            msgObj["id"] = msg.id;
            msgObj["role"] = msg.role;
            msgObj["content"] = msg.content;
            msgObj["timestamp"] = msg.timestamp.toString(Qt::ISODate);
            msgObj["metadata"] = msg.metadata;
            messagesArray.append(msgObj);
        }
        sessionObj["messages"] = messagesArray;

        sessionsArray.append(sessionObj);
    }

    QJsonDocument doc(sessionsArray);
    settings.setValue("AIChatWidget/sessions", QString::fromUtf8(doc.toJson()));
}

void AIChatWidget::restoreState()
{
    RAWRXD_TIMED_FUNC();
    QSettings settings;
    settings.beginGroup("AIChatWidget");

    if (settings.contains("mainSplitterSizes")) {
        mainSplitter_->restoreState(settings.value("mainSplitterSizes").toByteArray());
    }

    if (settings.contains("chatSplitterSizes")) {
        chatSplitter_->restoreState(settings.value("chatSplitterSizes").toByteArray());
    }

    QString model = settings.value("currentModel", "GPT-4").toString();
    modelCombo_->setCurrentText(model);
    modelSelector_->setCurrentText(model);

    temperatureInput_->setText(settings.value("temperature", "0.7").toString());
    maxTokensInput_->setText(settings.value("maxTokens", "2048").toString());
    streamResponsesCheck_->setChecked(settings.value("streamResponses", true).toBool());

    apiKey_ = settings.value("apiKey").toString();
    if (chatWorker_) {
        chatWorker_->setApiKey(apiKey_);
        chatWorker_->setModel(model);
    }

    // Restore sessions
    QString sessionsData = settings.value("sessions").toString();
    if (!sessionsData.isEmpty()) {
        RAWRXD_TIMED_NAMED("deserializeSessions");
        QJsonDocument doc = QJsonDocument::fromJson(sessionsData.toUtf8());
        if (doc.isArray()) {
            QJsonArray sessionsArray = doc.array();
            sessions_.clear();

            for (const QJsonValue& val : sessionsArray) {
                QJsonObject sessionObj = val.toObject();

                ChatSession session;
                session.id = sessionObj["id"].toString();
                session.title = sessionObj["title"].toString();
                session.created = QDateTime::fromString(sessionObj["created"].toString(), Qt::ISODate);
                session.lastModified = QDateTime::fromString(sessionObj["lastModified"].toString(), Qt::ISODate);
                session.model = sessionObj["model"].toString();
                session.settings = sessionObj["settings"].toObject();

                QJsonArray messagesArray = sessionObj["messages"].toArray();
                for (const QJsonValue& msgVal : messagesArray) {
                    QJsonObject msgObj = msgVal.toObject();

                    ChatMessage message;
                    message.id = msgObj["id"].toString();
                    message.role = msgObj["role"].toString();
                    message.content = msgObj["content"].toString();
                    message.timestamp = QDateTime::fromString(msgObj["timestamp"].toString(), Qt::ISODate);
                    message.metadata = msgObj["metadata"].toObject();

                    session.messages.append(message);
                }

                sessions_.append(session);
            }

            if (!sessions_.isEmpty()) {
                currentSessionId_ = sessions_.first().id;
            }
        }
    }

    settings.endGroup();
}

void AIChatWidget::showSessionContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = sessionList_->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    menu.addAction(tr("Rename"), [this, item]() {
        bool ok;
        QString newTitle = QInputDialog::getText(this, tr("Rename Session"), 
                                               tr("New title:"), QLineEdit::Normal, 
                                               item->text(), &ok);
        if (ok && !newTitle.isEmpty()) {
            QString sessionId = item->data(Qt::UserRole).toString();
            for (ChatSession& session : sessions_) {
                if (session.id == sessionId) {
                    session.title = newTitle;
                    updateSessionList();
                    break;
                }
            }
        }
    });

    menu.addAction(tr("Duplicate"), [this, item]() {
        QString sessionId = item->data(Qt::UserRole).toString();
        for (const ChatSession& session : sessions_) {
            if (session.id == sessionId) {
                ChatSession newSession = session;
                newSession.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                newSession.title += tr(" (Copy)");
                newSession.created = QDateTime::currentDateTime();
                newSession.lastModified = newSession.created;
                sessions_.prepend(newSession);
                currentSessionId_ = newSession.id;
                updateSessionList();
                updateChatDisplay();
                break;
            }
        }
    });

    menu.addSeparator();
    menu.addAction(tr("Delete"), [this, item]() {
        onDeleteChat();
    });

    if (!menu.isEmpty()) {
        menu.exec(sessionList_->mapToGlobal(pos));
    }
}