#include "cursor_workflow_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QSplitter>
#include <QDebug>

namespace RawrXD {

CursorWorkflowWidget::CursorWorkflowWidget(CursorWorkflowOrchestrator* orchestrator, QWidget* parent)
    : QWidget(parent)
    , m_orchestrator(orchestrator)
{
    setupUI();
    
    // Connect signals
    if (m_orchestrator) {
        connect(m_orchestrator, &CursorWorkflowOrchestrator::completionReady,
                this, &CursorWorkflowWidget::onCompletionReady);
        connect(m_orchestrator, &CursorWorkflowOrchestrator::cmdKCommandProgress,
                this, &CursorWorkflowWidget::onCmdKProgress);
        connect(m_orchestrator, &CursorWorkflowOrchestrator::cmdKCommandComplete,
                this, &CursorWorkflowWidget::onCmdKComplete);
        connect(m_orchestrator, &CursorWorkflowOrchestrator::prReviewProgress,
                this, &CursorWorkflowWidget::onPRReviewProgress);
        connect(m_orchestrator, &CursorWorkflowOrchestrator::prReviewComplete,
                this, &CursorWorkflowWidget::onPRReviewComplete);
        connect(m_orchestrator, &CursorWorkflowOrchestrator::securityScanComplete,
                this, &CursorWorkflowWidget::onSecurityScanComplete);
    }
}

CursorWorkflowWidget::~CursorWorkflowWidget() = default;

void CursorWorkflowWidget::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Title
    auto* titleLabel = new QLabel("<h2>Cursor + GitHub Copilot Workflow Demo</h2>");
    mainLayout->addWidget(titleLabel);
    
    // Status
    auto* statusLabel = new QLabel(
        m_orchestrator && m_orchestrator->isReady() ? 
        "✓ All systems ready" : "⚠ System not ready"
    );
    statusLabel->setStyleSheet(
        m_orchestrator && m_orchestrator->isReady() ? 
        "color: green;" : "color: orange;"
    );
    mainLayout->addWidget(statusLabel);
    
    // Tabs for different phases
    auto* tabWidget = new QTabWidget();
    mainLayout->addWidget(tabWidget);
    
    // Phase 1: Core Cursor Workflows
    auto* phase1Widget = new QWidget();
    createPhase1Tab();
    tabWidget->addTab(phase1Widget, "Phase 1: Cursor Workflows");
    
    // Phase 2: GitHub Copilot Enterprise
    auto* phase2Widget = new QWidget();
    createPhase2Tab();
    tabWidget->addTab(phase2Widget, "Phase 2: GitHub Copilot");
    
    // Phase 3: Advanced Agentic Features
    auto* phase3Widget = new QWidget();
    createPhase3Tab();
    tabWidget->addTab(phase3Widget, "Phase 3: Agentic Features");
    
    // Phase 4: Collaborative AI
    auto* phase4Widget = new QWidget();
    createPhase4Tab();
    tabWidget->addTab(phase4Widget, "Phase 4: Collaborative AI");
}

void CursorWorkflowWidget::createPhase1Tab()
{
    // TODO: Implement Phase 1 tab UI
    // Inline completion + Cmd+K commands
}

void CursorWorkflowWidget::createPhase2Tab()
{
    // TODO: Implement Phase 2 tab UI
    // PR review, security scanning, commit messages
}

void CursorWorkflowWidget::createPhase3Tab()
{
    // TODO: Implement Phase 3 tab UI
    // Codebase refactoring, semantic search, Q&A, migrations
}

void CursorWorkflowWidget::createPhase4Tab()
{
    // TODO: Implement Phase 4 tab UI
    // Collaborative sessions, live suggestions
}

// ============================================================
// PHASE 1 SLOTS
// ============================================================

void CursorWorkflowWidget::onRequestCompletion()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        qWarning() << "Orchestrator not ready";
        return;
    }
    
    CursorWorkflowOrchestrator::CompletionContext context;
    context.textBeforeCursor = m_codeInput->toPlainText();
    context.fileLanguage = m_languageInput->text();
    
    QString completion = m_orchestrator->requestInlineCompletion(context);
    // Result will come via signal
}

void CursorWorkflowWidget::onCompletionReady(const QString& completion, int latencyMs)
{
    if (m_completionOutput) {
        m_completionOutput->setPlainText(completion);
    }
    if (m_latencyLabel) {
        m_latencyLabel->setText(QString("Latency: %1 ms").arg(latencyMs));
        
        // Highlight if sub-50ms
        if (latencyMs < 50) {
            m_latencyLabel->setStyleSheet("color: green; font-weight: bold;");
        } else {
            m_latencyLabel->setStyleSheet("color: orange;");
        }
    }
}

void CursorWorkflowWidget::onExecuteCmdK()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::CmdKRequest request;
    request.command = static_cast<CursorWorkflowOrchestrator::CmdKCommand>(
        m_cmdKCommandCombo->currentIndex()
    );
    request.selectedText = m_cmdKInput->toPlainText();
    
    QString result = m_orchestrator->executeCmdKCommand(request);
    // Result will come via signal
}

void CursorWorkflowWidget::onCmdKProgress(const QString& status, int percentage)
{
    if (m_cmdKProgress) {
        m_cmdKProgress->setValue(percentage);
        m_cmdKProgress->setFormat(QString("%1 - %p%").arg(status));
    }
}

