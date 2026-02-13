// =============================================================================
// autonomous_communicator.cpp
// RawrXD v14.2.1 Cathedral — Tier 2.3: Autonomous Communication
//
// Implementation of reasoning traces, status reports, commit message
// generation, PR descriptions, and webhook delivery.
// =============================================================================

#include "autonomous_communicator.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <ctime>

#ifdef _WIN32
#pragma comment(lib, "winhttp.lib")
#endif

namespace RawrXD {
namespace Autonomy {

// =============================================================================
// CommitMessage::render
// =============================================================================

std::string CommitMessage::render() const {
    char buf[4096];
    switch (style) {
    case CommitStyle::Conventional:
    case CommitStyle::Angular:
        if (scope[0]) {
            snprintf(buf, sizeof(buf), "%s(%s): %s", type, scope, subject);
        } else {
            snprintf(buf, sizeof(buf), "%s: %s", type, subject);
        }
        break;
    case CommitStyle::Gitmoji:
        snprintf(buf, sizeof(buf), "%s %s", type, subject);
        break;
    case CommitStyle::Plain:
        snprintf(buf, sizeof(buf), "%s", subject);
        break;
    case CommitStyle::Detailed:
        if (scope[0]) {
            snprintf(buf, sizeof(buf), "%s(%s): %s\n\n%s", type, scope, subject, body);
        } else {
            snprintf(buf, sizeof(buf), "%s: %s\n\n%s", type, subject, body);
        }
        break;
    }

    std::string result(buf);

    if (body[0] && style != CommitStyle::Detailed && style != CommitStyle::Plain) {
        result += "\n\n";
        result += body;
    }

    if (footer[0]) {
        result += "\n\n";
        result += footer;
    }

    if (isBreaking && style == CommitStyle::Conventional) {
        result += "\n\nBREAKING CHANGE: see above";
    }

    return result;
}

// =============================================================================
// Singleton
// =============================================================================

AutonomousCommunicator& AutonomousCommunicator::instance() {
    static AutonomousCommunicator inst;
    return inst;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

AutonomousCommunicator::AutonomousCommunicator()
    : m_reportGen(nullptr),   m_reportGenUD(nullptr)
    , m_commitGen(nullptr),   m_commitGenUD(nullptr)
    , m_prDescGen(nullptr),   m_prDescGenUD(nullptr)
    , m_reportCb(nullptr),    m_reportCbUD(nullptr)
    , m_webhookCb(nullptr),   m_webhookCbUD(nullptr)
    , m_reasoningCb(nullptr), m_reasoningCbUD(nullptr)
{
    memset(&m_stats, 0, sizeof(m_stats));
}

AutonomousCommunicator::~AutonomousCommunicator() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

CommResult AutonomousCommunicator::initialize(const CommunicatorConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_active.load()) {
        return CommResult::error("Already initialized");
    }

    m_config = config;
    m_reasoningHistory.reserve(static_cast<size_t>(config.maxReasoningHistory));
    m_changes.reserve(256);
    m_webhooks.reserve(8);

    m_active.store(true);
    return CommResult::ok("Autonomous communicator initialized");
}

CommResult AutonomousCommunicator::initialize() {
    return initialize(CommunicatorConfig::defaults());
}

void AutonomousCommunicator::shutdown() {
    if (!m_active.exchange(false)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_reasoningHistory.clear();
    m_changes.clear();
    m_webhooks.clear();
}

// =============================================================================
// Reasoning Trace
// =============================================================================

uint64_t AutonomousCommunicator::recordReasoning(const char* action,
                                                    const char* rationale,
                                                    const char* alternative,
                                                    float confidence) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ReasoningStep step;
    memset(&step, 0, sizeof(step));
    step.stepId = nextReasoningId();
    step.confidence = confidence;

    auto now = std::chrono::system_clock::now();
    step.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(now));

    if (action)      strncpy(step.action, action, sizeof(step.action) - 1);
    if (rationale)   strncpy(step.rationale, rationale, sizeof(step.rationale) - 1);
    if (alternative) strncpy(step.alternative, alternative, sizeof(step.alternative) - 1);

