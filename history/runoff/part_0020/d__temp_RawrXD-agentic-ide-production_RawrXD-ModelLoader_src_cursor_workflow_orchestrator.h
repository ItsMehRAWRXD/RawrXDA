#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <functional>

// Forward declarations
class AIIntegrationHub;
class AgenticExecutor;
class GitHubCopilotIntegration;
class RealTimeCompletionEngine;

namespace RawrXD {

/**
 * @brief Comprehensive Cursor + GitHub Copilot workflow orchestrator
 * 
 * This class provides the complete enterprise development experience:
 * - Cursor-style inline completion (<50ms latency)
 * - 50+ Cmd+K refactoring commands
 * - Multi-file agentic workflows
 * - GitHub Copilot PR review, issue→code, security scanning
 * - Real-time collaborative AI
 * 
 * Integrates with existing 7 AI systems and extreme compression for
 * superior local performance vs cloud solutions.
 */
class CursorWorkflowOrchestrator : public QObject
{
    Q_OBJECT

public:
    explicit CursorWorkflowOrchestrator(QObject* parent = nullptr);
    ~CursorWorkflowOrchestrator() override;

    // ============================================================
    // INITIALIZATION
    // ============================================================
    void initialize(
        AIIntegrationHub* hub,
        AgenticExecutor* executor,
        GitHubCopilotIntegration* copilot,
        RealTimeCompletionEngine* completionEngine
    );
    bool isReady() const;

    // ============================================================
    // PHASE 1: CORE CURSOR-STYLE WORKFLOWS
    // ============================================================
    
    /**
     * @brief Inline completion with sub-50ms latency
     * @param context Current code context (file, cursor position, surrounding code)
     * @param maxTokens Maximum tokens to generate
     * @return Completion suggestion
     */
    struct CompletionContext {
        QString filePath;
        int cursorLine;
        int cursorColumn;
        QString textBeforeCursor;
        QString textAfterCursor;
        QString fileLanguage;
        QVector<QString> recentEdits;  // For learning user patterns
    };
    
    QString requestInlineCompletion(const CompletionContext& context, int maxTokens = 128);
    
    /**
     * @brief Cmd+K command execution (50+ refactoring commands)
     */
    enum class CmdKCommand {
        // Code generation
        GenerateFunction,
        GenerateClass,
        GenerateTest,
        GenerateDocstring,
        GenerateTypeHints,
        
        // Refactoring
        ExtractMethod,
        ExtractVariable,
        RenameSymbol,
        InlineVariable,
        ChangeSignature,
        
        // Transformation
        ConvertToAsync,
        AddErrorHandling,
        SimplifyExpression,
        OptimizePerformance,
        ModernizeSyntax,
        
        // Analysis
        ExplainCode,
        FindBugs,
        SuggestImprovements,
        CheckSecurity,
        AnalyzeComplexity,
        
        // Multi-file operations
        RefactorAcrossFiles,
        UpdateAllReferences,
        GenerateBoilerplate,
        CreateInterface,
        ImplementInterface,
        
        // AI-powered
        FixWithAI,
        RefactorWithAI,
        OptimizeWithAI,
        DocumentWithAI,
        TestWithAI,
        
        // Custom
        CustomCommand
    };
    
    struct CmdKRequest {
        CmdKCommand command;
        QString selectedText;
        QString filePath;
        int startLine;
        int endLine;
        QJsonObject additionalParams;
        QString customPrompt;  // For CustomCommand
    };
    
    QString executeCmdKCommand(const CmdKRequest& request);
    
    /**
     * @brief Multi-cursor editing with AI assistance
     */
    struct MultiCursorEdit {
        int line;
        int column;
        QString operation;  // "insert", "replace", "delete"
        QString text;
    };
    
    QVector<MultiCursorEdit> generateMultiCursorEdits(
        const QVector<QPair<int, int>>& cursorPositions,
        const QString& intent
    );

    // ============================================================
    // PHASE 2: GITHUB COPILOT ENTERPRISE FEATURES
    // ============================================================
    
    /**
     * @brief PR review with AI-powered analysis
     */
    struct PRReviewRequest {
        QString prTitle;
        QString prDescription;
        QString diffText;
        QVector<QString> changedFiles;
        QString baseBranch;
        QString headBranch;
        QMap<QString, QString> fileContents;  // filename → full content
    };
    
