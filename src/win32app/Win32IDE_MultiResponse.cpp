// ============================================================================
// Win32IDE_MultiResponse.cpp — Multi-Response Chain Generation
//
// Phase 9C: Generates up to 4 distinct responses per prompt using different
// response "templates" (styles), allowing the user to compare and select
// a preferred answer.
//
// IDM commands:   5099–5110
// HTTP endpoints: /api/multi-response/*
// React panel:    MultiResponsePanel
// ============================================================================

#include "Win32IDE.h"
#include "multi_response_engine.h"

#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <limits>

// ============================================================================
// Local HTTP utility (mirrors LocalServerUtil from Win32IDE_LocalServer.cpp)
// ============================================================================
namespace LocalServerUtil {
static std::string buildHttpResponse(int status, const std::string& body,
                                      const std::string& contentType = "application/json") {
    std::ostringstream oss;
    switch (status) {
        case 200: oss << "HTTP/1.1 200 OK\r\n"; break;
        case 204: oss << "HTTP/1.1 204 No Content\r\n"; break;
        case 400: oss << "HTTP/1.1 400 Bad Request\r\n"; break;
        case 404: oss << "HTTP/1.1 404 Not Found\r\n"; break;
        case 422: oss << "HTTP/1.1 422 Unprocessable Entity\r\n"; break;
        case 502: oss << "HTTP/1.1 502 Bad Gateway\r\n"; break;
        default:  oss << "HTTP/1.1 500 Internal Server Error\r\n"; break;
    }
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

static std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

static std::string trim(const std::string& value) {
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    if (first == value.size()) {
        return {};
    }

    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

static std::string buildErrorJson(const std::string& error, const std::string& message) {
    return std::string("{\"error\":\"") + escapeJson(error) +
           "\",\"message\":\"" + escapeJson(message) + "\"}";
}

static size_t skipWhitespace(const std::string& body, size_t pos) {
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }
    return pos;
}

static bool findJsonValueStart(const std::string& body, const std::string& key, size_t& valueStart) {
    const std::string quotedKey = "\"" + key + "\"";
    size_t pos = body.find(quotedKey);
    if (pos == std::string::npos) {
        return false;
    }

    size_t colon = body.find(':', pos + quotedKey.size());
    if (colon == std::string::npos) {
        return false;
    }

    valueStart = skipWhitespace(body, colon + 1);
    return valueStart < body.size();
}

static bool tryExtractJsonString(const std::string& body, const std::string& key, std::string& value) {
    size_t pos = 0;
    if (!findJsonValueStart(body, key, pos) || body[pos] != '"') {
        return false;
    }

    ++pos;
    std::string out;
    out.reserve(64);
    bool escaped = false;
    // Fail-closed: cap JSON string extraction at 64KB
    static constexpr size_t kMaxJsonStringSize = 65536;
    for (; pos < body.size() && out.size() < kMaxJsonStringSize; ++pos) {
        const char c = body[pos];
        if (escaped) {
            switch (c) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(c); break;
            }
            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            value = std::move(out);
            return true;
        }

        out.push_back(c);
    }

    return false;
}

static bool tryExtractJsonInt64(const std::string& body, const std::string& key, int64_t& value) {
    size_t pos = 0;
    if (!findJsonValueStart(body, key, pos)) {
        return false;
    }

    size_t end = pos;
    if (end < body.size() && (body[end] == '-' || body[end] == '+')) {
        ++end;
    }
    const size_t digitsStart = end;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) {
        ++end;
    }
    if (digitsStart == end) {
        return false;
    }

    try {
        value = std::stoll(body.substr(pos, end - pos));
    } catch (...) {
        return false;
    }
    return true;
}
} // namespace LocalServerUtil

// ============================================================================
// LIFECYCLE
// ============================================================================

void Win32IDE::initMultiResponse() {
    if (m_multiResponseInitialized) return;

    m_multiResponseEngine = std::make_unique<MultiResponseEngine>();
    MultiResponseResult r = m_multiResponseEngine->initialize();
    if (!r.success) {
        appendToOutput(std::string("[MultiResponse] Init failed: ") + r.detail,
                       "General", OutputSeverity::Error);
        return;
    }

    m_multiResponseInitialized = true;
    appendToOutput("[MultiResponse] Engine initialized — 4 templates ready "
                   "(Concise, Detailed, Creative, Technical).",
                   "General", OutputSeverity::Info);
}

