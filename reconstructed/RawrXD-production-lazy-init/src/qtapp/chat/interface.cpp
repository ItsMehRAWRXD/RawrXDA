#include "chat_interface.h"

#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QTextDocument>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

ChatInterface::ChatInterface(QWidget* parent)
    : QWidget(parent),
      message_history_(new QTextEdit(this)),
      message_input_(new QLineEdit(this)),
      modelSelector_(new QComboBox(this)),
      modelSelector2_(new QComboBox(this)),
      maxModeToggle_(new QCheckBox("Max Mode", this)),
      statusLabel_(new QLabel("Ready", this)),
      maxMode_(false),
      m_tokenProgress(new QProgressBar(this)),
      m_hideTimer(new QTimer(this))
{
    message_history_->setReadOnly(true);
    message_history_->setAcceptRichText(true);
    m_tokenProgress->setMinimum(0);
    m_tokenProgress->setMaximum(0); // Indeterminate by default; set during streaming
    m_tokenProgress->setVisible(false);

    auto* sendButton = new QPushButton("Send", this);
    auto* refreshButton = new QPushButton("Refresh Models", this);

    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel("Model:", this));
    topRow->addWidget(modelSelector_);
    topRow->addWidget(new QLabel("Alt:", this));
    topRow->addWidget(modelSelector2_);
    topRow->addWidget(maxModeToggle_);

    auto* inputRow = new QHBoxLayout();
    inputRow->addWidget(message_input_);
    inputRow->addWidget(sendButton);
    inputRow->addWidget(refreshButton);

    auto* layout = new QVBoxLayout();
    layout->addLayout(topRow);
    layout->addWidget(message_history_);
    layout->addLayout(inputRow);
    layout->addWidget(m_tokenProgress);
    layout->addWidget(statusLabel_);
    setLayout(layout);

    connect(sendButton, &QPushButton::clicked, this, &ChatInterface::sendMessage);
    connect(refreshButton, &QPushButton::clicked, this, &ChatInterface::refreshModels);
    connect(maxModeToggle_, &QCheckBox::toggled, this, &ChatInterface::onMaxModeToggled);
    connect(modelSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChatInterface::onModelChanged);
    connect(modelSelector2_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChatInterface::onModel2Changed);

    connect(m_hideTimer, &QTimer::timeout, this, &ChatInterface::hideProgress);
}

void ChatInterface::initialize() {
    loadAvailableModels();
    loadAvailableModelsForSecond();
    // Default stylesheet for rich content
    if (auto* doc = message_history_->document()) {
        doc->setDefaultStyleSheet(
            "pre, code { font-family: Consolas, 'Courier New', monospace; }"
            "pre { background:#f5f7fb; border:1px solid #e0e3e8; padding:8px; border-radius:6px; }"
            "blockquote { border-left:3px solid #e0e3e8; padding-left:8px; color:#555; }"
            "h1, h2, h3 { margin-top:8px; }"
        );
    }
}

void ChatInterface::addMessage(const QString& sender, const QString& message) {
    // Convert markdown to HTML and render with role styling + timestamp
    QTextDocument mdDoc;
    mdDoc.setMarkdown(message);
    const QString bodyHtml = mdDoc.toHtml("utf-8");

    const QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString color = (sender.compare("user", Qt::CaseInsensitive) == 0)
                              ? QString("#1976D2")
                              : QString("#2E7D32");
    const QString headerHtml = QString("<div style=\"color:%1; font-weight:600;\">%2 • %3</div>")
                                   .arg(color, sender, timeStr);
    const QString msgHtml = QString("<div class=\"msg\">%1<div class=\"body\">%2</div></div>")
                                .arg(headerHtml, bodyHtml);

    QTextCursor cursor = message_history_->textCursor();
    cursor.movePosition(QTextCursor::End);
    message_history_->setTextCursor(cursor);
    cursor.insertHtml(msgHtml);
    cursor.insertHtml("<hr style=\"border:0; border-top:1px solid #eee; margin:8px 0;\"/>");
}

QString ChatInterface::selectedModel() const {
    return modelSelector_->currentText();
}

bool ChatInterface::isMaxMode() const {
    return maxMode_;
}

void ChatInterface::sendMessageProgrammatically(const QString& message) {
    message_input_->setText(message);
    sendMessage();
}

void ChatInterface::executeAgentCommand(const QString& command, const QString& args) {
    // Placeholder for future agent commands (e.g., /plan, /run)
    Q_UNUSED(command);
    Q_UNUSED(args);
}

bool ChatInterface::isAgentCommand(const QString& message) const {
    return message.startsWith('/');
}

void ChatInterface::displayResponse(const QString& response) {
    addMessage("assistant", response);
    m_busy = false;
    hideProgress();
}

void ChatInterface::focusInput() {
    message_input_->setFocus();
}

void ChatInterface::sendMessage() {
    const QString text = message_input_->text();
    if (text.isEmpty() || m_busy) {
        return;
    }
    m_busy = true;
    m_lastPrompt = text;
    addMessage("user", text);
    message_input_->clear();
    statusLabel_->setText("Sending...");
    m_tokenProgress->setVisible(true);
    m_tokenProgress->setMaximum(0); // Indeterminate until tokens arrive
    emit messageSent(text);
}

void ChatInterface::refreshModels() {
    loadAvailableModels();
    loadAvailableModelsForSecond();
}

void ChatInterface::onModelChanged(int index) {
    Q_UNUSED(index);
    emit modelSelected(selectedModel());
}

void ChatInterface::onModel2Changed(int index) {
    Q_UNUSED(index);
    emit model2Selected(modelSelector2_->currentText());
}

void ChatInterface::onMaxModeToggled(bool enabled) {
    maxMode_ = enabled;
    emit maxModeChanged(enabled);
}

void ChatInterface::setCanSendMessage(bool enabled) {
    message_input_->setEnabled(enabled);
}

void ChatInterface::onTokenGenerated(int delta) {
    if (!m_tokenProgress->isVisible()) {
        m_tokenProgress->setVisible(true);
        m_tokenProgress->setMaximum(1000); // Switch to determinate mode
        m_tokenProgress->setValue(0);
    }
    int value = m_tokenProgress->value() + delta;
    if (value > m_tokenProgress->maximum()) value = m_tokenProgress->maximum();
    m_tokenProgress->setValue(value);
    m_hideTimer->start(750); // Auto-hide after inactivity
}

void ChatInterface::hideProgress() {
    m_hideTimer->stop();
    m_tokenProgress->setVisible(false);
}

void ChatInterface::loadAvailableModels() {
    modelSelector_->clear();
    // Placeholder: populate with common names; Phase 3 will query actual models
    modelSelector_->addItem("rawrxd:latest");
    modelSelector_->addItem("llama3:8b-instruct");
    modelSelector_->addItem("mistral:7b-instruct");
}

void ChatInterface::loadAvailableModelsForSecond() {
    modelSelector2_->clear();
    modelSelector2_->addItem("(None)");
    modelSelector2_->addItem("claude:haiku");
}

QString ChatInterface::resolveGgufPath(const QString& modelName) {
    // Placeholder resolver: Phase 3 will map Ollama names to GGUF paths
    return modelName;
}
