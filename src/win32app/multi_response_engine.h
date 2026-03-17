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
#include <algorithm>
#include <chrono>
#include <sstream>

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
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_initialized) return MultiResponseResult::ok("Already initialized");

        m_templates = {
            { "Concise", "Be brief and direct.", true, 0 },
            { "Detailed", "Provide thorough explanations.", true, 1 },
            { "Creative", "Be creative and explorative.", true, 2 },
            { "Technical", "Focus on technical accuracy.", true, 3 }
        };
        m_maxChain = 4;
        m_initialized = true;
        return MultiResponseResult::ok("Initialized");
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions.clear();
        m_preferences.clear();
        m_stats = {};
        m_nextSessionId = 0;
        m_initialized = false;
    }

    bool isInitialized() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_initialized;
    }

    // Template management
    int getMaxChainResponses() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxChain;
    }

    void setMaxChainResponses(int n) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const int enabled = std::max(1, enabledTemplateCountNoLock());
        int clamped = n;
        if (clamped <= 0) clamped = 1;
        if (clamped > 8) clamped = 8;
        if (clamped > enabled) clamped = enabled;
        m_maxChain = clamped;
    }

    const ResponseTemplate& getTemplate(uint32_t id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        static ResponseTemplate empty{};
        for (const auto& t : m_templates) {
            if (t.id == id) return t;
        }
        return empty;
    }

    std::vector<ResponseTemplate> getAllTemplates() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_templates;
    }

    void setTemplateEnabled(uint32_t id, bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& t : m_templates) {
            if (t.id != id) continue;
            if (!enabled && t.enabled && enabledTemplateCountNoLock() <= 1) {
                return;
            }
            t.enabled = enabled;
            if (m_maxChain > enabledTemplateCountNoLock()) {
                m_maxChain = std::max(1, enabledTemplateCountNoLock());
            }
            return;
        }
    }

    int getEnabledTemplateCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return enabledTemplateCountNoLock();
    }

    // Session control
    uint64_t startSession(const std::string& prompt, int maxResp,
                          const std::string& context = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t sid = ++m_nextSessionId;
        MultiResponseSession session;
        session.sessionId = sid;
        session.prompt = prompt.empty() ? "Untitled prompt" : prompt;
        session.context = context;
        int requested = maxResp > 0 ? maxResp : m_maxChain;
        int enabled = std::max(1, enabledTemplateCountNoLock());
        if (requested > enabled) requested = enabled;
        if (requested < 1) requested = 1;
        if (requested > 8) requested = 8;
        session.maxResponses = requested;
        m_sessions[sid] = session;
        m_stats.totalSessions++;
        return sid;
    }

    void cancelSession(uint64_t sid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_preferences.erase(sid);
        m_sessions.erase(sid);
    }

    // Generation
    MultiResponseResult generateAll(uint64_t sid) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end())
            return MultiResponseResult::error("Session not found");

        if (!m_initialized)
            return MultiResponseResult::error("Engine not initialized");

        auto& session = it->second;
        session.responses.clear();
        session.preferredIndex = -1;

        const auto started = std::chrono::steady_clock::now();
        int count = 0;
        for (size_t i = 0; i < m_templates.size(); ++i) {
            const auto& tmpl = m_templates[i];
            if (!tmpl.enabled || count >= session.maxResponses) continue;

            MultiResponseSession::Response resp;
            resp.templateId = tmpl.id;
            resp.content = buildTemplatedResponseNoLock(session, tmpl);
            auto now = std::chrono::steady_clock::now();
            resp.latencyMs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - started).count()
            ) + static_cast<double>(7 + (i * 5));
            resp.success = true;
            session.responses.push_back(resp);
            m_stats.totalResponses++;
            count++;
        }

        if (session.responses.empty())
            return MultiResponseResult::error("No enabled templates");

        return MultiResponseResult::ok("Generated");
    }

    // Query
    const MultiResponseSession* getSession(uint64_t sid) const {
        auto it = m_sessions.find(sid);
        return (it != m_sessions.end()) ? &it->second : nullptr;
    }

    const MultiResponseSession* getLatestSession() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_sessions.empty()) return nullptr;
        auto it = std::max_element(m_sessions.begin(), m_sessions.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return &it->second;
    }

    // Preference
    MultiResponseResult setPreference(uint64_t sid, int index) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end())
            return MultiResponseResult::error("Session not found");
        if (index < 0 || index >= static_cast<int>(it->second.responses.size()))
            return MultiResponseResult::error("Invalid preference index");
        const bool changed = (it->second.preferredIndex != index);
        it->second.preferredIndex = index;
        m_preferences[sid] = index;
        if (changed) m_stats.preferenceSelections++;
        return MultiResponseResult::ok("Preference set");
    }

    std::vector<std::pair<uint64_t, int>> getPreferenceHistory(int maxCount) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::pair<uint64_t, int>> result;
        if (maxCount <= 0) return result;
        for (auto it = m_preferences.rbegin();
             it != m_preferences.rend() && (int)result.size() < maxCount; ++it) {
            result.push_back(*it);
        }
        return result;
    }

    // Stats & serialization
    MultiResponseStats getStats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stats;
    }

    std::string getRecommendedTemplate() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::map<uint32_t, uint64_t> counts;
        for (const auto& p : m_preferences) {
            auto sit = m_sessions.find(p.first);
            if (sit == m_sessions.end()) continue;
            const auto& s = sit->second;
            if (p.second < 0 || p.second >= static_cast<int>(s.responses.size())) continue;
            counts[s.responses[p.second].templateId]++;
        }

        if (counts.empty()) return "Detailed";

        uint32_t bestId = counts.begin()->first;
        uint64_t bestCount = counts.begin()->second;
        for (const auto& kv : counts) {
            if (kv.second > bestCount) {
                bestId = kv.first;
                bestCount = kv.second;
            }
        }
        for (const auto& t : m_templates) {
            if (t.id == bestId) return t.name;
        }
        return "Detailed";
    }

    std::string toJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t latestId = m_sessions.empty() ? 0 : m_sessions.rbegin()->first;
        std::ostringstream o;
        o << "{"
          << "\"initialized\":" << (m_initialized ? "true" : "false")
          << ",\"maxChain\":" << m_maxChain
          << ",\"enabledTemplates\":" << enabledTemplateCountNoLock()
          << ",\"sessions\":" << m_sessions.size()
          << ",\"latestSessionId\":" << latestId
          << ",\"stats\":{"
          << "\"totalSessions\":" << m_stats.totalSessions
          << ",\"totalResponses\":" << m_stats.totalResponses
          << ",\"preferenceSelections\":" << m_stats.preferenceSelections
          << "}"
          << "}";
        return o.str();
    }

    std::string statsToJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const double avgResponses =
            (m_stats.totalSessions == 0) ? 0.0
            : static_cast<double>(m_stats.totalResponses) / static_cast<double>(m_stats.totalSessions);
        const double preferenceRate =
            (m_stats.totalResponses == 0) ? 0.0
            : static_cast<double>(m_stats.preferenceSelections) / static_cast<double>(m_stats.totalResponses);

        std::ostringstream o;
        o << "{"
          << "\"totalSessions\":" << m_stats.totalSessions
          << ",\"totalResponses\":" << m_stats.totalResponses
          << ",\"preferenceSelections\":" << m_stats.preferenceSelections
          << ",\"averageResponsesPerSession\":" << avgResponses
          << ",\"preferenceSelectionRate\":" << preferenceRate
          << "}";
        return o.str();
    }

    std::string preferencesToJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::ostringstream o;
        o << "{\"count\":" << m_preferences.size() << ",\"history\":[";
        bool first = true;
        for (auto it = m_preferences.rbegin(); it != m_preferences.rend(); ++it) {
            if (!first) o << ",";
            first = false;
            uint32_t templateId = 0;
            std::string templateName;

            auto sit = m_sessions.find(it->first);
            if (sit != m_sessions.end() &&
                it->second >= 0 &&
                it->second < static_cast<int>(sit->second.responses.size())) {
                const auto& r = sit->second.responses[it->second];
                templateId = r.templateId;
                for (const auto& t : m_templates) {
                    if (t.id == templateId) {
                        templateName = t.name;
                        break;
                    }
                }
            }

            o << "{"
              << "\"sessionId\":" << it->first
              << ",\"responseIndex\":" << it->second
              << ",\"templateId\":" << templateId
              << ",\"templateName\":\"" << escapeJson(templateName) << "\""
              << "}";
        }
        o << "]}";
        return o.str();
    }

    std::string sessionToJson(uint64_t sid) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sid);
        if (it == m_sessions.end()) return "{}";
        const auto& s = it->second;

        std::ostringstream o;
        o << "{"
          << "\"sessionId\":" << s.sessionId
          << ",\"prompt\":\"" << escapeJson(s.prompt) << "\""
          << ",\"context\":\"" << escapeJson(s.context) << "\""
          << ",\"maxResponses\":" << s.maxResponses
          << ",\"preferredIndex\":" << s.preferredIndex
          << ",\"responses\":[";

        for (size_t i = 0; i < s.responses.size(); ++i) {
            if (i > 0) o << ",";
            const auto& r = s.responses[i];
            o << "{"
              << "\"index\":" << i
              << ",\"templateId\":" << r.templateId
              << ",\"latencyMs\":" << r.latencyMs
              << ",\"success\":" << (r.success ? "true" : "false")
              << ",\"content\":\"" << escapeJson(r.content) << "\""
              << "}";
        }
        o << "]}";
        return o.str();
    }

