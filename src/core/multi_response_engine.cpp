// ============================================================================
// Multi-Response Engine — Implementation
//
// Generates up to 4 distinct responses per prompt using different "templates"
// (response styles), allowing the user to compare and pick a preferred answer.
//
// Phase 9C  |  IDM 5099–5110  |  /api/multi-response/*
// ============================================================================

#include "multi_response_engine.h"

#include <algorithm>
#include <sstream>
#include <cstring>
#include <cmath>
#include <ctime>

// ────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ────────────────────────────────────────────────────────────────────────────

MultiResponseEngine::MultiResponseEngine() {
    m_sessions.reserve(kMaxSessions);
}

MultiResponseEngine::~MultiResponseEngine() {
    shutdown();
}

// ────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ────────────────────────────────────────────────────────────────────────────

MultiResponseResult MultiResponseEngine::initialize() {
    if (m_initialized)
        return MultiResponseResult::ok("Already initialized");

    // ── Template 0: Strategic ──────────────────────────────────────────────
    m_templates[0].id                 = ResponseTemplateId::Strategic;
    m_templates[0].name               = "Strategic";
    m_templates[0].shortLabel         = "S";
    m_templates[0].description        = "High-confidence, executive / acquisition framing. "
                                        "Emphasizes strategic value, market comparables, "
                                        "competitive moats, and buyer appeal.";
    m_templates[0].systemPromptSuffix = "Respond with a strategic, executive-level perspective. "
                                        "Frame answers for investors, acquirers, and decision-makers. "
                                        "Emphasize competitive advantages, market positioning, "
                                        "and strategic value. Use confident, authoritative tone. "
                                        "Include valuation bands and buyer profiles where relevant.";
    m_templates[0].temperature        = 0.7f;
    m_templates[0].maxTokens          = 2048;
    m_templates[0].enabled            = true;

    // ── Template 1: Grounded ──────────────────────────────────────────────
    m_templates[1].id                 = ResponseTemplateId::Grounded;
    m_templates[1].name               = "Grounded";
    m_templates[1].shortLabel         = "G";
    m_templates[1].description        = "Conservative, engineering-centric, audit-safe. "
                                        "No inflation. Focuses on replacement cost, "
                                        "honest feature comparison, and technical accuracy.";
    m_templates[1].systemPromptSuffix = "Respond with a grounded, engineering-centric perspective. "
                                        "Be conservative and audit-safe. No hype or inflation. "
                                        "Focus on replacement cost analysis, honest feature comparison, "
                                        "and technical accuracy. Present data as an engineer would "
                                        "to a technical investor or hiring panel.";
    m_templates[1].temperature        = 0.4f;
    m_templates[1].maxTokens          = 2048;
    m_templates[1].enabled            = true;

    // ── Template 2: Creative ──────────────────────────────────────────────
    m_templates[2].id                 = ResponseTemplateId::Creative;
    m_templates[2].name               = "Creative";
    m_templates[2].shortLabel         = "C";
    m_templates[2].description        = "Exploratory, lateral-thinking, novel angles. "
                                        "Offers unconventional perspectives, analogies, "
                                        "and creative framing.";
    m_templates[2].systemPromptSuffix = "Respond with creative, exploratory, lateral thinking. "
                                        "Offer unconventional perspectives and fresh analogies. "
                                        "Challenge assumptions. Propose novel framings the user "
                                        "might not have considered. Think outside the box while "
                                        "staying technically grounded.";
    m_templates[2].temperature        = 0.9f;
    m_templates[2].maxTokens          = 2048;
    m_templates[2].enabled            = true;

    // ── Template 3: Concise ──────────────────────────────────────────────
    m_templates[3].id                 = ResponseTemplateId::Concise;
    m_templates[3].name               = "Concise";
    m_templates[3].shortLabel         = "X";
    m_templates[3].description        = "Minimal, direct, no fluff. Bullet-point style. "
                                        "TL;DR first, details only if asked.";
    m_templates[3].systemPromptSuffix = "Respond as concisely as possible. Use bullet points. "
                                        "Lead with TL;DR. No filler, no preamble, no pleasantries. "
                                        "Every sentence must carry information. If something can "
                                        "be said in 5 words, do not use 20. Be direct and minimal.";
    m_templates[3].temperature        = 0.3f;
    m_templates[3].maxTokens          = 1024;
    m_templates[3].enabled            = true;

    m_initialized = true;
    return MultiResponseResult::ok("Multi-response engine initialized with 4 templates");
}

