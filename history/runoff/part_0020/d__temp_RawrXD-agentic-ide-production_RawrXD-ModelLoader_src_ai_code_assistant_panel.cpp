#include "ai_code_assistant_panel.h"
#include "ai_code_assistant.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

AICodeAssistantPanel::AICodeAssistantPanel(QWidget *parent)
    : QWidget(parent)
    , m_suggestionsDisplay(nullptr)
    , m_resultsWidget(nullptr)
    , m_progressBar(nullptr)
    , m_searchInput(nullptr)
    , m_commandInput(nullptr)
    , m_latencyLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_completeBtn(nullptr)
    , m_refactorBtn(nullptr)
    , m_explainBtn(nullptr)
    , m_bugFixBtn(nullptr)
    , m_optimizeBtn(nullptr)
    , m_searchBtn(nullptr)
    , m_grepBtn(nullptr)
    , m_executeBtn(nullptr)
    , m_assistant(nullptr)
{
    setWindowTitle("AI Code Assistant");
    setMinimumSize(800, 600);
    setupUI();

    // Log initialization
    qDebug() << QString("[%1] AICodeAssistantPanel initialized").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

AICodeAssistantPanel::~AICodeAssistantPanel()
{
    // Cleanup
    qDebug() << QString("[%1] AICodeAssistantPanel destroyed").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

void AICodeAssistantPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ============================================================
    // Suggestions & Results Section
    // ============================================================
    QGroupBox *displayGroup = new QGroupBox("Suggestions & Results", this);
    QVBoxLayout *displayLayout = new QVBoxLayout(displayGroup);

    m_suggestionsDisplay = new QTextEdit(this);
    m_suggestionsDisplay->setReadOnly(true);
    m_suggestionsDisplay->setPlaceholderText("AI suggestions will appear here...");
    displayLayout->addWidget(m_suggestionsDisplay);

    m_resultsWidget = new QListWidget(this);
    m_resultsWidget->setMaximumHeight(150);
    displayLayout->addWidget(m_resultsWidget);

    mainLayout->addWidget(displayGroup);

    // ============================================================
    // AI Suggestions Section
    // ============================================================
    QGroupBox *aiGroup = new QGroupBox("AI Suggestions", this);
    QHBoxLayout *aiLayout = new QHBoxLayout(aiGroup);

    m_completeBtn = new QPushButton("Completion", this);
    m_refactorBtn = new QPushButton("Refactoring", this);
    m_explainBtn = new QPushButton("Explanation", this);
    m_bugFixBtn = new QPushButton("Bug Fix", this);
    m_optimizeBtn = new QPushButton("Optimize", this);

    connect(m_completeBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onRequestCompletion);
    connect(m_refactorBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onRequestRefactoring);
    connect(m_explainBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onRequestExplanation);
    connect(m_bugFixBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onRequestBugFix);
    connect(m_optimizeBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onRequestOptimization);

    aiLayout->addWidget(m_completeBtn);
    aiLayout->addWidget(m_refactorBtn);
    aiLayout->addWidget(m_explainBtn);
    aiLayout->addWidget(m_bugFixBtn);
    aiLayout->addWidget(m_optimizeBtn);

    mainLayout->addWidget(aiGroup);

    // ============================================================
    // File Operations Section
    // ============================================================
    QGroupBox *fileOpsGroup = new QGroupBox("File Operations", this);
    QVBoxLayout *fileOpsLayout = new QVBoxLayout(fileOpsGroup);

    // Search Files
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel("Search Pattern:", this);
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("*.cpp, *.h, etc.");
    m_searchBtn = new QPushButton("Search Files", this);
    connect(m_searchBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onSearchFiles);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchInput);
    searchLayout->addWidget(m_searchBtn);
    fileOpsLayout->addLayout(searchLayout);

    // Grep Files
    QHBoxLayout *grepLayout = new QHBoxLayout();
    QLabel *grepLabel = new QLabel("Grep Pattern:", this);
    QLineEdit *grepInput = new QLineEdit(this);
    grepInput->setPlaceholderText("Search text with regex");
    m_grepBtn = new QPushButton("Grep Files", this);
    connect(m_grepBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onGrepFiles);

    grepLayout->addWidget(grepLabel);
    grepLayout->addWidget(grepInput);
    grepLayout->addWidget(m_grepBtn);
    fileOpsLayout->addLayout(grepLayout);

    mainLayout->addWidget(fileOpsGroup);

    // ============================================================
    // Command Execution Section
    // ============================================================
    QGroupBox *cmdGroup = new QGroupBox("Command Execution", this);
    QHBoxLayout *cmdLayout = new QHBoxLayout(cmdGroup);

    QLabel *cmdLabel = new QLabel("Command:", this);
    m_commandInput = new QLineEdit(this);
    m_commandInput->setPlaceholderText("PowerShell command to execute");
    m_executeBtn = new QPushButton("Execute", this);
    connect(m_executeBtn, &QPushButton::clicked, this, &AICodeAssistantPanel::onExecuteCommand);

    cmdLayout->addWidget(cmdLabel);
    cmdLayout->addWidget(m_commandInput);
    cmdLayout->addWidget(m_executeBtn);

    mainLayout->addWidget(cmdGroup);

    // ============================================================
    // Progress & Metrics Section
    // ============================================================
    QGroupBox *metricsGroup = new QGroupBox("Progress & Metrics", this);
    QVBoxLayout *metricsLayout = new QVBoxLayout(metricsGroup);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setValue(0);
    metricsLayout->addWidget(m_progressBar);

    QHBoxLayout *metricLabelsLayout = new QHBoxLayout();
    m_latencyLabel = new QLabel("Latency: -- ms", this);
    m_statusLabel = new QLabel("Ready", this);
    metricLabelsLayout->addWidget(m_latencyLabel);
    metricLabelsLayout->addStretch();
    metricLabelsLayout->addWidget(m_statusLabel);
    metricsLayout->addLayout(metricLabelsLayout);

    mainLayout->addWidget(metricsGroup);

    setLayout(mainLayout);
}

