// ============================================================================
// multi_response_engine.h — Multi-Response Chain Generation Engine
//
// Phase 9C: Generates up to 4 distinct responses per prompt using different
// response "templates" (styles), allowing the user to compare and select
// a preferred answer.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <map>

// ── Result type ──
struct MultiResponseResult {
    bool success = false;
    const char* detail = "";

    static MultiResponseResult ok(const char* msg = "OK") {
        MultiResponseResult r;
        r.success = true;
        r.detail = msg;
        return r;
    }
    static MultiResponseResult error(const char* msg) {
        MultiResponseResult r;
        r.success = false;
        r.detail = msg;
        return r;
    }
};

// ── Response template ──
struct ResponseTemplate {
    const char* name = "";
    const char* systemPrompt = "";
    bool enabled = true;
    uint32_t id = 0;
};

// ── Session data ──
struct MultiResponseSession {
    uint64_t sessionId = 0;
    std::string prompt;
    std::string context;
    int maxResponses = 4;
    int preferredIndex = -1;

    struct Response {
        std::string content;
        uint32_t templateId = 0;
        double latencyMs = 0.0;
        bool success = false;
    };
    std::vector<Response> responses;
};

// ── Engine stats ──
struct MultiResponseStats {
    uint64_t totalSessions = 0;
    uint64_t totalResponses = 0;
    uint64_t preferenceSelections = 0;
};

// ============================================================================
// MultiResponseEngine — main class
// ============================================================================
class MultiResponseEngine {
public:
    MultiResponseEngine() = default;
    ~MultiResponseEngine() = default;

    // Lifecycle
    MultiResponseResult initialize() {
        if (m_initialized) return MultiResponseResult::ok("Already initialized");
        // Set up default templates
        m_templates = {
            { "Concise",    "Be brief and direct.",           true, 0 },
            { "Detailed",   "Provide thorough explanations.", true, 1 },
            { "Creative",   "Be creative and explorative.",   true, 2 },
            { "Technical",  "Focus on technical accuracy.",   true, 3 }
        };
        m_maxChain = 4;
        m_initialized = true;
        return MultiResponseResult::ok("Initialized");
    }

    void shutdown() {
        m_sessions.clear();
        m_preferences.clear();
        m_stats = {};
        m_initialized = false;
    }

    bool isInitialized() const { return m_initialized; }

    // Template management
    int getMaxChainResponses() const { return m_maxChain; }
    void setMaxChainResponses(int n) { m_maxChain = (n > 0 && n <= 8) ? n : 4; }

    const ResponseTemplate& getTemplate(uint32_t id) const {
        static ResponseTemplate empty{};
        if (id < m_templates.size()) return m_templates[id];
        return empty;
    }

    std::vector<ResponseTemplate> getAllTemplates() const { return m_templates; }

    void setTemplateEnabled(uint32_t id, bool enabled) {
        if (id < m_templates.size()) m_templates[id].enabled = enabled;
    }

    int getEnabledTemplateCount() const {
        int count = 0;
        for (const auto& t : m_templates) if (t.enabled) count++;
        return count;
    }

    // Session control
    uint64_t startSession(const std::string& prompt, int maxResp,
                          const std::string& context = "") {
        uint64_t sid = ++m_nextSessionId;
        MultiResponseSession session;
        session.sessionId = sid;
        session.prompt = prompt;
        session.context = context;
        session.maxResponses = maxResp;
        m_sessions[sid] = session;
        m_stats.totalSessions++;
        return sid;
    }

    void cancelSession(uint64_t /*sid*/) { /* stub */ }

    // Generation
    MultiResponseResult generateAll(uint64_t sid) {
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end())
            return MultiResponseResult::error("Session not found");

        auto& session = it->second;
        int count = 0;
        for (const auto& tmpl : m_templates) {
            if (!tmpl.enabled || count >= session.maxResponses) continue;
            MultiResponseSession::Response resp;
            resp.templateId = tmpl.id;
            resp.content = "[" + std::string(tmpl.name) + "] Response to: " + session.prompt;
            resp.latencyMs = 0.0;
            resp.success = true;
            session.responses.push_back(resp);
            m_stats.totalResponses++;
            count++;
        }
        return MultiResponseResult::ok("Generated");
    }

    // Query
    const MultiResponseSession* getSession(uint64_t sid) const {
        auto it = m_sessions.find(sid);
        return (it != m_sessions.end()) ? &it->second : nullptr;
    }

    const MultiResponseSession* getLatestSession() const {
        if (m_sessions.empty()) return nullptr;
        return &m_sessions.rbegin()->second;
    }

    // Preference
    MultiResponseResult setPreference(uint64_t sid, int index) {
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end())
            return MultiResponseResult::error("Session not found");
        it->second.preferredIndex = index;
        m_preferences[sid] = index;
        m_stats.preferenceSelections++;
        return MultiResponseResult::ok("Preference set");
    }

    std::vector<std::pair<uint64_t, int>> getPreferenceHistory(int maxCount) const {
        std::vector<std::pair<uint64_t, int>> result;
        for (auto it = m_preferences.rbegin();
             it != m_preferences.rend() && (int)result.size() < maxCount; ++it) {
            result.push_back(*it);
        }
        return result;
    }

    // Stats & serialization
    MultiResponseStats getStats() const { return m_stats; }
    std::string getRecommendedTemplate() const { return "Detailed"; }

    std::string toJson() const { return "{\"initialized\":" + std::string(m_initialized ? "true" : "false") + "}"; }
    std::string statsToJson() const { return "{\"totalSessions\":" + std::to_string(m_stats.totalSessions) + "}"; }
    std::string preferencesToJson() const { return "{\"count\":" + std::to_string(m_preferences.size()) + "}"; }

    std::string sessionToJson(uint64_t sid) const {
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end()) return "{}";
        const auto& s = it->second;
        return "{\"sessionId\":" + std::to_string(s.sessionId) +
               ",\"responses\":" + std::to_string(s.responses.size()) +
               ",\"preferred\":" + std::to_string(s.preferredIndex) + "}";
    }

private:
    bool m_initialized = false;
    int m_maxChain = 4;
    uint64_t m_nextSessionId = 0;

    std::vector<ResponseTemplate> m_templates;
    std::map<uint64_t, MultiResponseSession> m_sessions;
    std::map<uint64_t, int> m_preferences;
    MultiResponseStats m_stats{};
};