void MultiResponseEngine::shutdown() {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    m_initialized = false;
}

// ────────────────────────────────────────────────────────────────────────────
// Template management
// ────────────────────────────────────────────────────────────────────────────

const ResponseTemplate& MultiResponseEngine::getTemplate(ResponseTemplateId id) const {
    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= 4) idx = 0;
    return m_templates[idx];
}

void MultiResponseEngine::setTemplateEnabled(ResponseTemplateId id, bool enabled) {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < 4) m_templates[idx].enabled = enabled;
}

void MultiResponseEngine::setTemplateTemperature(ResponseTemplateId id, float temp) {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < 4) {
        m_templates[idx].temperature = std::max(0.0f, std::min(2.0f, temp));
    }
}

void MultiResponseEngine::setTemplateMaxTokens(ResponseTemplateId id, int maxTokens) {
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < 4) {
        m_templates[idx].maxTokens = std::max(64, std::min(8192, maxTokens));
    }
}

int MultiResponseEngine::getEnabledTemplateCount() const {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_templates[i].enabled) ++count;
    }
    return count;
}

std::vector<ResponseTemplate> MultiResponseEngine::getAllTemplates() const {
    std::vector<ResponseTemplate> result;
    for (int i = 0; i < 4; ++i) result.push_back(m_templates[i]);
    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// Session control
// ────────────────────────────────────────────────────────────────────────────

uint64_t MultiResponseEngine::nextSessionId() {
    return ++m_sessionCounter;
}

uint64_t MultiResponseEngine::startSession(const std::string& prompt, int maxResponses,
                                            const std::string& context) {
    int clamped = std::max(1, std::min(4, maxResponses));

    MultiResponseSession session;
    session.sessionId     = nextSessionId();
    session.prompt        = prompt;
    session.context       = context;
    session.maxResponses  = clamped;
    session.status        = MultiResponseStatus::Idle;
    session.startTime     = std::chrono::steady_clock::now();

    // Pre-allocate response slots for enabled templates up to maxResponses
    int slot = 0;
    for (int i = 0; i < 4 && slot < clamped; ++i) {
        if (m_templates[i].enabled) {
            GeneratedResponse resp;
            resp.index        = slot;
            resp.templateId   = m_templates[i].id;
            resp.templateName = m_templates[i].name;
            resp.complete     = false;
            resp.error        = false;
            session.responses.push_back(resp);
            ++slot;
        }
    }

    std::lock_guard<std::mutex> lock(m_sessionMutex);
    if (m_sessions.size() >= (size_t)kMaxSessions) {
        m_sessions.erase(m_sessions.begin());
    }
    m_sessions.push_back(std::move(session));

    return m_sessions.back().sessionId;
}

MultiResponseResult MultiResponseEngine::cancelSession(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& s : m_sessions) {
        if (s.sessionId == sessionId) {
            s.status = MultiResponseStatus::Error;
            return MultiResponseResult::ok("Session cancelled");
        }
    }
    return MultiResponseResult::error("Session not found", 404);
}

// ────────────────────────────────────────────────────────────────────────────
// Generation
// ────────────────────────────────────────────────────────────────────────────

std::string MultiResponseEngine::buildSystemPrompt(const ResponseTemplate& tmpl) const {
    std::string base = "You are an expert AI assistant integrated into the RawrXD IDE, "
                       "a local-first AI development platform with native inference, "
                       "engine routing, DMA streaming, and full LSP support. "
                       "Answer the user's question thoroughly and accurately. ";
    base += tmpl.systemPromptSuffix;
    return base;
}