    // Enforce max history size
    if ((int)m_reasoningHistory.size() >= m_config.maxReasoningHistory) {
        m_reasoningHistory.erase(m_reasoningHistory.begin());
    }

    m_reasoningHistory.push_back(step);
    m_stats.reasoningStepsRecorded++;

    // Fire callback
    if (m_reasoningCb) {
        m_reasoningCb(&step, m_reasoningCbUD);
    }

    return step.stepId;
}

void AutonomousCommunicator::updateReasoningOutcome(uint64_t stepId, const char* outcome) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& step : m_reasoningHistory) {
        if (step.stepId == stepId) {
            if (outcome) strncpy(step.outcome, outcome, sizeof(step.outcome) - 1);
            break;
        }
    }
}

std::vector<ReasoningStep> AutonomousCommunicator::getReasoningHistory(int maxSteps) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (maxSteps <= 0 || (int)m_reasoningHistory.size() <= maxSteps) {
        return m_reasoningHistory;
    }

    return std::vector<ReasoningStep>(
        m_reasoningHistory.end() - maxSteps,
        m_reasoningHistory.end());
}

std::string AutonomousCommunicator::reasoningToMarkdown(int maxSteps) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string md;
    md += "# Reasoning Trace\n\n";

    int start = ((int)m_reasoningHistory.size() > maxSteps)
                ? (int)m_reasoningHistory.size() - maxSteps
                : 0;

    for (int i = start; i < (int)m_reasoningHistory.size(); ++i) {
        const auto& step = m_reasoningHistory[i];
        char buf[2048];

        snprintf(buf, sizeof(buf),
            "## Step %llu (confidence: %.0f%%)\n"
            "- **Action:** %s\n"
            "- **Rationale:** %s\n",
            (unsigned long long)step.stepId,
            step.confidence * 100.0f,
            step.action,
            step.rationale);
        md += buf;

        if (step.alternative[0]) {
            snprintf(buf, sizeof(buf),
                "- **Alternative considered:** %s\n", step.alternative);
            md += buf;
        }
        if (step.outcome[0]) {
            snprintf(buf, sizeof(buf),
                "- **Outcome:** %s\n", step.outcome);
            md += buf;
        }
        md += "\n";
    }

    return md;
}

std::string AutonomousCommunicator::reasoningToJson(int maxSteps) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string json = "[";
    int start = ((int)m_reasoningHistory.size() > maxSteps)
                ? (int)m_reasoningHistory.size() - maxSteps
                : 0;

    for (int i = start; i < (int)m_reasoningHistory.size(); ++i) {
        if (i > start) json += ",";

        const auto& s = m_reasoningHistory[i];
        char buf[2048];
        snprintf(buf, sizeof(buf),
            "{"
            "\"stepId\":%llu,"
            "\"action\":\"%s\","
            "\"rationale\":\"%s\","
            "\"confidence\":%.3f,"
            "\"timestamp\":%llu"
            "}",
            (unsigned long long)s.stepId,
            s.action,
            s.rationale,
            s.confidence,
            (unsigned long long)s.timestamp);
        json += buf;
    }

    json += "]";
    return json;
}

// =============================================================================
// Change Tracking
// =============================================================================

void AutonomousCommunicator::recordChange(const char* file, int added, int removed,
                                            const char* changeType, const char* summary,
                                            float riskScore) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ChangeEntry entry;
    memset(&entry, 0, sizeof(entry));
    if (file)       strncpy(entry.file, file, sizeof(entry.file) - 1);
    entry.linesAdded   = added;
    entry.linesRemoved = removed;
    if (changeType) strncpy(entry.changeType, changeType, sizeof(entry.changeType) - 1);
    if (summary)    strncpy(entry.summary, summary, sizeof(entry.summary) - 1);
    entry.riskScore = riskScore;

    m_changes.push_back(entry);
}

void AutonomousCommunicator::clearChanges() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changes.clear();
}

