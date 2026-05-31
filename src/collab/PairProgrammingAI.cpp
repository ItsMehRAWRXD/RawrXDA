// ============================================================================
// PairProgrammingAI.cpp — AI-Assisted Pair Programming Implementation
// ============================================================================

#include "PairProgrammingAI.h"
#include "LiveShareSession.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace RawrXD {

// ============================================================================
// Construction / Destruction
// ============================================================================

PairProgrammingAI::PairProgrammingAI() = default;
PairProgrammingAI::~PairProgrammingAI() = default;

void PairProgrammingAI::setSession(LiveShareSession* session) {
    m_session = session;
}

// ============================================================================
// Role management
// ============================================================================

bool PairProgrammingAI::assignRole(const std::string& userId, PairRole role) {
    {
        std::lock_guard<std::mutex> lk(m_rolesMutex);
        PairRole old = m_roles[userId];
        if (old == role) return true;
        m_roles[userId] = role;
    }
    if (onRoleChanged) onRoleChanged(userId, role);
    return true;
}

PairRole PairProgrammingAI::getRole(const std::string& userId) const {
    std::lock_guard<std::mutex> lk(m_rolesMutex);
    auto it = m_roles.find(userId);
    return (it != m_roles.end()) ? it->second : PairRole::None;
}

void PairProgrammingAI::swapRoles(const std::string& userA, const std::string& userB) {
    PairRole rA, rB;
    {
        std::lock_guard<std::mutex> lk(m_rolesMutex);
        rA = m_roles[userA];
        rB = m_roles[userB];
        m_roles[userA] = rB;
        m_roles[userB] = rA;
    }
    if (onRoleChanged) {
        onRoleChanged(userA, rB);
        onRoleChanged(userB, rA);
    }
}

std::unordered_map<std::string, PairRole> PairProgrammingAI::getAllRoles() const {
    std::lock_guard<std::mutex> lk(m_rolesMutex);
    return m_roles;
}

bool PairProgrammingAI::autoAssignRoles() {
    auto now = std::chrono::steady_clock::now();
    std::unordered_map<std::string, int> editCounts;

    {
        std::lock_guard<std::mutex> lk(m_activityMutex);
        for (auto& entry : m_activityLog) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - entry.when).count();
            if (elapsed <= kAutoRoleActivityWindowSec && entry.isEdit)
                editCounts[entry.userId]++;
        }
    }

    if (editCounts.empty()) return false;

    // Most-active editor becomes Driver
    std::string mostActive;
    int maxEdits = 0;
    for (auto& [uid, count] : editCounts) {
        if (count > maxEdits) {
            maxEdits = count;
            mostActive = uid;
        }
    }

    {
        std::lock_guard<std::mutex> lk(m_rolesMutex);
        for (auto& [uid, _] : m_roles) {
            m_roles[uid] = (uid == mostActive) ? PairRole::Driver : PairRole::Navigator;
        }
        // Ensure mostActive is in the map
        m_roles[mostActive] = PairRole::Driver;
    }

    // Fire callbacks
    std::unordered_map<std::string, PairRole> snapshot;
    {
        std::lock_guard<std::mutex> lk(m_rolesMutex);
        snapshot = m_roles;
    }
    if (onRoleChanged) {
        for (auto& [uid, role] : snapshot)
            onRoleChanged(uid, role);
    }
    return true;
}

// ============================================================================
// Context tracking
// ============================================================================

void PairProgrammingAI::updateParticipantContext(const ParticipantContext& ctx) {
    std::lock_guard<std::mutex> lk(m_contextMutex);
    m_contexts[ctx.userId] = ctx;
}

ParticipantContext PairProgrammingAI::getParticipantContext(const std::string& userId) const {
    std::lock_guard<std::mutex> lk(m_contextMutex);
    auto it = m_contexts.find(userId);
    if (it != m_contexts.end()) return it->second;
    return {};
}

// ============================================================================
// Activity tracking
// ============================================================================

