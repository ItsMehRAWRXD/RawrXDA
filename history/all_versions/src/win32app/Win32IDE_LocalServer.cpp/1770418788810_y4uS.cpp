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

    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string resp = LocalServerUtil::buildHttpResponse(400,
            "{\"error\":{\"message\":\"No model loaded\"}}");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    auto tokens = m_nativeEngine->Tokenize(prompt);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);

    std::string requestId = "chatcmpl-rawrxd-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());

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
    return oss.str();
}
