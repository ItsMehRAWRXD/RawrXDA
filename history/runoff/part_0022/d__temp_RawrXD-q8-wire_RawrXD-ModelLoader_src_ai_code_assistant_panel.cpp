#include "ai_code_assistant_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QSplitter>
#include <QScrollArea>
#include <QDebug>

AICodeAssistantPanel::AICodeAssistantPanel(AICodeAssistant *assistant, QWidget *parent)
    : QWidget(parent)
    , m_assistant(assistant)
    , m_historyMaxSize(50)
{
    setupUI();
    connectSignals();
}

AICodeAssistantPanel::~AICodeAssistantPanel()
{
}

void AICodeAssistantPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // ========================================================================
    // Status and Configuration Bar
    // ========================================================================
    QHBoxLayout *statusLayout = new QHBoxLayout();
    
    // Connection Status
    connectionStatusLabel_ = new QLabel("●");
    connectionStatusLabel_->setStyleSheet("color: red; font-size: 16px;");
    statusLayout->addWidget(connectionStatusLabel_);
    
    statusLayout->addWidget(new QLabel("Connection:"));
    statusLayout->addStretch();
    
    // Latency indicator
    latencyLabel_ = new QLabel("Latency: -- ms");
    latencyLabel_->setStyleSheet("color: #666; font-family: monospace;");
    statusLayout->addWidget(latencyLabel_);
    
    mainLayout->addLayout(statusLayout);

    // ========================================================================
    // Configuration Controls
    // ========================================================================
    QGroupBox *configGroup = new QGroupBox("Configuration");
    QVBoxLayout *configLayout = new QVBoxLayout(configGroup);
    
    // Temperature control
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->addWidget(new QLabel("Temperature:"));
    temperatureSlider_ = new QSlider(Qt::Horizontal);
    temperatureSlider_->setRange(0, 200);
    temperatureSlider_->setValue(30);
    temperatureSlider_->setMaximumWidth(150);
    tempLayout->addWidget(temperatureSlider_);
    QLabel *tempValue = new QLabel("0.3");
    tempValue->setMaximumWidth(30);
    tempLayout->addWidget(tempValue);
    connect(temperatureSlider_, &QSlider::valueChanged, [tempValue](int value) {
        tempValue->setText(QString::number(value / 100.0, 'f', 2));
    });
    tempLayout->addStretch();
    configLayout->addLayout(tempLayout);
    
    // Max tokens control
    QHBoxLayout *tokensLayout = new QHBoxLayout();
    tokensLayout->addWidget(new QLabel("Max Tokens:"));
    maxTokensSlider_ = new QSlider(Qt::Horizontal);
    maxTokensSlider_->setRange(32, 2048);
    maxTokensSlider_->setValue(256);
    maxTokensSlider_->setMaximumWidth(150);
    tokensLayout->addWidget(maxTokensSlider_);
    QLabel *tokensValue = new QLabel("256");
    tokensValue->setMaximumWidth(40);
    tokensLayout->addWidget(tokensValue);
    connect(maxTokensSlider_, &QSlider::valueChanged, [tokensValue](int value) {
        tokensValue->setText(QString::number(value));
    });
    tokensLayout->addStretch();
    configLayout->addLayout(tokensLayout);
    
    // Suggestion type selector
    QHBoxLayout *typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("Suggestion Type:"));
    suggestionTypeCombo_ = new QComboBox();
    suggestionTypeCombo_->addItems({"Completion", "Refactoring", "Explanation", 
                                     "Bug Fix", "Optimization", "Analysis"});
    suggestionTypeCombo_->setMaximumWidth(150);
    typeLayout->addWidget(suggestionTypeCombo_);
    typeLayout->addStretch();
    configLayout->addLayout(typeLayout);
    
    mainLayout->addWidget(configGroup);

    // ========================================================================
    // IDE Integration Controls
    // ========================================================================
    QGroupBox *toolsGroup = new QGroupBox("IDE Tools");
    QHBoxLayout *toolsLayout = new QHBoxLayout(toolsGroup);
    
    QPushButton *searchBtn = new QPushButton("Search Files");
    QPushButton *grepBtn = new QPushButton("Grep");
    QPushButton *execBtn = new QPushButton("Execute Cmd");
    
    searchBtn->setMaximumWidth(120);
    grepBtn->setMaximumWidth(120);
    execBtn->setMaximumWidth(120);
    
    connect(searchBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onSearchButtonClicked);
    connect(grepBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onGrepButtonClicked);
    connect(execBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onExecuteCommandButtonClicked);
    
    toolsLayout->addWidget(searchBtn);
    toolsLayout->addWidget(grepBtn);
    toolsLayout->addWidget(execBtn);
    toolsLayout->addStretch();
    
    mainLayout->addWidget(toolsGroup);

    // ========================================================================
    // Code Display Areas (Splitter)
    // ========================================================================
    QSplitter *splitter = new QSplitter(Qt::Vertical);
    splitter->setStyleSheet("QSplitter::handle { background: #ccc; }");
    
    // Original code display
    QGroupBox *originalGroup = new QGroupBox("Original Code");
    QVBoxLayout *originalLayout = new QVBoxLayout(originalGroup);
    originalCodeDisplay_ = new QTextEdit();
    originalCodeDisplay_->setPlaceholderText("Paste code here or select from editor...");
    originalCodeDisplay_->setMaximumHeight(100);
    originalCodeDisplay_->setFont(QFont("Courier", 9));
    originalLayout->addWidget(originalCodeDisplay_);
    splitter->addWidget(originalGroup);
    
    // AI suggestion display
    QGroupBox *suggestionGroup = new QGroupBox("AI Suggestion");
    QVBoxLayout *suggestionLayout = new QVBoxLayout(suggestionGroup);
    suggestionDisplay_ = new QTextEdit();
    suggestionDisplay_->setPlaceholderText("AI suggestions will appear here...");
    suggestionDisplay_->setReadOnly(true);
    suggestionDisplay_->setFont(QFont("Courier", 9));
    suggestionDisplay_->setStyleSheet("background-color: #f0f8ff;");
    suggestionLayout->addWidget(suggestionDisplay_);
    splitter->addWidget(suggestionGroup);
    
    // Explanation display
    QGroupBox *explanationGroup = new QGroupBox("Explanation");
    QVBoxLayout *explanationLayout = new QVBoxLayout(explanationGroup);
    explanationDisplay_ = new QTextEdit();
    explanationDisplay_->setPlaceholderText("Explanations and details will appear here...");
    explanationDisplay_->setReadOnly(true);
    explanationDisplay_->setFont(QFont("Courier", 9));
    explanationDisplay_->setStyleSheet("background-color: #f0fff0;");
    explanationLayout->addWidget(explanationDisplay_);
    splitter->addWidget(explanationGroup);
    
    splitter->setSizes({100, 100, 100});
    mainLayout->addWidget(splitter, 1);

    // ========================================================================
    // Action Buttons
    // ========================================================================
    QHBoxLayout *actionLayout = new QHBoxLayout();
    
    QPushButton *applyBtn = new QPushButton("Apply");
    QPushButton *copyBtn = new QPushButton("Copy");
    QPushButton *exportBtn = new QPushButton("Export");
    QPushButton *clearHistoryBtn = new QPushButton("Clear History");
    
    applyBtn->setMaximumWidth(100);
    copyBtn->setMaximumWidth(100);
    exportBtn->setMaximumWidth(100);
    clearHistoryBtn->setMaximumWidth(120);
    
    connect(applyBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onApplyButtonClicked);
    connect(copyBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onCopyButtonClicked);
    connect(exportBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onExportButtonClicked);
    connect(clearHistoryBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onClearHistoryButtonClicked);
    
    actionLayout->addWidget(applyBtn);
    actionLayout->addWidget(copyBtn);
    actionLayout->addWidget(exportBtn);
    actionLayout->addStretch();
    actionLayout->addWidget(clearHistoryBtn);
    
    mainLayout->addLayout(actionLayout);

    // ========================================================================
    // History
    // ========================================================================
    QGroupBox *historyGroup = new QGroupBox("Suggestion History");
    QVBoxLayout *historyLayout = new QVBoxLayout(historyGroup);
    
    suggestionHistoryList_ = new QListWidget();
    suggestionHistoryList_->setMaximumHeight(100);
    connect(suggestionHistoryList_, &QListWidget::itemDoubleClicked,
            this, &AICodeAssistantPanel::onHistoryItemDoubleClicked);
    
    historyLayout->addWidget(suggestionHistoryList_);
    mainLayout->addWidget(historyGroup);

    setLayout(mainLayout);
}

