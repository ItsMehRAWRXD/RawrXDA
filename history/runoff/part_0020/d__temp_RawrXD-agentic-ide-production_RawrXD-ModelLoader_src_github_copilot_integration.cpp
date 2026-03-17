#include "github_copilot_integration.h"
#include "ai_integration_hub.h"
#include "agentic_executor.h"
#include <QDebug>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>

namespace RawrXD {

GitHubCopilotIntegration::GitHubCopilotIntegration(QObject* parent)
    : QObject(parent)
{
}

GitHubCopilotIntegration::~GitHubCopilotIntegration() = default;

void GitHubCopilotIntegration::initialize(AIIntegrationHub* hub, AgenticExecutor* executor)
{
    m_hub = hub;
    m_executor = executor;
    qDebug() << "[GitHubCopilot] Initialized";
}

bool GitHubCopilotIntegration::isReady() const
{
    return m_hub != nullptr && m_hub->isReady() && m_executor != nullptr;
}

void GitHubCopilotIntegration::reviewPullRequest(const QString& diffText, const CopilotContext& ctx)
{
    if (!isReady()) {
        emit errorOccurred("GitHubCopilotIntegration not ready");
        return;
    }

    emit progressUpdate("Analyzing pull request...", 10);

    QString prompt = buildPRReviewPrompt(diffText, ctx);

    // Route to agentic executor for comprehensive review
    auto result = m_executor->executeUserRequest(prompt);

    emit progressUpdate("Generating review comments...", 70);

    if (result.contains("output")) {
        QString response = result["output"].toString();
        auto comments = parseReviewComments(response);
        emit reviewComplete(comments);
    } else {
        emit errorOccurred("PR review failed");
    }

    emit progressUpdate("Review complete", 100);
}

QVector<GitHubCopilotIntegration::ReviewComment> GitHubCopilotIntegration::generateReviewComments(
    const QString& diff, const CopilotContext& ctx)
{
    QString prompt = buildPRReviewPrompt(diff, ctx);
    
    if (m_executor) {
        auto result = m_executor->executeUserRequest(prompt);
        if (result.contains("output")) {
            return parseReviewComments(result["output"].toString());
        }
    }

    return {};
}

void GitHubCopilotIntegration::implementIssue(const QString& issueText, const CopilotContext& ctx)
{
    if (!isReady()) {
        emit errorOccurred("Cannot implement issue: not ready");
        return;
    }

    emit progressUpdate("Planning implementation...", 20);

    QString prompt = buildIssueImplementationPrompt(issueText, ctx);

    auto result = m_executor->executeUserRequest(prompt);

    emit progressUpdate("Generating code...", 80);

    if (result.contains("output")) {
        emit issueImplementationComplete(result["output"].toString());
    } else {
        emit errorOccurred("Issue implementation failed");
    }

    emit progressUpdate("Implementation complete", 100);
}

QString GitHubCopilotIntegration::generateCommitMessage(
    const QVector<QString>& changedFiles, const QString& diff)
{
    if (!isReady()) {
        return "Update files";  // Fallback
    }

    QString prompt = buildCommitMessagePrompt(diff);
    prompt += "\n\nChanged files:\n";
    for (const auto& file : changedFiles) {
        prompt += QString("- %1\n").arg(file);
    }

    if (m_executor) {
        auto result = m_executor->executeUserRequest(prompt);
        if (result.contains("output")) {
            QString message = result["output"].toString().trimmed();
            emit commitMessageReady(message);
            return message;
        }
    }

    return "Update files";
}

QString GitHubCopilotIntegration::explainChanges(const QString& code, const QString& context)
{
    if (!isReady()) {
        return "Code explanation unavailable";
    }

    QString prompt = QString("Explain the following code changes in plain English:\n\n"
                           "Context: %1\n\n"
                           "Changes:\n```\n%2\n```\n\n"
                           "Provide a clear, concise explanation suitable for code review.")
                        .arg(context, code);

    if (m_executor) {
        auto result = m_executor->executeUserRequest(prompt);
        if (result.contains("output")) {
            QString explanation = result["output"].toString();
            emit codeExplanationReady(explanation);
            return explanation;
        }
    }

    return "Code explanation unavailable";
}

QVector<GitHubCopilotIntegration::SecurityIssue> GitHubCopilotIntegration::scanForVulnerabilities(
    const QString& code, const QString& language)
{
    if (!isReady()) {
        emit errorOccurred("Security scan unavailable");
        return {};
    }

    emit progressUpdate("Scanning for vulnerabilities...", 30);

    QString prompt = buildSecurityScanPrompt(code, language);

    auto result = m_executor->executeUserRequest(prompt);

    emit progressUpdate("Analyzing security issues...", 80);

    QVector<SecurityIssue> issues;
    if (result.contains("output")) {
        issues = parseSecurityIssues(result["output"].toString());
        emit vulnerabilitiesFound(issues);
    }

    emit progressUpdate("Security scan complete", 100);
    return issues;
}

void GitHubCopilotIntegration::generateTestsForChanges(const QString& implementation, const QString& testFilePath)
{
    if (!isReady()) {
        emit errorOccurred("Test generation unavailable");
        return;
    }

    emit progressUpdate("Generating tests...", 40);

    QString prompt = buildTestGenerationPrompt(implementation);
    prompt += QString("\n\nGenerate tests for file: %1").arg(testFilePath);

    auto result = m_executor->executeUserRequest(prompt);

    if (result.contains("output")) {
        emit testsGenerated(result["output"].toString());
    } else {
        emit errorOccurred("Test generation failed");
    }

    emit progressUpdate("Tests generated", 100);
}

QString GitHubCopilotIntegration::buildPRReviewPrompt(const QString& diff, const CopilotContext& ctx)
{
    QString prompt;
    prompt += "You are an expert code reviewer. Review this pull request thoroughly:\n\n";
    prompt += QString("Repository: %1\n").arg(ctx.repository);
    prompt += QString("Branch: %1 → %2\n").arg(ctx.branch, ctx.baseBranch);
    
    if (!ctx.issueDescription.isEmpty()) {
        prompt += QString("Related issue: %1\n").arg(ctx.issueDescription);
    }

    prompt += "\n=== DIFF ===\n" + diff + "\n=== END DIFF ===\n\n";

    prompt += "Review criteria:\n";
    prompt += "1. Code quality and readability\n";
    prompt += "2. Potential bugs or logic errors\n";
    prompt += "3. Performance implications\n";
    prompt += "4. Security vulnerabilities\n";
    prompt += "5. Test coverage\n";
    prompt += "6. Documentation completeness\n";
    prompt += "7. Code style and conventions\n\n";

    prompt += "Provide specific, actionable feedback with line numbers and severity levels.\n";

    return prompt;
}

QString GitHubCopilotIntegration::buildIssueImplementationPrompt(const QString& issue, const CopilotContext& ctx)
{
    QString prompt;
    prompt += "Implement the following GitHub issue:\n\n";
    prompt += QString("Issue: %1\n").arg(issue);
    prompt += QString("Repository: %1\n").arg(ctx.repository);
    prompt += QString("Target branch: %1\n\n").arg(ctx.branch);

    prompt += "Generate production-ready code that:\n";
    prompt += "- Solves the issue completely\n";
    prompt += "- Follows project conventions\n";
    prompt += "- Includes error handling\n";
    prompt += "- Is well-documented\n";
    prompt += "- Includes relevant tests\n\n";

    prompt += "Provide file paths, code changes, and implementation notes.\n";

    return prompt;
}

QString GitHubCopilotIntegration::buildCommitMessagePrompt(const QString& diff)
{
    QString prompt;
    prompt += "Generate a concise, informative Git commit message for these changes:\n\n";
    prompt += "Diff:\n" + diff + "\n\n";
    prompt += "Format:\n";
    prompt += "<type>(<scope>): <subject>\n\n";
    prompt += "<body>\n\n";
    prompt += "Types: feat, fix, docs, style, refactor, test, chore\n";

    return prompt;
}

QString GitHubCopilotIntegration::buildSecurityScanPrompt(const QString& code, const QString& language)
{
    QString prompt;
    prompt += QString("Scan this %1 code for security vulnerabilities:\n\n").arg(language);
    prompt += "```\n" + code + "\n```\n\n";
    prompt += "Identify:\n";
    prompt += "- Injection vulnerabilities (SQL, XSS, Command Injection)\n";
    prompt += "- Authentication/Authorization issues\n";
    prompt += "- Data exposure risks\n";
    prompt += "- Buffer overflows\n";
    prompt += "- Cryptographic weaknesses\n";
    prompt += "- Input validation problems\n\n";
    prompt += "For each issue, provide: type, severity, line number, description, and fix.\n";

    return prompt;
}

QString GitHubCopilotIntegration::buildTestGenerationPrompt(const QString& implementation)
{
    QString prompt;
    prompt += "Generate comprehensive unit tests for this implementation:\n\n";
    prompt += "```\n" + implementation + "\n```\n\n";
    prompt += "Tests should:\n";
    prompt += "- Cover all public methods\n";
    prompt += "- Test edge cases and error conditions\n";
    prompt += "- Use appropriate assertions\n";
    prompt += "- Follow testing best practices\n";
    prompt += "- Include setup/teardown if needed\n";

    return prompt;
}

QVector<GitHubCopilotIntegration::ReviewComment> GitHubCopilotIntegration::parseReviewComments(const QString& response)
{
    QVector<ReviewComment> comments;

    // Stub: Parse response into structured review comments
    // Real implementation would use regex or JSON parsing
    ReviewComment comment;
    comment.message = response;
    comment.severity = "info";
    comment.category = "quality";
    comment.confidence = 0.8;
    comments.append(comment);

    return comments;
}

QVector<GitHubCopilotIntegration::SecurityIssue> GitHubCopilotIntegration::parseSecurityIssues(const QString& response)
{
    QVector<SecurityIssue> issues;

    // Stub: Parse response into structured security issues
    // Real implementation would use regex or JSON parsing
    if (response.contains("vulnerability", Qt::CaseInsensitive) ||
        response.contains("security", Qt::CaseInsensitive)) {
        
        SecurityIssue issue;
        issue.type = "potential_issue";
        issue.severity = "medium";
        issue.description = response.left(200);
        issues.append(issue);
    }

    return issues;
}

} // namespace RawrXD
