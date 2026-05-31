// ============================================================================
// PairProgrammingAI.h — AI-Assisted Pair Programming
// ============================================================================
// Driver/Navigator role model with AI suggestions, conflict resolution,
// context-aware completions considering both participants' focus.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include <functional>
#include <deque>
#include <chrono>
#include <cstdint>

namespace RawrXD {

class LiveShareSession;

// ---- Pair programming roles ----
enum class PairRole : uint8_t {
    None      = 0,
    Driver    = 1,   // Actively typing / controlling
    Navigator = 2    // Reviewing, guiding, suggesting
};

// ---- AI suggestion from the pair programming engine ----
struct PairSuggestion {
    enum class Kind : uint8_t {
        CodeCompletion,    // Inline completion at cursor
        Refactor,          // Suggested refactoring
        BugWarning,        // Potential bug detected
        ConflictResolution,// Two users editing conflicting regions
        ReviewComment,     // Code review annotation
        NextStep           // Suggested next action in workflow
    };

    Kind kind             = Kind::CodeCompletion;
    std::string title;
    std::string description;
    std::string codeSnippet;         // Suggested code (if applicable)
    std::string filePath;
    int line              = 0;
    int column            = 0;
    float confidence      = 0.0f;    // 0.0–1.0
    std::string sourceUserId;        // Which user's context triggered this
    int64_t timestamp     = 0;
};

// ---- Context snapshot for a participant ----
struct ParticipantContext {
    std::string userId;
    std::string activeFile;
    int cursorLine = 0;
    int cursorColumn = 0;
    std::string visibleCode;          // ~50 lines around cursor
    std::string recentEdits;          // Last few edits summary
    PairRole role = PairRole::None;
    std::chrono::steady_clock::time_point lastActivity;
};

// ---- Conflict region between two users ----
struct EditConflict {
    std::string filePath;
    int startLine = 0;
    int endLine   = 0;
    std::string userA;
    std::string userB;
    std::string resolution;          // AI-suggested merged version
};

// ============================================================================
// PairProgrammingAI
// ============================================================================
class PairProgrammingAI {
public:
    PairProgrammingAI();
    ~PairProgrammingAI();

    PairProgrammingAI(const PairProgrammingAI&) = delete;
    PairProgrammingAI& operator=(const PairProgrammingAI&) = delete;

    // ---- Session binding ----
    void setSession(LiveShareSession* session);

    // ---- Role management ----
    bool assignRole(const std::string& userId, PairRole role);
    PairRole getRole(const std::string& userId) const;
    void swapRoles(const std::string& userA, const std::string& userB);
    bool autoAssignRoles();   // Heuristic: most-active = Driver
    std::unordered_map<std::string, PairRole> getAllRoles() const;

    // ---- Context tracking ----
    void updateParticipantContext(const ParticipantContext& ctx);
    ParticipantContext getParticipantContext(const std::string& userId) const;

    // ---- AI suggestion pipeline ----
    void requestSuggestion(const std::string& forUserId);
    std::vector<PairSuggestion> getPendingSuggestions(const std::string& forUserId, size_t maxCount = 5);
    void acceptSuggestion(const std::string& suggestionId);
    void dismissSuggestion(const std::string& suggestionId);

    // ---- Conflict detection & resolution ----
    std::vector<EditConflict> detectConflicts() const;
    PairSuggestion generateConflictResolution(const EditConflict& conflict);

    // ---- Code review assist ----
    PairSuggestion reviewCodeAtCursor(const std::string& filePath, int line, const std::string& visibleCode);

    // ---- Combined context prompt ----
    std::string buildPairContextPrompt() const;

    // ---- Activity tracking (for auto-role and focus) ----
    void recordEdit(const std::string& userId, const std::string& filePath, int line);
    void recordNavigation(const std::string& userId, const std::string& filePath, int line);

    // ---- Tick (periodic processing) ----
    void tick();

    // ---- Callbacks ----
    std::function<void(const PairSuggestion&)> onSuggestionGenerated;
    std::function<void(const EditConflict&)>   onConflictDetected;
    std::function<void(const std::string& userId, PairRole)> onRoleChanged;

    // ---- LLM integration hook ----
    // Set this to route prompts to the local inference engine.
    // Input: system prompt + user prompt. Output: model response.
    std::function<std::string(const std::string& systemPrompt, const std::string& userPrompt)> llmInfer;

    // ---- Diagnostics ----
    nlohmann::json getDiagnostics() const;

    static constexpr size_t kMaxSuggestionsPerUser  = 20;
    static constexpr int    kAutoRoleActivityWindowSec = 30;

private:
    struct ActivityEntry {
        std::string userId;
        std::string filePath;
        int line = 0;
        std::chrono::steady_clock::time_point when;
        bool isEdit = false;  // true = edit, false = navigation
    };

    PairSuggestion generateCompletionSuggestion(const ParticipantContext& ctx);
    PairSuggestion generateNavigatorHint(const ParticipantContext& driver, const ParticipantContext& navigator);
    std::string buildReviewPrompt(const std::string& code, int line) const;

    LiveShareSession* m_session = nullptr;

    mutable std::mutex m_rolesMutex;
    std::unordered_map<std::string, PairRole> m_roles;

    mutable std::mutex m_contextMutex;
    std::unordered_map<std::string, ParticipantContext> m_contexts;

    mutable std::mutex m_suggestionsMutex;
    // userId -> pending suggestions
    std::unordered_map<std::string, std::deque<PairSuggestion>> m_suggestions;
    uint64_t m_nextSuggestionId = 1;

    mutable std::mutex m_activityMutex;
    std::deque<ActivityEntry> m_activityLog;
};

} // namespace RawrXD