void AICodeAssistantPanel::connectSignals()
{
    if (!m_assistant) return;
    
    // AI response signals
    connect(m_assistant, &AICodeAssistant::suggestionReceived,
            this, &AICodeAssistantPanel::onSuggestionReceived);
    connect(m_assistant, &AICodeAssistant::suggestionStreamChunk,
            this, &AICodeAssistantPanel::onSuggestionStreamChunk);
    connect(m_assistant, &AICodeAssistant::suggestionComplete,
            this, &AICodeAssistantPanel::onSuggestionComplete);
    
    // File search signals
    connect(m_assistant, &AICodeAssistant::searchResultsReady,
            this, &AICodeAssistantPanel::onSearchResultsReady);
    connect(m_assistant, &AICodeAssistant::grepResultsReady,
            this, &AICodeAssistantPanel::onGrepResultsReady);
    connect(m_assistant, &AICodeAssistant::fileSearchProgress,
            this, &AICodeAssistantPanel::onFileSearchProgress);
    
    // Command execution signals
    connect(m_assistant, &AICodeAssistant::commandOutputReceived,
            this, &AICodeAssistantPanel::onCommandOutputReceived);
    connect(m_assistant, &AICodeAssistant::commandErrorReceived,
            this, &AICodeAssistantPanel::onCommandErrorReceived);
    connect(m_assistant, &AICodeAssistant::commandCompleted,
            this, &AICodeAssistantPanel::onCommandCompleted);
    connect(m_assistant, &AICodeAssistant::commandProgress,
            this, &AICodeAssistantPanel::onCommandProgress);
    
    // Metrics signals
    connect(m_assistant, &AICodeAssistant::latencyMeasured,
            this, &AICodeAssistantPanel::updateLatency);
    
    // Configuration signals
    connect(temperatureSlider_, &QSlider::valueChanged, [this](int value) {
        m_assistant->setTemperature(value / 100.0f);
    });
    connect(maxTokensSlider_, &QSlider::valueChanged, [this](int value) {
        m_assistant->setMaxTokens(value);
    });
}

