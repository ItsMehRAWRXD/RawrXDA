// SKIP_AUTOGEN
// Enterprise Team Collaboration Platform
// Real-time code review, shared debugging, presence management, conflict resolution
#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Agentic {

// ========== COLLABORATION STRUCTURES ==========

enum ReviewStatus {
    PENDING,
    IN_REVIEW,
    APPROVED,
    CHANGES_REQUESTED,
    REJECTED
};

enum ConflictResolutionMode {
    MANUAL,
    AUTO_MERGE,
    CUSTOM_RESOLUTION
};

struct TeamMember {
    QString memberId;
    QString username;
    QString email;
    QString role; // "admin", "maintainer", "developer", "viewer"
    QString status; // "online", "offline", "away"
    QDateTime lastActiveAt;
    QVector<QString> permissions;
    QMap<QString, QString> preferences;
};

struct CodeReview {
    QString reviewId;
    QString pullRequestId;
    QString authorId;
    QString reviewerId;
    ReviewStatus status;
    QVector<QString> comments;
    QVector<QString> approvals;
    QVector<QString> changeRequests;
    QDateTime createdAt;
    QDateTime completedAt;
    double approvalPercentage;
};

struct ReviewComment {
    QString commentId;
    QString reviewId;
    QString filePath;
    int lineNumber;
    QString code;
    QString comment;
    QString authorId;
    QDateTime createdAt;
    QVector<QString> replies;
    bool isResolved;
    QString resolution;
};

struct PullRequest {
    QString prId;
    QString title;
    QString description;
    QString sourceBranch;
    QString targetBranch;
    QString authorId;
    QDateTime createdAt;
    QDateTime mergedAt;
    int additions;
    int deletions;
    int fileChanges;
    QString status; // "open", "merged", "closed", "draft"
    QVector<QString> reviewerIds;
    QVector<CodeReview> reviews;
    QVector<QString> linkedIssues;
};

struct ConflictResolutionContext {
    QString fileName;
    QString ourVersion;
    QString theirVersion;
    QString baseVersion;
    ConflictResolutionMode resolutionMode;
    QString resolution;
    QString resolvedBy;
    QDateTime resolvedAt;
};

// SharedDebuggingSession data structure (not a QObject)
struct SharedDebuggingSessionData {
    QString sessionId;
    QString initiatorId;
    QString targetBreakpoint;
    QString filePath;
    int lineNumber;
    QVector<TeamMember> participants;
    QDateTime startTime;
    QDateTime endTime;
    bool isActive;
    QMap<QString, QString> sharedState; // variable watches, callstacks, etc.
};

struct CodingSession {
    QString sessionId;
    QString authorId;
    QString fileBeingEdited;
    int cursorLine;
    int cursorColumn;
    QDateTime lastActivityAt;
    QString activityType; // "typing", "reviewing", "debugging"
};

// ========== CODE REVIEW ENGINE ==========

class CodeReviewEngine : public QObject {
    Q_OBJECT

public:
    explicit CodeReviewEngine(QObject* parent = nullptr);
    ~CodeReviewEngine();

    // PR Management
    bool createPullRequest(const PullRequest& pr);
    bool updatePullRequest(const PullRequest& pr);
    bool mergePullRequest(const QString& prId);
    bool closePullRequest(const QString& prId);
    PullRequest getPullRequest(const QString& prId);
    QVector<PullRequest> getPullRequestsByAuthor(const QString& authorId);
    QVector<PullRequest> getPullRequestsNeedingReview(const QString& reviewerId);

    // Review Process
    bool startReview(const QString& prId, const QString& reviewerId);
    bool addReviewComment(const ReviewComment& comment);
    bool replyToComment(const QString& commentId, const QString& reply);
    bool resolveComment(const QString& commentId);
    bool approveReview(const QString& reviewId, const QString& reviewerId);
    bool requestChanges(const QString& reviewId, const QString& reason);

    // Automated Review
    bool enableAutomatedChecks(const QString& prId);
    bool runStaticAnalysisCheck(const QString& prId);
    bool runSecurityCheck(const QString& prId);
    bool runTestCheck(const QString& prId);
    QJsonObject getAutomatedCheckResults(const QString& prId);

    // Review Metrics
    QVector<CodeReview> getReviewsForPR(const QString& prId);
    int getApprovalCount(const QString& prId);
    double getAverageReviewTime();
    QString generateReviewMetricsReport();

signals:
    void reviewRequested(PullRequest pr);
    void commentAdded(ReviewComment comment);
    void reviewApproved(QString prId, QString reviewerId);
    void changesRequested(QString prId);
    void prMerged(QString prId);
    void automatedCheckFailed(QString prId, QString checkName);

private:
    QMap<QString, PullRequest> m_pullRequests;
    QMap<QString, QVector<CodeReview>> m_reviews;
    QMap<QString, QVector<ReviewComment>> m_comments;

