// ============================================================================
// Win32IDE_MultiResponse.cpp — Multi-Response Chain Integration
// ============================================================================
//
// Phase 9C: Generates up to 4 distinct responses per prompt using different
// "templates" (Strategic, Grounded, Creative, Concise).  Users can compare
// responses side-by-side and select a preferred answer, building a preference
// history that refines future recommendations.
//
// IDM commands:     5106–5117  (routed via handleToolsCommand)
// HTTP endpoints:   /api/multi-response/*
// React panel:      MultiResponsePanel (3 tabs: Generate, Compare, Stats)
//
// Part of RawrXD-Shell — Phase 9C
// ============================================================================

#include "Win32IDE.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>

#include "multi_response_engine.h"

using json = nlohmann::json;

// ============================================================================
// LIFECYCLE
// ============================================================================

void Win32IDE::initMultiResponse() {
    if (m_multiResponseInitialized) return;
    m_multiResponseEngine = std::make_unique<MultiResponseEngine>();
    auto r = m_multiResponseEngine->initialize();
    if (!r.success) {
        appendOutput(OutputSeverity::Error,
                     std::string("MultiResponse init failed: ") + r.detail);
        return;
    }
    m_multiResponseInitialized = true;
    appendOutput(OutputSeverity::Info, "Multi-Response Engine initialized (4 templates)");
}

void Win32IDE::shutdownMultiResponse() {
    if (!m_multiResponseInitialized) return;
    m_multiResponseEngine->shutdown();
    m_multiResponseEngine.reset();
    m_multiResponseInitialized = false;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void Win32IDE::cmdMultiResponseGenerate() {
    if (!m_multiResponseInitialized) { initMultiResponse(); }
    if (!m_multiResponseInitialized) return;

    std::string prompt = getSelectedText();
    if (prompt.empty()) {
        appendOutput(OutputSeverity::Info,
                     "MultiResp: Select text or type in the chat panel, then invoke Generate.");
        return;
    }

    int maxResp = m_multiResponseEngine->getMaxChainResponses();
    uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp);

    appendOutput(OutputSeverity::Info,
                 "MultiResp: Generating " + std::to_string(maxResp) +
                 " responses for session " + std::to_string(sid) + " ...");

    // Generate on a background thread to keep UI responsive
    std::thread([this, sid]() {
        auto r = m_multiResponseEngine->generateAll(sid);
        if (!r.success) {
            appendOutput(OutputSeverity::Error,
                         std::string("MultiResp generate error: ") + r.detail);
        } else {
            appendOutput(OutputSeverity::Info,
                         "MultiResp: All responses ready for session " +
                         std::to_string(sid));
        }
    }).detach();
}

void Win32IDE::cmdMultiResponseSetMax() {
    if (!m_multiResponseInitialized) { initMultiResponse(); }
    if (!m_multiResponseInitialized) return;

    int cur = m_multiResponseEngine->getMaxChainResponses();
    int next = (cur % 4) + 1;
    m_multiResponseEngine->setMaxChainResponses(next);
    appendOutput(OutputSeverity::Info,
                 "MultiResp: Max responses set to " + std::to_string(next));
}

void Win32IDE::cmdMultiResponseSelectPreferred() {
    if (!m_multiResponseInitialized) return;
    auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->responses.empty()) {
        appendOutput(OutputSeverity::Info, "MultiResp: No active session to select from.");
        return;
    }

    int next = session->preferredIndex + 1;
    if (next >= (int)session->responses.size()) next = -1;

    if (next >= 0) {
        m_multiResponseEngine->setPreference(session->sessionId, next);
        appendOutput(OutputSeverity::Info,
                     "MultiResp: Preferred response #" + std::to_string(next) +
                     " (" + session->responses[next].templateName + ")");
    } else {
        appendOutput(OutputSeverity::Info, "MultiResp: Preference cleared.");
    }
}

void Win32IDE::cmdMultiResponseCompare() {
    if (!m_multiResponseInitialized) return;
    auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->responses.empty()) {
        appendOutput(OutputSeverity::Info, "MultiResp: No session to compare.");
        return;
    }

    std::ostringstream oss;
    oss << "\n=== Multi-Response Compare (Session "
        << session->sessionId << ") ===\n";
    oss << "Prompt: " << session->prompt.substr(0, 120) << "\n\n";

    for (const auto& resp : session->responses) {
        oss << "--- [" << resp.templateName << "] "
            << (resp.complete ? "OK" : "INCOMPLETE")
            << " (" << resp.tokenCount << " tok, "
            << (int)resp.latencyMs << " ms) ---\n";
        oss << resp.content.substr(0, 300);
        if (resp.content.size() > 300) oss << "\n... (truncated)";
        oss << "\n\n";
    }

    if (session->preferredIndex >= 0) {
        oss << ">> Preferred: #" << session->preferredIndex
            << " (" << session->responses[session->preferredIndex].templateName << ")\n";
    }

    appendOutput(OutputSeverity::Info, oss.str());
}