    struct PRReviewResult {
        QString overallAssessment;
        QVector<ReviewComment> inlineComments;
        QVector<QString> securityIssues;
        QVector<QString> performanceIssues;
        QVector<QString> bestPracticeViolations;
        int qualityScore;  // 0-100
        QString suggestedImprovements;
    };
    
    struct ReviewComment {
        QString filePath;
        int lineNumber;
        QString severity;  // "info", "warning", "error", "critical"
        QString category;  // "security", "performance", "maintainability", "style"
        QString message;
        QString suggestedFix;
    };
    
    PRReviewResult reviewPullRequest(const PRReviewRequest& request);
    
    /**
     * @brief Issue → Code: Generate implementation from issue description
     */
    struct IssueImplementationRequest {
        QString issueTitle;
        QString issueDescription;
        QVector<QString> acceptanceCriteria;
        QString codebaseContext;  // Relevant existing code
        QString targetDirectory;
    };
    
    struct IssueImplementationResult {
        QMap<QString, QString> generatedFiles;  // filename → content
        QMap<QString, QString> modifiedFiles;   // filename → new content
        QString implementationPlan;
        QString testPlan;
        QVector<QString> dependencies;
    };
    
    IssueImplementationResult implementIssue(const IssueImplementationRequest& request);
    
    /**
     * @brief Security scanning with AI-powered vulnerability detection
     */
    struct SecurityScanRequest {
        QVector<QString> filePaths;
        QString scanScope;  // "changed_files", "all_files", "dependencies"
        bool includeThirdParty;
    };
    
    struct SecurityVulnerability {
        QString filePath;
        int lineNumber;
        QString severity;  // "critical", "high", "medium", "low"
        QString cweId;
        QString category;
        QString description;
        QString recommendation;
        QString codeSnippet;
    };
    
    QVector<SecurityVulnerability> scanSecurity(const SecurityScanRequest& request);
    
    /**
     * @brief Intelligent commit message generation
     */
    QString generateCommitMessage(
        const QVector<QString>& changedFiles,
        const QString& diff,
        bool useConventionalCommits = true
    );

    // ============================================================
    // PHASE 3: ADVANCED CURSOR-STYLE AGENTIC FEATURES
    // ============================================================
    
    /**
     * @brief Multi-file refactoring across entire codebase
     */
    struct CodebaseRefactoringRequest {
        QString intent;  // e.g., "Extract payment logic into service layer"
        QVector<QString> affectedFiles;
        QString refactoringScope;  // "file", "module", "codebase"
        bool includeTests;
        bool updateDocumentation;
    };
    
    struct CodebaseRefactoringResult {
        QMap<QString, QString> fileChanges;  // filename → new content
        QVector<QString> newFiles;
        QVector<QString> deletedFiles;
        QString refactoringReport;
        int filesAffected;
        int linesChanged;
    };
    
    CodebaseRefactoringResult refactorCodebase(const CodebaseRefactoringRequest& request);
    
    /**
     * @brief Intelligent code search with semantic understanding
     */
    struct SemanticSearchRequest {
        QString query;  // Natural language or code snippet
        QVector<QString> filePatterns;  // e.g., "*.cpp", "src/**/*.h"
        bool includeTests;
        int maxResults;
    };
    
    struct SemanticSearchResult {
        QString filePath;
        int lineNumber;
        QString codeSnippet;
        QString matchReason;
        float relevanceScore;  // 0.0 - 1.0
    };
    
    QVector<SemanticSearchResult> searchCodeSemantically(const SemanticSearchRequest& request);
    
    /**
     * @brief Codebase-aware Q&A
     */
    QString answerCodebaseQuestion(
        const QString& question,
        const QVector<QString>& relevantFiles = {}
    );
    
    /**
     * @brief Generate migration plan for major refactors
     */
    struct MigrationRequest {
        QString from;  // e.g., "REST API" or "Class-based components"
        QString to;    // e.g., "GraphQL" or "Functional components"
        QVector<QString> scopeFiles;
    };
    