    bool validateMergeability(const QString& prId);
};

// ========== CONFLICT RESOLUTION ==========

class ConflictResolver : public QObject {
    Q_OBJECT

public:
    explicit ConflictResolver(QObject* parent = nullptr);
    ~ConflictResolver();

    // Conflict Detection
    QVector<ConflictResolutionContext> detectConflicts(const QString& branch1, const QString& branch2);
    QVector<ConflictResolutionContext> detectConflictsInFile(const QString& filePath);

    // Automated Merging
    bool autoMerge(const QString& branch1, const QString& branch2);
    bool autoMergeFile(const QString& filePath, const QString& branch1, const QString& branch2);

    // Conflict Resolution
    bool manualResolveConflict(const ConflictResolutionContext& context);
    bool applyCustomResolution(const ConflictResolutionContext& context, const QString& resolution);
    bool acceptOurs(const ConflictResolutionContext& context);
    bool acceptTheirs(const ConflictResolutionContext& context);

    // 3-Way Merge
    QString performThreeWayMerge(const QString& baseVersion, const QString& ourVersion, const QString& theirVersion);
    QString mergeWithStrategy(const QString& filePath, const QString& strategy); // "ours", "theirs", "union", "recursive"

    // Conflict History
    QVector<ConflictResolutionContext> getConflictHistory();
    QString generateConflictReport();

signals:
    void conflictDetected(ConflictResolutionContext context);
    void conflictResolved(QString filePath);
    void mergeSucceeded(QString branch1, QString branch2);
    void mergeFailed(QString branch1, QString branch2, QString reason);

private:
    QVector<ConflictResolutionContext> m_resolvedConflicts;

    QString diffLines(const QString& text1, const QString& text2);
    QString findLongestCommonSubsequence(const QString& s1, const QString& s2);
};

// ========== TEAM PRESENCE & ACTIVITY ==========

class TeamPresenceManager : public QObject {
    Q_OBJECT

public:
    explicit TeamPresenceManager(QObject* parent = nullptr);
    ~TeamPresenceManager();

    // Team Management
    bool addTeamMember(const TeamMember& member);
    bool removeTeamMember(const QString& memberId);
    bool updateMemberStatus(const QString& memberId, const QString& status);
    TeamMember getTeamMember(const QString& memberId);
    QVector<TeamMember> getOnlineMembers();
    QVector<TeamMember> getAllMembers();

    // Permissions
    bool grantPermission(const QString& memberId, const QString& permission);
    bool revokePermission(const QString& memberId, const QString& permission);
    bool hasPermission(const QString& memberId, const QString& permission);
    QVector<QString> getMemberPermissions(const QString& memberId);

    // Coding Sessions
    void startCodingSession(const CodingSession& session);
    void updateCodingSession(const CodingSession& session);
    void endCodingSession(const QString& sessionId);
    CodingSession getCodingSession(const QString& sessionId);
    QVector<CodingSession> getActiveSessions();
    QVector<CodingSession> getSessionsByFile(const QString& filePath);

    // Notifications
    bool notifyTeam(const QString& message, const QVector<QString>& memberIds);
    bool notifyMember(const QString& memberId, const QString& message);
    QVector<QString> getUnreadNotifications(const QString& memberId);

    // Presence
    QString getPresenceStatus(const QString& memberId);
    int getOnlineMemberCount();
    bool isTeamAvailable(); // checks if at least one member online

signals:
    void memberOnline(TeamMember member);
    void memberOffline(TeamMember member);
    void codingSessionStarted(CodingSession session);
    void codingSessionEnded(CodingSession session);
    void notificationSent(QString memberId, QString message);

private:
    QMap<QString, TeamMember> m_members;
    QMap<QString, CodingSession> m_activeSessions;
    QMap<QString, QVector<QString>> m_notifications;
};

// ========== SHARED DEBUGGING ==========

class SharedDebuggingSession : public QObject {
    Q_OBJECT

public:
    explicit SharedDebuggingSession(QObject* parent = nullptr);
    ~SharedDebuggingSession();

    // Session Management
    QString createSession(const QString& initiatorId, const QString& filePath, int lineNumber);
    bool addParticipant(const QString& sessionId, const QString& memberId);
    bool removeParticipant(const QString& sessionId, const QString& memberId);
    bool endSession(const QString& sessionId);

    // Shared State
    bool setVariableWatch(const QString& sessionId, const QString& variableName);
    bool updateVariableWatch(const QString& sessionId, const QString& variableName, const QString& value);
    QString getVariableValue(const QString& sessionId, const QString& variableName);
    QMap<QString, QString> getSharedState(const QString& sessionId);

