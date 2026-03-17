#include "cursor_workflow_orchestrator.h"
#include "ai_integration_hub.h"
#include "agentic_executor.h"
#include "github_copilot_integration.h"
#include "real_time_completion_engine.h"
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>

namespace RawrXD {

CursorWorkflowOrchestrator::CursorWorkflowOrchestrator(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[CursorWorkflow] Orchestrator created";
}

CursorWorkflowOrchestrator::~CursorWorkflowOrchestrator() = default;

void CursorWorkflowOrchestrator::initialize(
    AIIntegrationHub* hub,
    AgenticExecutor* executor,
    GitHubCopilotIntegration* copilot,
    RealTimeCompletionEngine* completionEngine)
{
    m_hub = hub;
    m_executor = executor;
    m_copilot = copilot;
    m_completionEngine = completionEngine;
    
    qDebug() << "[CursorWorkflow] Initialized with all components";
}

bool CursorWorkflowOrchestrator::isReady() const
{
    return m_hub != nullptr && m_executor != nullptr && 
           m_copilot != nullptr && m_completionEngine != nullptr;
}

// ============================================================
// PHASE 1: CORE CURSOR-STYLE WORKFLOWS
// ============================================================

QString CursorWorkflowOrchestrator::requestInlineCompletion(
    const CompletionContext& context, int maxTokens)
{
    if (!isReady()) {
        qWarning() << "[CursorWorkflow] Not ready for inline completion";
        return QString();
    }
    
    startLatencyTimer("inline_completion");
    
    // Check cache first for sub-5ms response
    QString cacheKey = QString("%1:%2:%3").arg(
        context.filePath,
        QString::number(context.cursorLine),
        context.textBeforeCursor.right(100)  // Last 100 chars as key
    );
    
    QString cached = getCachedCompletion(cacheKey);
    if (!cached.isEmpty()) {
        int latency = stopLatencyTimer("inline_completion");
        emit completionReady(cached, latency);
        qDebug() << "[CursorWorkflow] Cache hit, latency:" << latency << "ms";
        return cached;
    }
    
    // Use real-time completion engine for <50ms latency
    if (m_completionEngine) {
        QString completion = m_completionEngine->getCompletion(
            context.textBeforeCursor,
            context.textAfterCursor,
            context.fileLanguage,
            maxTokens
        );
        
        int latency = stopLatencyTimer("inline_completion");
        
        // Cache for future use
        cacheCompletion(cacheKey, completion);
        
        emit completionReady(completion, latency);
        qDebug() << "[CursorWorkflow] Completion generated, latency:" << latency << "ms";
        return completion;
    }
    
    // Fallback to standard AI hub
    QJsonObject prompt;
    prompt["type"] = "code_completion";
    prompt["language"] = context.fileLanguage;
    prompt["before_cursor"] = context.textBeforeCursor;
    prompt["after_cursor"] = context.textAfterCursor;
    prompt["max_tokens"] = maxTokens;
    
    auto result = m_hub->routeRequest(prompt);
    QString completion = result.value("completion").toString();
    
    int latency = stopLatencyTimer("inline_completion");
    cacheCompletion(cacheKey, completion);
    
    emit completionReady(completion, latency);
    qDebug() << "[CursorWorkflow] Fallback completion, latency:" << latency << "ms";
    return completion;
}

QString CursorWorkflowOrchestrator::executeCmdKCommand(const CmdKRequest& request)
{
    if (!isReady()) {
        emit cmdKCommandError("Workflow orchestrator not ready");
        return QString();
    }
    
    emit cmdKCommandStarted(QString::number(static_cast<int>(request.command)));
    emit cmdKCommandProgress("Building prompt...", 10);
    
    QString prompt = buildPromptForCommand(request);
    
    emit cmdKCommandProgress("Executing AI agent...", 30);
    
    // Route through agentic executor for complex reasoning
    auto result = m_executor->executeUserRequest(prompt);
    
    emit cmdKCommandProgress("Processing result...", 80);
    
    QString output;
    if (result.contains("output")) {
        output = result["output"].toString();
    } else if (result.contains("error")) {
        emit cmdKCommandError(result["error"].toString());
        return QString();
    }
    
    emit cmdKCommandProgress("Complete", 100);
    emit cmdKCommandComplete(output);
    
    return output;
}

QVector<CursorWorkflowOrchestrator::MultiCursorEdit> 
CursorWorkflowOrchestrator::generateMultiCursorEdits(
    const QVector<QPair<int, int>>& cursorPositions,
    const QString& intent)
{
    QVector<MultiCursorEdit> edits;
    
    if (!isReady()) {
        return edits;
    }
    
    // Build context for multi-cursor operation
    QJsonObject context;
    context["intent"] = intent;
    
    QJsonArray positions;
    for (const auto& pos : cursorPositions) {
        QJsonObject posObj;
        posObj["line"] = pos.first;
        posObj["column"] = pos.second;
        positions.append(posObj);
    }
    context["cursor_positions"] = positions;
    
    // Use AI to generate synchronized edits
    QString prompt = QString(
        "Generate synchronized edits for multiple cursor positions.\n"
        "Intent: %1\n"
        "Positions: %2\n"
        "Provide edits in format: line:column:operation:text"
    ).arg(intent, QString::number(cursorPositions.size()));
    
    auto result = m_executor->executeUserRequest(prompt);
    
    if (result.contains("output")) {
        QString output = result["output"].toString();
        
        // Parse the edits
        QRegularExpression regex(R"((\d+):(\d+):(\w+):(.+))");
        auto matches = regex.globalMatch(output);
        
        while (matches.hasNext()) {
            auto match = matches.next();
            MultiCursorEdit edit;
            edit.line = match.captured(1).toInt();
            edit.column = match.captured(2).toInt();
            edit.operation = match.captured(3);
            edit.text = match.captured(4);
            edits.append(edit);
        }
    }
    
    return edits;
}

// ============================================================
// PHASE 2: GITHUB COPILOT ENTERPRISE FEATURES
// ============================================================

CursorWorkflowOrchestrator::PRReviewResult 
CursorWorkflowOrchestrator::reviewPullRequest(const PRReviewRequest& request)
{
    PRReviewResult result;
    
    if (!isReady()) {
        emit prReviewProgress("Error: Not ready", 0);
        return result;
    }
    
    emit prReviewProgress("Starting PR review...", 5);
    
    QString prompt = buildPRReviewPrompt(request);
    
    emit prReviewProgress("Analyzing diff...", 20);
    
    // Security scan first
    emit prReviewProgress("Scanning for security issues...", 30);
    SecurityScanRequest secScan;
    secScan.filePaths = request.changedFiles;
    secScan.scanScope = "changed_files";
    auto vulnerabilities = scanSecurity(secScan);
    
    for (const auto& vuln : vulnerabilities) {
        result.securityIssues.append(
            QString("[%1] %2 at %3:%4")
                .arg(vuln.severity, vuln.category, vuln.filePath)
                .arg(vuln.lineNumber)
        );
    }
    
    emit prReviewProgress("Analyzing code quality...", 50);
    
    // Use executor for comprehensive review
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    emit prReviewProgress("Generating review comments...", 70);
    
    if (aiResult.contains("output")) {
        QString aiResponse = aiResult["output"].toString();
        
        // Parse AI response
        result.inlineComments = parseReviewComments(aiResponse);
        
        // Extract overall assessment
        QRegularExpression assessmentRegex(R"(ASSESSMENT:\s*(.+?)(?:\n\n|$))", 
            QRegularExpression::DotMatchesEverythingOption);
        auto match = assessmentRegex.match(aiResponse);
        if (match.hasMatch()) {
            result.overallAssessment = match.captured(1).trimmed();
        }
        
        // Extract quality score
        QRegularExpression scoreRegex(R"(SCORE:\s*(\d+))");
        match = scoreRegex.match(aiResponse);
        if (match.hasMatch()) {
            result.qualityScore = match.captured(1).toInt();
        } else {
            result.qualityScore = 75;  // Default
        }
    }
    
    emit prReviewProgress("Review complete", 100);
    emit prReviewComplete(result);
    
    return result;
}

CursorWorkflowOrchestrator::IssueImplementationResult 
CursorWorkflowOrchestrator::implementIssue(const IssueImplementationRequest& request)
{
    IssueImplementationResult result;
    
    if (!isReady()) {
        return result;
    }
    
    emit workflowProgress("issue_implementation", "Analyzing issue...", 10);
    
    QString prompt = QString(
        "Implement the following feature/issue:\n\n"
        "Title: %1\n"
        "Description: %2\n\n"
        "Acceptance Criteria:\n%3\n\n"
        "Codebase Context:\n%4\n\n"
        "Generate:\n"
        "1. Implementation plan\n"
        "2. Code files (with full content)\n"
        "3. Test files\n"
        "4. Documentation updates\n"
    ).arg(
        request.issueTitle,
        request.issueDescription,
        request.acceptanceCriteria.join("\n- "),
        request.codebaseContext
    );
    
    emit workflowProgress("issue_implementation", "Generating implementation plan...", 30);
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    emit workflowProgress("issue_implementation", "Creating files...", 60);
    
    if (aiResult.contains("output")) {
        QString output = aiResult["output"].toString();
        
        // Parse implementation plan
        QRegularExpression planRegex(R"(IMPLEMENTATION_PLAN:\s*(.+?)(?:\n\n|FILE:))", 
            QRegularExpression::DotMatchesEverythingOption);
        auto match = planRegex.match(output);
        if (match.hasMatch()) {
            result.implementationPlan = match.captured(1).trimmed();
        }
        
        // Parse generated files
        QRegularExpression fileRegex(R"(FILE:\s*(.+?)\s*```(?:\w+)?\s*(.+?)```)", 
            QRegularExpression::DotMatchesEverythingOption);
        auto matches = fileRegex.globalMatch(output);
        
        while (matches.hasNext()) {
            match = matches.next();
            QString filename = match.captured(1).trimmed();
            QString content = match.captured(2).trimmed();
            result.generatedFiles[filename] = content;
        }
    }
    
    emit workflowProgress("issue_implementation", "Complete", 100);
    
    return result;
}

QVector<CursorWorkflowOrchestrator::SecurityVulnerability> 
CursorWorkflowOrchestrator::scanSecurity(const SecurityScanRequest& request)
{
    QVector<SecurityVulnerability> vulnerabilities;
    
    if (!isReady()) {
        return vulnerabilities;
    }
    
    emit securityScanProgress("Initializing scan...", 5);
    
    QString prompt = buildSecurityScanPrompt(request.filePaths);
    
    emit securityScanProgress("Analyzing code for vulnerabilities...", 30);
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    emit securityScanProgress("Processing results...", 80);
    
    if (aiResult.contains("output")) {
        vulnerabilities = parseSecurityResults(aiResult["output"].toString());
        
        // Emit critical vulnerabilities immediately
        for (const auto& vuln : vulnerabilities) {
            if (vuln.severity == "critical") {
                emit criticalVulnerabilityFound(vuln);
            }
        }
    }
    
    emit securityScanProgress("Scan complete", 100);
    emit securityScanComplete(vulnerabilities);
    
    return vulnerabilities;
}

QString CursorWorkflowOrchestrator::generateCommitMessage(
    const QVector<QString>& changedFiles,
    const QString& diff,
    bool useConventionalCommits)
{
    if (!isReady() || !m_copilot) {
        return "Update files";
    }
    
    return m_copilot->generateCommitMessage(changedFiles, diff);
}

// ============================================================
// PHASE 3: ADVANCED CURSOR-STYLE AGENTIC FEATURES
// ============================================================

CursorWorkflowOrchestrator::CodebaseRefactoringResult 
CursorWorkflowOrchestrator::refactorCodebase(const CodebaseRefactoringRequest& request)
{
    CodebaseRefactoringResult result;
    
    if (!isReady()) {
        return result;
    }
    
    emit workflowProgress("codebase_refactoring", "Analyzing codebase...", 10);
    
    QString context = gatherCodebaseContext(request.affectedFiles);
    
    QString prompt = QString(
        "Refactor the following codebase according to this intent:\n\n"
        "%1\n\n"
        "Scope: %2\n"
        "Include tests: %3\n"
        "Update docs: %4\n\n"
        "Codebase Context:\n%5\n\n"
        "Provide:\n"
        "1. List of file changes\n"
        "2. New file content for each changed file\n"
        "3. List of new files to create\n"
        "4. List of files to delete\n"
        "5. Refactoring report\n"
    ).arg(
        request.intent,
        request.refactoringScope,
        request.includeTests ? "Yes" : "No",
        request.updateDocumentation ? "Yes" : "No",
        context
    );
    
    emit workflowProgress("codebase_refactoring", "Planning refactoring...", 30);
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    emit workflowProgress("codebase_refactoring", "Applying changes...", 70);
    
    if (aiResult.contains("output")) {
        QString output = aiResult["output"].toString();
        
        // Parse refactoring report
        QRegularExpression reportRegex(R"(REPORT:\s*(.+?)(?:\n\n|FILE_CHANGES:))", 
            QRegularExpression::DotMatchesEverythingOption);
        auto match = reportRegex.match(output);
        if (match.hasMatch()) {
            result.refactoringReport = match.captured(1).trimmed();
        }
        
        // Parse file changes
        QRegularExpression fileRegex(R"(CHANGED_FILE:\s*(.+?)\s*```(?:\w+)?\s*(.+?)```)", 
            QRegularExpression::DotMatchesEverythingOption);
        auto matches = fileRegex.globalMatch(output);
        
        while (matches.hasNext()) {
            match = matches.next();
            QString filename = match.captured(1).trimmed();
            QString content = match.captured(2).trimmed();
            result.fileChanges[filename] = content;
            result.filesAffected++;
        }
    }
    
    emit workflowProgress("codebase_refactoring", "Complete", 100);
    
    return result;
}

QVector<CursorWorkflowOrchestrator::SemanticSearchResult> 
CursorWorkflowOrchestrator::searchCodeSemantically(const SemanticSearchRequest& request)
{
    QVector<SemanticSearchResult> results;
    
    if (!isReady()) {
        return results;
    }
    
    QString prompt = QString(
        "Search the codebase semantically for: %1\n\n"
        "File patterns: %2\n"
        "Max results: %3\n\n"
        "Return results in format:\n"
        "FILE: <path>\n"
        "LINE: <number>\n"
        "CODE: <snippet>\n"
        "REASON: <why it matches>\n"
        "SCORE: <0.0-1.0>\n"
    ).arg(
        request.query,
        request.filePatterns.join(", "),
        QString::number(request.maxResults)
    );
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    if (aiResult.contains("output")) {
        QString output = aiResult["output"].toString();
        
        // Parse search results
        QRegularExpression resultRegex(
            R"(FILE:\s*(.+?)\s*LINE:\s*(\d+)\s*CODE:\s*(.+?)\s*REASON:\s*(.+?)\s*SCORE:\s*([\d.]+))", 
            QRegularExpression::DotMatchesEverythingOption
        );
        auto matches = resultRegex.globalMatch(output);
        
        while (matches.hasNext() && results.size() < request.maxResults) {
            auto match = matches.next();
            SemanticSearchResult result;
            result.filePath = match.captured(1).trimmed();
            result.lineNumber = match.captured(2).toInt();
            result.codeSnippet = match.captured(3).trimmed();
            result.matchReason = match.captured(4).trimmed();
            result.relevanceScore = match.captured(5).toFloat();
            results.append(result);
        }
    }
    
    return results;
}

QString CursorWorkflowOrchestrator::answerCodebaseQuestion(
    const QString& question,
    const QVector<QString>& relevantFiles)
{
    if (!isReady()) {
        return "Unable to answer: system not ready";
    }
    
    QString context = relevantFiles.isEmpty() ? 
        QString() : gatherCodebaseContext(relevantFiles);
    
    QString prompt = QString(
        "Answer the following question about the codebase:\n\n"
        "%1\n\n"
        "Relevant code context:\n%2\n\n"
        "Provide a clear, detailed answer with code examples where appropriate."
    ).arg(question, context);
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    return aiResult.value("output").toString();
}

CursorWorkflowOrchestrator::MigrationPlan 
CursorWorkflowOrchestrator::generateMigrationPlan(const MigrationRequest& request)
{
    MigrationPlan plan;
    
    if (!isReady()) {
        return plan;
    }
    
    QString context = gatherCodebaseContext(request.scopeFiles);
    
    QString prompt = QString(
        "Generate a migration plan:\n\n"
        "From: %1\n"
        "To: %2\n\n"
        "Scope:\n%3\n\n"
        "Provide:\n"
        "1. Migration phases (high-level steps)\n"
        "2. Detailed steps for each phase\n"
        "3. File transformation plans\n"
        "4. Risk assessment\n"
        "5. Time estimate (in hours)\n"
    ).arg(request.from, request.to, context);
    
    auto aiResult = m_executor->executeUserRequest(prompt);
    
    if (aiResult.contains("output")) {
        QString output = aiResult["output"].toString();
        
        // Parse phases
        QRegularExpression phaseRegex(R"(PHASE:\s*(.+?)\s*(?:STEPS:|PHASE:|$))", 
            QRegularExpression::DotMatchesEverythingOption);
        auto matches = phaseRegex.globalMatch(output);
        
        while (matches.hasNext()) {
            auto match = matches.next();
            plan.phases.append(match.captured(1).trimmed());
        }
        
        // Parse risk assessment
        QRegularExpression riskRegex(R"(RISK_ASSESSMENT:\s*(.+?)(?:\n\n|ESTIMATE:))", 
            QRegularExpression::DotMatchesEverythingOption);
        auto match = riskRegex.match(output);
        if (match.hasMatch()) {
            plan.riskAssessment = match.captured(1).trimmed();
        }
        
        // Parse time estimate
        QRegularExpression timeRegex(R"(ESTIMATE:\s*(\d+)\s*hours?)");
        match = timeRegex.match(output);
        if (match.hasMatch()) {
            plan.estimatedHours = match.captured(1).toInt();
        }
    }
    
    return plan;
}

// ============================================================
// PHASE 4: REAL-TIME COLLABORATIVE AI
// ============================================================

void CursorWorkflowOrchestrator::startCollaborativeSession(const CollaborativeSession& session)
{
    m_activeSessions[session.sessionId] = session;
    
    qDebug() << "[CursorWorkflow] Started collaborative session:" << session.sessionId;
    
    for (const auto& participant : session.participants) {
        emit collaboratorJoined(session.sessionId, participant);
    }
}

void CursorWorkflowOrchestrator::endCollaborativeSession(const QString& sessionId)
{
    if (m_activeSessions.contains(sessionId)) {
        auto session = m_activeSessions.take(sessionId);
        qDebug() << "[CursorWorkflow] Ended collaborative session:" << sessionId;
    }
}

void CursorWorkflowOrchestrator::enableLiveCodeExplanation(bool enabled)
{
    m_liveExplanationsEnabled = enabled;
    qDebug() << "[CursorWorkflow] Live code explanation:" << (enabled ? "enabled" : "disabled");
}

void CursorWorkflowOrchestrator::enableLiveSuggestions(bool enabled, int sensitivityLevel)
{
    m_liveSuggestionsEnabled = enabled;
    m_suggestionSensitivity = sensitivityLevel;
    qDebug() << "[CursorWorkflow] Live suggestions:" << (enabled ? "enabled" : "disabled")
             << "sensitivity:" << sensitivityLevel;
}

// ============================================================
// HELPER METHODS
// ============================================================

QString CursorWorkflowOrchestrator::buildPromptForCommand(const CmdKRequest& request)
{
    QString commandName;
    QString commandIntent;
    
    switch (request.command) {
        case CmdKCommand::GenerateFunction:
            commandName = "Generate Function";
            commandIntent = "Generate a complete function implementation";
            break;
        case CmdKCommand::ExplainCode:
            commandName = "Explain Code";
            commandIntent = "Provide a clear explanation of the selected code";
            break;
        case CmdKCommand::RefactorWithAI:
            commandName = "Refactor with AI";
            commandIntent = "Intelligently refactor the code for better quality";
            break;
        case CmdKCommand::CustomCommand:
            commandName = "Custom Command";
            commandIntent = request.customPrompt;
            break;
        default:
            commandName = "Generic Command";
            commandIntent = "Process the selected code";
    }
    
    return QString(
        "Command: %1\n"
        "Intent: %2\n\n"
        "Selected Code:\n```\n%3\n```\n\n"
        "File: %4\n"
        "Lines: %5-%6\n"
    ).arg(
        commandName,
        commandIntent,
        request.selectedText,
        request.filePath,
        QString::number(request.startLine),
        QString::number(request.endLine)
    );
}

QString CursorWorkflowOrchestrator::buildPRReviewPrompt(const PRReviewRequest& request)
{
    return QString(
        "Review this pull request:\n\n"
        "Title: %1\n"
        "Description: %2\n\n"
        "Diff:\n%3\n\n"
        "Files changed: %4\n\n"
        "Provide:\n"
        "1. Overall assessment\n"
        "2. Inline comments (FILE:LINE: comment)\n"
        "3. Security issues\n"
        "4. Performance concerns\n"
        "5. Quality score (0-100)\n"
        "6. Suggested improvements\n"
    ).arg(
        request.prTitle,
        request.prDescription,
        request.diffText,
        request.changedFiles.join(", ")
    );
}

QString CursorWorkflowOrchestrator::buildSecurityScanPrompt(const QVector<QString>& files)
{
    return QString(
        "Scan the following files for security vulnerabilities:\n\n"
        "%1\n\n"
        "Report format:\n"
        "FILE: <path>\n"
        "LINE: <number>\n"
        "SEVERITY: critical|high|medium|low\n"
        "CWE: <id>\n"
        "CATEGORY: <type>\n"
        "DESCRIPTION: <details>\n"
        "RECOMMENDATION: <fix>\n"
    ).arg(files.join("\n"));
}

QVector<CursorWorkflowOrchestrator::ReviewComment> 
CursorWorkflowOrchestrator::parseReviewComments(const QString& aiResponse)
{
    QVector<ReviewComment> comments;
    
    QRegularExpression commentRegex(
        R"(FILE:\s*(.+?)\s*LINE:\s*(\d+)\s*SEVERITY:\s*(\w+)\s*CATEGORY:\s*(.+?)\s*MESSAGE:\s*(.+?)(?:\n\n|$))", 
        QRegularExpression::DotMatchesEverythingOption
    );
    
    auto matches = commentRegex.globalMatch(aiResponse);
    while (matches.hasNext()) {
        auto match = matches.next();
        ReviewComment comment;
        comment.filePath = match.captured(1).trimmed();
        comment.lineNumber = match.captured(2).toInt();
        comment.severity = match.captured(3).trimmed();
        comment.category = match.captured(4).trimmed();
        comment.message = match.captured(5).trimmed();
        comments.append(comment);
    }
    
    return comments;
}

QVector<CursorWorkflowOrchestrator::SecurityVulnerability> 
CursorWorkflowOrchestrator::parseSecurityResults(const QString& aiResponse)
{
    QVector<SecurityVulnerability> vulnerabilities;
    
    QRegularExpression vulnRegex(
        R"(FILE:\s*(.+?)\s*LINE:\s*(\d+)\s*SEVERITY:\s*(\w+)\s*CWE:\s*(.+?)\s*CATEGORY:\s*(.+?)\s*DESCRIPTION:\s*(.+?)\s*RECOMMENDATION:\s*(.+?)(?:\n\n|$))", 
        QRegularExpression::DotMatchesEverythingOption
    );
    
    auto matches = vulnRegex.globalMatch(aiResponse);
    while (matches.hasNext()) {
        auto match = matches.next();
        SecurityVulnerability vuln;
        vuln.filePath = match.captured(1).trimmed();
        vuln.lineNumber = match.captured(2).toInt();
        vuln.severity = match.captured(3).trimmed();
        vuln.cweId = match.captured(4).trimmed();
        vuln.category = match.captured(5).trimmed();
        vuln.description = match.captured(6).trimmed();
        vuln.recommendation = match.captured(7).trimmed();
        vulnerabilities.append(vuln);
    }
    
    return vulnerabilities;
}

QString CursorWorkflowOrchestrator::gatherCodebaseContext(const QVector<QString>& files)
{
    QString context;
    
    // TODO: Read file contents and gather relevant context
    // For now, just list the files
    context = "Files in scope:\n" + files.join("\n");
    
    return context;
}

QString CursorWorkflowOrchestrator::extractRelevantContext(const QString& query, int maxTokens)
{
    // TODO: Implement semantic context extraction
    // For now, return the query
    return query;
}

void CursorWorkflowOrchestrator::cacheCompletion(const QString& key, const QString& completion)
{
    // Prune cache if too large
    if (m_completionCache.size() >= MAX_CACHE_SIZE) {
        pruneCompletionCache();
    }
    
    m_completionCache[key] = completion;
    m_cacheTimestamps[key] = QDateTime::currentMSecsSinceEpoch();
}

QString CursorWorkflowOrchestrator::getCachedCompletion(const QString& key)
{
    if (!m_completionCache.contains(key)) {
        return QString();
    }
    
    // Check if cache entry is still valid
    qint64 timestamp = m_cacheTimestamps.value(key, 0);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    if (now - timestamp > CACHE_TTL_MS) {
        // Expired
        m_completionCache.remove(key);
        m_cacheTimestamps.remove(key);
        return QString();
    }
    
    return m_completionCache.value(key);
}

void CursorWorkflowOrchestrator::pruneCompletionCache()
{
    // Remove oldest 20% of cache entries
    int toRemove = MAX_CACHE_SIZE / 5;
    
    QList<QPair<qint64, QString>> timestampKeys;
    for (auto it = m_cacheTimestamps.begin(); it != m_cacheTimestamps.end(); ++it) {
        timestampKeys.append({it.value(), it.key()});
    }
    
    std::sort(timestampKeys.begin(), timestampKeys.end());
    
    for (int i = 0; i < toRemove && i < timestampKeys.size(); ++i) {
        QString key = timestampKeys[i].second;
        m_completionCache.remove(key);
        m_cacheTimestamps.remove(key);
    }
}

void CursorWorkflowOrchestrator::startLatencyTimer(const QString& operation)
{
    m_operationStartTimes[operation] = QDateTime::currentMSecsSinceEpoch();
}

int CursorWorkflowOrchestrator::stopLatencyTimer(const QString& operation)
{
    qint64 start = m_operationStartTimes.value(operation, 0);
    if (start == 0) {
        return -1;
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int latency = static_cast<int>(now - start);
    
    m_operationStartTimes.remove(operation);
    return latency;
}

} // namespace RawrXD
