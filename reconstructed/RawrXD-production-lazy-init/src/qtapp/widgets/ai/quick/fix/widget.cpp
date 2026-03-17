#include "ai_quick_fix_widget.h"
#include "../../model_router_adapter.h"
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

AIQuickFixWidget::AIQuickFixWidget(QWidget* parent) : QWidget(parent)
    , m_modelRouter(nullptr)
    , m_isGenerating(false)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QLabel* title = new QLabel("AI Quick Fix", this);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    layout->addWidget(title);
    
    // Issue description
    QLabel* issueLabel = new QLabel("Detected Issues", this);
    issueLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    layout->addWidget(issueLabel);
    
    m_issueList = new QListWidget(this);
    m_issueList->setStyleSheet("background-color: #2d2d2d; color: #e0e0e0; border: 1px solid #404040;");
    m_issueList->addItem("No diagnostics available. Analyze code to detect issues.");
    layout->addWidget(m_issueList);
    
    // Solution
    QLabel* solutionLabel = new QLabel("Suggested Fix", this);
    solutionLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    layout->addWidget(solutionLabel);
    
    m_solutionText = new QTextEdit(this);
    m_solutionText->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #404040;");
    m_solutionText->setPlainText("// Click 'Analyze Code' to generate AI-powered fix suggestions");
    layout->addWidget(m_solutionText);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_analyzeButton = new QPushButton("Analyze Code", this);
    m_analyzeButton->setStyleSheet("background-color: #007acc; color: white; padding: 8px;");
    connect(m_analyzeButton, &QPushButton::clicked, this, &AIQuickFixWidget::analyzeCode);
    buttonLayout->addWidget(m_analyzeButton);
    
    m_applyButton = new QPushButton("Apply Fix", this);
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, &AIQuickFixWidget::applyFix);
    buttonLayout->addWidget(m_applyButton);
    
    layout->addLayout(buttonLayout);
    
    // Initialize AI model router
    setupModelRouter();
}

AIQuickFixWidget::~AIQuickFixWidget()
{
    // Cleanup if needed
}

void AIQuickFixWidget::setupModelRouter()
{
    m_modelRouter = new ModelRouterAdapter(this);
    
    // Initialize with config file
    QString configPath = "model_config.json";
    if (!QFile::exists(configPath)) {
        configPath = "../model_config.json";
    }
    if (!QFile::exists(configPath)) {
        configPath = "../../model_config.json";
    }
    
    if (!m_modelRouter->initialize(configPath)) {
        qWarning() << "[AIQuickFixWidget] Failed to initialize model router from" << configPath;
    }
    
    if (!m_modelRouter->loadApiKeys()) {
        qWarning() << "[AIQuickFixWidget] Failed to load API keys";
    }
    
    // Connect AI signals
    connect(m_modelRouter, &ModelRouterAdapter::generationComplete,
            this, &AIQuickFixWidget::onGenerationComplete);
    connect(m_modelRouter, &ModelRouterAdapter::generationError,
            this, &AIQuickFixWidget::onGenerationError);
    
    qDebug() << "[AIQuickFixWidget] Model router initialized. Ready:" << m_modelRouter->isReady();
}

void AIQuickFixWidget::extractIssuesFromDiagnostics()
{
    m_issueList->clear();
    
    if (m_currentDiagnostics.isEmpty()) {
        m_issueList->addItem("No LSP diagnostics available");
        return;
    }
    
    for (const QJsonValue& diag : m_currentDiagnostics) {
        QJsonObject obj = diag.toObject();
        QString message = obj["message"].toString();
        int line = obj["line"].toInt(-1);
        QString severity = obj["severity"].toString("error");
        
        QString icon = "⚠️";
        if (severity == "error") icon = "❌";
        else if (severity == "warning") icon = "⚠️";
        else if (severity == "info") icon = "ℹ️";
        
        QString displayText = QString("%1 Line %2: %3").arg(icon).arg(line).arg(message);
        m_issueList->addItem(displayText);
    }
}

