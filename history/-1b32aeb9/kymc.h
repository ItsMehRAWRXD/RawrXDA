#pragma once

#include "cursor_workflow_orchestrator.h"
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>

namespace RawrXD {

/**
 * @brief UI widget for Cursor + GitHub Copilot workflow demo
 * 
 * Provides interactive interface for all workflow features:
 * - Inline completion testing
 * - Cmd+K command execution
 * - PR review
 * - Security scanning
 * - Multi-file operations
 */
class CursorWorkflowWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CursorWorkflowWidget(CursorWorkflowOrchestrator* orchestrator, QWidget* parent = nullptr);
    ~CursorWorkflowWidget() override;

private slots:
    // Phase 1: Completion
    void onRequestCompletion();
    void onCompletionReady(const QString& completion, int latencyMs);
    
    // Phase 1: Cmd+K
    void onExecuteCmdK();
    void onCmdKProgress(const QString& status, int percentage);
    void onCmdKComplete(const QString& result);
    
    // Phase 2: PR Review
    void onReviewPR();
    void onPRReviewProgress(const QString& status, int percentage);
    void onPRReviewComplete(const CursorWorkflowOrchestrator::PRReviewResult& result);
    
    // Phase 2: Security Scan
    void onScanSecurity();
    void onSecurityScanComplete(const QVector<CursorWorkflowOrchestrator::SecurityVulnerability>& vulns);
    
    // Phase 3: Codebase Operations
    void onRefactorCodebase();
    void onSemanticSearch();
    void onAnswerQuestion();
    void onGenerateMigration();
    
    // Phase 4: Collaborative
    void onStartCollaborativeSession();
    void onToggleLiveFeatures();

private:
    void setupUI();
    void createPhase1Tab();
    void createPhase2Tab();
    void createPhase3Tab();
    void createPhase4Tab();
    
    CursorWorkflowOrchestrator* m_orchestrator;
    
    // Phase 1 widgets
    QTextEdit* m_codeInput;
    QLineEdit* m_languageInput;
    QTextEdit* m_completionOutput;
    QLabel* m_latencyLabel;
    QComboBox* m_cmdKCommandCombo;
    QTextEdit* m_cmdKInput;
    QTextEdit* m_cmdKOutput;
    QProgressBar* m_cmdKProgress;
    
    // Phase 2 widgets
    QTextEdit* m_prDiffInput;
    QLineEdit* m_prTitleInput;
    QTextEdit* m_prReviewOutput;
    QListWidget* m_securityVulnsList;
    QProgressBar* m_prProgress;
    
    // Phase 3 widgets
    QTextEdit* m_refactorInput;
    QComboBox* m_refactorScope;
    QTextEdit* m_refactorOutput;
    QLineEdit* m_searchQuery;
    QListWidget* m_searchResults;
    QTextEdit* m_questionInput;
    QTextEdit* m_questionOutput;
    
    // Phase 4 widgets
    QLineEdit* m_sessionIdInput;
    QLineEdit* m_participantsInput;
    QListWidget* m_collaboratorsList;
    QListWidget* m_liveSuggestionsList;
};

} // namespace RawrXD