std::vector<ChangeEntry> AutonomousCommunicator::getChanges() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_changes;
}

// =============================================================================
// Report Generation
// =============================================================================

StatusReport AutonomousCommunicator::generateReport(ReportType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    StatusReport report;
    memset(&report, 0, sizeof(report));
    report.reportId = const_cast<AutonomousCommunicator*>(this)->nextReportId();
    report.type     = type;

    auto now = std::chrono::system_clock::now();
    report.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(now));

    strncpy(report.author, m_config.agentName, sizeof(report.author) - 1);

    // Compute metrics from changes
    report.filesChanged = (int)m_changes.size();
    for (const auto& c : m_changes) {
        report.totalAdded   += c.linesAdded;
        report.totalRemoved += c.linesRemoved;
    }

    // If LLM report generator is set, use it
    if (m_reportGen) {
        std::string body = m_reportGen(type,
                                        m_changes.data(), (int)m_changes.size(),
                                        m_reasoningHistory.data(), (int)m_reasoningHistory.size(),
                                        m_reportGenUD);
        strncpy(report.body, body.c_str(), sizeof(report.body) - 1);
    } else {
        // Heuristic report
        std::string body = buildReportBody(type);
        strncpy(report.body, body.c_str(), sizeof(report.body) - 1);
    }

    // Generate title
    switch (type) {
        case ReportType::Standup:
            strncpy(report.title, "Daily Standup Report", sizeof(report.title) - 1);
            break;
        case ReportType::SprintSummary:
            strncpy(report.title, "Sprint Summary", sizeof(report.title) - 1);
            break;
        case ReportType::ChangeImpact:
            strncpy(report.title, "Change Impact Analysis", sizeof(report.title) - 1);
            break;
        case ReportType::SecurityAudit:
            strncpy(report.title, "Security Audit Report", sizeof(report.title) - 1);
            break;
        case ReportType::PerformanceReport:
            strncpy(report.title, "Performance Report", sizeof(report.title) - 1);
            break;
        case ReportType::CoverageReport:
            strncpy(report.title, "Test Coverage Report", sizeof(report.title) - 1);
            break;
        case ReportType::CustomReport:
            strncpy(report.title, "Custom Report", sizeof(report.title) - 1);
            break;
    }

    const_cast<Stats&>(m_stats).reportsGenerated++;

    // Fire callback
    if (m_reportCb) {
        m_reportCb(&report, m_reportCbUD);
    }

    return report;
}

std::string AutonomousCommunicator::buildReportBody(ReportType type) const {
    std::string body;
    char buf[1024];

    // Summary section
    snprintf(buf, sizeof(buf),
        "## Summary\n"
        "- Files changed: %d\n"
        "- Lines added: %d\n"
        "- Lines removed: %d\n"
        "- Reasoning steps: %d\n\n",
        (int)m_changes.size(),
        0, 0, // will be computed below
        (int)m_reasoningHistory.size());

    int totalAdded = 0, totalRemoved = 0;
    float maxRisk = 0.0f;
    for (const auto& c : m_changes) {
        totalAdded   += c.linesAdded;
        totalRemoved += c.linesRemoved;
        if (c.riskScore > maxRisk) maxRisk = c.riskScore;
    }

    snprintf(buf, sizeof(buf),
        "## Summary\n"
        "- Files changed: %d\n"
        "- Lines added: %d\n"
        "- Lines removed: %d\n"
        "- Net change: %+d\n"
        "- Max risk score: %.1f%%\n\n",
        (int)m_changes.size(), totalAdded, totalRemoved,
        totalAdded - totalRemoved, maxRisk * 100.0f);
    body += buf;

    // Changes detail
    if (!m_changes.empty()) {
        body += "## Changes\n";
        for (const auto& c : m_changes) {
            snprintf(buf, sizeof(buf),
                "- **%s** (%s): +%d/-%d — %s\n",
                c.file, c.changeType, c.linesAdded, c.linesRemoved, c.summary);
            body += buf;
        }
        body += "\n";
    }

    // Risk assessment
    if (type == ReportType::ChangeImpact || type == ReportType::SecurityAudit) {
        body += "## Risk Assessment\n";
        for (const auto& c : m_changes) {
            if (c.riskScore > 0.3f) {
                const char* level = (c.riskScore > 0.7f) ? "HIGH"
                                  : (c.riskScore > 0.5f) ? "MEDIUM"
                                  : "LOW";
                snprintf(buf, sizeof(buf),
                    "- [%s] %s (risk: %.0f%%)\n", level, c.file, c.riskScore * 100.0f);
                body += buf;
            }
        }
        body += "\n";
    }

    // Recent reasoning (last 5 steps)
    if (!m_reasoningHistory.empty()) {
        body += "## Recent Decisions\n";
        int start = ((int)m_reasoningHistory.size() > 5)
                    ? (int)m_reasoningHistory.size() - 5
                    : 0;
        for (int i = start; i < (int)m_reasoningHistory.size(); ++i) {
            const auto& s = m_reasoningHistory[i];
            snprintf(buf, sizeof(buf),
                "- %s — *%s*\n", s.action, s.rationale);
            body += buf;
        }
    }

    return body;
}