void AIQuickFixWidget::setDiagnostics(const QJsonArray& diagnostics)
{
    m_currentDiagnostics = diagnostics;
    extractIssuesFromDiagnostics();
    qDebug() << "[AIQuickFixWidget] Received" << diagnostics.size() << "diagnostics";
}

void AIQuickFixWidget::setCurrentCode(const QString& code, const QString& filePath)
{
    m_currentCode = code;
    m_currentFilePath = filePath;
    qDebug() << "[AIQuickFixWidget] Set current code:" << code.length() << "chars from" << filePath;
}

void AIQuickFixWidget::onGenerationComplete(const QString& result, int tokens_used, double latency_ms)
{
    Q_UNUSED(tokens_used);
    Q_UNUSED(latency_ms);
    
    m_generatedFix = result;
    m_solutionText->setPlainText(result);
    
    m_isGenerating = false;
    m_analyzeButton->setEnabled(true);
    m_analyzeButton->setText("Analyze Code");
    m_applyButton->setEnabled(true);
    
    qDebug() << "[AIQuickFixWidget] Generation complete:" << tokens_used << "tokens," << latency_ms << "ms";
}

void AIQuickFixWidget::onGenerationError(const QString& error)
{
    qWarning() << "[AIQuickFixWidget] Generation error:" << error;
    
    m_solutionText->setPlainText("// ERROR: Failed to generate fix\\n// " + error);
    
    m_isGenerating = false;
    m_analyzeButton->setEnabled(true);
    m_analyzeButton->setText("Analyze Code");
    m_applyButton->setEnabled(false);
    
    QMessageBox::critical(this, "Generation Error", "Failed to generate fix: " + error);
}

void AIQuickFixWidget::applyFix()
{
    if (m_generatedFix.isEmpty()) {
        QMessageBox::warning(this, "No Fix", "No fix available. Analyze code first.");
        return;
    }
    
    // In production: apply fix to actual file
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::warning(this, "No File", "No file path set. Cannot apply fix.");
        return;
    }
    
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Failed to open file for writing: " + file.errorString());
        return;
    }
    
    QTextStream out(&file);
    out << m_generatedFix;
    file.close();
    
    QMessageBox::information(this, "Success", "Fix applied successfully to " + QFileInfo(m_currentFilePath).fileName());
    m_solutionText->setPlainText("// Fix applied!\n" + m_generatedFix);
    m_applyButton->setEnabled(false);
}

void AIQuickFixWidget::analyzeCode()
{
    if (m_currentCode.isEmpty()) {
        QMessageBox::warning(this, "No Code", "No code to analyze. Set code first.");
        return;
    }
    
    if (!m_modelRouter || !m_modelRouter->isReady()) {
        QMessageBox::critical(this, "Error", "AI model router not initialized. Check configuration.");
        return;
    }
    
    if (m_isGenerating) {
        QMessageBox::warning(this, "Busy", "Analysis in progress. Please wait.");
        return;
    }
    
    m_isGenerating = true;
    m_analyzeButton->setEnabled(false);
    m_analyzeButton->setText("Analyzing...");
    m_solutionText->setPlainText("// Analyzing code and generating fixes...\n// This may take a few moments.");
    
    // Extract issues from diagnostics if available
    extractIssuesFromDiagnostics();
    
    // Build comprehensive prompt
    QString prompt = "You are a code analysis and fixing expert. Analyze the following code and fix all issues.\n\n";
    
    if (!m_currentDiagnostics.isEmpty()) {
        prompt += "Detected issues:\n";
        for (const QJsonValue& diag : m_currentDiagnostics) {
            QJsonObject obj = diag.toObject();
            QString message = obj["message"].toString();
            int line = obj["line"].toInt(-1);
            QString severity = obj["severity"].toString("error");
            prompt += QString("- Line %1 [%2]: %3\n").arg(line).arg(severity).arg(message);
        }
        prompt += "\n";
    }
    
    prompt += "Code:\n```\n" + m_currentCode + "\n```\n\n";
    prompt += "Provide the complete fixed code without explanations. Output only the corrected code.";
    
    // Request AI fix
    QString model = m_modelRouter->selectBestModel("code_generation", "cpp", false);
    m_modelRouter->generateAsync(prompt, model, 8192);
}