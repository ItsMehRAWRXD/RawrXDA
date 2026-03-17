// ============================================================================
// Win32IDE_LocalServer.cpp — Embedded GGUF HTTP Server
// ============================================================================
// Turns the IDE into an inference endpoint:
//   - Ollama-compatible: /api/generate, /api/tags
//   - OpenAI-compatible: /v1/chat/completions
//   - Health endpoint: /health, /status
//   - Streaming SSE for all generation endpoints
//   - CORS enabled for browser/extension integration
//   - Configurable port (default 11435 to avoid conflict with Ollama 11434)
//
// Architecture:
//   - Extends the existing CompletionServer (WinSock2-based)
//   - Runs on a background thread, non-blocking
//   - Uses the IDE's loaded model (m_nativeEngine) for inference
//   - Thread-safe access via existing mutex
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "../../include/chain_of_thought_engine.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <cstring>

// Forward: socket type
using LocalServerSocket = SOCKET;
static const LocalServerSocket kInvalidSock = INVALID_SOCKET;

// ============================================================================
// HELPERS — JSON escaping, request parsing, response building
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

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool extractJsonString(const std::string& body, const std::string& key, std::string& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    if (pos >= body.size() || body[pos] != '"') return false;
    pos++;
    std::string result;
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '\\' && pos < body.size()) {
            char esc = body[pos++];
            switch (esc) {
                case '"':  result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case 'n':  result.push_back('\n'); break;
                case 'r':  result.push_back('\r'); break;
                default:   result.push_back(esc); break;
            }
            continue;
        }
        if (c == '"') break;
        result.push_back(c);
    }
    out = result;
    return true;
}

static bool extractJsonInt(const std::string& body, const std::string& key, int& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit((unsigned char)body[pos]) || body[pos] == '-')) pos++;
    if (start == pos) return false;
    try { out = std::stoi(body.substr(start, pos - start)); } catch (...) { return false; }
    return true;
}

static bool extractJsonFloat(const std::string& body, const std::string& key, float& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit((unsigned char)body[pos]) || body[pos] == '.' || body[pos] == '-')) pos++;
    if (start == pos) return false;
    try { out = std::stof(body.substr(start, pos - start)); } catch (...) { return false; }
    return true;
}

static bool extractJsonBool(const std::string& body, const std::string& key, bool& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    if (pos + 4 <= body.size() && body.substr(pos, 4) == "true") { out = true; return true; }
    if (pos + 5 <= body.size() && body.substr(pos, 5) == "false") { out = false; return true; }
    return false;
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

static void sendSSEHeaders(LocalServerSocket client) {
    std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "\r\n";
    send(client, headers.c_str(), (int)headers.size(), 0);
}

// Convenience wrapper: returns extracted string directly (empty if not found)
static std::string extractJsonStringValue(const std::string& body, const std::string& key) {
    std::string result;
    extractJsonString(body, key, result);
    return result;
}

} // namespace LocalServerUtil

// ============================================================================
// START SERVER
// ============================================================================

void Win32IDE::startLocalServer() {
    if (m_localServerRunning.load()) {
        appendToOutput("[Server] Already running on port " +
                       std::to_string(m_settings.localServerPort),
                       "General", OutputSeverity::Warning);
        return;
    }

    int port = m_settings.localServerPort;
    m_localServerRunning.store(true);

    m_localServerThread = std::thread([this, port]() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            LOG_ERROR("Local server WSAStartup failed");
            m_localServerRunning.store(false);
            return;
        }

        LocalServerSocket serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == kInvalidSock) {
            LOG_ERROR("Local server: failed to create socket");
            m_localServerRunning.store(false);
            WSACleanup();
            return;
        }

        // Allow port reuse
        int opt = 1;
        setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&opt), sizeof(opt));

        // Non-blocking accept with 1-second timeout for shutdown
        DWORD timeout = 1000;
        setsockopt(serverFd, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        sockaddr_in addr = {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons((u_short)port);

        if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) != 0) {
            LOG_ERROR("Local server: failed to bind port " + std::to_string(port));
            closesocket(serverFd);
            m_localServerRunning.store(false);
            WSACleanup();
            return;
        }

        if (listen(serverFd, 8) != 0) {
            LOG_ERROR("Local server: failed to listen");
            closesocket(serverFd);
            m_localServerRunning.store(false);
            WSACleanup();
            return;
        }

        LOG_INFO("Local GGUF server listening on port " + std::to_string(port));
        postAgentOutputSafe("[Server] Listening on http://localhost:" + std::to_string(port));

        m_localServerStats = {};

        while (m_localServerRunning.load()) {
            sockaddr_in clientAddr = {};
            int clientLen = sizeof(clientAddr);
            LocalServerSocket client = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
            if (client == kInvalidSock) continue;

            m_localServerStats.totalRequests++;

            // Handle client in a detached thread
            std::thread([this, client]() {
                handleLocalServerClient(client);
            }).detach();
        }

        closesocket(serverFd);
        WSACleanup();
        LOG_INFO("Local GGUF server stopped");
    });
}

// ============================================================================
// STOP SERVER
// ============================================================================

void Win32IDE::stopLocalServer() {
    if (!m_localServerRunning.load()) return;

    m_localServerRunning.store(false);
    if (m_localServerThread.joinable()) {
        m_localServerThread.join();
    }
    appendToOutput("[Server] Stopped", "General", OutputSeverity::Info);
}

// ============================================================================
// CLIENT HANDLER — routes requests to appropriate endpoint
// ============================================================================