void AICodeAssistantPanel::setConnectionStatus(bool connected)
{
    if (connectionStatusLabel_) {
        connectionStatusLabel_->setStyleSheet(
            connected ? "color: green; font-size: 16px;" : "color: red; font-size: 16px;"
        );
    }
}

void AICodeAssistantPanel::updateLatency(qint64 ms)
{
    if (latencyLabel_) {
        latencyLabel_->setText(QString("Latency: %1 ms").arg(ms));
    }
}

// ============================================================================
// Slot Implementations
// ============================================================================

void AICodeAssistantPanel::onSuggestionReceived(const QString &suggestion, const QString &type)
{
    suggestionDisplay_->setText(suggestion);
    addToHistory(suggestion, type);
}

void AICodeAssistantPanel::onSuggestionStreamChunk(const QString &chunk)
{
    suggestionDisplay_->append(chunk);
}

void AICodeAssistantPanel::onSuggestionComplete(bool success, const QString &message)
{
    if (!success) {
        explanationDisplay_->setText(QString("Error: %1").arg(message));
    }
}

void AICodeAssistantPanel::onSearchResultsReady(const QStringList &results)
{
    m_currentResults = results;
    explanationDisplay_->setText(formatResultsForDisplay(results));
}

void AICodeAssistantPanel::onGrepResultsReady(const QStringList &results)
{
    m_currentResults = results;
    explanationDisplay_->setText(formatResultsForDisplay(results));
}

void AICodeAssistantPanel::onFileSearchProgress(int processed, int total)
{
    explanationDisplay_->setText(QString("Searched %1 files...").arg(processed));
}

void AICodeAssistantPanel::onCommandOutputReceived(const QString &output)
{
    explanationDisplay_->append(output);
}

void AICodeAssistantPanel::onCommandErrorReceived(const QString &error)
{
    explanationDisplay_->setText(QString("Error:\n%1").arg(error));
    explanationDisplay_->setStyleSheet("background-color: #ffe0e0;");
}