void PairProgrammingAI::recordEdit(const std::string& userId, const std::string& filePath, int line) {
    std::lock_guard<std::mutex> lk(m_activityMutex);
    m_activityLog.push_back({userId, filePath, line, std::chrono::steady_clock::now(), true});
    while (m_activityLog.size() > 1000)
        m_activityLog.pop_front();
}

void PairProgrammingAI::recordNavigation(const std::string& userId, const std::string& filePath, int line) {
    std::lock_guard<std::mutex> lk(m_activityMutex);
    m_activityLog.push_back({userId, filePath, line, std::chrono::steady_clock::now(), false});
    while (m_activityLog.size() > 1000)
        m_activityLog.pop_front();
}

// ============================================================================
// AI suggestion pipeline
// ============================================================================

void PairProgrammingAI::requestSuggestion(const std::string& forUserId) {
    ParticipantContext ctx;
    {
        std::lock_guard<std::mutex> lk(m_contextMutex);
        auto it = m_contexts.find(forUserId);
        if (it == m_contexts.end()) return;
        ctx = it->second;
    }

    PairRole role = getRole(forUserId);
    PairSuggestion suggestion;

    if (role == PairRole::Driver) {
        suggestion = generateCompletionSuggestion(ctx);
    } else if (role == PairRole::Navigator) {
        // Find the driver's context for navigator hints
        ParticipantContext driverCtx;
        {
            std::lock_guard<std::mutex> lk(m_rolesMutex);
            for (auto& [uid, r] : m_roles) {
                if (r == PairRole::Driver && uid != forUserId) {
                    std::lock_guard<std::mutex> clk(m_contextMutex);
                    auto dit = m_contexts.find(uid);
                    if (dit != m_contexts.end()) driverCtx = dit->second;
                    break;
                }
            }
        }
        suggestion = generateNavigatorHint(driverCtx, ctx);
    } else {
        suggestion = generateCompletionSuggestion(ctx);
    }

    suggestion.sourceUserId = forUserId;
    suggestion.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lk(m_suggestionsMutex);
        auto& q = m_suggestions[forUserId];
        q.push_back(suggestion);
        while (q.size() > kMaxSuggestionsPerUser)
            q.pop_front();
    }

    if (onSuggestionGenerated) onSuggestionGenerated(suggestion);
}

std::vector<PairSuggestion> PairProgrammingAI::getPendingSuggestions(const std::string& forUserId, size_t maxCount) {
    std::lock_guard<std::mutex> lk(m_suggestionsMutex);
    auto it = m_suggestions.find(forUserId);
    if (it == m_suggestions.end()) return {};

    std::vector<PairSuggestion> out;
    size_t count = std::min(maxCount, it->second.size());
    for (size_t i = 0; i < count; ++i)
        out.push_back(it->second[i]);
    return out;
}

void PairProgrammingAI::acceptSuggestion(const std::string& /*suggestionId*/) {
    // In a full implementation, this would apply the suggestion's code
    // and log the acceptance for learning feedback.
}

void PairProgrammingAI::dismissSuggestion(const std::string& /*suggestionId*/) {
    // Log dismissal for negative feedback to the suggestion model.
}

// ============================================================================
// Conflict detection & resolution
// ============================================================================

std::vector<EditConflict> PairProgrammingAI::detectConflicts() const {
    std::vector<EditConflict> conflicts;
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lk(m_activityMutex);

    // Group recent edits by file+region (within 5 seconds, within 5 lines)
    struct RecentEdit { std::string userId; std::string file; int line; };
    std::vector<RecentEdit> recentEdits;

    for (auto& entry : m_activityLog) {
        if (!entry.isEdit) continue;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - entry.when).count();
        if (elapsed > 10) continue;
        recentEdits.push_back({entry.userId, entry.filePath, entry.line});
    }

    // O(n^2) pairwise check — fine for small N (typical 2-5 participants)
    for (size_t i = 0; i < recentEdits.size(); ++i) {
        for (size_t j = i + 1; j < recentEdits.size(); ++j) {
            auto& a = recentEdits[i];
            auto& b = recentEdits[j];
            if (a.userId == b.userId) continue;
            if (a.file != b.file) continue;
            if (std::abs(a.line - b.line) > 5) continue;

            // Conflict: two users editing nearby lines in the same file
            EditConflict c;
            c.filePath = a.file;
            c.startLine = std::min(a.line, b.line);
            c.endLine = std::max(a.line, b.line);
            c.userA = a.userId;
            c.userB = b.userId;
            conflicts.push_back(c);
        }
    }

    return conflicts;
}