std::string AutonomousCommunicator::reportToMarkdown(const StatusReport& report) const {
    char buf[256];
    std::string md;

    snprintf(buf, sizeof(buf), "# %s\n", report.title);
    md += buf;

    snprintf(buf, sizeof(buf), "*Generated by %s at %llu*\n\n",
             report.author, (unsigned long long)report.timestamp);
    md += buf;

    md += report.body;
    return md;
}

std::string AutonomousCommunicator::reportToJson(const StatusReport& report) const {
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{"
        "\"reportId\":%llu,"
        "\"type\":%d,"
        "\"title\":\"%s\","
        "\"author\":\"%s\","
        "\"filesChanged\":%d,"
        "\"totalAdded\":%d,"
        "\"totalRemoved\":%d,"
        "\"timestamp\":%llu"
        "}",
        (unsigned long long)report.reportId,
        (int)report.type,
        report.title,
        report.author,
        report.filesChanged,
        report.totalAdded,
        report.totalRemoved,
        (unsigned long long)report.timestamp);
    return std::string(buf);
}

// =============================================================================
// Commit Message Generation
// =============================================================================

CommitMessage AutonomousCommunicator::generateCommitMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return generateCommitMessage(m_changes.data(), (int)m_changes.size());
}

CommitMessage AutonomousCommunicator::generateCommitMessage(
    const ChangeEntry* changes, int count) const {

    // If LLM generator is set, use it
    if (m_commitGen) {
        std::string llmMsg = m_commitGen(changes, count, m_config.commitStyle, m_commitGenUD);
        CommitMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.style = m_config.commitStyle;
        strncpy(msg.subject, llmMsg.c_str(),
                (llmMsg.length() < sizeof(msg.subject) - 1) ? llmMsg.length() : sizeof(msg.subject) - 1);
        return msg;
    }

    // Heuristic commit message generation
    CommitMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.style = m_config.commitStyle;

    if (count == 0) {
        strncpy(msg.type, "chore", sizeof(msg.type) - 1);
        strncpy(msg.subject, "empty commit", sizeof(msg.subject) - 1);
        return msg;
    }

    // Determine type from change patterns
    bool hasNew = false, hasModified = false, hasDeleted = false;
    int totalAdded = 0, totalRemoved = 0;

    for (int i = 0; i < count; ++i) {
        if (strcmp(changes[i].changeType, "added") == 0)    hasNew = true;
        if (strcmp(changes[i].changeType, "modified") == 0) hasModified = true;
        if (strcmp(changes[i].changeType, "deleted") == 0)  hasDeleted = true;
        totalAdded   += changes[i].linesAdded;
        totalRemoved += changes[i].linesRemoved;
    }

    if (hasNew && !hasModified && !hasDeleted) {
        strncpy(msg.type, "feat", sizeof(msg.type) - 1);
    } else if (hasDeleted && !hasNew) {
        strncpy(msg.type, "refactor", sizeof(msg.type) - 1);
    } else if (count == 1 && totalRemoved > totalAdded) {
        strncpy(msg.type, "fix", sizeof(msg.type) - 1);
    } else {
        strncpy(msg.type, "feat", sizeof(msg.type) - 1);
    }

    // Extract scope from first file's directory
    if (count > 0 && changes[0].file[0]) {
        const char* lastSlash = strrchr(changes[0].file, '/');
        const char* lastBackslash = strrchr(changes[0].file, '\\');
        const char* sep = lastSlash > lastBackslash ? lastSlash : lastBackslash;
        if (sep) {
            // Find the directory component
            const char* start = changes[0].file;
            // Find second-to-last separator for scope
            const char* prevSep = nullptr;
            for (const char* p = start; p < sep; ++p) {
                if (*p == '/' || *p == '\\') prevSep = p;
            }
            if (prevSep) {
                size_t len = static_cast<size_t>(sep - prevSep - 1);
                if (len > sizeof(msg.scope) - 1) len = sizeof(msg.scope) - 1;
                strncpy(msg.scope, prevSep + 1, len);
            }
        }
    }

    // Subject from change summaries
    if (count == 1) {
        strncpy(msg.subject, changes[0].summary, sizeof(msg.subject) - 1);
    } else {
        snprintf(msg.subject, sizeof(msg.subject),
                 "update %d files (+%d/-%d)", count, totalAdded, totalRemoved);
    }

    // Body with file list
    std::string body;
    for (int i = 0; i < count && i < 20; ++i) {
        char line[512];
        snprintf(line, sizeof(line), "- %s: %s\n", changes[i].file, changes[i].summary);
        body += line;
    }
    strncpy(msg.body, body.c_str(),
            (body.length() < sizeof(msg.body) - 1) ? body.length() : sizeof(msg.body) - 1);

    const_cast<Stats&>(m_stats).commitsGenerated++;
    return msg;
}

