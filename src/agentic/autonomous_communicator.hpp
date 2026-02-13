// =============================================================================
// autonomous_communicator.hpp
// RawrXD v14.2.1 Cathedral — Tier 2.3: Autonomous Communication
//
// Transparent reasoning & stakeholder reporting:
//   - Automated standup/status report generation
//   - PR description & commit message generation
//   - Reasoning trace output (explainable decisions)
//   - Slack/Teams/Discord webhook integration stubs
//   - Change summary & impact analysis reports
//
// =============================================================================
#pragma once
#ifndef RAWRXD_AUTONOMOUS_COMMUNICATOR_HPP
#define RAWRXD_AUTONOMOUS_COMMUNICATOR_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace RawrXD {
namespace Autonomy {

// =============================================================================
// Result Type
// =============================================================================

struct CommResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static CommResult ok(const char* msg) {
        CommResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static CommResult error(const char* msg, int code = -1) {
        CommResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class ReportType : uint8_t {
    Standup,             // Daily standup summary
    SprintSummary,       // Sprint/iteration summary
    ChangeImpact,        // Impact analysis of changes
    SecurityAudit,       // Security findings report
    PerformanceReport,   // Performance metrics
    CoverageReport,      // Test coverage changes
    CustomReport         // User-defined template
};

enum class WebhookTarget : uint8_t {
    Slack,
    Teams,
    Discord,
    Generic,             // Generic HTTP POST
    Console,             // stdout (for piping)
    File                 // Write to file
};

enum class ReasoningLevel : uint8_t {
    Silent,              // No reasoning output
    Brief,               // One-line summaries
    Normal,              // Standard explanations
    Verbose,             // Detailed chain-of-thought
    Debug                // Full internal state dumps
};

enum class CommitStyle : uint8_t {
    Conventional,        // feat(scope): description
    Angular,             // type(scope): subject
    Gitmoji,             // :emoji: description
    Plain,               // Simple description
    Detailed             // Multi-line with body
};

// =============================================================================
// Data Types
// =============================================================================

struct ReasoningStep {
    uint64_t    stepId;
    char        action[256];        // What was done
    char        rationale[512];     // Why it was done
    char        alternative[256];   // What was considered but rejected
    char        outcome[256];       // Result of the action
    float       confidence;         // 0.0 – 1.0
    uint64_t    timestamp;
    uint64_t    durationMs;
};

struct ChangeEntry {
    char        file[260];
    int         linesAdded;
    int         linesRemoved;
    char        changeType[32];     // "added", "modified", "deleted", "renamed"
    char        summary[512];       // Human-readable change description
    float       riskScore;          // 0.0 – 1.0
};

struct StatusReport {
    uint64_t    reportId;
    ReportType  type;
    char        title[256];
    char        body[8192];
    char        author[64];         // "RawrXD-Agent" typically
    uint64_t    timestamp;

    // Metrics
    int         filesChanged;
    int         totalAdded;
    int         totalRemoved;
    int         testsRun;
    int         testsPassed;
    int         testsFailed;
    float       coverageDelta;      // e.g., +2.3%
};

struct WebhookConfig {
    WebhookTarget target;
    char        url[1024];
    char        authToken[512];     // Bearer token or bot token
    char        channel[128];       // Slack channel, Teams channel, etc.
    char        templatePath[260];  // Custom message template file
    bool        enabled;
    int         rateLimit;          // Max messages per hour
    int         sentThisHour;       // Counter

    static WebhookConfig console() {
        WebhookConfig c;
        memset(&c, 0, sizeof(c));
        c.target = WebhookTarget::Console;
        c.enabled = true;
        c.rateLimit = 1000;
        return c;
    }
};

struct PRDescription {
    char        title[256];
    char        body[8192];
    char        labels[512];        // Comma-separated
    char        reviewers[512];     // Comma-separated
    char        branch[128];

    struct Section {
        char heading[64];
        char content[2048];
    };
    Section     sections[8];        // Summary, Changes, Testing, etc.
    int         sectionCount;
};

struct CommitMessage {
    CommitStyle style;
    char        type[32];           // feat, fix, refactor, etc.
    char        scope[64];          // Module scope
    char        subject[128];       // Short description
    char        body[2048];         // Detailed description
    char        footer[512];        // Breaking changes, issue refs
    bool        isBreaking;

    // Render to a single string
    std::string render() const;
};

// =============================================================================
// Callbacks
// =============================================================================

typedef void (*ReportGeneratedCallback)(const StatusReport* report, void* userData);
typedef void (*WebhookSentCallback)(WebhookTarget target, bool success, void* userData);
typedef void (*ReasoningStepCallback)(const ReasoningStep* step, void* userData);

// LLM injection for report generation
typedef std::string (*ReportGeneratorFn)(ReportType type,
                                          const ChangeEntry* changes, int changeCount,
                                          const ReasoningStep* reasoning, int reasoningCount,
                                          void* userData);

typedef std::string (*CommitMsgGeneratorFn)(const ChangeEntry* changes, int changeCount,
                                             CommitStyle style, void* userData);

typedef PRDescription (*PRDescGeneratorFn)(const ChangeEntry* changes, int changeCount,
                                            const char* branchName, void* userData);

// =============================================================================
// Configuration
// =============================================================================

struct CommunicatorConfig {
    ReasoningLevel  reasoningLevel;
    CommitStyle     commitStyle;
    char            agentName[64];
    bool            autoGenerateCommits;
    bool            autoGeneratePR;
    bool            autoSendStandups;
    int             standupHour;     // 0-23, hour of day for standup
    int             maxReasoningHistory;

    static CommunicatorConfig defaults() {
        CommunicatorConfig c;
        memset(&c, 0, sizeof(c));
        c.reasoningLevel = ReasoningLevel::Normal;
        c.commitStyle = CommitStyle::Conventional;
        strncpy(c.agentName, "RawrXD-Agent", sizeof(c.agentName) - 1);
        c.autoGenerateCommits = true;
        c.autoGeneratePR      = true;
        c.autoSendStandups    = false;
        c.standupHour         = 9;
        c.maxReasoningHistory = 1000;
        return c;
    }
};

// =============================================================================
// Core Class: AutonomousCommunicator
// =============================================================================

class AutonomousCommunicator {
public:
    static AutonomousCommunicator& instance();

    // ── Lifecycle ─────────────────────────────────────────────────────────
    CommResult initialize(const CommunicatorConfig& config);
    CommResult initialize();
    void shutdown();
    bool isActive() const { return m_active.load(); }

    // ── Reasoning Trace ───────────────────────────────────────────────────
    // Record a reasoning step (explainable AI output)
    uint64_t recordReasoning(const char* action, const char* rationale,
                              const char* alternative = nullptr,
                              float confidence = 1.0f);

    // Record reasoning with outcome
    void updateReasoningOutcome(uint64_t stepId, const char* outcome);

    // Get reasoning history
    std::vector<ReasoningStep> getReasoningHistory(int maxSteps = 50) const;
    std::string reasoningToMarkdown(int maxSteps = 20) const;
    std::string reasoningToJson(int maxSteps = 50) const;

    // ── Change Tracking ───────────────────────────────────────────────────
    void recordChange(const char* file, int added, int removed,
                       const char* changeType, const char* summary,
                       float riskScore = 0.0f);
    void clearChanges();
    std::vector<ChangeEntry> getChanges() const;

    // ── Report Generation ─────────────────────────────────────────────────
    StatusReport generateReport(ReportType type) const;
    std::string  reportToMarkdown(const StatusReport& report) const;
    std::string  reportToJson(const StatusReport& report) const;

    // ── Commit Messages ───────────────────────────────────────────────────
    CommitMessage generateCommitMessage() const;
    CommitMessage generateCommitMessage(const ChangeEntry* changes, int count) const;

    // ── PR Descriptions ───────────────────────────────────────────────────
    PRDescription generatePRDescription(const char* branchName = nullptr) const;

    // ── Webhook Delivery ──────────────────────────────────────────────────
    CommResult addWebhook(const WebhookConfig& config);
    CommResult removeWebhook(WebhookTarget target);
    CommResult sendToWebhook(WebhookTarget target, const char* message);
    CommResult broadcastReport(const StatusReport& report);

    // ── Sub-system Injection ──────────────────────────────────────────────
    void setReportGenerator(ReportGeneratorFn fn, void* ud);
    void setCommitMsgGenerator(CommitMsgGeneratorFn fn, void* ud);
    void setPRDescGenerator(PRDescGeneratorFn fn, void* ud);

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setReportCallback(ReportGeneratedCallback cb, void* ud);
    void setWebhookCallback(WebhookSentCallback cb, void* ud);
    void setReasoningCallback(ReasoningStepCallback cb, void* ud);

    // ── Statistics ─────────────────────────────────────────────────────────
    struct Stats {
        uint64_t reasoningStepsRecorded;
        uint64_t reportsGenerated;
        uint64_t webhooksSent;
        uint64_t webhooksFailed;
        uint64_t commitsGenerated;
        uint64_t prsGenerated;
    };

    Stats getStats() const;
    std::string statsToJson() const;

private:
    AutonomousCommunicator();
    ~AutonomousCommunicator();
    AutonomousCommunicator(const AutonomousCommunicator&) = delete;
    AutonomousCommunicator& operator=(const AutonomousCommunicator&) = delete;

    // Internal helpers
    std::string formatConventional(const CommitMessage& msg) const;
    std::string formatGitmoji(const CommitMessage& msg) const;
    std::string buildReportBody(ReportType type) const;
    CommResult sendHttp(const char* url, const char* authToken,
                         const char* body, const char* contentType);
    CommResult sendSlack(const WebhookConfig& wh, const char* message);
    CommResult sendTeams(const WebhookConfig& wh, const char* message);
    CommResult sendDiscord(const WebhookConfig& wh, const char* message);

    uint64_t nextReasoningId();
    uint64_t nextReportId();

    // ── State ─────────────────────────────────────────────────────────────
    mutable std::mutex          m_mutex;
    std::atomic<bool>           m_active{false};
    std::atomic<uint64_t>       m_nextReasoning{1};
    std::atomic<uint64_t>       m_nextReport{1};

    CommunicatorConfig          m_config;

    std::vector<ReasoningStep>  m_reasoningHistory;
    std::vector<ChangeEntry>    m_changes;
    std::vector<WebhookConfig>  m_webhooks;

    // Injected subsystems
    ReportGeneratorFn           m_reportGen;     void* m_reportGenUD;
    CommitMsgGeneratorFn        m_commitGen;     void* m_commitGenUD;
    PRDescGeneratorFn           m_prDescGen;     void* m_prDescGenUD;

    // Callbacks
    ReportGeneratedCallback     m_reportCb;      void* m_reportCbUD;
    WebhookSentCallback         m_webhookCb;     void* m_webhookCbUD;
    ReasoningStepCallback       m_reasoningCb;   void* m_reasoningCbUD;

    alignas(64) Stats           m_stats;
};

} // namespace Autonomy
} // namespace RawrXD

#endif // RAWRXD_AUTONOMOUS_COMMUNICATOR_HPP