PairSuggestion PairProgrammingAI::generateConflictResolution(const EditConflict& conflict) {
    PairSuggestion s;
    s.kind = PairSuggestion::Kind::ConflictResolution;
    s.title = "Edit Conflict Detected";
    s.filePath = conflict.filePath;
    s.line = conflict.startLine;
    s.confidence = 0.7f;

    if (llmInfer) {
        std::string systemPrompt =
            "You are a pair programming assistant. Two developers are editing the same region of code. "
            "Suggest how to merge their changes cleanly. Be concise.";
        std::string userPrompt =
            "File: " + conflict.filePath + "\n"
            "Lines " + std::to_string(conflict.startLine) + "-" + std::to_string(conflict.endLine) + "\n"
            "User A: " + conflict.userA + "\n"
            "User B: " + conflict.userB + "\n"
            "Suggest a resolution strategy.";
        s.description = llmInfer(systemPrompt, userPrompt);
    } else {
        s.description = "Both " + conflict.userA + " and " + conflict.userB +
                        " are editing lines " + std::to_string(conflict.startLine) + "-" +
                        std::to_string(conflict.endLine) + " in " + conflict.filePath +
                        ". Consider coordinating: one person takes the lead on this section.";
    }

    return s;
}

// ============================================================================
// Code review assist
// ============================================================================

PairSuggestion PairProgrammingAI::reviewCodeAtCursor(const std::string& filePath, int line, const std::string& visibleCode) {
    PairSuggestion s;
    s.kind = PairSuggestion::Kind::ReviewComment;
    s.title = "Code Review";
    s.filePath = filePath;
    s.line = line;
    s.confidence = 0.6f;

    if (llmInfer) {
        s.description = llmInfer(buildReviewPrompt(visibleCode, line), "Review this code region.");
    } else {
        s.description = "Review the code around line " + std::to_string(line) +
                        " in " + filePath + " for correctness and clarity.";
    }

    s.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return s;
}

// ============================================================================
// Combined context prompt
// ============================================================================

std::string PairProgrammingAI::buildPairContextPrompt() const {
    std::ostringstream oss;
    oss << "=== Pair Programming Context ===\n\n";

    std::lock_guard<std::mutex> clk(m_contextMutex);
    std::lock_guard<std::mutex> rlk(m_rolesMutex);

    for (auto& [uid, ctx] : m_contexts) {
        auto rit = m_roles.find(uid);
        std::string roleStr = "none";
        if (rit != m_roles.end()) {
            switch (rit->second) {
                case PairRole::Driver:    roleStr = "Driver"; break;
                case PairRole::Navigator: roleStr = "Navigator"; break;
                default: break;
            }
        }

        oss << "--- Participant: " << uid << " (Role: " << roleStr << ") ---\n";
        oss << "Active file: " << ctx.activeFile << "\n";
        oss << "Cursor: line " << ctx.cursorLine << ", col " << ctx.cursorColumn << "\n";
        if (!ctx.visibleCode.empty()) {
            oss << "Visible code:\n```\n" << ctx.visibleCode << "\n```\n";
        }
        if (!ctx.recentEdits.empty()) {
            oss << "Recent edits: " << ctx.recentEdits << "\n";
        }
        oss << "\n";
    }

    return oss.str();
}

// ============================================================================
// Tick (periodic)
// ============================================================================