void Win32IDE::cmdMultiResponseShowStats() {
    if (!m_multiResponseInitialized) return;
    auto stats = m_multiResponseEngine->getStats();

    std::ostringstream oss;
    oss << "\n=== Multi-Response Stats ===\n";
    oss << "Sessions:    " << stats.totalSessions << "\n";
    oss << "Responses:   " << stats.totalResponsesGenerated << "\n";
    oss << "Preferences: " << stats.totalPreferencesRecorded << "\n";
    oss << "Errors:      " << stats.errorCount << "\n";
    oss << "\nPer-template breakdown:\n";

    const char* names[] = {"Strategic", "Grounded", "Creative", "Concise"};
    for (int i = 0; i < 4; i++) {
        oss << "  " << names[i] << ": preferred "
            << stats.preferenceCount[i] << "x, avg "
            << (int)stats.avgLatencyMs[i] << " ms\n";
    }
    oss << "\nRecommended: " << m_multiResponseEngine->getRecommendedTemplate() << "\n";

    appendOutput(OutputSeverity::Info, oss.str());
}

void Win32IDE::cmdMultiResponseShowTemplates() {
    if (!m_multiResponseInitialized) { initMultiResponse(); }
    if (!m_multiResponseInitialized) return;

    auto templates = m_multiResponseEngine->getAllTemplates();
    std::ostringstream oss;
    oss << "\n=== Response Templates ===\n";
    for (const auto& t : templates) {
        oss << "  [" << t.shortLabel << "] " << t.name
            << " -- temp=" << t.temperature
            << " maxTok=" << t.maxTokens
            << " " << (t.enabled ? "ON" : "OFF") << "\n";
    }
    appendOutput(OutputSeverity::Info, oss.str());
}

void Win32IDE::cmdMultiResponseToggleTemplate() {
    if (!m_multiResponseInitialized) return;
    static int toggleIdx = 0;
    auto tmpl = m_multiResponseEngine->getTemplate(
        static_cast<ResponseTemplateId>(toggleIdx));
    m_multiResponseEngine->setTemplateEnabled(
        static_cast<ResponseTemplateId>(toggleIdx), !tmpl.enabled);
    appendOutput(OutputSeverity::Info,
                 std::string("MultiResp: ") + tmpl.name +
                 (tmpl.enabled ? " DISABLED" : " ENABLED"));
    toggleIdx = (toggleIdx + 1) % 4;
}

void Win32IDE::cmdMultiResponseShowPreferences() {
    if (!m_multiResponseInitialized) return;
    auto prefs = m_multiResponseEngine->getPreferenceHistory(20);

    std::ostringstream oss;
    oss << "\n=== Preference History (last " << prefs.size() << ") ===\n";
    const char* names[] = {"Strategic", "Grounded", "Creative", "Concise"};
    for (const auto& p : prefs) {
        oss << "  Session " << p.sessionId << ": "
            << names[static_cast<int>(p.preferredTemplate)]
            << " -- \"" << p.promptSnippet << "\"\n";
    }
    appendOutput(OutputSeverity::Info, oss.str());
}

void Win32IDE::cmdMultiResponseShowLatest() {
    if (!m_multiResponseInitialized) return;
    auto* session = m_multiResponseEngine->getLatestSession();
    if (!session) {
        appendOutput(OutputSeverity::Info, "MultiResp: No sessions yet.");
        return;
    }
    appendOutput(OutputSeverity::Info,
                 "MultiResp: Latest session JSON:\n" +
                 m_multiResponseEngine->sessionToJson(session->sessionId));
}

void Win32IDE::cmdMultiResponseShowStatus() {
    if (!m_multiResponseInitialized) {
        appendOutput(OutputSeverity::Info, "MultiResp: Not initialized.");
        return;
    }
    appendOutput(OutputSeverity::Info,
                 "MultiResp: Status JSON:\n" + m_multiResponseEngine->toJson());
}

void Win32IDE::cmdMultiResponseClearHistory() {
    if (!m_multiResponseInitialized) return;
    m_multiResponseEngine->shutdown();
    m_multiResponseEngine->initialize();
    appendOutput(OutputSeverity::Info, "MultiResp: History cleared.");
}

void Win32IDE::cmdMultiResponseApplyPreferred() {
    if (!m_multiResponseInitialized) return;
    auto* session = m_multiResponseEngine->getLatestSession();
    if (!session || session->preferredIndex < 0) {
        appendOutput(OutputSeverity::Info,
                     "MultiResp: No preferred response selected.");
        return;
    }
    const auto& resp = session->responses[session->preferredIndex];
    appendOutput(OutputSeverity::Info,
                 "MultiResp: Applied preferred response [" +
                 resp.templateName + "]:\n" + resp.content);
}

// ============================================================================
// HTTP ENDPOINT HANDLERS
// ============================================================================