void AICodeAssistantPanel::onCommandCompleted(int exitCode)
{
    explanationDisplay_->append(QString("\n[Command exited with code %1]").arg(exitCode));
}

void AICodeAssistantPanel::onCommandProgress(const QString &status)
{
    explanationDisplay_->setText(status);
}

void AICodeAssistantPanel::onApplyButtonClicked()
{
    QString suggestion = suggestionDisplay_->toPlainText();
    if (suggestion.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No suggestion to apply");
        return;
    }
    emit applyButtonClicked(suggestion);
}

void AICodeAssistantPanel::onCopyButtonClicked()
{
    QString suggestion = suggestionDisplay_->toPlainText();
    if (suggestion.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No suggestion to copy");
        return;
    }
    QApplication::clipboard()->setText(suggestion);
    QMessageBox::information(this, "Success", "Suggestion copied to clipboard");
}

void AICodeAssistantPanel::onExportButtonClicked()
{
    QString suggestion = suggestionDisplay_->toPlainText();
    QString explanation = explanationDisplay_->toPlainText();
    
    QString export_text = QString("Original Code:\n%1\n\nSuggestion:\n%2\n\nExplanation:\n%3")
        .arg(originalCodeDisplay_->toPlainText())
        .arg(suggestion)
        .arg(explanation);
    
    QApplication::clipboard()->setText(export_text);
    QMessageBox::information(this, "Success", "Full suggestion exported to clipboard");
}

void AICodeAssistantPanel::onClearHistoryButtonClicked()
{
    suggestionHistoryList_->clear();
}

void AICodeAssistantPanel::onTemperatureChanged(int value)
{
    m_assistant->setTemperature(value / 100.0f);
}

void AICodeAssistantPanel::onMaxTokensChanged(int value)
{
    m_assistant->setMaxTokens(value);
}

void AICodeAssistantPanel::onSearchButtonClicked()
{
    showSearchDialog();
}

void AICodeAssistantPanel::onGrepButtonClicked()
{
    showGrepDialog();
}

void AICodeAssistantPanel::onExecuteCommandButtonClicked()
{
    showExecuteCommandDialog();
}

void AICodeAssistantPanel::onHistoryItemDoubleClicked()
{
    QListWidgetItem *item = suggestionHistoryList_->currentItem();
    if (item) {
        suggestionDisplay_->setText(item->text());
    }
}

void AICodeAssistantPanel::addToHistory(const QString &suggestion, const QString &type)
{
    QString entry = QString("[%1] %2...").arg(type).arg(suggestion.left(50));
    suggestionHistoryList_->insertItem(0, entry);
    
    // Keep only recent items
    while (suggestionHistoryList_->count() > m_historyMaxSize) {
        delete suggestionHistoryList_->takeItem(suggestionHistoryList_->count() - 1);
    }
}

void AICodeAssistantPanel::showSearchDialog()
{
    bool ok;
    QString pattern = QInputDialog::getText(this, "Search Files", 
        "File pattern:", QLineEdit::Normal, "", &ok);
    
    if (ok && !pattern.isEmpty()) {
        m_assistant->searchFiles(pattern);
    }
}

void AICodeAssistantPanel::showGrepDialog()
{
    bool ok;
    QString pattern = QInputDialog::getText(this, "Grep Files", 
        "Search pattern:", QLineEdit::Normal, "", &ok);
    
    if (ok && !pattern.isEmpty()) {
        m_assistant->grepFiles(pattern);
    }
}

void AICodeAssistantPanel::showExecuteCommandDialog()
{
    bool ok;
    QString command = QInputDialog::getText(this, "Execute Command", 
        "PowerShell command:", QLineEdit::Normal, "", &ok);
    
    if (ok && !command.isEmpty()) {
        m_assistant->executePowerShellCommand(command);
    }
}

QString AICodeAssistantPanel::formatResultsForDisplay(const QStringList &results)
{
    if (results.isEmpty()) {
        return "No results found.";
    }
    
    QString formatted = QString("Found %1 results:\n\n").arg(results.length());
    for (int i = 0; i < qMin(50, results.length()); ++i) {
        formatted += results[i] + "\n";
    }
    
    if (results.length() > 50) {
        formatted += QString("\n... and %1 more results").arg(results.length() - 50);
    }
    
    return formatted;
}