GeneratedResponse MultiResponseEngine::generateSingleResponse(const std::string& prompt,
                                                                const std::string& context,
                                                                const ResponseTemplate& tmpl,
                                                                int index) {
    GeneratedResponse resp;
    resp.index        = index;
    resp.templateId   = tmpl.id;
    resp.templateName = tmpl.name;

    auto t0 = std::chrono::steady_clock::now();

    // Build the request payload. The actual HTTP call to the active backend
    // is handled by the existing inference path. We format the prompt with
    // the template's system prompt and parameters.
    std::string systemPrompt = buildSystemPrompt(tmpl);

    // ── Construct the completion request body (OpenAI-compatible) ──
    // This will be sent to the active backend via the existing
    // Win32IDE::sendInferenceRequest() path.
    std::ostringstream reqBody;
    reqBody << "{\"model\":\"default\","
            << "\"temperature\":" << tmpl.temperature << ","
            << "\"max_tokens\":" << tmpl.maxTokens << ","
            << "\"messages\":["
            << "{\"role\":\"system\",\"content\":\"";

    // Escape the system prompt for JSON
    for (char c : systemPrompt) {
        switch (c) {
            case '"':  reqBody << "\\\""; break;
            case '\\': reqBody << "\\\\"; break;
            case '\n': reqBody << "\\n";  break;
            case '\r': reqBody << "\\r";  break;
            case '\t': reqBody << "\\t";  break;
            default:   reqBody << c;      break;
        }
    }
    reqBody << "\"},";

    // Include context if provided
    if (!context.empty()) {
        reqBody << "{\"role\":\"system\",\"content\":\"Context: ";
        for (char c : context) {
            switch (c) {
                case '"':  reqBody << "\\\""; break;
                case '\\': reqBody << "\\\\"; break;
                case '\n': reqBody << "\\n";  break;
                case '\r': reqBody << "\\r";  break;
                case '\t': reqBody << "\\t";  break;
                default:   reqBody << c;      break;
            }
        }
        reqBody << "\"},";
    }

    reqBody << "{\"role\":\"user\",\"content\":\"";
    for (char c : prompt) {
        switch (c) {
            case '"':  reqBody << "\\\""; break;
            case '\\': reqBody << "\\\\"; break;
            case '\n': reqBody << "\\n";  break;
            case '\r': reqBody << "\\r";  break;
            case '\t': reqBody << "\\t";  break;
            default:   reqBody << c;      break;
        }
    }
    reqBody << "\"}]}";

    resp.content  = reqBody.str();  // Store the request body — actual send handled by caller
    resp.complete = true;
    resp.error    = false;

    auto t1 = std::chrono::steady_clock::now();
    resp.latencyMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return resp;
}

MultiResponseResult MultiResponseEngine::generateAll(uint64_t sessionId,
                                                       MultiResponseCallback perResponseCb,
                                                       void* cbUserData,
                                                       SessionCompleteCallback sessionCb,
                                                       void* sessionCbData) {
    MultiResponseSession* session = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        for (auto& s : m_sessions) {
            if (s.sessionId == sessionId) {
                session = &s;
                break;
            }
        }
    }

    if (!session) return MultiResponseResult::error("Session not found", 404);

    session->status = MultiResponseStatus::Generating;

    int generated = 0;
    int errors    = 0;

    for (size_t i = 0; i < session->responses.size(); ++i) {
        auto& slot = session->responses[i];
        const ResponseTemplate& tmpl = getTemplate(slot.templateId);

        GeneratedResponse result = generateSingleResponse(
            session->prompt, session->context, tmpl, (int)i);

        slot.content     = std::move(result.content);
        slot.tokenCount  = result.tokenCount;
        slot.latencyMs   = result.latencyMs;
        slot.complete    = result.complete;
        slot.error       = result.error;
        slot.errorDetail = std::move(result.errorDetail);

        if (slot.error) {
            ++errors;
        } else {
            ++generated;
        }

        // Notify per-response callback
        if (perResponseCb) {
            perResponseCb(slot, cbUserData);
        }

        // Update stats
        {
            std::lock_guard<std::mutex> slock(m_statsMutex);
            m_stats.totalResponsesGenerated++;
            int tidx = static_cast<int>(slot.templateId);
            if (tidx >= 0 && tidx < 4) {
                double prevAvg = m_stats.avgLatencyMs[tidx];
                uint64_t count = m_stats.totalResponsesGenerated; // approximation
                m_stats.avgLatencyMs[tidx] = prevAvg + (slot.latencyMs - prevAvg) / (double)count;
            }
            if (slot.error) m_stats.errorCount++;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    session->totalMs = std::chrono::duration<double, std::milli>(endTime - session->startTime).count();
    session->status  = (errors == (int)session->responses.size())
                       ? MultiResponseStatus::Error
                       : MultiResponseStatus::Complete;

    {
        std::lock_guard<std::mutex> slock(m_statsMutex);
        m_stats.totalSessions++;
    }

    // Notify session-complete callback
    if (sessionCb) {
        sessionCb(*session, sessionCbData);
    }

    if (errors > 0 && generated > 0) {
        return MultiResponseResult::ok("Completed with partial errors");
    }
    if (errors > 0) {
        return MultiResponseResult::error("All responses failed", -1);
    }
    return MultiResponseResult::ok("All responses generated");
}

// ────────────────────────────────────────────────────────────────────────────
// Preference
// ────────────────────────────────────────────────────────────────────────────

MultiResponseResult MultiResponseEngine::setPreference(uint64_t sessionId, int responseIndex,
                                                        const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& s : m_sessions) {
        if (s.sessionId == sessionId) {
            if (responseIndex < 0 || responseIndex >= (int)s.responses.size()) {
                return MultiResponseResult::error("Invalid response index", 400);
            }
            s.preferredIndex  = responseIndex;
            s.preferenceReason = reason;

            ResponseTemplateId tid = s.responses[responseIndex].templateId;

            // Record in stats
            {
                std::lock_guard<std::mutex> slock(m_statsMutex);
                m_stats.totalPreferencesRecorded++;
                int tidx = static_cast<int>(tid);
                if (tidx >= 0 && tidx < 4) m_stats.preferenceCount[tidx]++;
            }

            // Record in preference history
            {
                std::lock_guard<std::mutex> plock(m_prefMutex);
                PreferenceRecord rec;
                rec.sessionId        = sessionId;
                rec.preferredTemplate = tid;
                rec.promptSnippet     = s.prompt.substr(0, 120);
                rec.timestampEpoch    = (double)std::time(nullptr);
                m_preferenceHistory.push_back(std::move(rec));
                // Keep history bounded
                if (m_preferenceHistory.size() > 500) {
                    m_preferenceHistory.erase(m_preferenceHistory.begin(),
                                             m_preferenceHistory.begin() + 100);
                }
            }

            return MultiResponseResult::ok("Preference recorded");
        }
    }
    return MultiResponseResult::error("Session not found", 404);
}

