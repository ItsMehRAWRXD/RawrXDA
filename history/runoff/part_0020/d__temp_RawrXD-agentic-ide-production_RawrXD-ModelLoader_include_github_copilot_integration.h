#pragma once

#include <QString>
#include <QObject>
#include <QVector>
#include <QJsonObject>
#include <memory>

// Forward declarations
class AIIntegrationHub;
class AgenticExecutor;

namespace RawrXD {

/**
 * @brief GitHub Copilot Enterprise-style integration
 * 
 * Provides enterprise development workflow features:
 * - PR review automation
 * - Issue→Code implementation
 * - Smart commit messages
 * - Security vulnerability scanning
 * - Test generation
 */
class GitHubCopilotIntegration : public QObject {
    Q_OBJECT

public:
    struct CopilotContext {
        QString repository;
        QString branch;
        QString pullRequest;
        QVector<QString> changedFiles;
        QString commitMessage;
        QString issueDescription;
        QVector<QString> labels;
        QString author;
        QString baseBranch;
    };

    struct ReviewComment {
        QString filePath;
        int lineNumber{0};
        QString severity;  // "info", "warning", "error"
        QString category;  // "quality", "security", "performance", "style"
        QString message;
        QString suggestion;
        double confidence{0.0};
    };

    struct SecurityIssue {
        QString type;          // "injection", "xss", "buffer-overflow", etc.
        QString severity;      // "critical", "high", "medium", "low"
        QString filePath;
        int lineNumber{0};
        QString vulnerableCode;
        QString description;
        QString fix;
        QString cve;           // CVE identifier if applicable
    };

    explicit GitHubCopilotIntegration(QObject* parent = nullptr);
    ~GitHubCopilotIntegration();

    // Initialization
    void initialize(AIIntegrationHub* hub, AgenticExecutor* executor);
    bool isReady() const;

    // PR Review Assistant
    void reviewPullRequest(const QString& diffText, const CopilotContext& ctx);
    QVector<ReviewComment> generateReviewComments(const QString& diff, const CopilotContext& ctx);

    // Issue → Code workflow
    void implementIssue(const QString& issueText, const CopilotContext& ctx);

    // Smart commit messages
    QString generateCommitMessage(const QVector<QString>& changedFiles, const QString& diff);

    // Code explanation for PRs
    QString explainChanges(const QString& code, const QString& context);

    // Security vulnerability detection
    QVector<SecurityIssue> scanForVulnerabilities(const QString& code, const QString& language);

    // Test generation from implementation
    void generateTestsForChanges(const QString& implementation, const QString& testFilePath);

signals:
    void reviewComplete(const QVector<ReviewComment>& comments);
    void issueImplementationComplete(const QString& code);
    void commitMessageReady(const QString& message);
    void codeExplanationReady(const QString& explanation);
    void vulnerabilitiesFound(const QVector<SecurityIssue>& issues);
    void testsGenerated(const QString& testCode);
    void errorOccurred(const QString& error);
    void progressUpdate(const QString& status, int percentage);

private:
    QString buildPRReviewPrompt(const QString& diff, const CopilotContext& ctx);
    QString buildIssueImplementationPrompt(const QString& issue, const CopilotContext& ctx);
    QString buildCommitMessagePrompt(const QString& diff);
    QString buildSecurityScanPrompt(const QString& code, const QString& language);
    QString buildTestGenerationPrompt(const QString& implementation);
    
    QVector<ReviewComment> parseReviewComments(const QString& response);
    QVector<SecurityIssue> parseSecurityIssues(const QString& response);

    AIIntegrationHub* m_hub{nullptr};
    AgenticExecutor* m_executor{nullptr};
};

} // namespace RawrXD
