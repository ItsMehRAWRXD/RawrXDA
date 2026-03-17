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

// ============================================================================
// Local HTTP helpers (mirrors LocalServerUtil in Win32IDE_LocalServer.cpp)
// ============================================================================
namespace LocalServerUtil {

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

static std::string buildHttpResponse(int status, const std::string& body,
                                      const std::string& contentType = "application/json") {
    std::ostringstream oss;
    switch (status) {
        case 200: oss << "HTTP/1.1 200 OK\r\n"; break;
        case 204: oss << "HTTP/1.1 204 No Content\r\n"; break;
        case 400: oss << "HTTP/1.1 400 Bad Request\r\n"; break;
        case 404: oss << "HTTP/1.1 404 Not Found\r\n"; break;
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
                   "(Strategic, Grounded, Creative, Concise).",
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

    // Use whatever is in the chat input or last user message from chat history
    std::string prompt;
    {
        std::string chatText = getWindowText(m_hwndCopilotChatInput);
        if (!chatText.empty()) {
            prompt = chatText;
        } else {
            // Walk chat history backwards for last user message
            for (auto it = m_chatHistory.rbegin(); it != m_chatHistory.rend(); ++it) {
                if (it->first == "user" && !it->second.empty()) {
                    prompt = it->second;
                    break;
                }
            }
        }
    }
    if (prompt.empty()) {
        appendToOutput("[MultiResponse] No prompt available. Type a question first.",
                       "General", OutputSeverity::Warning);
        return;
    }

    int maxResp = m_multiResponseEngine->getMaxChainResponses();
    appendToOutput("[MultiResponse] Generating " + std::to_string(maxResp) +
                   " responses for: \"" + prompt.substr(0, 80) + "...\"",
                   "General", OutputSeverity::Info);

    uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp);
    MultiResponseResult r = m_multiResponseEngine->generateAll(sid);

    if (r.success) {
        const auto* session = m_multiResponseEngine->getSession(sid);
        if (session) {
            std::ostringstream oss;
            oss << "[MultiResponse] Session #" << sid << " complete — "
                << session->responses.size() << " responses generated in "
                << (int)session->totalMs << "ms\n";
            for (const auto& resp : session->responses) {
                oss << "  [" << resp.index << "] " << resp.templateName
                    << " — " << (int)resp.latencyMs << "ms"
                    << (resp.error ? " (ERROR)" : " OK") << "\n";
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

    // Cycle: 1 → 2 → 3 → 4 → 1
    int current = m_multiResponseEngine->getMaxChainResponses();
    int next = (current % 4) + 1;
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
    appendToOutput("[MultiResponse] Preferred response: #" + std::to_string(next) +
                   " (" + session->responses[next].templateName + ")",
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
    oss << "══════════ Multi-Response Comparison (Session #"
        << session->sessionId << ") ══════════\n";
    oss << "Prompt: \"" << session->prompt.substr(0, 100) << "\"\n";
    oss << "Responses: " << session->responses.size() << " | Total: "
        << (int)session->totalMs << "ms\n\n";

    for (const auto& resp : session->responses) {
        oss << "──── Response #" << resp.index << ": " << resp.templateName;
        if (session->preferredIndex == resp.index) oss << " ★ PREFERRED";
        oss << " ────\n";
        oss << "Latency: " << (int)resp.latencyMs << "ms | Tokens: " << resp.tokenCount;
        if (resp.error) oss << " | ERROR: " << resp.errorDetail;
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
    oss << "Total responses:  " << stats.totalResponsesGenerated << "\n";
    oss << "Total prefs:      " << stats.totalPreferencesRecorded << "\n";
    oss << "Errors:           " << stats.errorCount << "\n\n";
    oss << "Preference breakdown:\n";

    const char* names[] = {"Strategic", "Grounded", "Creative", "Concise"};
    for (int i = 0; i < 4; ++i) {
        oss << "  " << names[i] << ": " << stats.preferenceCount[i]
            << " picks (avg " << (int)stats.avgLatencyMs[i] << "ms)\n";
    }
    oss << "\nRecommended template: " << recommended << "\n";

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseShowTemplates() {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto templates = m_multiResponseEngine->getAllTemplates();
    std::ostringstream oss;
    oss << "══════════ Response Templates ══════════\n";
    for (const auto& t : templates) {
        oss << "[" << t.shortLabel << "] " << t.name
            << (t.enabled ? " ✓" : " ✗")
            << " | temp=" << t.temperature
            << " | maxTok=" << t.maxTokens << "\n"
            << "    " << t.description << "\n\n";
    }
    oss << "Enabled: " << m_multiResponseEngine->getEnabledTemplateCount() << "/4\n";
    oss << "Max chain: " << m_multiResponseEngine->getMaxChainResponses() << "\n";
    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseToggleTemplate() {
    if (!m_multiResponseInitialized) initMultiResponse();

    // Cycle through templates and toggle the next one
    static int toggleIdx = 0;
    auto tmplId = static_cast<ResponseTemplateId>(toggleIdx);
    const auto& tmpl = m_multiResponseEngine->getTemplate(tmplId);
    bool newState = !tmpl.enabled;
    m_multiResponseEngine->setTemplateEnabled(tmplId, newState);

    appendToOutput("[MultiResponse] Template '" + std::string(tmpl.name) + "' " +
                   (newState ? "ENABLED" : "DISABLED"),
                   "General", OutputSeverity::Info);

    toggleIdx = (toggleIdx + 1) % 4;
}

void Win32IDE::cmdMultiResponseShowPreferences() {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto history = m_multiResponseEngine->getPreferenceHistory(20);
    if (history.empty()) {
        appendToOutput("[MultiResponse] No preferences recorded yet.",
                       "General", OutputSeverity::Info);
        return;
    }

    const char* names[] = {"Strategic", "Grounded", "Creative", "Concise"};
    std::ostringstream oss;
    oss << "══════════ Preference History (last " << history.size() << ") ══════════\n";
    for (const auto& rec : history) {
        int tidx = static_cast<int>(rec.preferredTemplate);
        oss << "Session #" << rec.sessionId << " → "
            << (tidx >= 0 && tidx < 4 ? names[tidx] : "?")
            << " | \"" << rec.promptSnippet.substr(0, 60) << "...\"\n";
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

    // Re-initialize to clear all state
    m_multiResponseEngine->shutdown();
    m_multiResponseEngine->initialize();
    appendToOutput("[MultiResponse] All sessions and preferences cleared.",
                   "General", OutputSeverity::Info);
}

void Win32IDE::cmdMultiResponseApplyPreferred() {
    if (!m_multiResponseInitialized) initMultiResponse();

    const auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->preferredIndex < 0) {
        appendToOutput("[MultiResponse] No preferred response selected.",
                       "General", OutputSeverity::Warning);
        return;
    }

    const auto& resp = session->responses[session->preferredIndex];
    appendToOutput("[MultiResponse] Applying preferred response #" +
                   std::to_string(resp.index) + " (" + resp.templateName + ")...",
                   "General", OutputSeverity::Info);

    // Insert the preferred response content into the chat output
    appendToOutput("\n═══ Preferred Response (" + resp.templateName + ") ═══\n" +
                   resp.content, "General", OutputSeverity::Info);
}

// ============================================================================
// HTTP ENDPOINT HANDLERS (called from LocalServer routing)
// ============================================================================

// GET /api/multi-response/status — engine overview
void Win32IDE::handleMultiResponseStatusEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();

    std::string json = m_multiResponseEngine->toJson();
    std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/templates — list all templates
void Win32IDE::handleMultiResponseTemplatesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();

    auto templates = m_multiResponseEngine->getAllTemplates();
    std::ostringstream o;
    o << "{\"templates\":[";
    for (size_t i = 0; i < templates.size(); ++i) {
        if (i > 0) o << ",";
        const auto& t = templates[i];
        o << "{\"id\":" << (int)t.id
          << ",\"name\":\"" << t.name << "\""
          << ",\"shortLabel\":\"" << t.shortLabel << "\""
          << ",\"temperature\":" << t.temperature
          << ",\"maxTokens\":" << t.maxTokens
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

    // Parse prompt and maxResponses from body
    std::string prompt;
    int maxResp = m_multiResponseEngine->getMaxChainResponses();
    std::string context;

    // Simple JSON parsing (project convention — manual parse)
    auto extractStr = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = body.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = body.find("\"", pos);
        if (end == std::string::npos) return "";
        return body.substr(pos, end - pos);
    };
    auto extractInt = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        auto pos = body.find(search);
        if (pos == std::string::npos) return -1;
        pos += search.size();
        return std::atoi(body.c_str() + pos);
    };

    prompt  = extractStr("prompt");
    context = extractStr("context");
    int mr  = extractInt("maxResponses");
    if (mr > 0) maxResp = mr;

    if (prompt.empty()) {
        std::string errResp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"missing_prompt\",\"message\":\"'prompt' field is required\"}");
        send(client, errResp.c_str(), (int)errResp.size(), 0);
        return;
    }

    uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp, context);
    MultiResponseResult r = m_multiResponseEngine->generateAll(sid);

    std::string json = m_multiResponseEngine->sessionToJson(sid);
    int code = r.success ? 200 : 500;
    std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/results — get latest session results
void Win32IDE::handleMultiResponseResultsEndpoint(SOCKET client, const std::string& sessionId) {
    if (!m_multiResponseInitialized) initMultiResponse();

    // If sessionId provided, use it; otherwise get latest session
    const auto* session = sessionId.empty()
        ? m_multiResponseEngine->getLatestSession()
        : nullptr;  // TODO: lookup by sessionId when engine supports it

    // For now, always show latest session
    if (!session) session = m_multiResponseEngine->getLatestSession();

    std::string json = session
        ? m_multiResponseEngine->sessionToJson(session->sessionId)
        : "{\"error\":\"no_sessions\",\"message\":\"No multi-response sessions yet\"}";

    int code = session ? 200 : 404;
    std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/multi-response/prefer — set preferred response
void Win32IDE::handleMultiResponsePreferEndpoint(SOCKET client, const std::string& body) {
    if (!m_multiResponseInitialized) initMultiResponse();

    // Parse sessionId and responseIndex
    auto extractInt = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        auto pos = body.find(search);
        if (pos == std::string::npos) return -1;
        pos += search.size();
        return std::atoi(body.c_str() + pos);
    };
    auto extractStr = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        auto pos = body.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        auto end = body.find("\"", pos);
        if (end == std::string::npos) return "";
        return body.substr(pos, end - pos);
    };

    int sessionId     = extractInt("sessionId");
    int responseIndex = extractInt("responseIndex");
    std::string reason = extractStr("reason");

    if (sessionId < 0 || responseIndex < 0) {
        std::string errResp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"invalid_params\",\"message\":\"Need sessionId and responseIndex\"}");
        send(client, errResp.c_str(), (int)errResp.size(), 0);
        return;
    }

    MultiResponseResult r = m_multiResponseEngine->setPreference(
        (uint64_t)sessionId, responseIndex, reason);

    std::string json = r.success
        ? "{\"success\":true,\"message\":\"Preference recorded\"}"
        : ("{\"success\":false,\"message\":\"" + std::string(r.detail) + "\"}");

    int code = r.success ? 200 : (r.errorCode == 404 ? 404 : 400);
    std::string response = LocalServerUtil::buildHttpResponse(code, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/stats — statistics and preference breakdown
void Win32IDE::handleMultiResponseStatsEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();

    std::string json = m_multiResponseEngine->statsToJson();
    std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/multi-response/preferences — preference history
void Win32IDE::handleMultiResponsePreferencesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) initMultiResponse();

    std::string json = m_multiResponseEngine->preferencesToJson();
    std::string response = LocalServerUtil::buildHttpResponse(200, json);
    send(client, response.c_str(), (int)response.size(), 0);
}