// =============================================================================
// PR Description Generation
// =============================================================================

PRDescription AutonomousCommunicator::generatePRDescription(const char* branchName) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // If LLM generator is set, use it
    if (m_prDescGen) {
        return m_prDescGen(m_changes.data(), (int)m_changes.size(), branchName, m_prDescGenUD);
    }

    PRDescription pr;
    memset(&pr, 0, sizeof(pr));
    pr.sectionCount = 0;

    if (branchName) strncpy(pr.branch, branchName, sizeof(pr.branch) - 1);

    // Generate title from changes
    if (m_changes.empty()) {
        strncpy(pr.title, "Empty PR", sizeof(pr.title) - 1);
    } else if (m_changes.size() == 1) {
        snprintf(pr.title, sizeof(pr.title), "%s", m_changes[0].summary);
    } else {
        int totalAdded = 0, totalRemoved = 0;
        for (const auto& c : m_changes) {
            totalAdded   += c.linesAdded;
            totalRemoved += c.linesRemoved;
        }
        snprintf(pr.title, sizeof(pr.title),
                 "Update %zu files (+%d/-%d)",
                 m_changes.size(), totalAdded, totalRemoved);
    }

    // Section 1: Summary
    auto& summary = pr.sections[pr.sectionCount++];
    strncpy(summary.heading, "Summary", sizeof(summary.heading) - 1);

    std::string summaryContent;
    for (const auto& c : m_changes) {
        char buf[512];
        snprintf(buf, sizeof(buf), "- **%s** (%s): %s\n",
                 c.file, c.changeType, c.summary);
        summaryContent += buf;
    }
    strncpy(summary.content, summaryContent.c_str(), sizeof(summary.content) - 1);

    // Section 2: Reasoning
    if (!m_reasoningHistory.empty()) {
        auto& reasoning = pr.sections[pr.sectionCount++];
        strncpy(reasoning.heading, "Reasoning", sizeof(reasoning.heading) - 1);

        std::string reasoningContent;
        int start = ((int)m_reasoningHistory.size() > 10)
                    ? (int)m_reasoningHistory.size() - 10
                    : 0;
        for (int i = start; i < (int)m_reasoningHistory.size(); ++i) {
            char buf[768];
            snprintf(buf, sizeof(buf), "- %s: *%s*\n",
                     m_reasoningHistory[i].action, m_reasoningHistory[i].rationale);
            reasoningContent += buf;
        }
        strncpy(reasoning.content, reasoningContent.c_str(), sizeof(reasoning.content) - 1);
    }

    // Section 3: Risk
    {
        auto& risk = pr.sections[pr.sectionCount++];
        strncpy(risk.heading, "Risk Assessment", sizeof(risk.heading) - 1);

        float maxRisk = 0.0f;
        for (const auto& c : m_changes) {
            if (c.riskScore > maxRisk) maxRisk = c.riskScore;
        }

        const char* level = (maxRisk > 0.7f) ? "HIGH"
                          : (maxRisk > 0.4f) ? "MEDIUM"
                          : "LOW";
        snprintf(risk.content, sizeof(risk.content),
                 "Overall risk level: **%s** (max score: %.0f%%)", level, maxRisk * 100.0f);
    }

    // Build body from sections
    std::string body;
    for (int i = 0; i < pr.sectionCount; ++i) {
        char buf[128];
        snprintf(buf, sizeof(buf), "## %s\n", pr.sections[i].heading);
        body += buf;
        body += pr.sections[i].content;
        body += "\n\n";
    }
    strncpy(pr.body, body.c_str(), sizeof(pr.body) - 1);

    const_cast<Stats&>(m_stats).prsGenerated++;
    return pr;
}