void AICodeAssistantPanel::setAssistant(AICodeAssistant *assistant)
{
    m_assistant = assistant;

    if (!m_assistant) {
        qWarning() << "[AICodeAssistantPanel] No assistant provided";
        return;
    }

    // ============================================================
    // Connect AI Suggestion Signals
    // ============================================================
    connect(m_assistant, &AICodeAssistant::completionReady,
            this, &AICodeAssistantPanel::onCompletionReady);
    connect(m_assistant, &AICodeAssistant::refactoringReady,
            this, &AICodeAssistantPanel::onRefactoringReady);
    connect(m_assistant, &AICodeAssistant::explanationReady,
            this, &AICodeAssistantPanel::onExplanationReady);
    connect(m_assistant, &AICodeAssistant::bugFixReady,
            this, &AICodeAssistantPanel::onBugFixReady);
    connect(m_assistant, &AICodeAssistant::optimizationReady,
            this, &AICodeAssistantPanel::onOptimizationReady);

    // ============================================================
    // Connect File Operation Signals
    // ============================================================
    connect(m_assistant, &AICodeAssistant::fileSearchProgress,
            this, &AICodeAssistantPanel::onFileSearchProgress);
    connect(m_assistant, &AICodeAssistant::searchResultsReady,
            this, &AICodeAssistantPanel::onSearchResultsReady);
    connect(m_assistant, &AICodeAssistant::grepResultsReady,
            this, &AICodeAssistantPanel::onGrepResultsReady);

    // ============================================================
    // Connect Command Execution Signals
    // ============================================================
    connect(m_assistant, &AICodeAssistant::commandProgress,
            this, &AICodeAssistantPanel::onCommandProgress);
    connect(m_assistant, &AICodeAssistant::commandOutputReceived,
            this, &AICodeAssistantPanel::onCommandOutputReceived);
    connect(m_assistant, &AICodeAssistant::commandCompleted,
            this, &AICodeAssistantPanel::onCommandCompleted);

    // ============================================================
    // Connect Metrics Signals
    // ============================================================
    connect(m_assistant, &AICodeAssistant::latencyMeasured,
            this, &AICodeAssistantPanel::onLatencyMeasured);
    connect(m_assistant, &AICodeAssistant::operationMetrics,
            this, &AICodeAssistantPanel::onOperationMetrics);

    // ============================================================
    // Connect Error Signal
    // ============================================================
    connect(m_assistant, &AICodeAssistant::errorOccurred,
            this, &AICodeAssistantPanel::onErrorOccurred);

    qDebug() << QString("[%1] AICodeAssistantPanel connected to AICodeAssistant")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

// ============================================================
// AI Suggestion Handlers
// ============================================================

void AICodeAssistantPanel::onCompletionReady(const QString &suggestion)
{
    displaySuggestion("Code Completion", suggestion);
}

void AICodeAssistantPanel::onRefactoringReady(const QString &suggestion)
{
    displaySuggestion("Refactoring Suggestion", suggestion);
}

void AICodeAssistantPanel::onExplanationReady(const QString &explanation)
{
    displaySuggestion("Code Explanation", explanation);
}

void AICodeAssistantPanel::onBugFixReady(const QString &suggestion)
{
    displaySuggestion("Bug Fix Suggestion", suggestion);
}

void AICodeAssistantPanel::onOptimizationReady(const QString &suggestion)
{
    displaySuggestion("Optimization Suggestion", suggestion);
}

// ============================================================
// File Operation Handlers
// ============================================================

void AICodeAssistantPanel::onFileSearchProgress(int processed, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(processed);
    m_statusLabel->setText(QString("Searching: %1/%2").arg(processed).arg(total));
}

void AICodeAssistantPanel::onSearchResultsReady(const QStringList &results)
{
    displayResults(results);
    m_progressBar->setValue(m_progressBar->maximum());
    m_statusLabel->setText("Search complete");
}

void AICodeAssistantPanel::onGrepResultsReady(const QJsonArray &results)
{
    displayGrepResults(results);
    m_statusLabel->setText("Grep complete");
}

// ============================================================
// Command Execution Handlers
// ============================================================

void AICodeAssistantPanel::onCommandProgress(const QString &status)
{
    m_statusLabel->setText(status);
    qDebug() << "[Command Progress]" << status;
}

void AICodeAssistantPanel::onCommandOutputReceived(const QString &output)
{
    m_suggestionsDisplay->append(QString("=== Command Output ===\n%1").arg(output));
}

void AICodeAssistantPanel::onCommandCompleted(int exitCode)
{
    m_statusLabel->setText(QString("Command completed (Exit Code: %1)").arg(exitCode));
    m_progressBar->setValue(m_progressBar->maximum());
}

// ============================================================
// Metrics Handlers
// ============================================================

void AICodeAssistantPanel::onLatencyMeasured(qint64 milliseconds)
{
    m_latencyLabel->setText(QString("Latency: %1 ms").arg(milliseconds));
}

void AICodeAssistantPanel::onOperationMetrics(const QJsonObject &metrics)
{
    QJsonDocument doc(metrics);
    QString metricsStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    qDebug() << "[Metrics]" << metricsStr;
}

// ============================================================
// Error Handler
// ============================================================

void AICodeAssistantPanel::onErrorOccurred(const QString &error)
{
    displayError(error);
}

// ============================================================
// UI Action Handlers
// ============================================================

void AICodeAssistantPanel::onRequestCompletion()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString code = m_suggestionsDisplay->toPlainText();
    if (code.isEmpty()) {
        displayError("Please provide code for completion");
        return;
    }

    m_statusLabel->setText("Requesting code completion...");
    m_assistant->getCodeCompletion(code);
}

