#pragma once
// ============================================================================
// Multi-Response Engine — Chain multiple distinct responses to the same prompt
//
// Supports up to 4 response "templates" (styles) per query, allowing the user
// to compare and select a preferred response.  Each template shapes the system
// prompt / temperature / structure so every response is genuinely different.
//
// Integration:
//   - IDM commands:     5099–5110
//   - HTTP endpoints:   /api/multi-response/*
//   - React panel:      MultiResponsePanel
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <functional>
#include <atomic>
#include <chrono>

// ────────────────────────────────────────────────────────────────────────────
// Enums
// ────────────────────────────────────────────────────────────────────────────

enum class ResponseTemplateId : int {
    Strategic      = 0,   // High-confidence, executive / acquisition framing
    Grounded       = 1,   // Conservative, engineering-centric, audit-safe
    Creative       = 2,   // Exploratory, lateral-thinking, novel angles
    Concise        = 3,   // Minimal, direct, no fluff — bullet-point style
    Count          = 4
};

enum class MultiResponseStatus : int {
    Idle           = 0,
    Generating     = 1,   // At least one response in flight
    Complete       = 2,   // All responses delivered
    Error          = 3
};

// ────────────────────────────────────────────────────────────────────────────
// Structs
// ────────────────────────────────────────────────────────────────────────────

struct ResponseTemplate {
    ResponseTemplateId id                  = ResponseTemplateId::Strategic;
    const char*        name                = "Strategic";
    const char*        shortLabel          = "S";       // 1-char badge for UI
    const char*        description         = "";
    const char*        systemPromptSuffix  = "";        // appended to base system prompt
    float              temperature         = 0.7f;
    int                maxTokens           = 2048;
    bool               enabled             = true;
};

struct GeneratedResponse {
    int                     index          = -1;        // 0..3
    ResponseTemplateId      templateId     = ResponseTemplateId::Strategic;
    std::string             templateName;
    std::string             content;                    // full markdown body
    int                     tokenCount     = 0;
    double                  latencyMs      = 0.0;
    bool                    complete       = false;
    bool                    error          = false;
    std::string             errorDetail;
};

struct MultiResponseSession {
    uint64_t                sessionId      = 0;
    std::string             prompt;                     // original user query
    std::string             context;                    // optional extra context
    int                     maxResponses   = 4;         // 1–4
    MultiResponseStatus     status         = MultiResponseStatus::Idle;
    std::vector<GeneratedResponse> responses;           // indexed by template slot
    int                     preferredIndex = -1;        // user's pick (-1 = none)
    std::string             preferenceReason;           // optional reason
    std::chrono::steady_clock::time_point startTime;
    double                  totalMs        = 0.0;
};

struct MultiResponseStats {
    uint64_t totalSessions              = 0;
    uint64_t totalResponsesGenerated    = 0;
    uint64_t totalPreferencesRecorded   = 0;
    uint64_t preferenceCount[4]         = {};  // how many times each template was preferred
    double   avgLatencyMs[4]            = {};  // running average per template
    uint64_t errorCount                 = 0;
};

// Preference history entry — for learning which template the user gravitates to
struct PreferenceRecord {
    uint64_t           sessionId         = 0;
    ResponseTemplateId preferredTemplate = ResponseTemplateId::Strategic;
    std::string        promptSnippet;                   // first 120 chars of prompt
    double             timestampEpoch    = 0.0;
};

// ────────────────────────────────────────────────────────────────────────────
// Result type (No exceptions — per project convention)
// ────────────────────────────────────────────────────────────────────────────

struct MultiResponseResult {
    bool        success    = false;
    const char* detail     = "";
    int         errorCode  = 0;

    static MultiResponseResult ok(const char* msg = "OK") {
        MultiResponseResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }
    static MultiResponseResult error(const char* msg, int code = -1) {
        MultiResponseResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ────────────────────────────────────────────────────────────────────────────
// Callback types (no std::function in hot path — project convention)
// ────────────────────────────────────────────────────────────────────────────

// Called when a single response within a session completes
using MultiResponseCallback = void(*)(const GeneratedResponse& resp, void* userData);

// Called when all responses in a session are done
using SessionCompleteCallback = void(*)(const MultiResponseSession& session, void* userData);

// ────────────────────────────────────────────────────────────────────────────
// Engine class
// ────────────────────────────────────────────────────────────────────────────

class MultiResponseEngine {
public:
    MultiResponseEngine();
    ~MultiResponseEngine();

    // ── Lifecycle ──
    MultiResponseResult initialize();
    void                shutdown();
    bool                isInitialized() const { return m_initialized; }

    // ── Template management ──
    const ResponseTemplate& getTemplate(ResponseTemplateId id) const;
    void                    setTemplateEnabled(ResponseTemplateId id, bool enabled);
    void                    setTemplateTemperature(ResponseTemplateId id, float temp);
    void                    setTemplateMaxTokens(ResponseTemplateId id, int maxTokens);
    int                     getEnabledTemplateCount() const;
    std::vector<ResponseTemplate> getAllTemplates() const;

    // ── Session control ──
    // maxResponses: 1–4.  Returns session ID.
    uint64_t  startSession(const std::string& prompt, int maxResponses = 4,
                           const std::string& context = "");
    MultiResponseResult cancelSession(uint64_t sessionId);

    // ── Generation ──
    // Generates all N responses sequentially (call from worker thread).
    // Invokes perResponseCb after each response and sessionCb when all done.
    MultiResponseResult generateAll(uint64_t sessionId,
                                    MultiResponseCallback perResponseCb = nullptr,
                                    void* cbUserData = nullptr,
                                    SessionCompleteCallback sessionCb = nullptr,
                                    void* sessionCbData = nullptr);

    // ── Preference ──
    MultiResponseResult setPreference(uint64_t sessionId, int responseIndex,
                                      const std::string& reason = "");

    // ── Queries ──
    const MultiResponseSession* getSession(uint64_t sessionId) const;
    const MultiResponseSession* getLatestSession() const;
    MultiResponseStats          getStats() const;
    std::vector<PreferenceRecord> getPreferenceHistory(int maxRecords = 50) const;
    std::string                 getRecommendedTemplate() const;   // based on preference history

    // ── Configuration ──
    void setMaxChainResponses(int n);   // clamp 1–4
    int  getMaxChainResponses() const { return m_defaultMaxResponses; }

    // ── Serialization ──
    std::string toJson() const;                     // full state snapshot
    std::string sessionToJson(uint64_t id) const;   // single session
    std::string statsToJson() const;
    std::string preferencesToJson() const;

private:
    // Generate a single response for a given template
    GeneratedResponse generateSingleResponse(const std::string& prompt,
                                              const std::string& context,
                                              const ResponseTemplate& tmpl,
                                              int index);

    // Build the full system prompt for a template
    std::string buildSystemPrompt(const ResponseTemplate& tmpl) const;

    uint64_t nextSessionId();

    bool                        m_initialized          = false;
    int                         m_defaultMaxResponses   = 4;
    std::atomic<uint64_t>       m_sessionCounter{0};

    ResponseTemplate            m_templates[4];        // indexed by ResponseTemplateId

    // Session storage (ring — keep last 100)
    static constexpr int        kMaxSessions = 100;
    std::vector<MultiResponseSession> m_sessions;
    mutable std::mutex          m_sessionMutex;

    // Preference history
    std::vector<PreferenceRecord> m_preferenceHistory;
    mutable std::mutex            m_prefMutex;

    // Stats
    mutable MultiResponseStats  m_stats;
    mutable std::mutex          m_statsMutex;
};

#endif // header guard — use #pragma once above