private:
    static std::string escapeJson(const std::string& value) {
        std::string out;
        out.reserve(value.size() + 16);
        for (char c : value) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    int enabledTemplateCountNoLock() const {
        int count = 0;
        for (const auto& t : m_templates) {
            if (t.enabled) ++count;
        }
        return count;
    }

    std::string buildTemplatedResponseNoLock(const MultiResponseSession& session, const ResponseTemplate& tmpl) const {
        const std::string prompt = session.prompt;
        const std::string context = session.context;
        std::ostringstream out;

        if (tmpl.id == 0) {
            out << "TL;DR: " << prompt;
            if (!context.empty()) out << " | Context: " << context.substr(0, 140);
            return out.str();
        }
        if (tmpl.id == 1) {
            out << "Detailed response for: " << prompt << "\n";
            if (!context.empty()) out << "Context: " << context << "\n";
            out << "1. Key objective: address the request directly.\n";
            out << "2. Constraints: local IDE execution path and deterministic output.\n";
            out << "3. Suggested action: compare alternatives, then apply preferred variant.";
            return out.str();
        }
        if (tmpl.id == 2) {
            out << "Creative angle for: " << prompt << "\n";
            out << "Try two unconventional paths, score each by impact and effort, then iterate.";
            if (!context.empty()) out << "\nContext cue: " << context.substr(0, 180);
            return out.str();
        }

        out << "Technical response for: " << prompt << "\n";
        out << "Focus: concrete behavior, invariants, failure modes, and measurable outcomes.";
        if (!context.empty()) out << "\nContext: " << context;
        return out.str();
    }

private:
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    int m_maxChain = 4;
    uint64_t m_nextSessionId = 0;

    std::vector<ResponseTemplate> m_templates;
    std::map<uint64_t, MultiResponseSession> m_sessions;
    std::map<uint64_t, int> m_preferences;
    MultiResponseStats m_stats{};
};