void AICodeAssistantPanel::onRequestRefactoring()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString code = m_suggestionsDisplay->toPlainText();
    if (code.isEmpty()) {
        displayError("Please provide code for refactoring");
        return;
    }

    m_statusLabel->setText("Requesting refactoring suggestions...");
    m_assistant->getRefactoringSuggestions(code);
}

void AICodeAssistantPanel::onRequestExplanation()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString code = m_suggestionsDisplay->toPlainText();
    if (code.isEmpty()) {
        displayError("Please provide code to explain");
        return;
    }

    m_statusLabel->setText("Requesting code explanation...");
    m_assistant->getCodeExplanation(code);
}

void AICodeAssistantPanel::onRequestBugFix()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString code = m_suggestionsDisplay->toPlainText();
    if (code.isEmpty()) {
        displayError("Please provide code for bug fixing");
        return;
    }

    m_statusLabel->setText("Requesting bug fix suggestions...");
    m_assistant->getBugFixSuggestions(code, "");
}

void AICodeAssistantPanel::onRequestOptimization()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString code = m_suggestionsDisplay->toPlainText();
    if (code.isEmpty()) {
        displayError("Please provide code for optimization");
        return;
    }

    m_statusLabel->setText("Requesting optimization suggestions...");
    m_assistant->getOptimizationSuggestions(code);
}