// =============================================================================
// Webhook Delivery
// =============================================================================

CommResult AutonomousCommunicator::addWebhook(const WebhookConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate
    for (const auto& wh : m_webhooks) {
        if (wh.target == config.target && strcmp(wh.url, config.url) == 0) {
            return CommResult::error("Webhook already registered");
        }
    }

    m_webhooks.push_back(config);
    return CommResult::ok("Webhook added");
}

CommResult AutonomousCommunicator::removeWebhook(WebhookTarget target) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_webhooks.begin(), m_webhooks.end(),
                            [target](const WebhookConfig& wh) { return wh.target == target; });
    if (it == m_webhooks.end()) {
        return CommResult::error("Webhook not found");
    }

    m_webhooks.erase(it);
    return CommResult::ok("Webhook removed");
}

CommResult AutonomousCommunicator::sendToWebhook(WebhookTarget target, const char* message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& wh : m_webhooks) {
        if (wh.target == target && wh.enabled) {
            // Rate limit check
            if (wh.sentThisHour >= wh.rateLimit) {
                return CommResult::error("Rate limit exceeded");
            }

            CommResult result = CommResult::ok("Sent");

            switch (target) {
                case WebhookTarget::Console:
                    printf("[%s] %s\n", m_config.agentName, message);
                    result = CommResult::ok("Printed to console");
                    break;
                case WebhookTarget::Slack:
                    result = sendSlack(wh, message);
                    break;
                case WebhookTarget::Teams:
                    result = sendTeams(wh, message);
                    break;
                case WebhookTarget::Discord:
                    result = sendDiscord(wh, message);
                    break;
                case WebhookTarget::Generic:
                    result = sendHttp(wh.url, wh.authToken, message, "application/json");
                    break;
                case WebhookTarget::File: {
                    FILE* f = fopen(wh.url, "a");
                    if (f) {
                        fprintf(f, "%s\n", message);
                        fclose(f);
                        result = CommResult::ok("Written to file");
                    } else {
                        result = CommResult::error("Failed to open output file");
                    }
                    break;
                }
            }

            if (result.success) {
                wh.sentThisHour++;
                m_stats.webhooksSent++;
            } else {
                m_stats.webhooksFailed++;
            }

            if (m_webhookCb) {
                m_webhookCb(target, result.success, m_webhookCbUD);
            }

            return result;
        }
    }

    return CommResult::error("No enabled webhook found for target");
}

