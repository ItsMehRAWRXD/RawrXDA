/**
 * @file inline_chat_widget.cpp
 * @brief Implementation of InlineChatWidget - Inline code assistance chat
 */

#include "inline_chat_widget.h"
#include "../../model_router_adapter.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QClipboard>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>
#include <QDateTime>

InlineChatWidget::InlineChatWidget(QWidget* parent)
    : QWidget(parent)
    , mModelRouter(nullptr)
    , mIsGenerating(false)
    , mCurrentResponseItem(nullptr)
    , mAccumulatedResponse()
{
    setupUI();
    setupModelRouter();
    connectSignals();
    restoreState();
    
    setWindowTitle("Inline Chat");
}

InlineChatWidget::~InlineChatWidget()
{
    saveState();
}

void InlineChatWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    // Context display
    mContextEditor = new QTextEdit(this);
    mContextEditor->setReadOnly(true);
    mContextEditor->setFont(QFont("Courier", 9));
    mContextEditor->setMaximumHeight(80);
    mContextEditor->setPlaceholderText("Code context will appear here...");
    mMainLayout->addWidget(new QLabel("Context:", this));
    mMainLayout->addWidget(mContextEditor);
    
    // Chat history
    mChatHistory = new QListWidget(this);
    mChatHistory->setStyleSheet("QListWidget { border: 1px solid #ccc; border-radius: 4px; }");
    mMainLayout->addWidget(new QLabel("Chat History:", this));
    mMainLayout->addWidget(mChatHistory);
    
    // Input area
    mMainLayout->addWidget(new QLabel("Message:", this));
    mInputEditor = new QTextEdit(this);
    mInputEditor->setPlaceholderText("Type your question or code assistance request here...");
    mInputEditor->setMaximumHeight(100);
    mMainLayout->addWidget(mInputEditor);
    
    // Button layout
    mButtonLayout = new QHBoxLayout();
    
    mSendButton = new QPushButton("Send", this);
    mSendButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    mButtonLayout->addWidget(mSendButton);
    
    mCopyButton = new QPushButton("Copy Response", this);
    mButtonLayout->addWidget(mCopyButton);
    
    mInsertButton = new QPushButton("Insert Code", this);
    mButtonLayout->addWidget(mInsertButton);
    
    mContextButton = new QPushButton("Set Context", this);
    mButtonLayout->addWidget(mContextButton);
    
    mExportButton = new QPushButton("Export Chat", this);
    mButtonLayout->addWidget(mExportButton);
    
    mClearButton = new QPushButton("Clear", this);
    mButtonLayout->addWidget(mClearButton);
    
    mButtonLayout->addStretch();
    
    mMainLayout->addLayout(mButtonLayout);
}

void InlineChatWidget::connectSignals()
{
    connect(mSendButton, &QPushButton::clicked, this, &InlineChatWidget::onSendMessage);
    connect(mClearButton, &QPushButton::clicked, this, &InlineChatWidget::onClearChat);
    connect(mCopyButton, &QPushButton::clicked, this, &InlineChatWidget::onCopyResponse);
    connect(mInsertButton, &QPushButton::clicked, this, &InlineChatWidget::onInsertCodeBlock);
    connect(mContextButton, &QPushButton::clicked, this, &InlineChatWidget::onShowContext);
    connect(mExportButton, &QPushButton::clicked, this, &InlineChatWidget::onExportChat);
    connect(mChatHistory, &QListWidget::itemSelectionChanged, this, [this]() {
        QList<QListWidgetItem*> selected = mChatHistory->selectedItems();
        if (!selected.isEmpty()) {
            onMessageSelected(selected.first());
        }
    });
}