// ────────────────────────────────────────────────────────────────────────────
// Queries
// ────────────────────────────────────────────────────────────────────────────

const MultiResponseSession* MultiResponseEngine::getSession(uint64_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    for (auto& s : m_sessions) {
        if (s.sessionId == sessionId) return &s;
    }
    return nullptr;
}

const MultiResponseSession* MultiResponseEngine::getLatestSession() const {
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    if (m_sessions.empty()) return nullptr;
    return &m_sessions.back();
}

MultiResponseStats MultiResponseEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

std::vector<PreferenceRecord> MultiResponseEngine::getPreferenceHistory(int maxRecords) const {
    std::lock_guard<std::mutex> lock(m_prefMutex);
    int count = std::min(maxRecords, (int)m_preferenceHistory.size());
    if (count <= 0) return {};
    auto begin = m_preferenceHistory.end() - count;
    return std::vector<PreferenceRecord>(begin, m_preferenceHistory.end());
}

std::string MultiResponseEngine::getRecommendedTemplate() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    int bestIdx   = 0;
    uint64_t best = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_stats.preferenceCount[i] > best) {
            best    = m_stats.preferenceCount[i];
            bestIdx = i;
        }
    }
    return m_templates[bestIdx].name;
}

void MultiResponseEngine::setMaxChainResponses(int n) {
    m_defaultMaxResponses = std::max(1, std::min(4, n));
}

// ────────────────────────────────────────────────────────────────────────────
// JSON Serialization (manual — per project convention, no nlohmann in core)
// ────────────────────────────────────────────────────────────────────────────

static std::string escJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string MultiResponseEngine::statsToJson() const {
    auto st = getStats();
    std::ostringstream o;
    o << "{";
    o << "\"totalSessions\":" << st.totalSessions << ",";
    o << "\"totalResponsesGenerated\":" << st.totalResponsesGenerated << ",";
    o << "\"totalPreferencesRecorded\":" << st.totalPreferencesRecorded << ",";
    o << "\"errorCount\":" << st.errorCount << ",";
    o << "\"preferenceCount\":[" << st.preferenceCount[0] << "," << st.preferenceCount[1]
      << "," << st.preferenceCount[2] << "," << st.preferenceCount[3] << "],";
    o << "\"avgLatencyMs\":[" << st.avgLatencyMs[0] << "," << st.avgLatencyMs[1]
      << "," << st.avgLatencyMs[2] << "," << st.avgLatencyMs[3] << "],";
    o << "\"templateNames\":[\"Strategic\",\"Grounded\",\"Creative\",\"Concise\"],";
    o << "\"recommendedTemplate\":\"" << escJson(getRecommendedTemplate()) << "\"";
    o << "}";
    return o.str();
}