CommResult AutonomousCommunicator::broadcastReport(const StatusReport& report) {
    std::string md = reportToMarkdown(report);

    int sent = 0, failed = 0;
    for (auto& wh : m_webhooks) {
        if (wh.enabled) {
            CommResult r = sendToWebhook(wh.target, md.c_str());
            if (r.success) sent++;
            else failed++;
        }
    }

    if (sent == 0 && failed > 0) {
        return CommResult::error("All webhook deliveries failed");
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Broadcast to %d/%d webhooks", sent, sent + failed);
    return CommResult::ok(msg);
}

// =============================================================================
// HTTP Helpers (Windows WinHTTP)
// =============================================================================

CommResult AutonomousCommunicator::sendHttp(const char* url, const char* authToken,
                                              const char* body, const char* contentType) {
#ifdef _WIN32
    // Parse URL
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    // Convert URL to wide string
    wchar_t wideUrl[1024] = {};
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wideUrl, 1024);

    if (!WinHttpCrackUrl(wideUrl, 0, 0, &urlComp)) {
        return CommResult::error("Failed to parse URL", (int)GetLastError());
    }

    HINTERNET session = WinHttpOpen(L"RawrXD-Agent/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return CommResult::error("WinHTTP session failed", (int)GetLastError());
    }

    HINTERNET connect = WinHttpConnect(session, hostName, urlComp.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return CommResult::error("WinHTTP connect failed", (int)GetLastError());
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"POST", urlPath,
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return CommResult::error("WinHTTP request failed", (int)GetLastError());
    }

    // Add auth header
    if (authToken && authToken[0]) {
        wchar_t authHeader[768];
        wchar_t wideToken[512];
        MultiByteToWideChar(CP_UTF8, 0, authToken, -1, wideToken, 512);
        swprintf(authHeader, 768, L"Authorization: Bearer %s", wideToken);
        WinHttpAddRequestHeaders(request, authHeader, (ULONG)-1,
                                  WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Content-Type header
    wchar_t ctHeader[256];
    wchar_t wideCT[128];
    MultiByteToWideChar(CP_UTF8, 0, contentType, -1, wideCT, 128);
    swprintf(ctHeader, 256, L"Content-Type: %s", wideCT);
    WinHttpAddRequestHeaders(request, ctHeader, (ULONG)-1,
                              WINHTTP_ADDREQ_FLAG_ADD);

    DWORD bodyLen = (DWORD)strlen(body);
    BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)body, bodyLen, bodyLen, 0);
    if (!sent) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return CommResult::error("WinHTTP send failed", (int)GetLastError());
    }

    WinHttpReceiveResponse(request, NULL);

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request,
                         WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                         WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, NULL);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (statusCode >= 200 && statusCode < 300) {
        return CommResult::ok("HTTP POST successful");
    }

    return CommResult::error("HTTP POST failed", (int)statusCode);
#else
    // POSIX: HTTP POST via raw socket
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>

    // Parse host/port from URL
    std::string urlStr(url);
    std::string host;
    int port = 80;
    size_t schemeEnd = urlStr.find("://");
    size_t hostStart = (schemeEnd != std::string::npos) ? schemeEnd + 3 : 0;
    if (urlStr.substr(0, 5) == "https") port = 443;
    size_t pathStart = urlStr.find('/', hostStart);
    std::string hostPort = urlStr.substr(hostStart, pathStart - hostStart);
    std::string path = (pathStart != std::string::npos) ? urlStr.substr(pathStart) : "/";
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = hostPort.substr(0, colonPos);
        port = std::atoi(hostPort.substr(colonPos + 1).c_str());
    } else {
        host = hostPort;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        return CommResult::error("DNS resolution failed");
    }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0 || connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        return CommResult::error("TCP connection failed");
    }
    freeaddrinfo(res);

    std::string bodyStr(body);
    std::string ct(contentType);
    std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + host +
        "\r\nContent-Type: " + ct +
        "\r\nContent-Length: " + std::to_string(bodyStr.size());
    if (authToken && authToken[0]) {
        req += "\r\nAuthorization: Bearer ";
        req += authToken;
    }
    req += "\r\nConnection: close\r\n\r\n" + bodyStr;
    ::send(sock, req.c_str(), req.size(), 0);

    // Read response status
    char buf[512];
    ssize_t n = ::recv(sock, buf, sizeof(buf) - 1, 0);
    ::close(sock);
    if (n <= 0) return CommResult::error("No response from server");
    buf[n] = '\0';

    // Parse HTTP status code
    int statusCode = 0;
    if (sscanf(buf, "HTTP/%*s %d", &statusCode) == 1 && statusCode >= 200 && statusCode < 300) {
        return CommResult::ok("HTTP POST successful");
    }
    return CommResult::error("HTTP POST failed", statusCode);