    // Breakpoint Management
    bool setBreakpoint(const QString& sessionId, const QString& filePath, int lineNumber);
    bool removeBreakpoint(const QString& sessionId, const QString& filePath, int lineNumber);
    QVector<QString> getBreakpoints(const QString& sessionId);

    // Execution Control
    bool stepInto(const QString& sessionId);
    bool stepOver(const QString& sessionId);
    bool stepOut(const QString& sessionId);
    bool continueExecution(const QString& sessionId);
    QString getCallStack(const QString& sessionId);

    // Communication
    bool sendMessage(const QString& sessionId, const QString& memberId, const QString& message);
    QVector<QString> getSessionMessages(const QString& sessionId);

signals:
    void sessionCreated(QString sessionId);
    void participantJoined(QString sessionId, QString memberId);
    void breakpointHit(QString sessionId, QString filePath, int lineNumber);
    void variableChanged(QString sessionId, QString variableName, QString newValue);
    void messageReceived(QString sessionId, QString message);

private:
    QMap<QString, SharedDebuggingSessionData> m_sessions;
    QMap<QString, QVector<QString>> m_breakpoints;
};

// ========== KNOWLEDGE MANAGEMENT ==========

class KnowledgeBase : public QObject {
    Q_OBJECT

public:
    explicit KnowledgeBase(QObject* parent = nullptr);
    ~KnowledgeBase();

    // Documentation
    bool createDocument(const QString& title, const QString& content, const QString& authorId);
    bool updateDocument(const QString& documentId, const QString& content);
    bool deleteDocument(const QString& documentId);
    QString getDocument(const QString& documentId);
    QVector<QString> searchDocuments(const QString& query);

    // Code Snippets
    bool saveCodeSnippet(const QString& title, const QString& code, const QString& language, const QString& description);
    QString getCodeSnippet(const QString& snippetId);
    QVector<QString> getCodeSnippetsByLanguage(const QString& language);
    QVector<QString> getCodeSnippetsByAuthor(const QString& authorId);

    // Design Patterns
    bool documentDesignPattern(const QString& patternName, const QString& description, const QString& code);
    QString getDesignPattern(const QString& patternName);
    QVector<QString> getAllDesignPatterns();

    // Lessons Learned
    bool recordLessonLearned(const QString& title, const QString& description, const QString& context);
    QVector<QString> getLessonsLearned();

signals:
    void documentCreated(QString documentId);
    void snippetSaved(QString snippetId);
    void knowledgeUpdated();

private:
    QMap<QString, QString> m_documents;
    QMap<QString, QJsonObject> m_codeSnippets;
    QMap<QString, QJsonObject> m_designPatterns;
};

// ========== COLLABORATION COORDINATOR ==========

class CollaborationCoordinator : public QObject {
    Q_OBJECT

public:
    explicit CollaborationCoordinator(QObject* parent = nullptr);
    ~CollaborationCoordinator();

    void initialize(const QVector<TeamMember>& team);

    // Integrated Collaboration
    QString initiateCodeReview(const PullRequest& pr, const QVector<QString>& reviewerIds);
    int conductTeamReview(const QString& prId);
    bool mergeWithTeamConsensus(const QString& prId);

    // Team Workflows
    bool startFeatureDevelopment(const QString& featureName, const QString& developerId);
    bool completeFeatureDevelopment(const QString& featureName);
    bool conductDesignReview(const QString& designDocument, const QVector<QString>& reviewerIds);

    // Performance Analytics
    QString generateTeamAnalytics();
    QString generateCodeQualityReport();
    QString generateCollaborationMetrics();

signals:
    void collaborationStarted(QString activity);
    void collaborationCompleted(QString activity);
    void teamInsightDiscovered(QString insight);

private:
    std::unique_ptr<CodeReviewEngine> m_reviewEngine;
    std::unique_ptr<ConflictResolver> m_conflictResolver;
    std::unique_ptr<TeamPresenceManager> m_presenceManager;
    std::unique_ptr<SharedDebuggingSession> m_debuggingSession;
    std::unique_ptr<KnowledgeBase> m_knowledgeBase;

    QVector<TeamMember> m_team;
};

// ========== UTILITIES ==========

class CollaborationUtils {
public:
    static QString getResponsibleReviewers(const QString& filePath, const QVector<TeamMember>& team);
    static int calculateReviewScore(const CodeReview& review);
    static QString suggestMergeConflictResolution(const ConflictResolutionContext& context);
    static bool shouldAutoMerge(const CodeReview& review);
};

} // namespace Agentic
} // namespace RawrXD