    struct MigrationPlan {
        QVector<QString> phases;
        QMap<QString, QVector<QString>> phaseSteps;  // phase → steps
        QMap<QString, QString> fileTransformations;  // file → transformation plan
        QString riskAssessment;
        int estimatedHours;
    };
    
    MigrationPlan generateMigrationPlan(const MigrationRequest& request);

    // ============================================================
    // PHASE 4: REAL-TIME COLLABORATIVE AI
    // ============================================================
    
    /**
     * @brief Shared coding session with AI assistance
     */
    struct CollaborativeSession {
        QString sessionId;
        QVector<QString> participants;
        QString sharedContext;
        QVector<QString> sharedFiles;
    };
    
    void startCollaborativeSession(const CollaborativeSession& session);
    void endCollaborativeSession(const QString& sessionId);
    
    /**
     * @brief Real-time code explanation as teammates type
     */
    void enableLiveCodeExplanation(bool enabled);
    
    /**
     * @brief Suggest improvements in real-time
     */
    struct LiveSuggestion {
        QString filePath;
        int lineNumber;
        QString suggestionType;  // "refactor", "optimize", "fix", "improve"
        QString suggestion;
        QString reasoning;
        int confidenceScore;  // 0-100
    };
    
    void enableLiveSuggestions(bool enabled, int sensitivityLevel = 50);

signals:
    // Completion signals
    void completionReady(const QString& completion, int latencyMs);
    void completionCancelled();
    
    // Command signals
    void cmdKCommandStarted(const QString& commandName);
    void cmdKCommandProgress(const QString& status, int percentage);
    void cmdKCommandComplete(const QString& result);
    void cmdKCommandError(const QString& error);
    
    // PR review signals
    void prReviewProgress(const QString& status, int percentage);
    void prReviewComplete(const PRReviewResult& result);
    
    // Security signals
    void securityScanProgress(const QString& status, int percentage);
    void securityScanComplete(const QVector<SecurityVulnerability>& vulnerabilities);
    void criticalVulnerabilityFound(const SecurityVulnerability& vuln);
    
    // Collaborative signals
    void collaboratorJoined(const QString& sessionId, const QString& participantName);
    void collaboratorLeft(const QString& sessionId, const QString& participantName);
    void liveSuggestionAvailable(const LiveSuggestion& suggestion);
    
    // General signals
    void workflowProgress(const QString& workflow, const QString& status, int percentage);
    void workflowError(const QString& workflow, const QString& error);

private:
    // Helper methods
    QString buildPromptForCommand(const CmdKRequest& request);
    QString buildPRReviewPrompt(const PRReviewRequest& request);
    QString buildSecurityScanPrompt(const QVector<QString>& files);
    QVector<ReviewComment> parseReviewComments(const QString& aiResponse);
    QVector<SecurityVulnerability> parseSecurityResults(const QString& aiResponse);
    
    // Context management
    QString gatherCodebaseContext(const QVector<QString>& files);
    QString extractRelevantContext(const QString& query, int maxTokens = 8000);
    
    // Performance optimization
    void cacheCompletion(const QString& key, const QString& completion);
    QString getCachedCompletion(const QString& key);
    void pruneCompletionCache();
    
    // Latency monitoring
    void startLatencyTimer(const QString& operation);
    int stopLatencyTimer(const QString& operation);

private:
    // Core components
    AIIntegrationHub* m_hub{nullptr};
    AgenticExecutor* m_executor{nullptr};
    GitHubCopilotIntegration* m_copilot{nullptr};
    RealTimeCompletionEngine* m_completionEngine{nullptr};
    
    // State
    bool m_liveExplanationsEnabled{false};
    bool m_liveSuggestionsEnabled{false};
    int m_suggestionSensitivity{50};
    
    // Active sessions
    QMap<QString, CollaborativeSession> m_activeSessions;
    
    // Caching
    QMap<QString, QString> m_completionCache;
    QMap<QString, qint64> m_cacheTimestamps;
    static constexpr int MAX_CACHE_SIZE = 1000;
    static constexpr int CACHE_TTL_MS = 300000;  // 5 minutes
    
    // Performance tracking
    QMap<QString, qint64> m_operationStartTimes;
};

} // namespace RawrXD