std::string MultiResponseEngine::sessionToJson(uint64_t id) const {
    const MultiResponseSession* s = getSession(id);
    if (!s) return "{\"error\":\"session_not_found\"}";

    std::ostringstream o;
    o << "{";
    o << "\"sessionId\":" << s->sessionId << ",";
    o << "\"prompt\":\"" << escJson(s->prompt) << "\",";
    o << "\"maxResponses\":" << s->maxResponses << ",";
    o << "\"status\":" << (int)s->status << ",";
    o << "\"statusLabel\":\"";
    switch (s->status) {
        case MultiResponseStatus::Idle:       o << "idle";       break;
        case MultiResponseStatus::Generating: o << "generating"; break;
        case MultiResponseStatus::Complete:   o << "complete";   break;
        case MultiResponseStatus::Error:      o << "error";      break;
    }
    o << "\",";
    o << "\"totalMs\":" << s->totalMs << ",";
    o << "\"preferredIndex\":" << s->preferredIndex << ",";
    o << "\"preferenceReason\":\"" << escJson(s->preferenceReason) << "\",";
    o << "\"responses\":[";
    for (size_t i = 0; i < s->responses.size(); ++i) {
        const auto& r = s->responses[i];
        if (i > 0) o << ",";
        o << "{";
        o << "\"index\":" << r.index << ",";
        o << "\"templateId\":" << (int)r.templateId << ",";
        o << "\"templateName\":\"" << escJson(r.templateName) << "\",";
        o << "\"content\":\"" << escJson(r.content) << "\",";
        o << "\"tokenCount\":" << r.tokenCount << ",";
        o << "\"latencyMs\":" << r.latencyMs << ",";
        o << "\"complete\":" << (r.complete ? "true" : "false") << ",";
        o << "\"error\":" << (r.error ? "true" : "false");
        if (r.error) o << ",\"errorDetail\":\"" << escJson(r.errorDetail) << "\"";
        o << "}";
    }
    o << "]";
    o << "}";
    return o.str();
}

std::string MultiResponseEngine::toJson() const {
    std::ostringstream o;
    o << "{";
    o << "\"initialized\":" << (m_initialized ? "true" : "false") << ",";
    o << "\"defaultMaxResponses\":" << m_defaultMaxResponses << ",";
    o << "\"enabledTemplates\":" << getEnabledTemplateCount() << ",";
    o << "\"templates\":[";
    for (int i = 0; i < 4; ++i) {
        if (i > 0) o << ",";
        o << "{";
        o << "\"id\":" << i << ",";
        o << "\"name\":\"" << m_templates[i].name << "\",";
        o << "\"shortLabel\":\"" << m_templates[i].shortLabel << "\",";
        o << "\"description\":\"" << escJson(m_templates[i].description) << "\",";
        o << "\"temperature\":" << m_templates[i].temperature << ",";
        o << "\"maxTokens\":" << m_templates[i].maxTokens << ",";
        o << "\"enabled\":" << (m_templates[i].enabled ? "true" : "false");
        o << "}";
    }
    o << "],";
    o << "\"stats\":" << statsToJson() << ",";
    // Latest session ID
    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        o << "\"sessionCount\":" << m_sessions.size() << ",";
        o << "\"latestSessionId\":" << (m_sessions.empty() ? 0 : m_sessions.back().sessionId);
    }
    o << "}";
    return o.str();
}

std::string MultiResponseEngine::preferencesToJson() const {
    auto history = getPreferenceHistory(50);
    std::ostringstream o;
    o << "{\"count\":" << history.size() << ",\"records\":[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) o << ",";
        o << "{\"sessionId\":" << history[i].sessionId
          << ",\"templateId\":" << (int)history[i].preferredTemplate
          << ",\"promptSnippet\":\"" << escJson(history[i].promptSnippet) << "\""
          << ",\"timestamp\":" << (int64_t)history[i].timestampEpoch << "}";
    }
    o << "]}";
    return o.str();
}