void CursorWorkflowWidget::onCmdKComplete(const QString& result)
{
    if (m_cmdKOutput) {
        m_cmdKOutput->setPlainText(result);
    }
}

// ============================================================
// PHASE 2 SLOTS
// ============================================================

void CursorWorkflowWidget::onReviewPR()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::PRReviewRequest request;
    request.prTitle = m_prTitleInput->text();
    request.diffText = m_prDiffInput->toPlainText();
    
    auto result = m_orchestrator->reviewPullRequest(request);
    // Result will come via signal
}

void CursorWorkflowWidget::onPRReviewProgress(const QString& status, int percentage)
{
    if (m_prProgress) {
        m_prProgress->setValue(percentage);
        m_prProgress->setFormat(QString("%1 - %p%").arg(status));
    }
}

void CursorWorkflowWidget::onPRReviewComplete(
    const CursorWorkflowOrchestrator::PRReviewResult& result)
{
    if (m_prReviewOutput) {
        QString output = QString(
            "Overall Assessment:\n%1\n\n"
            "Quality Score: %2/100\n\n"
            "Comments: %3\n"
            "Security Issues: %4\n"
            "Performance Issues: %5\n"
        ).arg(
            result.overallAssessment,
            QString::number(result.qualityScore),
            QString::number(result.inlineComments.size()),
            QString::number(result.securityIssues.size()),
            QString::number(result.performanceIssues.size())
        );
        m_prReviewOutput->setPlainText(output);
    }
}

void CursorWorkflowWidget::onScanSecurity()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::SecurityScanRequest request;
    request.scanScope = "changed_files";
    
    auto vulns = m_orchestrator->scanSecurity(request);
    // Result will come via signal
}

void CursorWorkflowWidget::onSecurityScanComplete(
    const QVector<CursorWorkflowOrchestrator::SecurityVulnerability>& vulns)
{
    if (m_securityVulnsList) {
        m_securityVulnsList->clear();
        for (const auto& vuln : vulns) {
            QString item = QString("[%1] %2:%3 - %4")
                .arg(vuln.severity, vuln.filePath)
                .arg(vuln.lineNumber)
                .arg(vuln.description);
            m_securityVulnsList->addItem(item);
        }
    }
}

// ============================================================
// PHASE 3 SLOTS
// ============================================================

void CursorWorkflowWidget::onRefactorCodebase()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::CodebaseRefactoringRequest request;
    request.intent = m_refactorInput->toPlainText();
    request.refactoringScope = m_refactorScope->currentText();
    
    auto result = m_orchestrator->refactorCodebase(request);
    
    if (m_refactorOutput) {
        QString output = QString(
            "Refactoring Report:\n%1\n\n"
            "Files Affected: %2\n"
            "Lines Changed: %3\n"
        ).arg(
            result.refactoringReport,
            QString::number(result.filesAffected),
            QString::number(result.linesChanged)
        );
        m_refactorOutput->setPlainText(output);
    }
}

void CursorWorkflowWidget::onSemanticSearch()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::SemanticSearchRequest request;
    request.query = m_searchQuery->text();
    request.maxResults = 10;
    
    auto results = m_orchestrator->searchCodeSemantically(request);
    
    if (m_searchResults) {
        m_searchResults->clear();
        for (const auto& result : results) {
            QString item = QString("%1:%2 (%.2f) - %3")
                .arg(result.filePath)
                .arg(result.lineNumber)
                .arg(result.relevanceScore)
                .arg(result.matchReason);
            m_searchResults->addItem(item);
        }
    }
}

void CursorWorkflowWidget::onAnswerQuestion()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    QString question = m_questionInput->toPlainText();
    QString answer = m_orchestrator->answerCodebaseQuestion(question);
    
    if (m_questionOutput) {
        m_questionOutput->setPlainText(answer);
    }
}

void CursorWorkflowWidget::onGenerateMigration()
{
    // TODO: Implement migration plan generation UI
}

// ============================================================
// PHASE 4 SLOTS
// ============================================================

void CursorWorkflowWidget::onStartCollaborativeSession()
{
    if (!m_orchestrator || !m_orchestrator->isReady()) {
        return;
    }
    
    CursorWorkflowOrchestrator::CollaborativeSession session;
    session.sessionId = m_sessionIdInput->text();
    
    QStringList participants = m_participantsInput->text().split(",", Qt::SkipEmptyParts);
    for (const auto& p : participants) {
        session.participants.append(p.trimmed());
    }
    
    m_orchestrator->startCollaborativeSession(session);
    
    if (m_collaboratorsList) {
        m_collaboratorsList->clear();
        for (const auto& p : session.participants) {
            m_collaboratorsList->addItem(p);
        }
    }
}

void CursorWorkflowWidget::onToggleLiveFeatures()
{
    if (!m_orchestrator) {
        return;
    }
    
    // Toggle live explanations and suggestions
    static bool enabled = false;
    enabled = !enabled;
    
    m_orchestrator->enableLiveCodeExplanation(enabled);
    m_orchestrator->enableLiveSuggestions(enabled);
    
    qDebug() << "Live features:" << (enabled ? "enabled" : "disabled");
}

} // namespace RawrXD