#endif
}

CommResult AutonomousCommunicator::sendSlack(const WebhookConfig& wh, const char* message) {
    // Slack webhook format: {"text": "message"}
    char payload[8192];
    snprintf(payload, sizeof(payload), "{\"text\":\"%s\"}", message);
    return sendHttp(wh.url, wh.authToken, payload, "application/json");
}

CommResult AutonomousCommunicator::sendTeams(const WebhookConfig& wh, const char* message) {
    // Teams webhook format: {"@type":"MessageCard","text":"message"}
    char payload[8192];
    snprintf(payload, sizeof(payload),
        "{\"@type\":\"MessageCard\",\"text\":\"%s\"}", message);
    return sendHttp(wh.url, wh.authToken, payload, "application/json");
}

CommResult AutonomousCommunicator::sendDiscord(const WebhookConfig& wh, const char* message) {
    // Discord webhook format: {"content":"message"}
    char payload[8192];
    snprintf(payload, sizeof(payload), "{\"content\":\"%s\"}", message);
    return sendHttp(wh.url, wh.authToken, payload, "application/json");
}

// =============================================================================
// Sub-system Injection
// =============================================================================

void AutonomousCommunicator::setReportGenerator(ReportGeneratorFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reportGen = fn;
    m_reportGenUD = ud;
}

void AutonomousCommunicator::setCommitMsgGenerator(CommitMsgGeneratorFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commitGen = fn;
    m_commitGenUD = ud;
}

void AutonomousCommunicator::setPRDescGenerator(PRDescGeneratorFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_prDescGen = fn;
    m_prDescGenUD = ud;
}

// =============================================================================
// Callbacks
// =============================================================================

void AutonomousCommunicator::setReportCallback(ReportGeneratedCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reportCb = cb;
    m_reportCbUD = ud;
}

void AutonomousCommunicator::setWebhookCallback(WebhookSentCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_webhookCb = cb;
    m_webhookCbUD = ud;
}

void AutonomousCommunicator::setReasoningCallback(ReasoningStepCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_reasoningCb = cb;
    m_reasoningCbUD = ud;
}

// =============================================================================
// ID Generation
// =============================================================================

uint64_t AutonomousCommunicator::nextReasoningId() {
    return m_nextReasoning.fetch_add(1);
}

uint64_t AutonomousCommunicator::nextReportId() {
    return m_nextReport.fetch_add(1);
}

// =============================================================================
// Stats
// =============================================================================

AutonomousCommunicator::Stats AutonomousCommunicator::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

std::string AutonomousCommunicator::statsToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{"
        "\"reasoningStepsRecorded\":%llu,"
        "\"reportsGenerated\":%llu,"
        "\"webhooksSent\":%llu,"
        "\"webhooksFailed\":%llu,"
        "\"commitsGenerated\":%llu,"
        "\"prsGenerated\":%llu"
        "}",
        (unsigned long long)m_stats.reasoningStepsRecorded,
        (unsigned long long)m_stats.reportsGenerated,
        (unsigned long long)m_stats.webhooksSent,
        (unsigned long long)m_stats.webhooksFailed,
        (unsigned long long)m_stats.commitsGenerated,
        (unsigned long long)m_stats.prsGenerated);
    return std::string(buf);
}

} // namespace Autonomy
} // namespace RawrXD
