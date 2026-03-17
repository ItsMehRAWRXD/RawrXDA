#include "cursor_workflow_engine.h"
#include "ai_integration_hub.h"
#include "agentic_executor.h"
#include "multi_modal_model_router.h"
#include <QDebug>
#include <QRegularExpression>

namespace RawrXD {

CursorWorkflowEngine::CursorWorkflowEngine(QObject* parent)
    : QObject(parent)
{
}

CursorWorkflowEngine::~CursorWorkflowEngine() = default;

void CursorWorkflowEngine::initialize(AIIntegrationHub* hub, AgenticExecutor* executor)
{
    m_hub = hub;
    m_executor = executor;
    
    if (m_hub) {
        // Router will be accessed through hub when needed
        qDebug() << "[CursorWorkflow] Initialized with AIIntegrationHub";
    }
}

bool CursorWorkflowEngine::isReady() const
{
    // Consider ready when constructed; upstream components gate UX
    return true;
}

void CursorWorkflowEngine::triggerInlineCompletion(const CursorContext& ctx)
{
    if (!isReady()) {
        emit errorOccurred("CursorWorkflowEngine not ready");
        return;
    }

    // Build a lightweight heuristic suggestion when hub isn't wired
    QString suggestion;
    const QString& pre = ctx.linePrefix;
    const QString& suf = ctx.lineSuffix;

    if (suf.startsWith(')') && !pre.endsWith('(')) {
        suggestion = ";";
    } else if (pre.trimmed().endsWith("if") && suf.trimmed().isEmpty()) {
        suggestion = " () {\n}\n";
    } else if (pre.endsWith('(') && !suf.startsWith(')')) {
        suggestion = ")";
    } else if (pre.trimmed().endsWith('{') && suf.trimmed().isEmpty()) {
        suggestion = "\n}\n";
    } else {
        suggestion = ";";
    }

    m_currentCompletion = suggestion;
    m_completionActive = true;

    CodeCompletion completion;
    completion.text = m_currentCompletion;
    completion.confidence = 0.6;
    completion.description = "Heuristic inline completion";
    completion.priority = 1;
    emit completionReady(completion);
}

void CursorWorkflowEngine::acceptInlineCompletion()
{
    if (m_completionActive) {
        qDebug() << "[CursorWorkflow] Completion accepted:" << m_currentCompletion;
        m_completionActive = false;
        m_currentCompletion.clear();
    }
}

void CursorWorkflowEngine::rejectInlineCompletion()
{
    if (m_completionActive) {
        qDebug() << "[CursorWorkflow] Completion rejected";
        m_completionActive = false;
        m_currentCompletion.clear();
    }
}

QString CursorWorkflowEngine::getCurrentCompletion() const
{
    return m_currentCompletion;
}

void CursorWorkflowEngine::applyMultiCursorEdits(const QVector<CursorContext>& cursors)
{
    // Stub: Apply edits at multiple cursor positions simultaneously
    qDebug() << "[CursorWorkflow] Multi-cursor edit for" << cursors.size() << "positions";
    
    for (const auto& ctx : cursors) {
        triggerInlineCompletion(ctx);
    }
}

void CursorWorkflowEngine::smartRefactor(const QString& instruction, const CursorContext& ctx)
{
    if (!isReady() || !m_executor) {
        emit errorOccurred("Cannot refactor: engine not ready");
        return;
    }

    // Build refactoring request
    QString prompt = QString("Refactor the following code according to this instruction: %1\n\n"
                           "File: %2\nCode:\n```\n%3\n```\n\n"
                           "Provide refactored code with reasoning.")
                        .arg(instruction, ctx.currentFile, ctx.selectedText);

    // Route to agentic executor for autonomous refactoring
    QJsonObject request;
    request["task"] = "refactor_code";
    request["instruction"] = instruction;
    request["code"] = ctx.selectedText;
    request["file"] = ctx.currentFile;
    request["context"] = ctx.linePrefix + ctx.lineSuffix;

    auto result = m_executor->executeUserRequest(prompt);

    if (result.contains("output")) {
        QString refactoredCode = result["output"].toString();
        
        QVector<CodeChange> changes;
        CodeChange change;
        change.filePath = ctx.currentFile;
        change.startLine = ctx.cursorLine;
        change.originalCode = ctx.selectedText;
        change.newCode = refactoredCode;
        change.reasoning = instruction;
        changes.append(change);

        emit refactoringComplete(changes);
    } else {
        emit errorOccurred("Refactoring failed");
    }
}

void CursorWorkflowEngine::generateFromComment(const QString& comment, const CursorContext& ctx)
{
    if (!isReady()) {
        emit errorOccurred("Cannot generate: engine not ready");
        return;
    }

    QString prompt = QString("Generate code implementation for this comment:\n\n"
                           "Comment: %1\n"
                           "File: %2\n"
                           "Language: %3\n"
                           "Context:\n%4\n\n"
                           "Generate idiomatic, production-ready code.")
                        .arg(comment, ctx.currentFile, ctx.language, ctx.linePrefix);

    // Use executor to generate code
    if (m_executor) {
        auto result = m_executor->executeUserRequest(prompt);
        if (result.contains("output")) {
            emit codeGenerated(result["output"].toString());
        }
    }
}

QVector<QString> CursorWorkflowEngine::getContextualSuggestions(const CursorContext& ctx)
{
    QVector<QString> suggestions;

    // Stub: Return context-aware suggestions based on current state
    if (ctx.selectedText.isEmpty()) {
        suggestions << "Complete this line"
                   << "Generate documentation"
                   << "Add error handling";
    } else {
        suggestions << "Refactor selection"
                   << "Extract to function"
                   << "Add unit tests"
                   << "Optimize code";
    }

    emit suggestionsReady(suggestions);
    return suggestions;
}

void CursorWorkflowEngine::setLatencyTarget(int milliseconds)
{
    m_latencyTarget = milliseconds;
    qDebug() << "[CursorWorkflow] Latency target set to" << milliseconds << "ms";
}

void CursorWorkflowEngine::setConfidenceThreshold(double threshold)
{
    m_confidenceThreshold = threshold;
    qDebug() << "[CursorWorkflow] Confidence threshold set to" << threshold;
}

QString CursorWorkflowEngine::buildCursorPrompt(const CursorContext& ctx)
{
    QString prompt;
    prompt += QString("Complete the code at cursor position.\n");
    prompt += QString("File: %1\n").arg(ctx.currentFile);
    prompt += QString("Language: %1\n").arg(ctx.language);
    prompt += QString("Line %1, Column %2\n").arg(ctx.cursorLine).arg(ctx.cursorColumn);
    prompt += QString("\nContext before cursor:\n%1\n").arg(ctx.linePrefix);
    prompt += QString("\nContext after cursor:\n%1\n").arg(ctx.lineSuffix);
    
    if (!ctx.selectedText.isEmpty()) {
        prompt += QString("\nSelected text:\n%1\n").arg(ctx.selectedText);
    }

    return prompt;
}

QVector<CursorWorkflowEngine::CodeChange> CursorWorkflowEngine::parseCursorResponse(const QString& response)
{
    QVector<CodeChange> changes;

    // Stub: Parse response into code changes
    // Real implementation would use regex or structured parsing
    CodeChange change;
    change.newCode = response;
    changes.append(change);

    return changes;
}

QString CursorWorkflowEngine::selectOptimalModel(const CursorContext& ctx)
{
    Q_UNUSED(ctx);
    // Stub: Select model based on complexity and latency requirements
    return "fast-completion-model";
}

int CursorWorkflowEngine::calculateComplexity(const CursorContext& ctx)
{
    int complexity = 0;
    complexity += ctx.selectedText.length() / 10;
    complexity += ctx.openFiles.size();
    return complexity;
}

int CursorWorkflowEngine::estimateContext(const CursorContext& ctx)
{
    int tokens = 0;
    tokens += ctx.linePrefix.length() / 4;  // Rough token estimate
    tokens += ctx.lineSuffix.length() / 4;
    tokens += ctx.selectedText.length() / 4;
    return tokens;
}

} // namespace RawrXD