static std::string buildMRJsonResponse(int status, const std::string& body) {
    std::string statusText = (status == 200) ? "OK" :
                             (status == 400) ? "Bad Request" :
                             (status == 404) ? "Not Found" : "Internal Server Error";
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << statusText << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n" << body;
    return oss.str();
}

static void sendMRResponse(SOCKET client, const std::string& resp) {
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleMultiResponseStatusEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(200,
            R"({"initialized":false,"status":"not_initialized"})"));
        return;
    }
    sendMRResponse(client, buildMRJsonResponse(200, m_multiResponseEngine->toJson()));
}

void Win32IDE::handleMultiResponseTemplatesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) { initMultiResponse(); }
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(500, R"({"error":"init_failed"})"));
        return;
    }

    auto templates = m_multiResponseEngine->getAllTemplates();
    json j = json::array();
    for (const auto& t : templates) {
        j.push_back({
            {"id", static_cast<int>(t.id)},
            {"name", t.name},
            {"shortLabel", t.shortLabel},
            {"description", t.description},
            {"temperature", t.temperature},
            {"maxTokens", t.maxTokens},
            {"enabled", t.enabled}
        });
    }
    sendMRResponse(client, buildMRJsonResponse(200, j.dump(2)));
}

void Win32IDE::handleMultiResponseGenerateEndpoint(SOCKET client,
                                                    const std::string& body) {
    if (!m_multiResponseInitialized) { initMultiResponse(); }
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(500, R"({"error":"init_failed"})"));
        return;
    }

    try {
        json req = json::parse(body);
        std::string prompt = req.value("prompt", "");
        int maxResp = req.value("maxResponses", 4);
        std::string context = req.value("context", "");

        if (prompt.empty()) {
            sendMRResponse(client, buildMRJsonResponse(400,
                R"({"error":"prompt_required"})"));
            return;
        }

        uint64_t sid = m_multiResponseEngine->startSession(prompt, maxResp, context);
        auto r = m_multiResponseEngine->generateAll(sid);

        if (!r.success) {
            json errJ = {{"error", r.detail}, {"sessionId", sid}};
            sendMRResponse(client, buildMRJsonResponse(500, errJ.dump()));
            return;
        }

        sendMRResponse(client, buildMRJsonResponse(200,
            m_multiResponseEngine->sessionToJson(sid)));
    } catch (const std::exception& e) {
        json errJ = {{"error", "parse_error"}, {"detail", e.what()}};
        sendMRResponse(client, buildMRJsonResponse(400, errJ.dump()));
    }
}

void Win32IDE::handleMultiResponseResultsEndpoint(SOCKET client,
                                                   const std::string& sessionId) {
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(404, R"({"error":"not_initialized"})"));
        return;
    }

    uint64_t sid = 0;
    try { sid = std::stoull(sessionId); }
    catch (...) {
        auto* latest = m_multiResponseEngine->getLatestSession();
        if (!latest) {
            sendMRResponse(client, buildMRJsonResponse(404,
                R"({"error":"no_sessions"})"));
            return;
        }
        sid = latest->sessionId;
    }

    sendMRResponse(client, buildMRJsonResponse(200,
        m_multiResponseEngine->sessionToJson(sid)));
}

void Win32IDE::handleMultiResponsePreferEndpoint(SOCKET client,
                                                  const std::string& body) {
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(404, R"({"error":"not_initialized"})"));
        return;
    }

    try {
        json req = json::parse(body);
        uint64_t sid = req.value("sessionId", (uint64_t)0);
        int idx = req.value("responseIndex", -1);
        std::string reason = req.value("reason", "");

        if (sid == 0) {
            auto* latest = m_multiResponseEngine->getLatestSession();
            if (latest) sid = latest->sessionId;
        }

        auto r = m_multiResponseEngine->setPreference(sid, idx, reason);
        if (!r.success) {
            json errJ = {{"error", r.detail}};
            sendMRResponse(client, buildMRJsonResponse(400, errJ.dump()));
            return;
        }

        sendMRResponse(client, buildMRJsonResponse(200,
            R"({"success":true,"message":"Preference recorded"})"));
    } catch (const std::exception& e) {
        json errJ = {{"error", "parse_error"}, {"detail", e.what()}};
        sendMRResponse(client, buildMRJsonResponse(400, errJ.dump()));
    }
}

void Win32IDE::handleMultiResponseStatsEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(200,
            R"({"initialized":false})"));
        return;
    }
    sendMRResponse(client, buildMRJsonResponse(200,
        m_multiResponseEngine->statsToJson()));
}

void Win32IDE::handleMultiResponsePreferencesEndpoint(SOCKET client) {
    if (!m_multiResponseInitialized) {
        sendMRResponse(client, buildMRJsonResponse(200,
            R"({"initialized":false,"preferences":[]})"));
        return;
    }
    sendMRResponse(client, buildMRJsonResponse(200,
        m_multiResponseEngine->preferencesToJson()));
}