void AICodeAssistantPanel::onSearchFiles()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString pattern = m_searchInput->text();
    if (pattern.isEmpty()) {
        displayError("Please enter a search pattern");
        return;
    }

    m_statusLabel->setText("Searching files...");
    m_progressBar->setValue(0);
    m_resultsWidget->clear();
    m_assistant->searchFiles(pattern);
}

void AICodeAssistantPanel::onGrepFiles()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString pattern = m_searchInput->text();
    if (pattern.isEmpty()) {
        displayError("Please enter a grep pattern");
        return;
    }

    m_statusLabel->setText("Grepping files...");
    m_progressBar->setValue(0);
    m_resultsWidget->clear();
    m_assistant->grepFiles(pattern);
}

void AICodeAssistantPanel::onExecuteCommand()
{
    if (!m_assistant) {
        displayError("Assistant not configured");
        return;
    }

    QString command = m_commandInput->text();
    if (command.isEmpty()) {
        displayError("Please enter a command");
        return;
    }

    m_statusLabel->setText("Executing command...");
    m_progressBar->setValue(0);
    m_suggestionsDisplay->clear();
    m_assistant->executePowerShellCommand(command);
}

// ============================================================
// Display Helpers
// ============================================================

void AICodeAssistantPanel::displaySuggestion(const QString &title, const QString &suggestion)
{
    QString content = QString("=== %1 ===\n%2").arg(title, suggestion);
    m_suggestionsDisplay->setText(content);
    m_statusLabel->setText("Suggestion ready");
}

void AICodeAssistantPanel::displayResults(const QStringList &results)
{
    m_resultsWidget->clear();
    for (const QString &result : results) {
        m_resultsWidget->addItem(result);
    }
    m_statusLabel->setText(QString("Found %1 results").arg(results.count()));
}

void AICodeAssistantPanel::displayGrepResults(const QJsonArray &results)
{
    m_resultsWidget->clear();
    int count = 0;

    for (const QJsonValue &val : results) {
        QJsonObject obj = val.toObject();
        QString file = obj.value("file").toString();
        int line = obj.value("lineNumber").toInt();
        QString content = obj.value("content").toString();

        m_resultsWidget->addItem(QString("[%1:%2] %3").arg(file).arg(line).arg(content));
        count++;
    }

    m_statusLabel->setText(QString("Found %1 matches").arg(count));
}

void AICodeAssistantPanel::displayError(const QString &error)
{
    m_suggestionsDisplay->setText(QString("=== ERROR ===\n%1").arg(error));
    m_statusLabel->setText("Error occurred");
    qWarning() << "[AICodeAssistantPanel Error]" << error;
}