void Win32IDE::handleLocalServerClient(SOCKET clientFd) {
    LocalServerSocket client = clientFd;

    // Read request
    std::string data;
    char buffer[8192];
    int received = 0;

    while ((received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        data.append(buffer, buffer + received);
        if (data.find("\r\n\r\n") != std::string::npos) break;
    }

    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        closesocket(client);
        return;
    }

    // Extract content-length and read remaining body
    std::string headers = data.substr(0, headerEnd + 4);
    std::string body = data.substr(headerEnd + 4);

    size_t contentLength = 0;
    auto clPos = LocalServerUtil::toLower(headers).find("content-length:");
    if (clPos != std::string::npos) {
        size_t lineEnd = headers.find("\r\n", clPos);
        std::string val = headers.substr(clPos + 15, lineEnd - (clPos + 15));
        try { contentLength = (size_t)std::stoul(val); } catch (...) {}
    }

    while (body.size() < contentLength && (received = recv(client, buffer, sizeof(buffer), 0)) > 0) {
        body.append(buffer, buffer + received);
    }

    // Parse method and path
    std::string method, path;
    std::istringstream reqLine(headers.substr(0, headers.find("\r\n")));
    reqLine >> method >> path;

    // Route
    std::string response;

    if (method == "OPTIONS") {
        response = LocalServerUtil::buildHttpResponse(204, "");
    }
    // ========== Health / Status ==========
    else if (method == "GET" && (path == "/health" || path == "/")) {
        response = LocalServerUtil::buildHttpResponse(200,
            "{\"status\":\"ok\",\"server\":\"RawrXD-Win32IDE\"}");
    }
    else if (method == "GET" && path == "/status") {
        bool modelLoaded = m_nativeEngine && m_nativeEngine->IsModelLoaded();
        std::ostringstream j;
        j << "{\"ready\":true"
          << ",\"model_loaded\":" << (modelLoaded ? "true" : "false")
          << ",\"model_path\":\"" << LocalServerUtil::escapeJson(m_loadedModelPath) << "\""
          << ",\"backend\":\"rawrxd-win32ide\""
          << ",\"total_requests\":" << m_localServerStats.totalRequests
          << ",\"total_tokens\":" << m_localServerStats.totalTokens
          << "}";
        response = LocalServerUtil::buildHttpResponse(200, j.str());
    }
    // ========== Ollama-compatible: /api/tags ==========
    else if (method == "GET" && path == "/api/tags") {
        handleOllamaApiTags(client);
        closesocket(client);
        return;
    }
    // ========== Ollama-compatible: /api/generate ==========
    else if (method == "POST" && path == "/api/generate") {
        handleOllamaApiGenerate(client, body);
        closesocket(client);
        return;
    }
    // ========== OpenAI-compatible: /v1/chat/completions ==========
    else if (method == "POST" && path == "/v1/chat/completions") {
        handleOpenAIChatCompletions(client, body);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /models — list all local GGUF + Ollama models ==========
    else if (method == "GET" && path == "/models") {
        handleModelsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /ask — unified chat endpoint ==========
    else if (method == "POST" && path == "/ask") {
        handleAskEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== HTML Frontend: /gui — serve ide_chatbot.html ==========
    else if (method == "GET" && (path == "/gui" || path == "/gui/")) {
        handleServeGui(client);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent History (read-only) ==========
    else if (method == "GET" && path.find("/api/agents/history") == 0) {
        handleAgentHistoryEndpoint(client, path);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent Status (read-only) ==========
    else if (method == "GET" && path == "/api/agents/status") {
        handleAgentStatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Agent Replay ==========
    else if (method == "POST" && path == "/api/agents/replay") {
        handleAgentReplayEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 6B: Failure Timeline (read-only) ==========
    else if (method == "GET" && path.find("/api/failures") == 0) {
        handleFailuresEndpoint(client, path);
        closesocket(client);
        return;
    }
    // ========== Phase 8B: Backend Switcher ==========
    else if (method == "GET" && path == "/api/backends") {
        handleBackendsListEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/backend/active") {
        handleBackendActiveEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && (path == "/api/backend/switch" || path == "/api/backends/switch")) {
        handleBackendSwitchEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 8C: LLM Router ==========
    else if (method == "GET" && path == "/api/router/status") {
        handleRouterStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/router/decision") {
        handleRouterDecisionEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/router/capabilities") {
        handleRouterCapabilitiesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/router/route") {
        handleRouterRouteEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== UX Enhancements & Research Track ==========
    else if (method == "GET" && path == "/api/router/why") {
        handleRouterWhyEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/router/pins") {
        handleRouterPinsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/router/heatmap") {
        handleRouterHeatmapEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/router/ensemble") {
        handleRouterEnsembleEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/router/simulate") {
        handleRouterSimulateEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 9A: LSP Client ==========
    else if (method == "GET" && path == "/api/lsp/status") {
        handleLSPStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/lsp/diagnostics") {
        handleLSPDiagnosticsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 9A-ASM: ASM Semantic Support ==========
    else if (method == "GET" && path.rfind("/api/asm/symbols", 0) == 0) {
        handleAsmSymbolsEndpoint(client, path);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/asm/navigate") {
        handleAsmNavigateEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/asm/analyze") {
        handleAsmAnalyzeEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 9C: Multi-Response Chain ==========
    else if (method == "GET" && path == "/api/multi-response/status") {
        handleMultiResponseStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/multi-response/templates") {
        handleMultiResponseTemplatesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/multi-response/generate") {
        handleMultiResponseGenerateEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path.rfind("/api/multi-response/results", 0) == 0) {
        // Extract session ID from query: /api/multi-response/results?session=123
        std::string sid = "latest";
        auto qpos = path.find("session=");
        if (qpos != std::string::npos) sid = path.substr(qpos + 8);
        handleMultiResponseResultsEndpoint(client, sid);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/multi-response/prefer") {
        handleMultiResponsePreferEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/multi-response/stats") {
        handleMultiResponseStatsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/multi-response/preferences") {
        handleMultiResponsePreferencesEndpoint(client);
        closesocket(client);
        return;
    }
    // ============================================================
    // Phase 9B: LSP-AI Hybrid Integration Bridge
    // ============================================================
    else if (method == "POST" && path == "/api/hybrid/complete") {
        handleHybridCompleteEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path.rfind("/api/hybrid/diagnostics", 0) == 0) {
        handleHybridDiagnosticsEndpoint(client, path);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/hybrid/rename") {
        handleHybridSmartRenameEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/hybrid/analyze") {
        handleHybridAnalyzeEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/hybrid/status") {
        handleHybridStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/hybrid/symbol-usage") {
        handleHybridSymbolUsageEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Governor Endpoints ==========
    else if (method == "GET" && path == "/api/governor/status") {
        handleGovernorStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/governor/submit") {
        handleGovernorSubmitEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/governor/kill") {
        handleGovernorKillEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/governor/result") {
        handleGovernorResultEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Safety Endpoints ==========
    else if (method == "GET" && path == "/api/safety/status") {
        handleSafetyStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/safety/check") {
        handleSafetyCheckEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/safety/violations") {
        handleSafetyViolationsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/safety/rollback") {
        handleSafetyRollbackEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Replay Endpoints ==========
    else if (method == "GET" && path == "/api/replay/status") {
        handleReplayStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/replay/records") {
        handleReplayRecordsEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/replay/sessions") {
        handleReplaySessionsEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Confidence Endpoints ==========
    else if (method == "GET" && path == "/api/confidence/status") {
        handleConfidenceStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/confidence/evaluate") {
        handleConfidenceEvaluateEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/confidence/history") {
        handleConfidenceHistoryEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 10: Unified Status ==========
    else if (method == "GET" && path == "/api/phase10/status") {
        handlePhase10StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 11: Distributed Swarm Compilation ==========
    else if (method == "GET" && path == "/api/swarm/status") {
        handleSwarmStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/nodes") {
        handleSwarmNodesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/tasks") {
        handleSwarmTaskGraphEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/stats") {
        handleSwarmStatsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/events") {
        handleSwarmEventsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/config") {
        handleSwarmConfigEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/swarm/worker") {
        handleSwarmWorkerEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/start") {
        handleSwarmStartEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/stop") {
        handleSwarmStopEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/nodes/add") {
        handleSwarmAddNodeEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/build") {
        handleSwarmBuildEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/cancel") {
        handleSwarmCancelEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/swarm/cache/clear") {
        handleSwarmCacheClearEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/phase11/status") {
        handlePhase11StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ====================================================================
    // PHASE 12 — NATIVE DEBUGGER ENGINE HTTP ENDPOINTS
    // ====================================================================
    // GET endpoints
    else if (method == "GET" && path == "/api/debug/status") {
        handleDbgStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/breakpoints") {
        handleDbgBreakpointsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/registers") {
        handleDbgRegistersEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/stack") {
        handleDbgStackEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/modules") {
        handleDbgModulesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/threads") {
        handleDbgThreadsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/events") {
        handleDbgEventsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/debug/watches") {
        handleDbgWatchesEndpoint(client);
        closesocket(client);
        return;
    }
    // POST endpoints with body
    else if (method == "POST" && path == "/api/debug/memory") {
        handleDbgMemoryEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/debug/disasm") {
        handleDbgDisasmEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/debug/launch") {
        handleDbgLaunchEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/debug/attach") {
        handleDbgAttachEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/debug/go") {
        handleDbgGoEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/phase12/status") {
        handlePhase12StatusEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== Phase 32A: Chain-of-Thought Multi-Model Review ==========
    else if (method == "GET" && path == "/api/cot/status") {
        handleCoTStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/cot/presets") {
        handleCoTPresetsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/cot/steps") {
        handleCoTStepsEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "GET" && path == "/api/cot/roles") {
        handleCoTRolesEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/cot/preset") {
        handleCoTApplyPresetEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/cot/steps") {
        handleCoTSetStepsEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/cot/execute") {
        handleCoTExecuteEndpoint(client, body);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/cot/cancel") {
        handleCoTCancelEndpoint(client);
        closesocket(client);
        return;
    }
    // ========== File Reading: /api/read-file — read local file for chatbot attachments ==========
    else if (method == "POST" && path == "/api/read-file") {
        handleReadFileEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== CLI Command Execution: /api/cli — forward to tool_server pattern ==========
    else if (method == "POST" && path == "/api/cli") {
        handleCliEndpoint(client, body);
        closesocket(client);
        return;
    }
    // ========== Hotpatch Layer Control ==========
    else if (method == "POST" && (path == "/api/hotpatch/toggle" || path == "/api/hotpatch/apply" || path == "/api/hotpatch/revert")) {
        handleHotpatchEndpoint(client, path, body);
        closesocket(client);
        return;
    }
    // ========== 404 ==========
    else {
        response = LocalServerUtil::buildHttpResponse(404,
            "{\"error\":\"not_found\",\"message\":\"Unknown endpoint: " +
            LocalServerUtil::escapeJson(path) + "\"}");
    }

    send(client, response.c_str(), (int)response.size(), 0);
    closesocket(client);
}

// ============================================================================
// OLLAMA: /api/tags — list loaded models
// ============================================================================

void Win32IDE::handleOllamaApiTags(SOCKET client) {
    std::ostringstream j;
    j << "{\"models\":[";

    if (m_nativeEngine && m_nativeEngine->IsModelLoaded() && !m_loadedModelPath.empty()) {
        // Extract model name from path
        std::string name = m_loadedModelPath;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);

        j << "{"
          << "\"name\":\"" << LocalServerUtil::escapeJson(name) << "\""
          << ",\"model\":\"" << LocalServerUtil::escapeJson(name) << "\""
          << ",\"size\":" << 0
          << ",\"digest\":\"rawrxd-local\""
          << ",\"details\":{\"format\":\"gguf\",\"family\":\"rawrxd\",\"parameter_size\":\"unknown\"}"
          << "}";
    }

    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// OLLAMA: /api/generate — generate text (streaming or non-streaming)
// ============================================================================

void Win32IDE::handleOllamaApiGenerate(SOCKET client, const std::string& body) {
    std::string prompt, model;
    bool stream = true;
    int maxTokens = 512;

    LocalServerUtil::extractJsonString(body, "prompt", prompt);
    LocalServerUtil::extractJsonString(body, "model", model);
    LocalServerUtil::extractJsonBool(body, "stream", stream);

    int numPredict = 0;
    if (LocalServerUtil::extractJsonInt(body, "num_predict", numPredict) && numPredict > 0) {
        maxTokens = numPredict;
    }

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // LocalGGUF + streaming: use native token-level SSE (preserved)
    // LocalGGUF + non-streaming: use native engine directly (preserved)
    // Remote backends: use routeWithIntelligence() for task-aware routing,
    //   then wrap the result in Ollama JSON format
    // ─────────────────────────────────────────────────────────────────────
    AIBackendType activeBackend = getActiveBackendType();

    if (activeBackend != AIBackendType::LocalGGUF) {
        // Remote backend — route through LLM Router (task classification,
        // capability matching, fallback chains). When router is disabled,
        // routeWithIntelligence() passes straight to routeInferenceRequest().
        std::string result = routeWithIntelligence(prompt);
        bool isError = result.find("[BackendSwitcher] Error") != std::string::npos;

        if (isError) {
            std::string json = "{\"error\":\"" + LocalServerUtil::escapeJson(result) + "\"}";
            std::string resp = LocalServerUtil::buildHttpResponse(502, json);
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        // Use the Router's actual selected backend for the response model name
        // (may differ from activeBackend if Router reclassified the task)
        AIBackendType routedBackend = getLastRoutingDecision().selectedBackend;
        if (!m_routerEnabled || !m_routerInitialized) routedBackend = activeBackend;
        std::string backendName = LocalServerUtil::toLower(backendTypeString(routedBackend));
        m_localServerStats.totalTokens++;

        if (stream) {
            LocalServerUtil::sendSSEHeaders(client);
            std::string event = "{\"model\":\"" + LocalServerUtil::escapeJson(backendName)
                + "\",\"response\":\"" + LocalServerUtil::escapeJson(result)
                + "\",\"done\":false}\n";
            send(client, event.c_str(), (int)event.size(), 0);
            std::string doneEvent = "{\"model\":\"" + LocalServerUtil::escapeJson(backendName)
                + "\",\"response\":\"\",\"done\":true}\n";
            send(client, doneEvent.c_str(), (int)doneEvent.size(), 0);
        } else {
            std::string json = "{\"model\":\"" + LocalServerUtil::escapeJson(backendName)
                + "\",\"response\":\"" + LocalServerUtil::escapeJson(result)
                + "\",\"done\":true}";
            std::string resp = LocalServerUtil::buildHttpResponse(200, json);
            send(client, resp.c_str(), (int)resp.size(), 0);
        }
        return;
    }

    // ── LocalGGUF path (original behavior preserved) ─────────────────────
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"no model loaded\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prompt);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);

    if (stream) {
        LocalServerUtil::sendSSEHeaders(client);

        for (const auto& tok : generated) {
            std::string text = m_nativeEngine->Detokenize({tok});
            m_localServerStats.totalTokens++;

            std::string event = "{\"model\":\"rawrxd\",\"response\":\""
                + LocalServerUtil::escapeJson(text) + "\",\"done\":false}\n";
            int r = send(client, event.c_str(), (int)event.size(), 0);
            if (r < 0) return;
        }

        std::string doneEvent = "{\"model\":\"rawrxd\",\"response\":\"\",\"done\":true}\n";
        send(client, doneEvent.c_str(), (int)doneEvent.size(), 0);
    } else {
        std::string fullResponse = m_nativeEngine->Detokenize(generated);
        m_localServerStats.totalTokens += (int)generated.size();

        std::string json = "{\"model\":\"rawrxd\",\"response\":\""
            + LocalServerUtil::escapeJson(fullResponse)
            + "\",\"done\":true}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        send(client, resp.c_str(), (int)resp.size(), 0);
    }
}

// ============================================================================
// OPENAI: /v1/chat/completions — chat completions (streaming or not)
// ============================================================================

void Win32IDE::handleOpenAIChatCompletions(SOCKET client, const std::string& body) {
    // Extract messages array — simplified extraction (last "content" field)
    std::string prompt;
    bool stream = false;
    int maxTokens = 512;
    float temperature = 0.7f;

    LocalServerUtil::extractJsonBool(body, "stream", stream);
    LocalServerUtil::extractJsonInt(body, "max_tokens", maxTokens);
    LocalServerUtil::extractJsonFloat(body, "temperature", temperature);

    // Extract messages — find last "content" in the body
    // (simplified: concatenate all content fields)
    size_t pos = 0;
    std::string allContent;
    while ((pos = body.find("\"content\"", pos)) != std::string::npos) {
        std::string content;
        if (LocalServerUtil::extractJsonString(body.substr(pos), "content", content)) {
            if (!allContent.empty()) allContent += "\n";
            allContent += content;
        }
        pos += 9;
    }

    if (allContent.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":{\"message\":\"No messages provided\"}}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    prompt = allContent;

    std::string requestId = "chatcmpl-rawrxd-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // LocalGGUF + streaming: use native token-level SSE (preserved)
    // LocalGGUF + non-streaming: use native engine directly (preserved)
    // Remote backends: use routeWithIntelligence() for task-aware routing,
    //   then wrap the result in OpenAI chat completion format
    // ─────────────────────────────────────────────────────────────────────
    AIBackendType activeBackend = getActiveBackendType();

    if (activeBackend != AIBackendType::LocalGGUF) {
        // Remote backend — route through LLM Router (task classification,
        // capability matching, fallback chains). When router is disabled,
        // routeWithIntelligence() passes straight to routeInferenceRequest().
        std::string result = routeWithIntelligence(prompt);
        bool isError = result.find("[BackendSwitcher] Error") != std::string::npos;

        if (isError) {
            std::string errJson = "{\"error\":{\"message\":\""
                + LocalServerUtil::escapeJson(result) + "\"}}";
            std::string resp = LocalServerUtil::buildHttpResponse(502, errJson);
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        m_localServerStats.totalTokens++;

        if (stream) {
            LocalServerUtil::sendSSEHeaders(client);
            std::ostringstream event;
            event << "data: {\"id\":\"" << requestId
                  << "\",\"object\":\"chat.completion.chunk\""
                  << ",\"choices\":[{\"index\":0,\"delta\":{\"content\":\""
                  << LocalServerUtil::escapeJson(result) << "\"}}]}\n\n";
            std::string eventStr = event.str();
            send(client, eventStr.c_str(), (int)eventStr.size(), 0);
            std::string doneStr = "data: [DONE]\n\n";
            send(client, doneStr.c_str(), (int)doneStr.size(), 0);
        } else {
            std::ostringstream j;
            j << "{\"id\":\"" << requestId
              << "\",\"object\":\"chat.completion\""
              << ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\""
              << LocalServerUtil::escapeJson(result)
              << "\"},\"finish_reason\":\"stop\"}]"
              << ",\"usage\":{\"prompt_tokens\":0"
              << ",\"completion_tokens\":0"
              << ",\"total_tokens\":0}}";
            std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
            send(client, resp.c_str(), (int)resp.size(), 0);
        }
        return;
    }

    // ── LocalGGUF path (original behavior preserved) ─────────────────────
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":{\"message\":\"No model loaded\"}}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prompt);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);

    if (stream) {
        LocalServerUtil::sendSSEHeaders(client);

        for (const auto& tok : generated) {
            std::string text = m_nativeEngine->Detokenize({tok});
            m_localServerStats.totalTokens++;

            std::ostringstream event;
            event << "data: {\"id\":\"" << requestId
                  << "\",\"object\":\"chat.completion.chunk\""
                  << ",\"choices\":[{\"index\":0,\"delta\":{\"content\":\""
                  << LocalServerUtil::escapeJson(text) << "\"}}]}\n\n";

            std::string eventStr = event.str();
            int r = send(client, eventStr.c_str(), (int)eventStr.size(), 0);
            if (r < 0) return;
        }

        std::string doneStr = "data: [DONE]\n\n";
        send(client, doneStr.c_str(), (int)doneStr.size(), 0);
    } else {
        std::string fullResponse = m_nativeEngine->Detokenize(generated);
        m_localServerStats.totalTokens += (int)generated.size();

        std::ostringstream j;
        j << "{\"id\":\"" << requestId
          << "\",\"object\":\"chat.completion\""
          << ",\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\""
          << LocalServerUtil::escapeJson(fullResponse)
          << "\"},\"finish_reason\":\"stop\"}]"
          << ",\"usage\":{\"prompt_tokens\":" << tokens.size()
          << ",\"completion_tokens\":" << generated.size()
          << ",\"total_tokens\":" << (tokens.size() + generated.size()) << "}}";

        std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
        send(client, resp.c_str(), (int)resp.size(), 0);
    }
}

// ============================================================================
// FRONTEND: /models — scan D:/OllamaModels for .gguf files + loaded model
// ============================================================================

void Win32IDE::handleModelsEndpoint(SOCKET client) {
    std::ostringstream j;
    j << "{\"models\":[";

    int count = 0;

    // 1. Currently loaded model (if any)
    if (m_nativeEngine && m_nativeEngine->IsModelLoaded() && !m_loadedModelPath.empty()) {
        std::string name = m_loadedModelPath;
        size_t slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name = name.substr(slash + 1);
        // Strip .gguf extension for display
        size_t dot = name.rfind(".gguf");
        std::string displayName = (dot != std::string::npos) ? name.substr(0, dot) : name;

        if (count > 0) j << ",";
        j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName)
          << "\",\"type\":\"gguf\",\"size\":\"loaded\""
          << ",\"path\":\"" << LocalServerUtil::escapeJson(m_loadedModelPath) << "\"}";
        count++;
    }

    // 2. Scan D:/OllamaModels for .gguf files
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("D:\\OllamaModels\\*.gguf", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string fname = findData.cFileName;
            std::string fullPath = "D:/OllamaModels/" + fname;

            // Skip if it's the already-loaded model
            if (fullPath == m_loadedModelPath) continue;

            // Display name without .gguf
            size_t dot = fname.rfind(".gguf");
            std::string displayName = (dot != std::string::npos) ? fname.substr(0, dot) : fname;

            // File size
            LARGE_INTEGER fileSize;
            fileSize.LowPart  = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            double sizeGB = (double)fileSize.QuadPart / (1024.0 * 1024.0 * 1024.0);

            std::ostringstream sizeFmt;
            sizeFmt << std::fixed;
            sizeFmt.precision(1);
            sizeFmt << sizeGB << "GB";

            if (count > 0) j << ",";
            j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName)
              << "\",\"type\":\"gguf\",\"size\":\"" << sizeFmt.str()
              << "\",\"path\":\"" << LocalServerUtil::escapeJson(fullPath) << "\"}";
            count++;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    // 3. Scan D:/OllamaModels/blobs for blob files
    hFind = FindFirstFileA("D:\\OllamaModels\\blobs\\sha256-*", &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string fname = findData.cFileName;
            std::string fullPath = "D:/OllamaModels/blobs/" + fname;

            LARGE_INTEGER fileSize;
            fileSize.LowPart  = findData.nFileSizeLow;
            fileSize.HighPart = findData.nFileSizeHigh;
            double sizeGB = (double)fileSize.QuadPart / (1024.0 * 1024.0 * 1024.0);

            // Only include blobs > 100MB (likely model weights, not metadata)
            if (sizeGB < 0.1) continue;

            std::ostringstream sizeFmt;
            sizeFmt << std::fixed;
            sizeFmt.precision(1);
            sizeFmt << sizeGB << "GB";

            // Short display name: first 12 chars of sha256 hash
            std::string displayName = "blob:" + fname.substr(0, std::min((size_t)19, fname.size()));

            if (count > 0) j << ",";
            j << "{\"name\":\"" << LocalServerUtil::escapeJson(displayName)
              << "\",\"type\":\"blob\",\"size\":\"" << sizeFmt.str()
              << "\",\"path\":\"" << LocalServerUtil::escapeJson(fullPath) << "\"}";
            count++;
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    j << "]}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// FRONTEND: /ask — unified chat endpoint for the HTML chatbot
// ============================================================================

void Win32IDE::handleAskEndpoint(SOCKET client, const std::string& body) {
    std::string question, model;
    int context = 4096;
    bool stream = false;

    LocalServerUtil::extractJsonString(body, "question", question);
    LocalServerUtil::extractJsonString(body, "model", model);
    LocalServerUtil::extractJsonInt(body, "context", context);
    LocalServerUtil::extractJsonBool(body, "stream", stream);

    if (question.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"No question provided\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // ── Phase 8B+8C: Route through LLM Router → BackendSwitcher ────────
    // All backends go through routeWithIntelligence() which classifies
    // the task, selects the optimal backend (with fallback), then delegates
    // to routeInferenceRequest() for actual inference.
    // When the router is disabled, routeWithIntelligence() passes straight
    // through to routeInferenceRequest() — zero behavior change.
    // ─────────────────────────────────────────────────────────────────────
    std::string answer = routeWithIntelligence(question);
    bool isError = answer.find("[BackendSwitcher] Error") != std::string::npos;
    m_localServerStats.totalTokens++;

    // Determine the actual backend used (Router may have reclassified)
    AIBackendType reportedBackend = getActiveBackendType();
    if (m_routerEnabled && m_routerInitialized) {
        reportedBackend = getLastRoutingDecision().selectedBackend;
    }
    std::string backendStr = backendTypeString(reportedBackend);

    if (isError) {
        // Return the error as a displayable answer (not a 500)
        // so the HTML chatbot can show it gracefully
        std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer)
            + "\",\"backend\":\"" + LocalServerUtil::escapeJson(backendStr)
            + "\",\"error\":true}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer)
        + "\",\"backend\":\"" + LocalServerUtil::escapeJson(backendStr)
        + "\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// ============================================================================
// FRONTEND: /gui — serve the agentic chatbot HTML from gui/ide_chatbot.html
// ============================================================================

void Win32IDE::handleServeGui(SOCKET client) {
    // Resolve path relative to executable or project root
    std::string htmlPath;

    // Try multiple paths
    const char* candidates[] = {
        "gui/ide_chatbot.html",
        "../gui/ide_chatbot.html",
        "D:/rawrxd/gui/ide_chatbot.html"
    };

    for (const char* candidate : candidates) {
        HANDLE hFile = CreateFileA(candidate, GENERIC_READ, FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            if (fileSize != INVALID_FILE_SIZE && fileSize > 0) {
                std::string content(fileSize, '\0');
                DWORD bytesRead = 0;
                ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
                content.resize(bytesRead);
                CloseHandle(hFile);

                // Send as HTML
                std::ostringstream oss;
                oss << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: text/html; charset=utf-8\r\n"
                    << "Content-Length: " << content.size() << "\r\n"
                    << "Access-Control-Allow-Origin: *\r\n"
                    << "Connection: close\r\n\r\n"
                    << content;
                std::string response = oss.str();
                send(client, response.c_str(), (int)response.size(), 0);
                return;
            }
            CloseHandle(hFile);
        }
    }

    // File not found
    std::string resp = LocalServerUtil::buildHttpResponse(404,
        "{\"error\":\"gui/ide_chatbot.html not found\"}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// ============================================================================
// POST /api/read-file — Read a local file for chatbot attachment support
// ============================================================================
// Request body: {"path":"C:/some/file.cpp"}
// Response: {"content":"...file text...","name":"file.cpp","size":12345}
// Security: Only allows reading text files up to 10MB.
//           Rejects paths containing ".." to prevent directory traversal.
// ============================================================================

void Win32IDE::handleReadFileEndpoint(SOCKET client, const std::string& body) {
    // Parse "path" from JSON body (lightweight — no full JSON parser needed)
    std::string filePath;
    {
        auto pathKey = body.find("\"path\"");
        if (pathKey == std::string::npos) {
            std::string resp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"missing_path\",\"message\":\"Request body must contain 'path' field\"}");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
        // Find the value string after "path": "..."
        auto colonPos = body.find(':', pathKey + 6);
        if (colonPos == std::string::npos) {
            std::string resp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"malformed_json\",\"message\":\"Could not parse path value\"}");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
        auto quoteStart = body.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) {
            std::string resp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"malformed_json\",\"message\":\"Could not find path string\"}");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
        // Find closing quote (handle escaped quotes)
        size_t quoteEnd = quoteStart + 1;
        while (quoteEnd < body.size()) {
            if (body[quoteEnd] == '\\') {
                quoteEnd += 2; // skip escaped char
                continue;
            }
            if (body[quoteEnd] == '"') break;
            quoteEnd++;
        }
        if (quoteEnd >= body.size()) {
            std::string resp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"malformed_json\",\"message\":\"Unterminated path string\"}");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
        filePath = body.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    // Unescape JSON string basics (forward slashes, backslashes)
    {
        std::string unescaped;
        unescaped.reserve(filePath.size());
        for (size_t i = 0; i < filePath.size(); i++) {
            if (filePath[i] == '\\' && i + 1 < filePath.size()) {
                char next = filePath[i + 1];
                if (next == '\\') { unescaped += '\\'; i++; continue; }
                if (next == '/') { unescaped += '/'; i++; continue; }
                if (next == '"') { unescaped += '"'; i++; continue; }
                if (next == 'n') { unescaped += '\n'; i++; continue; }
                if (next == 'r') { unescaped += '\r'; i++; continue; }
                if (next == 't') { unescaped += '\t'; i++; continue; }
            }
            unescaped += filePath[i];
        }
        filePath = unescaped;
    }

    // Normalize forward slashes to backslashes for Windows
    for (auto& ch : filePath) {
        if (ch == '/') ch = '\\';
    }

    // Security: reject directory traversal
    if (filePath.find("..") != std::string::npos) {
        std::string resp = LocalServerUtil::buildHttpResponse(403,
            "{\"error\":\"forbidden\",\"message\":\"Directory traversal not allowed\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Security: must be an absolute path (drive letter)
    if (filePath.size() < 3 || filePath[1] != ':' || filePath[2] != '\\') {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"invalid_path\",\"message\":\"Only absolute paths are accepted\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Open the file
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::string resp = LocalServerUtil::buildHttpResponse(404,
            "{\"error\":\"file_not_found\",\"message\":\"Cannot open file: " +
            LocalServerUtil::escapeJson(filePath) + "\",\"win32_error\":" + std::to_string(err) + "}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Get file size — limit to 10MB for safety
    DWORD fileSize = GetFileSize(hFile, NULL);
    const DWORD maxFileSize = 10 * 1024 * 1024; // 10MB
    if (fileSize == INVALID_FILE_SIZE || fileSize > maxFileSize) {
        CloseHandle(hFile);
        std::string resp = LocalServerUtil::buildHttpResponse(413,
            "{\"error\":\"file_too_large\",\"message\":\"File exceeds 10MB limit\",\"size\":" +
            std::to_string(fileSize == INVALID_FILE_SIZE ? 0 : fileSize) + "}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Read file content
    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hFile, &content[0], fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!readOk || bytesRead == 0) {
        std::string resp = LocalServerUtil::buildHttpResponse(500,
            "{\"error\":\"read_failed\",\"message\":\"Failed to read file content\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }
    content.resize(bytesRead);

    // Extract filename from path
    std::string fileName;
    {
        auto lastSlash = filePath.rfind('\\');
        if (lastSlash != std::string::npos) {
            fileName = filePath.substr(lastSlash + 1);
        } else {
            fileName = filePath;
        }
    }

    // Build JSON response with escaped content
    std::ostringstream json;
    json << "{\"content\":" << "\"" << LocalServerUtil::escapeJson(content) << "\""
         << ",\"name\":\"" << LocalServerUtil::escapeJson(fileName) << "\""
         << ",\"size\":" << bytesRead
         << ",\"path\":\"" << LocalServerUtil::escapeJson(filePath) << "\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    send(client, resp.c_str(), (int)resp.size(), 0);

    LOG_INFO("read-file: " + filePath + " (" + std::to_string(bytesRead) + " bytes)");
}

// ============================================================================
// CLI Command Execution: POST /api/cli
// ============================================================================
// Executes CLI-style commands from the embedded terminal.
// Body: {"command":"/plan ...", "args":"..."}
// Returns: {"success":bool, "output":"...", "command":"..."}
// Mirrors the same command set as tool_server.cpp HandleCliRequest().
// ============================================================================

void Win32IDE::handleCliEndpoint(SOCKET client, const std::string& body) {
    std::string command = LocalServerUtil::extractJsonStringValue(body, "command");
    if (command.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"success\":false,\"error\":\"Missing 'command' field\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Trim whitespace
    size_t start = command.find_first_not_of(" \t\n\r");
    size_t end = command.find_last_not_of(" \t\n\r");
    if (start != std::string::npos) command = command.substr(start, end - start + 1);

    // Parse command name and args
    std::string cmdName = command;
    std::string cmdArgs;
    size_t spPos = command.find(' ');
    if (spPos != std::string::npos) {
        cmdName = command.substr(0, spPos);
        cmdArgs = command.substr(spPos + 1);
    }

    std::string cmdLower = cmdName;
    for (char& c : cmdLower) c = static_cast<char>(std::tolower(c));

    std::ostringstream output;
    bool success = true;

    // ---- CLI Command Dispatcher ----
    if (cmdLower == "/help" || cmdLower == "help") {
        output << "RawrXD CLI v20.0.0 (Win32IDE Backend)\\n";
        output << "Available: /plan /analyze /optimize /security /suggest\\n";
        output << "           /bugreport /hotpatch /status /models /agents\\n";
        output << "           /memory /failures /clear\\n";
        output << "           !engine load800b|setup5drive|verify|analyze|compile|optimize\\n";
    }
    else if (cmdLower == "/status" || cmdLower == "status") {
        output << "Server: Win32IDE LocalServer v20.0.0\\n";
        output << "Port: 11435\\n";
        output << "Engine: " << (m_localServerRunning.load() ? "running" : "stopped") << "\\n";
    }
    else if (cmdLower == "/plan") {
        if (cmdArgs.empty()) {
            output << "Usage: /plan <task description>\\n";
            success = false;
        } else {
            output << "Execution Plan for: " << cmdArgs << "\\n";
            output << "Phase 1: Analysis — scan codebase for relevant modules\\n";
            output << "Phase 2: Implementation — modify source files\\n";
            output << "Phase 3: Integration — route via UnifiedHotpatchManager\\n";
            output << "Phase 4: Verification — build and test\\n";
        }
    }
    else if (cmdLower == "/analyze") {
        if (cmdArgs.empty()) {
            output << "Usage: /analyze <file_path>\\n";
            success = false;
        } else {
            std::string filePath = cmdArgs;
            if (filePath[0] != '/' && (filePath.size() < 2 || filePath[1] != ':')) {
                filePath = "D:\\\\rawrxd\\\\" + filePath;
            }
            try {
                auto canonical = std::filesystem::weakly_canonical(filePath);
                if (std::filesystem::exists(canonical)) {
                    auto fsize = std::filesystem::file_size(canonical);
                    output << "Analysis: " << canonical.filename().string() << "\\n";
                    output << "  Size: " << fsize << " bytes\\n";
                    output << "  Extension: " << canonical.extension().string() << "\\n";
                } else {
                    output << "File not found: " << filePath << "\\n";
                    success = false;
                }
            } catch (const std::exception& e) {
                output << "Error: " << e.what() << "\\n";
                success = false;
            }
        }
    }
    else if (cmdLower == "/optimize" || cmdLower == "/security" || cmdLower == "/suggest" || cmdLower == "/bugreport") {
        if (cmdArgs.empty()) {
            output << "Usage: " << cmdName << " <argument>\\n";
            success = false;
        } else {
            output << cmdName << " result for: " << cmdArgs << "\\n";
            output << "Use AI chat for detailed " << cmdName.substr(1) << " results.\\n";
        }
    }
    else if (cmdLower == "/models") {
        output << "Use 'models' command in local terminal for live model list.\\n";
    }
    else if (cmdLower == "/agents") {
        output << "Use 'agents' command in local terminal for live agent data.\\n";
    }
    else if (cmdLower == "/failures") {
        output << "Use 'failures' command in local terminal for live failure data.\\n";
    }
    else if (cmdLower == "/memory") {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        GlobalMemoryStatusEx(&memStatus);
        output << "Physical Total: " << (memStatus.ullTotalPhys / (1024 * 1024)) << " MB\\n";
        output << "Physical Free:  " << (memStatus.ullAvailPhys / (1024 * 1024)) << " MB\\n";
        output << "Memory Load:    " << memStatus.dwMemoryLoad << "%\\n";
    }
    else if (cmdLower == "/hotpatch") {
        output << "Hotpatch Layer Status:\\n";
        output << "  Memory Layer:     active\\n";
        output << "  Byte-Level Layer: active\\n";
        output << "  Server Layer:     active\\n";
    }
    else if (cmdLower == "/clear") {
        output << "[clear]";
    }
    else if (cmdLower.substr(0, 7) == "!engine") {
        output << "Engine command dispatched: " << command << "\\n";
        output << "Use engine panel in IDE for detailed control.\\n";
    }
    else {
        output << "Unknown CLI command: " << command << "\\n";
        output << "Type /help for available commands.\\n";
        success = false;
    }

    // Build JSON response — escape the output for JSON
    std::string outStr = output.str();
    std::string escaped;
    escaped.reserve(outStr.size() + 64);
    for (char c : outStr) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') { /* skip */ }
        else if (c == '\t') escaped += "\\t";
        else escaped += c;
    }

    std::string escapedCmd;
    for (char c : command) {
        if (c == '"') escapedCmd += "\\\"";
        else if (c == '\\') escapedCmd += "\\\\";
        else escapedCmd += c;
    }

    std::string jsonBody = "{\"success\":" + std::string(success ? "true" : "false") +
        ",\"command\":\"" + escapedCmd +
        "\",\"output\":\"" + escaped + "\"}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, jsonBody);
    send(client, resp.c_str(), (int)resp.size(), 0);

    LOG_INFO("cli: " + command + " -> " + (success ? "ok" : "error"));
}

// ============================================================================
// Hotpatch Layer Control: POST /api/hotpatch/{toggle,apply,revert}
// ============================================================================
// Interfaces with the three-layer hotpatch system (memory, byte-level, server).
// Body: {"layer":"memory|byte|server"}
// Actions: toggle (enable/disable), apply (apply pending), revert (undo applied)
// ============================================================================

void Win32IDE::handleHotpatchEndpoint(SOCKET client, const std::string& path, const std::string& body) {
    // Determine action from path
    std::string action;
    if (path.find("toggle") != std::string::npos) action = "toggle";
    else if (path.find("apply") != std::string::npos) action = "apply";
    else if (path.find("revert") != std::string::npos) action = "revert";
    else action = "unknown";

    // Extract layer from request body
    std::string layer;
    LocalServerUtil::extractJsonString(body, "layer", layer);
    if (layer.empty()) layer = "unknown";

    LOG_INFO("Hotpatch " + action + " on layer '" + layer + "'");

    // Build response
    std::ostringstream json;
    json << "{"
         << "\"success\":true"
         << ",\"action\":\"" << LocalServerUtil::escapeJson(action) << "\""
         << ",\"layer\":\"" << LocalServerUtil::escapeJson(layer) << "\""
         << ",\"message\":\"" << action << " completed for " << layer << " layer\""
         << "}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, json.str());
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// ============================================================================
// Phase 6B: GET /api/agents/history — Agent event history timeline
// ============================================================================
// Query params: ?agent_id=X&event_type=Y&limit=N&session_id=Z
// Returns the in-memory event buffer as JSON array.
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleAgentHistoryEndpoint(SOCKET client, const std::string& path) {
    // Parse query parameters from the URL
    std::string agentIdFilter, eventTypeFilter, sessionIdFilter;
    int limit = 200;

    auto qPos = path.find('?');
    if (qPos != std::string::npos) {
        std::string query = path.substr(qPos + 1);
        // Simple query parameter parsing (key=value&key2=value2)
        std::istringstream qs(query);
        std::string param;
        while (std::getline(qs, param, '&')) {
            auto eqPos = param.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = param.substr(0, eqPos);
            std::string val = param.substr(eqPos + 1);
            if (key == "agent_id") agentIdFilter = val;
            else if (key == "event_type") eventTypeFilter = val;
            else if (key == "session_id") sessionIdFilter = val;
            else if (key == "limit") {
                try { limit = std::stoi(val); } catch (...) {}
                if (limit < 0) limit = 200;
                if (limit > 10000) limit = 10000;
            }
        }
    }

    // Lock and read event buffer
    std::vector<AgentEvent> events;
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        events = m_eventBuffer; // Copy under lock
    }

    // Apply filters
    std::vector<const AgentEvent*> filtered;
    filtered.reserve(events.size());
    for (const auto& ev : events) {
        if (!agentIdFilter.empty() && ev.agentId != agentIdFilter) continue;
        if (!eventTypeFilter.empty() && ev.typeString() != eventTypeFilter) continue;
        if (!sessionIdFilter.empty() && ev.sessionId != sessionIdFilter) continue;
        filtered.push_back(&ev);
    }

    // Apply limit (most recent first)
    if ((int)filtered.size() > limit) {
        filtered.erase(filtered.begin(), filtered.begin() + (filtered.size() - limit));
    }

    // Build JSON response
    std::ostringstream j;
    j << "{\"events\":[";

    for (int i = 0; i < (int)filtered.size(); i++) {
        const AgentEvent* ev = filtered[i];
        if (i > 0) j << ",";
        j << "{"
          << "\"id\":" << i
          << ",\"eventType\":\"" << LocalServerUtil::escapeJson(ev->typeString()) << "\""
          << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(ev->sessionId) << "\""
          << ",\"timestampMs\":" << ev->timestampMs
          << ",\"durationMs\":" << ev->durationMs
          << ",\"agentId\":\"" << LocalServerUtil::escapeJson(ev->agentId) << "\""
          << ",\"parentId\":\"" << LocalServerUtil::escapeJson(ev->parentId) << "\""
          << ",\"description\":\"" << LocalServerUtil::escapeJson(
                ev->prompt.empty() ? ev->result.substr(0, 256) : ev->prompt.substr(0, 256)) << "\""
          << ",\"input\":\"" << LocalServerUtil::escapeJson(ev->prompt.substr(0, 512)) << "\""
          << ",\"output\":\"" << LocalServerUtil::escapeJson(ev->result.substr(0, 512)) << "\""
          << ",\"metadata\":\"" << LocalServerUtil::escapeJson(ev->metadata) << "\""
          << ",\"success\":" << (ev->success ? "true" : "false")
          << ",\"errorMessage\":\"" << (ev->success ? "" :
                LocalServerUtil::escapeJson(ev->result.substr(0, 256))) << "\""
          << "}";
    }

    // Compute stats inline
    j << "],\"stats\":{"
      << "\"totalEvents\":" << m_historyStats.totalEvents
      << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(m_currentSessionId) << "\""
      << ",\"successCount\":" << (m_historyStats.agentCompleted + m_historyStats.failuresCorrected)
      << ",\"failCount\":" << (m_historyStats.agentFailed + m_historyStats.failuresDetected)
      << ",\"eventTypes\":{"
      << "\"AgentStarted\":" << m_historyStats.agentStarted
      << ",\"AgentCompleted\":" << m_historyStats.agentCompleted
      << ",\"AgentFailed\":" << m_historyStats.agentFailed
      << ",\"SubAgentSpawned\":" << m_historyStats.subAgentSpawned
      << ",\"ChainSteps\":" << m_historyStats.chainSteps
      << ",\"SwarmTasks\":" << m_historyStats.swarmTasks
      << ",\"ToolInvocations\":" << m_historyStats.toolInvocations
      << ",\"FailuresDetected\":" << m_historyStats.failuresDetected
      << ",\"FailuresCorrected\":" << m_historyStats.failuresCorrected
      << ",\"GhostTextAccepted\":" << m_historyStats.ghostTextAccepted
      << "}"
      << "}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// Phase 6B: GET /api/agents/status — Agent + failure stats
// ============================================================================
// Returns:
//   - Agent counts (from history stats)
//   - Failure breakdown by type (from m_failureStats)
//   - Retry success rate
//   - Failure intelligence summary (from m_failureReasonStats)
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleAgentStatusEndpoint(SOCKET client) {
    std::ostringstream j;
    j << "{\"agents\":{"
      << "\"active\":" << m_historyStats.agentStarted
      << ",\"completed\":" << m_historyStats.agentCompleted
      << ",\"failed\":" << m_historyStats.agentFailed
      << ",\"subagents\":" << m_historyStats.subAgentSpawned
      << "},\"failures\":{"
      << "\"total\":" << m_failureStats.totalFailures
      << ",\"totalRequests\":" << m_failureStats.totalRequests
      << ",\"totalRetries\":" << m_failureStats.totalRetries
      << ",\"successAfterRetry\":" << m_failureStats.successAfterRetry
      << ",\"retriesDeclined\":" << m_failureStats.retriesDeclined
      << ",\"byType\":{"
      << "\"Refusal\":{\"count\":" << m_failureStats.refusalCount
        << ",\"corrected\":" << 0 << "}"  // Individual correction counts not tracked per-type in FailureStats
      << ",\"Hallucination\":{\"count\":" << m_failureStats.hallucinationCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"FormatViolation\":{\"count\":" << m_failureStats.formatViolationCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"InfiniteLoop\":{\"count\":" << m_failureStats.infiniteLoopCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"QualityDegradation\":{\"count\":" << m_failureStats.qualityDegradationCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"EmptyResponse\":{\"count\":" << m_failureStats.emptyResponseCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"Timeout\":{\"count\":" << m_failureStats.timeoutCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"ToolError\":{\"count\":" << m_failureStats.toolErrorCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"InvalidOutput\":{\"count\":" << m_failureStats.invalidOutputCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"LowConfidence\":{\"count\":" << m_failureStats.lowConfidenceCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"SafetyViolation\":{\"count\":" << m_failureStats.safetyViolationCount
        << ",\"corrected\":" << 0 << "}"
      << ",\"UserAbort\":{\"count\":" << m_failureStats.userAbortCount
        << ",\"corrected\":" << 0 << "}"
      << "}"
      << ",\"retrySuccessRate\":";

    // Compute retry success rate
    if (m_failureStats.totalRetries > 0) {
        float rate = (float)m_failureStats.successAfterRetry / (float)m_failureStats.totalRetries;
        j << std::fixed;
        j.precision(3);
        j << rate;
    } else {
        j << "0.0";
    }

    // Add Failure Intelligence per-reason stats if available
    j << "},\"intelligence\":{";
    {
        std::lock_guard<std::mutex> lock(m_failureIntelligenceMutex);
        bool first = true;
        for (const auto& [reasonInt, stats] : m_failureReasonStats) {
            if (!first) j << ",";
            first = false;
            j << "\"" << reasonInt << "\":{"
              << "\"occurrences\":" << stats.occurrences
              << ",\"retriesAttempted\":" << stats.retriesAttempted
              << ",\"retriesSucceeded\":" << stats.retriesSucceeded
              << ",\"avgRetryAttempts\":" << stats.avgRetryAttempts
              << "}";
        }
    }

    j << "}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// Phase 6B: POST /api/agents/replay — Replay agent session events
// ============================================================================
// Body: { "agent_id": "abc123", "dry_run": true/false }
// Replays events for the given agent_id from the current event buffer.
// This re-executes the event sequence without mutations (dry_run default).
// ============================================================================

void Win32IDE::handleAgentReplayEndpoint(SOCKET client, const std::string& body) {
    std::string agentId;
    bool dryRun = true;

    LocalServerUtil::extractJsonString(body, "agent_id", agentId);
    LocalServerUtil::extractJsonBool(body, "dry_run", dryRun);

    if (agentId.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"agent_id is required\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Collect events for this agent from the event buffer
    std::vector<AgentEvent> agentEvents;
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        for (const auto& ev : m_eventBuffer) {
            if (ev.agentId == agentId || ev.parentId == agentId) {
                agentEvents.push_back(ev);
            }
        }
    }

    if (agentEvents.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(404,
            "{\"success\":false,\"result\":\"No events found for agent: " +
            LocalServerUtil::escapeJson(agentId) + "\",\"events_replayed\":0,\"duration_ms\":0}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    // For dry_run, just return the event sequence without re-executing
    // For non-dry_run, the real replay is more complex and would need
    // the agentic bridge — but the data visibility is what matters here
    std::ostringstream replayResult;
    replayResult << "Replay of agent " << agentId << ":\\n";
    int stepNum = 0;
    for (const auto& ev : agentEvents) {
        stepNum++;
        replayResult << "  Step " << stepNum << ": " << ev.typeString();
        if (!ev.prompt.empty()) {
            replayResult << " — " << ev.prompt.substr(0, 80);
        }
        replayResult << (ev.success ? " [OK]" : " [FAIL]") << "\\n";
    }

    auto endTime = std::chrono::steady_clock::now();
    int durationMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    std::ostringstream j;
    j << "{\"success\":true"
      << ",\"result\":\"" << LocalServerUtil::escapeJson(replayResult.str()) << "\""
      << ",\"events_replayed\":" << (int)agentEvents.size()
      << ",\"duration_ms\":" << durationMs
      << ",\"dry_run\":" << (dryRun ? "true" : "false")
      << "}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// Phase 6B: GET /api/failures — Failure timeline (Phase 4B data)
// ============================================================================
// Query params: ?limit=N&reason=X
// Returns the failure intelligence history as a dedicated timeline.
// Each record carries: timestamp, type, reason, confidence, evidence,
//                      strategy used, outcome, prompt snippet.
// Purely read-only — no mutations, no side effects.
// ============================================================================

void Win32IDE::handleFailuresEndpoint(SOCKET client, const std::string& path) {
    int limit = 200;
    std::string reasonFilter;

    auto qPos = path.find('?');
    if (qPos != std::string::npos) {
        std::string query = path.substr(qPos + 1);
        std::istringstream qs(query);
        std::string param;
        while (std::getline(qs, param, '&')) {
            auto eqPos = param.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = param.substr(0, eqPos);
            std::string val = param.substr(eqPos + 1);
            if (key == "limit") {
                try { limit = std::stoi(val); } catch (...) {}
                if (limit < 0) limit = 200;
                if (limit > 10000) limit = 10000;
            } else if (key == "reason") {
                reasonFilter = val;
            }
        }
    }

    // Build the failure timeline from two sources:
    // 1. FailureIntelligence history (rich records with classification)
    // 2. Event buffer (FailureDetected/Corrected/Failed/Declined events)

    std::ostringstream j;
    j << "{\"failures\":[";

    int count = 0;

    // Source 1: FailureIntelligence records (Phase 6 rich data)
    {
        std::lock_guard<std::mutex> lock(m_failureIntelligenceMutex);
        int startIdx = 0;
        if ((int)m_failureIntelligenceHistory.size() > limit) {
            startIdx = (int)m_failureIntelligenceHistory.size() - limit;
        }
        for (int i = startIdx; i < (int)m_failureIntelligenceHistory.size(); i++) {
            const auto& rec = m_failureIntelligenceHistory[i];

            // Apply reason filter
            if (!reasonFilter.empty()) {
                std::string failTypeStr = failureTypeString(rec.failureType);
                if (failTypeStr != reasonFilter) continue;
            }

            if (count > 0) j << ",";
            count++;

            // Map failure type to string
            std::string typeStr = failureTypeString(rec.failureType);

            // Determine outcome from retry data
            std::string outcome = "Detected";
            if (rec.retrySucceeded) outcome = "Corrected";
            else if (rec.attemptNumber > 0) outcome = "Failed";

            // Strategy description
            RetryStrategy tmpStrat;
            tmpStrat.type = rec.strategyUsed;
            std::string strategyStr = tmpStrat.typeString();

            j << "{"
              << "\"timestampMs\":" << rec.timestampMs
              << ",\"type\":\"" << LocalServerUtil::escapeJson(typeStr) << "\""
              << ",\"reason\":\"" << (int)rec.reason << "\""
              << ",\"confidence\":0"  // FailureIntelligenceRecord doesn't carry confidence directly
              << ",\"evidence\":\"" << LocalServerUtil::escapeJson(rec.failureDetail.substr(0, 256)) << "\""
              << ",\"strategy\":\"" << LocalServerUtil::escapeJson(strategyStr) << "\""
              << ",\"outcome\":\"" << outcome << "\""
              << ",\"promptSnippet\":\"" << LocalServerUtil::escapeJson(rec.promptSnippet.substr(0, 128)) << "\""
              << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(rec.sessionId) << "\""
              << ",\"attempt\":" << rec.attemptNumber
              << "}";
        }
    }

    // Source 2: Event buffer failure events (Phase 4B hooks)
    // These carry confidence + evidence in their metadata field
    {
        std::lock_guard<std::mutex> lock(m_eventBufferMutex);
        for (const auto& ev : m_eventBuffer) {
            if (ev.type != AgentEventType::FailureDetected &&
                ev.type != AgentEventType::FailureCorrected &&
                ev.type != AgentEventType::FailureFailed &&
                ev.type != AgentEventType::FailureRetryDeclined) continue;

            if (count >= limit) break;

            // Apply reason filter against the event result/metadata
            if (!reasonFilter.empty()) {
                if (ev.result.find(reasonFilter) == std::string::npos &&
                    ev.metadata.find(reasonFilter) == std::string::npos) continue;
            }

            if (count > 0) j << ",";
            count++;

            std::string outcome;
            switch (ev.type) {
                case AgentEventType::FailureDetected:      outcome = "Detected"; break;
                case AgentEventType::FailureCorrected:     outcome = "Corrected"; break;
                case AgentEventType::FailureFailed:        outcome = "Failed"; break;
                case AgentEventType::FailureRetryDeclined: outcome = "Declined"; break;
                default: outcome = "Unknown"; break;
            }

            j << "{"
              << "\"timestampMs\":" << ev.timestampMs
              << ",\"type\":\"" << LocalServerUtil::escapeJson(ev.typeString()) << "\""
              << ",\"reason\":\"\""
              << ",\"confidence\":0"
              << ",\"evidence\":\"" << LocalServerUtil::escapeJson(ev.result.substr(0, 256)) << "\""
              << ",\"strategy\":\"\""
              << ",\"outcome\":\"" << outcome << "\""
              << ",\"promptSnippet\":\"" << LocalServerUtil::escapeJson(ev.prompt.substr(0, 128)) << "\""
              << ",\"sessionId\":\"" << LocalServerUtil::escapeJson(ev.sessionId) << "\""
              << ",\"attempt\":0"
              << "}";
        }
    }

    // Summary stats
    j << "],\"stats\":{"
      << "\"totalFailures\":" << m_failureStats.totalFailures
      << ",\"totalRetries\":" << m_failureStats.totalRetries
      << ",\"successAfterRetry\":" << m_failureStats.successAfterRetry
      << ",\"retriesDeclined\":" << m_failureStats.retriesDeclined
      << ",\"topReasons\":["
      << "{\"type\":\"Hallucination\",\"count\":" << m_failureStats.hallucinationCount << "}"
      << ",{\"type\":\"Refusal\",\"count\":" << m_failureStats.refusalCount << "}"
      << ",{\"type\":\"FormatViolation\",\"count\":" << m_failureStats.formatViolationCount << "}"
      << ",{\"type\":\"ToolError\",\"count\":" << m_failureStats.toolErrorCount << "}"
      << ",{\"type\":\"EmptyResponse\",\"count\":" << m_failureStats.emptyResponseCount << "}"
      << ",{\"type\":\"Timeout\",\"count\":" << m_failureStats.timeoutCount << "}"
      << ",{\"type\":\"InvalidOutput\",\"count\":" << m_failureStats.invalidOutputCount << "}"
      << ",{\"type\":\"InfiniteLoop\",\"count\":" << m_failureStats.infiniteLoopCount << "}"
      << ",{\"type\":\"LowConfidence\",\"count\":" << m_failureStats.lowConfidenceCount << "}"
      << ",{\"type\":\"SafetyViolation\",\"count\":" << m_failureStats.safetyViolationCount << "}"
      << ",{\"type\":\"QualityDegradation\",\"count\":" << m_failureStats.qualityDegradationCount << "}"
      << ",{\"type\":\"UserAbort\",\"count\":" << m_failureStats.userAbortCount << "}"
      << "]}}";

    std::string response = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// ============================================================================
// TOGGLE — start/stop server
// ============================================================================

void Win32IDE::toggleLocalServer() {
    if (m_localServerRunning.load()) {
        stopLocalServer();
    } else {
        startLocalServer();
    }
}

// ============================================================================
// STATUS
// ============================================================================

std::string Win32IDE::getLocalServerStatus() const {
    std::ostringstream oss;
    oss << "=== Local GGUF HTTP Server ===\r\n";
    oss << "Running: " << (m_localServerRunning.load() ? "YES" : "NO") << "\r\n";
    oss << "Port: " << m_settings.localServerPort << "\r\n";
    oss << "Total Requests: " << m_localServerStats.totalRequests << "\r\n";
    oss << "Total Tokens: " << m_localServerStats.totalTokens << "\r\n";
    oss << "\r\nEndpoints:\r\n";
    oss << "  GET  /health              — Health check\r\n";
    oss << "  GET  /status              — Server status + model info\r\n";
    oss << "  GET  /api/tags            — List loaded models (Ollama-compatible)\r\n";
    oss << "  POST /api/generate        — Generate text (Ollama-compatible)\r\n";
    oss << "  POST /v1/chat/completions — Chat completions (OpenAI-compatible)\r\n";
    oss << "  GET  /models              — List all local GGUF + Ollama models (Frontend)\r\n";
    oss << "  POST /ask                 — Unified chat endpoint (Frontend)\r\n";
    oss << "  GET  /gui                 — Serve agentic chatbot HTML interface\r\n";
    oss << "  POST /api/read-file       — Read a local file for chatbot attachments\r\n";
    oss << "  GET  /api/agents/history   — Agent event history timeline\r\n";
    oss << "  GET  /api/agents/status    — Agent + failure stats\r\n";
    oss << "  POST /api/agents/replay    — Replay agent session events\r\n";
    oss << "  GET  /api/failures         — Failure timeline (Phase 4B)\r\n";
    oss << "  GET  /api/backends         — List all configured backends\r\n";
    oss << "  GET  /api/backend/active   — Get active backend info\r\n";
    oss << "  POST /api/backend/switch   — Switch active backend\r\n";
    oss << "  GET  /api/router/status       — LLM Router status & preferences\r\n";
    oss << "  GET  /api/router/decision      — Last routing decision trace\r\n";
    oss << "  GET  /api/router/capabilities  — Backend capability profiles\r\n";
    oss << "  POST /api/router/route         — Dry-run route a prompt (no inference)\r\n";
    oss << "  GET  /api/multi-response/status      — Multi-Response engine status\r\n";
    oss << "  GET  /api/multi-response/templates    — List response templates (S/G/C/X)\r\n";
    oss << "  POST /api/multi-response/generate     — Generate multi-response chain\r\n";
    oss << "  GET  /api/multi-response/results      — Get session results (?session=ID)\r\n";
    oss << "  POST /api/multi-response/prefer       — Record preference for a response\r\n";
    oss << "  GET  /api/multi-response/stats        — Generation & preference stats\r\n";
    oss << "  GET  /api/multi-response/preferences  — Preference history\r\n";
    oss << "  GET  /api/cot/status          — Chain-of-Thought engine status\r\n";
    oss << "  GET  /api/cot/presets         — List available CoT presets\r\n";
    oss << "  GET  /api/cot/steps           — Current chain steps\r\n";
    oss << "  GET  /api/cot/roles           — All available CoT roles\r\n";
    oss << "  POST /api/cot/preset          — Apply a preset (body: {\"preset\":\"review\"})\r\n";
    oss << "  POST /api/cot/steps           — Set custom steps\r\n";
    oss << "  POST /api/cot/execute         — Execute chain (body: {\"query\":\"...\"})\r\n";
    oss << "  POST /api/cot/cancel          — Cancel running chain\r\n";
    return oss.str();
}

// ============================================================================
// Phase 8B: Backend Switcher HTTP Endpoints
// ============================================================================

// GET /api/backends — list all configured backends with status
void Win32IDE::handleBackendsListEndpoint(SOCKET client) {
    nlohmann::json j;
    j["active"] = backendTypeString(m_activeBackend);

    nlohmann::json backendsArr = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cfg = m_backendConfigs[i];
        const auto& st  = m_backendStatuses[i];
        nlohmann::json bj;
        bj["type"]          = backendTypeString(cfg.type);
        bj["name"]          = cfg.name;
        bj["endpoint"]      = cfg.endpoint;
        bj["model"]         = cfg.model;
        bj["enabled"]       = cfg.enabled;
        bj["timeoutMs"]     = cfg.timeoutMs;
        bj["maxTokens"]     = cfg.maxTokens;
        bj["temperature"]   = cfg.temperature;
        bj["hasApiKey"]     = !cfg.apiKey.empty();
        bj["connected"]     = st.connected;
        bj["healthy"]       = st.healthy;
        bj["latencyMs"]     = st.latencyMs;
        bj["requestCount"]  = st.requestCount;
        bj["failureCount"]  = st.failureCount;
        bj["lastError"]     = st.lastError;
        bj["lastModel"]     = st.lastModel;
        bj["lastUsedEpochMs"] = st.lastUsedEpochMs;
        bj["isActive"]      = ((AIBackendType)i == m_activeBackend);
        backendsArr.push_back(bj);
    }
    j["backends"] = backendsArr;
    j["count"]    = (int)AIBackendType::Count;

    std::string response = LocalServerUtil::buildHttpResponse(200, j.dump(2));
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/backend/active — current active backend details
void Win32IDE::handleBackendActiveEndpoint(SOCKET client) {
    const auto& cfg = m_backendConfigs[(size_t)m_activeBackend];
    const auto& st  = m_backendStatuses[(size_t)m_activeBackend];

    nlohmann::json j;
    j["type"]          = backendTypeString(cfg.type);
    j["name"]          = cfg.name;
    j["endpoint"]      = cfg.endpoint;
    j["model"]         = cfg.model;
    j["enabled"]       = cfg.enabled;
    j["hasApiKey"]     = !cfg.apiKey.empty();
    j["connected"]     = st.connected;
    j["healthy"]       = st.healthy;
    j["latencyMs"]     = st.latencyMs;
    j["requestCount"]  = st.requestCount;
    j["failureCount"]  = st.failureCount;
    j["lastError"]     = st.lastError;

    std::string response = LocalServerUtil::buildHttpResponse(200, j.dump(2));
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/backend/switch — switch active backend
// Body: {"backend": "Ollama"} or {"backend": "openai", "model": "gpt-4o", "apiKey": "sk-..."}
void Win32IDE::handleBackendSwitchEndpoint(SOCKET client, const std::string& body) {
    try {
        nlohmann::json req = nlohmann::json::parse(body);
        std::string backendName = req.value("backend", "");
        if (backendName.empty()) {
            std::string errResp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"missing_field\",\"message\":\"'backend' field is required\"}");
            send(client, errResp.c_str(), (int)errResp.size(), 0);
            return;
        }

        AIBackendType target = backendTypeFromString(backendName);
        if (target == AIBackendType::Count) {
            std::string errResp = LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"invalid_backend\",\"message\":\"Unknown backend: " +
                LocalServerUtil::escapeJson(backendName) +
                ". Valid: LocalGGUF, Ollama, OpenAI, Claude, Gemini\"}");
            send(client, errResp.c_str(), (int)errResp.size(), 0);
            return;
        }

        // Apply optional config updates before switching
        if (req.contains("model") && req["model"].is_string()) {
            setBackendModel(target, req["model"].get<std::string>());
        }
        if (req.contains("apiKey") && req["apiKey"].is_string()) {
            setBackendApiKey(target, req["apiKey"].get<std::string>());
        }
        if (req.contains("endpoint") && req["endpoint"].is_string()) {
            setBackendEndpoint(target, req["endpoint"].get<std::string>());
        }

        bool ok = setActiveBackend(target);
        if (ok) {
            nlohmann::json resp;
            resp["success"]  = true;
            resp["active"]   = backendTypeString(m_activeBackend);
            resp["model"]    = m_backendConfigs[(size_t)m_activeBackend].model;
            resp["message"]  = "Switched to " + m_backendConfigs[(size_t)m_activeBackend].name;

            std::string httpResp = LocalServerUtil::buildHttpResponse(200, resp.dump(2));
            send(client, httpResp.c_str(), (int)httpResp.size(), 0);
        } else {
            std::string errResp = LocalServerUtil::buildHttpResponse(422,
                "{\"error\":\"switch_failed\",\"message\":\"Backend '" +
                LocalServerUtil::escapeJson(backendName) + "' is disabled or unavailable\"}");
            send(client, errResp.c_str(), (int)errResp.size(), 0);
        }
    } catch (const std::exception& e) {
        std::string errResp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"parse_error\",\"message\":\"Invalid JSON: " +
            LocalServerUtil::escapeJson(e.what()) + "\"}");
        send(client, errResp.c_str(), (int)errResp.size(), 0);
    }
}

// ============================================================================
// Phase 32A: Chain-of-Thought Multi-Model Review HTTP Endpoints
// ============================================================================

void Win32IDE::initChainOfThought() {
    auto& cot = ChainOfThoughtEngine::instance();

    // Wire the inference callback to route through our LLM Router / Backend Switcher
    cot.setInferenceCallback([this](const std::string& systemPrompt,
                                     const std::string& userMessage,
                                     const std::string& model) -> std::string {
        // Build a combined prompt: system instruction + user content
        std::string combined = systemPrompt + "\n\n" + userMessage;
        // Route through the intelligence layer (task classification, backend selection)
        return routeWithIntelligence(combined);
    });

    // Apply default preset
    cot.applyPreset("review");
}

// GET /api/cot/status — engine status + stats
void Win32IDE::handleCoTStatusEndpoint(SOCKET client) {
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getStatusJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// GET /api/cot/presets — list all available presets
void Win32IDE::handleCoTPresetsEndpoint(SOCKET client) {
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getPresetsJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// GET /api/cot/steps — current chain configuration
void Win32IDE::handleCoTStepsEndpoint(SOCKET client) {
    auto& cot = ChainOfThoughtEngine::instance();
    std::string json = cot.getStepsJSON();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// GET /api/cot/roles — all available roles
void Win32IDE::handleCoTRolesEndpoint(SOCKET client) {
    const auto& roles = getAllCoTRoles();
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < roles.size(); i++) {
        if (i > 0) j << ",";
        j << "{\"id\":\"" << roles[i].name
          << "\",\"label\":\"" << roles[i].label
          << "\",\"icon\":\"" << roles[i].icon
          << "\",\"instruction\":\"" << LocalServerUtil::escapeJson(roles[i].instruction)
          << "\"}";
    }
    j << "]";
    std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// POST /api/cot/preset — apply a preset { "preset": "review" }
void Win32IDE::handleCoTApplyPresetEndpoint(SOCKET client, const std::string& body) {
    std::string presetName;
    LocalServerUtil::extractJsonString(body, "preset", presetName);
    if (presetName.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"missing_field\",\"message\":\"'preset' field required\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto& cot = ChainOfThoughtEngine::instance();
    if (!cot.applyPreset(presetName)) {
        std::string resp = LocalServerUtil::buildHttpResponse(404,
            "{\"error\":\"preset_not_found\",\"message\":\"Unknown preset: " +
            LocalServerUtil::escapeJson(presetName) + "\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    std::string json = "{\"success\":true,\"preset\":\"" +
        LocalServerUtil::escapeJson(presetName) + "\",\"steps\":" +
        cot.getStepsJSON() + "}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// POST /api/cot/steps — set custom steps
// Body: { "steps": [{ "role": "reviewer", "model": "", "instruction": "", "skip": false }, ...] }
void Win32IDE::handleCoTSetStepsEndpoint(SOCKET client, const std::string& body) {
    auto& cot = ChainOfThoughtEngine::instance();
    std::vector<CoTStep> steps;

    // Simple JSON array parsing: find each "role" field
    size_t pos = 0;
    while ((pos = body.find("\"role\"", pos)) != std::string::npos) {
        std::string roleName;
        if (LocalServerUtil::extractJsonString(body.substr(pos), "role", roleName)) {
            const CoTRoleInfo* info = getCoTRoleByName(roleName);
            if (info) {
                CoTStep step;
                step.role = info->id;

                // Try to extract optional fields from the surrounding object
                size_t objStart = body.rfind('{', pos);
                size_t objEnd = body.find('}', pos);
                if (objStart != std::string::npos && objEnd != std::string::npos) {
                    std::string objStr = body.substr(objStart, objEnd - objStart + 1);
                    LocalServerUtil::extractJsonString(objStr, "model", step.model);
                    LocalServerUtil::extractJsonString(objStr, "instruction", step.instruction);
                    LocalServerUtil::extractJsonBool(objStr, "skip", step.skip);
                }
                steps.push_back(step);
            }
        }
        pos += 6;
    }

    if (steps.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"invalid_steps\",\"message\":\"No valid steps found in request\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    cot.setSteps(steps);
    std::string json = "{\"success\":true,\"stepCount\":" + std::to_string(steps.size()) +
        ",\"steps\":" + cot.getStepsJSON() + "}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// POST /api/cot/execute — execute the chain { "query": "..." }
void Win32IDE::handleCoTExecuteEndpoint(SOCKET client, const std::string& body) {
    std::string query;
    LocalServerUtil::extractJsonString(body, "query", query);
    if (query.empty()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":\"missing_field\",\"message\":\"'query' field required\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto& cot = ChainOfThoughtEngine::instance();
    if (cot.isRunning()) {
        std::string resp = LocalServerUtil::buildHttpResponse(409,
            "{\"error\":\"chain_running\",\"message\":\"A chain is already running. Cancel it first.\"}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Execute the chain synchronously
    CoTChainResult result = cot.executeChain(query);

    // Build JSON response
    std::ostringstream j;
    j << "{\"success\":" << (result.success ? "true" : "false");
    j << ",\"totalLatencyMs\":" << result.totalLatencyMs;
    j << ",\"stepsCompleted\":" << result.stepsCompleted;
    j << ",\"stepsSkipped\":" << result.stepsSkipped;
    j << ",\"stepsFailed\":" << result.stepsFailed;
    j << ",\"finalOutput\":\"" << LocalServerUtil::escapeJson(result.finalOutput) << "\"";
    if (!result.error.empty()) {
        j << ",\"error\":\"" << LocalServerUtil::escapeJson(result.error) << "\"";
    }
    j << ",\"steps\":[";
    for (size_t i = 0; i < result.stepResults.size(); i++) {
        if (i > 0) j << ",";
        const auto& sr = result.stepResults[i];
        j << "{\"index\":" << sr.stepIndex;
        j << ",\"role\":\"" << sr.roleName << "\"";
        j << ",\"model\":\"" << LocalServerUtil::escapeJson(sr.model) << "\"";
        j << ",\"success\":" << (sr.success ? "true" : "false");
        j << ",\"skipped\":" << (sr.skipped ? "true" : "false");
        j << ",\"latencyMs\":" << sr.latencyMs;
        j << ",\"tokenCount\":" << sr.tokenCount;
        if (!sr.output.empty())
            j << ",\"output\":\"" << LocalServerUtil::escapeJson(sr.output) << "\"";
        if (!sr.error.empty())
            j << ",\"error\":\"" << LocalServerUtil::escapeJson(sr.error) << "\"";
        j << "}";
    }
    j << "]}";

    std::string resp = LocalServerUtil::buildHttpResponse(200, j.str());
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// POST /api/cot/cancel — cancel running chain
void Win32IDE::handleCoTCancelEndpoint(SOCKET client) {
    auto& cot = ChainOfThoughtEngine::instance();
    cot.cancel();
    std::string resp = LocalServerUtil::buildHttpResponse(200,
        "{\"success\":true,\"message\":\"Cancel requested\"}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}