void InlineChatWidget::onSendMessage()
{
    QString message = mInputEditor->toPlainText().trimmed();
    if (message.isEmpty()) {
        QMessageBox::warning(this, "Empty Message", "Please enter a message.");
        return;
    }
    
    if (mIsGenerating) {
        QMessageBox::warning(this, "Busy", "Please wait for the current generation to complete.");
        return;
    }
    
    appendMessage("user", message);
    emit messageSent(message);
    
    // Real AI integration with streaming
    if (!mModelRouter || !mModelRouter->isReady()) {
        appendMessage("system", "[ERROR] AI model router not initialized. Check configuration.");
        qWarning() << "[InlineChatWidget] Model router not ready";
        return;
    }
    
    // Add context from editor if available
    QString context = mContextEditor->toPlainText();
    QString fullPrompt = message;
    if (!context.isEmpty()) {
        fullPrompt = QString("Code context:\n```\n%1\n```\n\nUser request: %2")
                         .arg(context, message);
    }
    
    // Disable send button during generation
    mIsGenerating = true;
    mSendButton->setEnabled(false);
    mSendButton->setText("Generating...");
    mAccumulatedResponse.clear();
    
    // Create placeholder for streaming response
    InlineMessage msg;
    msg.id = QString::number(mMessages.size());
    msg.sender = "ai";
    msg.content = "";
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    mMessages.append(msg);
    
    QString displayText = QString("[%1] ai: [Generating...]").arg(msg.timestamp);
    mCurrentResponseItem = new QListWidgetItem(displayText);
    mCurrentResponseItem->setBackground(QColor(230, 245, 255));
    mChatHistory->addItem(mCurrentResponseItem);
    mChatHistory->scrollToBottom();
    
    // Start streaming generation
    QString selectedModel = mModelRouter->selectBestModel("chat", "general", false);
    mModelRouter->generateStream(fullPrompt, selectedModel, 4096);
    
    mInputEditor->clear();
}

void InlineChatWidget::onClearChat()
{
    int ret = QMessageBox::question(this, "Clear Chat", "Clear all chat history?");
    if (ret == QMessageBox::Yes) {
        mChatHistory->clear();
        mMessages.clear();
        mContextEditor->clear();
    }
}

void InlineChatWidget::onCopyResponse()
{
    QList<QListWidgetItem*> selected = mChatHistory->selectedItems();
    if (!selected.isEmpty()) {
        QString text = selected.first()->text();
        QApplication::clipboard()->setText(text);
        QMessageBox::information(this, "Copied", "Response copied to clipboard!");
    }
}

void InlineChatWidget::onInsertCodeBlock()
{
    QList<QListWidgetItem*> selected = mChatHistory->selectedItems();
    if (!selected.isEmpty()) {
        QString code = selected.first()->text();
        emit codeInserted(code);
        QMessageBox::information(this, "Inserted", "Code block inserted into editor!");
    }
}

void InlineChatWidget::onShowContext()
{
    QString context = mContextEditor->toPlainText();
    if (context.isEmpty()) {
        QMessageBox::warning(this, "No Context", "No code context set. Select code in the editor and click 'Set Context'.");
    } else {
        QMessageBox::information(this, "Current Context", "Current context:\n\n" + context);
    }
}

void InlineChatWidget::onExportChat()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Chat", "", "Text Files (*.txt);;HTML Files (*.html)");
    if (!filename.isEmpty()) {
        emit chatExported(filename);
        QMessageBox::information(this, "Exported", "Chat exported successfully!");
    }
}

void InlineChatWidget::onMessageSelected(QListWidgetItem* item)
{
    qDebug() << "Selected message:" << item->text();
}

void InlineChatWidget::appendMessage(const QString& sender, const QString& content)
{
    InlineMessage msg;
    msg.id = QString::number(mMessages.size());
    msg.sender = sender;
    msg.content = content;
    msg.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    mMessages.append(msg);
    
    QString displayText = QString("[%1] %2: %3").arg(msg.timestamp, sender, content.left(80));
    if (content.length() > 80) displayText += "...";
    
    QListWidgetItem* item = new QListWidgetItem(displayText);
    if (sender == "ai") {
        item->setBackground(QColor(230, 245, 255));
    } else {
        item->setBackground(QColor(240, 255, 240));
    }
    
    mChatHistory->addItem(item);
    mChatHistory->scrollToBottom();
}

void InlineChatWidget::restoreState()
{
    QSettings settings("RawrXD", "IDE");
    // Restore last chat state if available
}

void InlineChatWidget::saveState()
{
    QSettings settings("RawrXD", "IDE");
    // Save current chat state for restoration
}