void Win32IDE::shutdownMultiResponse() {
    if (m_multiResponseEngine) {
        m_multiResponseEngine->shutdown();
    }
    m_multiResponseInitialized = false;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void Win32IDE::cmdMultiResponseGenerate() {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        appendToOutput("[MultiResponse] Engine is unavailable.",
                       "General", OutputSeverity::Error);
        return;
    }

    // Use whatever is in the chat input or last user message
    std::string prompt;
    for (auto it = m_chatHistory.rbegin(); it != m_chatHistory.rend(); ++it) {
        if (it->first == "user" && !it->second.empty()) {
            prompt = it->second;
            break;
        }
    }
    prompt = LocalServerUtil::trim(prompt);
    if (prompt.empty()) {
        appendToOutput("[MultiResponse] No prompt available. Type a question first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    std::string preview = prompt.substr(0, 80);
    if (prompt.size() > 80) {
        preview += "...";
    }
    int maxResp = m_multiResponseEngine->getMaxChainResponses();
    appendToOutput("[MultiResponse] Generating " + std::to_string(maxResp) +
                   " responses for: \"" + preview + "\"",
                   "General", OutputSeverity::Info);

    uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp);
    MultiResponseResult r = m_multiResponseEngine->generateAll(sid);

    if (r.success) {
        const auto* session = m_multiResponseEngine->getSession(sid);
        if (session) {
            std::ostringstream oss;
            double totalMs = 0.0;
            for (const auto& resp : session->responses) totalMs += resp.latencyMs;
            oss << "[MultiResponse] Session #" << sid << " complete — "
                << session->responses.size() << " responses generated in "
                << (int)totalMs << "ms\n";
            for (size_t i = 0; i < session->responses.size(); ++i) {
                const auto& resp = session->responses[i];
                const auto& tmpl = m_multiResponseEngine->getTemplate(resp.templateId);
                oss << "  [" << i << "] " << tmpl.name
                    << " — " << (int)resp.latencyMs << "ms"
                    << (resp.success ? " OK" : " (ERROR)") << "\n";
            }
            oss << "Use 'MultiResp: Select Preferred' to pick your favorite.";
            appendToOutput(oss.str(), "General", OutputSeverity::Info);
        }
    } else {
        appendToOutput(std::string("[MultiResponse] Generation failed: ") + r.detail,
                       "General", OutputSeverity::Error);
    }
}

void Win32IDE::cmdMultiResponseSetMax() {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        appendToOutput("[MultiResponse] Engine is unavailable.",
                       "General", OutputSeverity::Error);
        return;
    }

    // Cycle according to currently enabled templates.
    int current = m_multiResponseEngine->getMaxChainResponses();
    int enabled = m_multiResponseEngine->getEnabledTemplateCount();
    if (enabled <= 0) enabled = 1;
    int next = (current % enabled) + 1;
    m_multiResponseEngine->setMaxChainResponses(next);
    appendToOutput("[MultiResponse] Max chain responses set to " + std::to_string(next),
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseSelectPreferred() {
    if (!m_multiResponseInitialized) initMultiResponse();

    const auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->responses.empty()) {
        appendToOutput("[MultiResponse] No active session. Generate responses first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    // If only one response, auto-select it
    if (session->responses.size() == 1) {
        m_multiResponseEngine->setPreference(session->sessionId, 0);
        appendToOutput("[MultiResponse] Only one response — auto-selected.",
                       "General", OutputSeverity::Info);
        return;
    }

    // Cycle through: -1 → 0 → 1 → 2 → 3 → -1
    int current = session->preferredIndex;
    int next = current + 1;
    if (next >= (int)session->responses.size()) next = 0;

    m_multiResponseEngine->setPreference(session->sessionId, next);
    const auto& selected = session->responses[next];
    const auto& tmpl = m_multiResponseEngine->getTemplate(selected.templateId);
    appendToOutput("[MultiResponse] Preferred response: #" + std::to_string(next) +
                   " (" + std::string(tmpl.name) + ")",
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseCompare() {
    if (!m_multiResponseInitialized) initMultiResponse();

    const auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->responses.empty()) {
        appendToOutput("[MultiResponse] No session to compare.",
                       "General", OutputSeverity::Warning);
        return;
    }

    std::ostringstream oss;
    double totalMs = 0.0;
    for (const auto& resp : session->responses) totalMs += resp.latencyMs;
    oss << "══════════ Multi-Response Comparison (Session #"
        << session->sessionId << ") ══════════\n";
    oss << "Prompt: \"" << session->prompt.substr(0, 100) << "\"\n";
    oss << "Responses: " << session->responses.size() << " | Total: "
        << (int)totalMs << "ms\n\n";

    for (size_t i = 0; i < session->responses.size(); ++i) {
        const auto& resp = session->responses[i];
        const auto& tmpl = m_multiResponseEngine->getTemplate(resp.templateId);
        oss << "──── Response #" << i << ": " << tmpl.name;
        if (session->preferredIndex == static_cast<int>(i)) oss << " ★ PREFERRED";
        oss << " ────\n";
        oss << "Latency: " << (int)resp.latencyMs << "ms";
        if (!resp.success) oss << " | ERROR";
        oss << "\n";
        // Show first 200 chars of content preview
        std::string preview = resp.content.substr(0, 200);
        if (resp.content.size() > 200) preview += "...";
        oss << preview << "\n\n";
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseShowStats() {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto stats = m_multiResponseEngine->getStats();
    std::string recommended = m_multiResponseEngine->getRecommendedTemplate();

    std::ostringstream oss;
    oss << "══════════ Multi-Response Statistics ══════════\n";
    oss << "Total sessions:   " << stats.totalSessions << "\n";
    oss << "Total responses:  " << stats.totalResponses << "\n";
    oss << "Total prefs:      " << stats.preferenceSelections << "\n\n";
    oss << "\nRecommended template: " << recommended << "\n";

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseShowTemplates() {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto templates = m_multiResponseEngine->getAllTemplates();
    std::ostringstream oss;
    oss << "══════════ Response Templates ══════════\n";
    for (const auto& t : templates) {
        oss << "[" << t.id << "] " << t.name
            << (t.enabled ? " ✓" : " ✗") << "\n"
            << "    " << t.systemPrompt << "\n\n";
    }
    oss << "Enabled: " << m_multiResponseEngine->getEnabledTemplateCount()
        << "/" << templates.size() << "\n";
    oss << "Max chain: " << m_multiResponseEngine->getMaxChainResponses() << "\n";
    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseToggleTemplate() {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        appendToOutput("[MultiResponse] Engine is unavailable.",
                       "General", OutputSeverity::Error);
        return;
    }

    // Cycle through templates and toggle the next one
    static int toggleIdx = 0;
    auto templates = m_multiResponseEngine->getAllTemplates();
    if (templates.empty()) {
        appendToOutput("[MultiResponse] No templates available.",
                       "General", OutputSeverity::Warning);
        return;
    }
    if (toggleIdx < 0 || toggleIdx >= static_cast<int>(templates.size())) {
        toggleIdx = 0;
    }

    const auto& current = templates[static_cast<size_t>(toggleIdx)];
    const uint32_t tmplId = current.id;
    const std::string tmplName = current.name;
    const bool desiredState = !current.enabled;
    m_multiResponseEngine->setTemplateEnabled(tmplId, desiredState);
    const auto& updated = m_multiResponseEngine->getTemplate(tmplId);

    if (updated.enabled != desiredState) {
        appendToOutput("[MultiResponse] Template '" + tmplName +
                       "' could not be disabled (at least one template must stay enabled).",
                       "General", OutputSeverity::Warning);
    } else {
        appendToOutput("[MultiResponse] Template '" + tmplName + "' " +
                       (updated.enabled ? "ENABLED" : "DISABLED"),
                       "General", OutputSeverity::Info);
    }

    toggleIdx = (toggleIdx + 1) % static_cast<int>(templates.size());
}

void Win32IDE::cmdMultiResponseShowPreferences() {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto history = m_multiResponseEngine->getPreferenceHistory(20);
    if (history.empty()) {
        appendToOutput("[MultiResponse] No preferences recorded yet.",
                       "General", OutputSeverity::Info);
        return;
    }

    std::ostringstream oss;
    oss << "══════════ Preference History (last " << history.size() << ") ══════════\n";
    for (const auto& rec : history) {
        uint64_t sid = rec.first;
        int preferred = rec.second;
        std::string tmplName = "?";
        if (const auto* session = m_multiResponseEngine->getSession(sid)) {
            if (preferred >= 0 && preferred < static_cast<int>(session->responses.size())) {
                const auto& resp = session->responses[preferred];
                tmplName = m_multiResponseEngine->getTemplate(resp.templateId).name;
            }
        }
        oss << "Session #" << sid << " → #" << preferred
            << " (" << tmplName << ")\n";
    }
    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseShowLatest() {
    if (!m_multiResponseInitialized) initMultiResponse();

    const auto* session = m_multiResponseEngine->getLatestSession();
    if (!session) {
        appendToOutput("[MultiResponse] No sessions yet.", "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[MultiResponse] Latest session JSON:\n" +
                   m_multiResponseEngine->sessionToJson(session->sessionId),
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseShowStatus() {
    if (!m_multiResponseInitialized) {
        appendToOutput("[MultiResponse] Not initialized. Run any MultiResp command to init.",
                       "General", OutputSeverity::Info);
        return;
    }
    appendToOutput("[MultiResponse] Engine status:\n" + m_multiResponseEngine->toJson(),
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseClearHistory() {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        appendToOutput("[MultiResponse] Engine is unavailable.",
                       "General", OutputSeverity::Error);
        return;
    }

    // Re-initialize to clear all state
    m_multiResponseEngine->shutdown();
    MultiResponseResult r = m_multiResponseEngine->initialize();
    if (!r.success) {
        appendToOutput(std::string("[MultiResponse] Failed to clear history: ") + r.detail,
                       "General", OutputSeverity::Error);
        return;
    }
    m_multiResponseInitialized = true;
    appendToOutput("[MultiResponse] All sessions and preferences cleared.",
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseApplyPreferred() {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        appendToOutput("[MultiResponse] Engine is unavailable.",
                       "General", OutputSeverity::Error);
        return;
    }

    const auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->preferredIndex < 0) {
        appendToOutput("[MultiResponse] No preferred response selected.",
                       "General", OutputSeverity::Warning);
        return;
    }
    if (session->preferredIndex >= static_cast<int>(session->responses.size())) {
        appendToOutput("[MultiResponse] Preferred response index is out of range.",
                       "General", OutputSeverity::Warning);
        return;
    }

    const auto& resp = session->responses[session->preferredIndex];
    const auto& tmpl = m_multiResponseEngine->getTemplate(resp.templateId);
    appendToOutput("[MultiResponse] Applying preferred response #" +
                   std::to_string(session->preferredIndex) + " (" + tmpl.name + ")...",
                   "General", OutputSeverity::Info);

    // Insert the preferred response content into the chat output
    appendToOutput("\n═══ Preferred Response (" + std::string(tmpl.name) + ") ═══\n" +
                   resp.content, "General", OutputSeverity::Info);
}

// ============================================================================
// HTTP ENDPOINT HANDLERS (called from LocalServer routing)
// ============================================================================

// GET /api/multi-response/status — engine overview
void Win32IDE::handleMultiResponseStatusEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const std::string json = m_multiResponseEngine->toJson();
    const std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/templates — list all templates
void Win32IDE::handleMultiResponseTemplatesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    auto templates = m_multiResponseEngine->getAllTemplates();
    std::ostringstream o;
    o << "{\"templates\":[";
    for (size_t i = 0; i < templates.size(); ++i) {
        if (i > 0) o << ",";
        const auto& t = templates[i];
        o << "{\"id\":" << (int)t.id
          << ",\"name\":\"" << t.name << "\""
          << ",\"systemPrompt\":\"" << LocalServerUtil::escapeJson(t.systemPrompt) << "\""
          << ",\"enabled\":" << (t.enabled ? "true" : "false")
          << "}";
    }
    o << "],\"maxChain\":" << m_multiResponseEngine->getMaxChainResponses()
      << ",\"enabled\":" << m_multiResponseEngine->getEnabledTemplateCount() << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, o.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/multi-response/generate — start a multi-response session
void Win32IDE::handleMultiResponseGenerateEndpoint(SOCKET client, const std::string& body) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    // Parse prompt and maxResponses from body
    std::string prompt;
    int maxResp = m_multiResponseEngine->getMaxChainResponses();
    std::string context;

    (void)LocalServerUtil::tryExtractJsonString(body, "prompt", prompt);
    (void)LocalServerUtil::tryExtractJsonString(body, "context", context);
    prompt = LocalServerUtil::trim(prompt);
    context = LocalServerUtil::trim(context);

    // Fail-closed: cap prompt at 256KB, context at 128KB (DoS prevention)
    static constexpr size_t kMaxPromptSize = 262144;
    static constexpr size_t kMaxContextSize = 131072;
    if (prompt.size() > kMaxPromptSize) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            400, LocalServerUtil::buildErrorJson("prompt_too_large", "Prompt exceeds maximum size"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }
    if (context.size() > kMaxContextSize) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            400, LocalServerUtil::buildErrorJson("context_too_large", "Context exceeds maximum size"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    int64_t mr = 0;
    if (LocalServerUtil::tryExtractJsonInt64(body, "maxResponses", mr)) {
        if (mr <= 0 || mr > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            const std::string response = LocalServerUtil::buildHttpResponse(
                400, LocalServerUtil::buildErrorJson("invalid_maxResponses",
                "'maxResponses' must be a positive integer"));
            send(client, response.c_str(), (int)response.size(), 0);
            return;
        }
        maxResp = static_cast<int>(mr);
    }

    if (prompt.empty()) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            400, LocalServerUtil::buildErrorJson("missing_prompt", "'prompt' field is required"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp, context);
    const MultiResponseResult r = m_multiResponseEngine->generateAll(sid);

    std::string json;
    int code = 200;
    if (!r.success) {
        code = 422;
        json = LocalServerUtil::buildErrorJson(
            "generation_failed", r.detail ? r.detail : "Failed to generate responses");
    } else {
        json = m_multiResponseEngine->sessionToJson(sid);
    }
    const std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/results — get latest session results
void Win32IDE::handleMultiResponseResultsEndpoint(SOCKET client, const std::string& sessionId) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const std::string sidParam = LocalServerUtil::trim(sessionId);
    const MultiResponseSession* session = nullptr;
    if (!sidParam.empty()) {
        uint64_t sid = 0;
        try {
            sid = std::stoull(sidParam);
        } catch (...) {
            const std::string response = LocalServerUtil::buildHttpResponse(
                400, LocalServerUtil::buildErrorJson("invalid_sessionId", "sessionId must be numeric"));
            send(client, response.c_str(), (int)response.size(), 0);
            return;
        }
        session = m_multiResponseEngine->getSession(sid);
        if (!session) {
            const std::string response = LocalServerUtil::buildHttpResponse(
                404, LocalServerUtil::buildErrorJson("session_not_found", "Requested session does not exist"));
            send(client, response.c_str(), (int)response.size(), 0);
            return;
        }
    } else {
        session = m_multiResponseEngine->getLatestSession();
    }

    std::string json;
    int code = 200;
    if (!session) {
        code = 404;
        json = LocalServerUtil::buildErrorJson("no_sessions", "No multi-response sessions yet");
    } else {
        json = m_multiResponseEngine->sessionToJson(session->sessionId);
    }
    const std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/multi-response/prefer — set preferred response
void Win32IDE::handleMultiResponsePreferEndpoint(SOCKET client, const std::string& body) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    int64_t sessionId = -1;
    int64_t responseIndex = -1;
    const bool hasSessionId = LocalServerUtil::tryExtractJsonInt64(body, "sessionId", sessionId);
    const bool hasResponseIndex =
        LocalServerUtil::tryExtractJsonInt64(body, "responseIndex", responseIndex);
    std::string reason;
    (void)LocalServerUtil::tryExtractJsonString(body, "reason", reason);
    reason = LocalServerUtil::trim(reason);

    // Fail-closed: cap reason field at 4KB to prevent DoS
    static constexpr size_t kMaxReasonSize = 4096;
    if (reason.size() > kMaxReasonSize) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            400, LocalServerUtil::buildErrorJson("reason_too_large", "Reason field exceeds maximum size"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    if (!hasSessionId || !hasResponseIndex ||
        sessionId < 0 || responseIndex < 0 ||
        responseIndex > static_cast<int64_t>(std::numeric_limits<int>::max())) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            400, LocalServerUtil::buildErrorJson(
                "invalid_params", "Need non-negative numeric sessionId and responseIndex"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const MultiResponseResult r = m_multiResponseEngine->setPreference(
        static_cast<uint64_t>(sessionId), static_cast<int>(responseIndex));
    if (r.success && !reason.empty()) {
        appendToOutput("[MultiResponse] Preference reason: " + reason,
                       "General", OutputSeverity::Info);
    }

    std::string json;
    int code = 200;
    if (r.success) {
        json = "{\"success\":true,\"message\":\"Preference recorded\"}";
    } else {
        const std::string detail = (r.detail != nullptr) ? r.detail : "Preference update failed";
        code = (detail.find("Session not found") != std::string::npos) ? 404 : 400;
        json = LocalServerUtil::buildErrorJson("set_preference_failed", detail);
    }

    const std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/stats — statistics and preference breakdown
void Win32IDE::handleMultiResponseStatsEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const std::string json = m_multiResponseEngine->statsToJson();
    const std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/preferences — preference history
void Win32IDE::handleMultiResponsePreferencesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();
    if (!m_multiResponseInitialized || !m_multiResponseEngine) {
        const std::string response = LocalServerUtil::buildHttpResponse(
            500, LocalServerUtil::buildErrorJson("init_failed", "Multi-response engine unavailable"));
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    const std::string json = m_multiResponseEngine->preferencesToJson();
    const std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}