void PairProgrammingAI::tick() {
    // Detect conflicts
    auto conflicts = detectConflicts();
    for (auto& c : conflicts) {
        if (onConflictDetected) onConflictDetected(c);
    }

    // Prune old activity
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    {
        std::lock_guard<std::mutex> lk(m_activityMutex);
        while (!m_activityLog.empty() && m_activityLog.front().when < cutoff)
            m_activityLog.pop_front();
    }
}

// ============================================================================
// Internal: generate suggestions
// ============================================================================

PairSuggestion PairProgrammingAI::generateCompletionSuggestion(const ParticipantContext& ctx) {
    PairSuggestion s;
    s.kind = PairSuggestion::Kind::CodeCompletion;
    s.title = "Completion Suggestion";
    s.filePath = ctx.activeFile;
    s.line = ctx.cursorLine;
    s.column = ctx.cursorColumn;
    s.confidence = 0.5f;

    if (llmInfer) {
        std::string systemPrompt =
            "You are a pair programming AI assistant. Generate a short, useful code completion "
            "for the driver at their current cursor position. Output ONLY the code to insert.";
        std::string pairCtx = buildPairContextPrompt();
        std::string userPrompt = pairCtx + "\nGenerate completion for " + ctx.userId +
                                  " at line " + std::to_string(ctx.cursorLine) +
                                  " in " + ctx.activeFile + ".";
        s.codeSnippet = llmInfer(systemPrompt, userPrompt);
    } else {
        s.description = "Code completion at line " + std::to_string(ctx.cursorLine) +
                        " (LLM not connected — hook llmInfer for live suggestions).";
    }

    return s;
}

PairSuggestion PairProgrammingAI::generateNavigatorHint(const ParticipantContext& driver, const ParticipantContext& navigator) {
    PairSuggestion s;
    s.kind = PairSuggestion::Kind::NextStep;
    s.title = "Navigator Hint";
    s.filePath = driver.activeFile;
    s.line = driver.cursorLine;
    s.confidence = 0.4f;

    if (llmInfer) {
        std::string systemPrompt =
            "You are a pair programming navigator AI. The driver is actively coding. "
            "Suggest the next logical step or point out something the driver might miss. Be concise.";
        std::string pairCtx = buildPairContextPrompt();
        s.description = llmInfer(systemPrompt, pairCtx + "\nWhat should the navigator suggest next?");
    } else {
        s.description = "Navigator hint: driver is working on " + driver.activeFile +
                        " line " + std::to_string(driver.cursorLine) +
                        ". Consider reviewing the surrounding logic.";
    }

    return s;
}

std::string PairProgrammingAI::buildReviewPrompt(const std::string& code, int line) const {
    return "You are a senior code reviewer in a pair programming session. "
           "Review the following code around line " + std::to_string(line) +
           " for bugs, style issues, and improvement opportunities. Be concise.\n\n"
           "Code:\n```\n" + code + "\n```";
}

// ============================================================================
// Diagnostics
// ============================================================================

nlohmann::json PairProgrammingAI::getDiagnostics() const {
    nlohmann::json diag;

    {
        std::lock_guard<std::mutex> lk(m_rolesMutex);
        nlohmann::json roles;
        for (auto& [uid, role] : m_roles) {
            std::string rs;
            switch (role) {
                case PairRole::Driver:    rs = "driver"; break;
                case PairRole::Navigator: rs = "navigator"; break;
                default: rs = "none"; break;
            }
            roles[uid] = rs;
        }
        diag["roles"] = roles;
    }

    {
        std::lock_guard<std::mutex> lk(m_contextMutex);
        diag["trackedParticipants"] = m_contexts.size();
    }

    {
        std::lock_guard<std::mutex> lk(m_suggestionsMutex);
        size_t total = 0;
        for (auto& [_, q] : m_suggestions) total += q.size();
        diag["pendingSuggestions"] = total;
    }

    {
        std::lock_guard<std::mutex> lk(m_activityMutex);
        diag["activityLogSize"] = m_activityLog.size();
    }

    diag["llmConnected"] = (bool)llmInfer;
    return diag;
}

} // namespace RawrXD