void InlineChatWidget::setupModelRouter()
{
    mModelRouter = new ModelRouterAdapter(this);
    
    // Initialize with config file (try multiple locations)
    QString configPath = "model_config.json";
    if (!QFile::exists(configPath)) {
        configPath = "../model_config.json";
    }
    if (!QFile::exists(configPath)) {
        configPath = "../../model_config.json";
    }
    
    if (!mModelRouter->initialize(configPath)) {
        qWarning() << "[InlineChatWidget] Failed to initialize model router from" << configPath;
    }
    
    if (!mModelRouter->loadApiKeys()) {
        qWarning() << "[InlineChatWidget] Failed to load API keys from environment";
    }
    
    // Connect streaming signals
    connect(mModelRouter, &ModelRouterAdapter::generationChunk,
            this, &InlineChatWidget::onGenerationChunk);
    connect(mModelRouter, &ModelRouterAdapter::generationComplete,
            this, &InlineChatWidget::onGenerationComplete);
    connect(mModelRouter, &ModelRouterAdapter::generationError,
            this, &InlineChatWidget::onGenerationError);
    
    qDebug() << "[InlineChatWidget] Model router initialized. Ready:" << mModelRouter->isReady();
}

void InlineChatWidget::onGenerationChunk(const QString& chunk)
{
    if (!mCurrentResponseItem) {
        qWarning() << "[InlineChatWidget] Received chunk but no current response item";
        return;
    }
    
    // Accumulate response
    mAccumulatedResponse += chunk;
    
    // Update display with accumulated text
    if (!mMessages.isEmpty()) {
        mMessages.last().content = mAccumulatedResponse;
    }
    
    QString displayText = QString("[%1] ai: %2")
                              .arg(mMessages.isEmpty() ? "??:??:??" : mMessages.last().timestamp)
                              .arg(mAccumulatedResponse.left(100));
    if (mAccumulatedResponse.length() > 100) displayText += "...";
    
    mCurrentResponseItem->setText(displayText);
    mChatHistory->scrollToBottom();
}

void InlineChatWidget::onGenerationComplete(const QString& result, int tokens_used, double latency_ms)
{
    Q_UNUSED(tokens_used);
    Q_UNUSED(latency_ms);
    
    if (!mCurrentResponseItem) {
        qWarning() << "[InlineChatWidget] Generation complete but no current response item";
        mIsGenerating = false;
        mSendButton->setEnabled(true);
        mSendButton->setText("Send");
        return;
    }
    
    // Update final message
    if (!mMessages.isEmpty()) {
        mMessages.last().content = result;
    }
    
    QString displayText = QString("[%1] ai: %2")
                              .arg(mMessages.isEmpty() ? "??:??:??" : mMessages.last().timestamp)
                              .arg(result.left(80));
    if (result.length() > 80) displayText += "...";
    
    mCurrentResponseItem->setText(displayText);
    mChatHistory->scrollToBottom();
    
    // Reset state
    mCurrentResponseItem = nullptr;
    mAccumulatedResponse.clear();
    mIsGenerating = false;
    mSendButton->setEnabled(true);
    mSendButton->setText("Send");
    
    qDebug() << "[InlineChatWidget] Generation complete:" << tokens_used << "tokens," << latency_ms << "ms";
}

void InlineChatWidget::onGenerationError(const QString& error)
{
    qWarning() << "[InlineChatWidget] Generation error:" << error;
    
    if (mCurrentResponseItem) {
        QString errorText = QString("[ERROR] Failed to generate response: %1").arg(error);
        mCurrentResponseItem->setText(errorText);
        mCurrentResponseItem->setBackground(QColor(255, 230, 230));
        
        if (!mMessages.isEmpty()) {
            mMessages.last().content = errorText;
        }
    } else {
        appendMessage("system", QString("[ERROR] %1").arg(error));
    }
    
    // Reset state
    mCurrentResponseItem = nullptr;
    mAccumulatedResponse.clear();
    mIsGenerating = false;
    mSendButton->setEnabled(true);
    mSendButton->setText("Send");
}

