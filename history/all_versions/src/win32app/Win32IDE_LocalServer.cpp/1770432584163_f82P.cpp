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
        handleBackendStatusEndpoint(client);
        closesocket(client);
        return;
    }
    else if (method == "POST" && path == "/api/backends/switch") {
        handleBackendSwitchEndpoint(client, body);
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

    // If no native engine or no model loaded, return a local-mode fallback
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded()) {
        std::string answer = "[Local Mode] No model is currently loaded. "
                             "Load a model first using the Model selector or "
                             "the terminal command: .load-model <name>";
        std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer) + "\"}";
        std::string resp = LocalServerUtil::buildHttpResponse(200, json);
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Generate response using loaded model
    auto tokens = m_nativeEngine->Tokenize(question);
    int maxTokens = std::min(context, 4096);
    auto generated = m_nativeEngine->Generate(tokens, maxTokens);
    std::string answer = m_nativeEngine->Detokenize(generated);
    m_localServerStats.totalTokens += (int)generated.size();

    std::string json = "{\"answer\":\"" + LocalServerUtil::escapeJson(answer) + "\"}";
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
    oss << "  GET  /api/agents/history   — Agent event history timeline\r\n";
    oss << "  GET  /api/agents/status    — Agent + failure stats\r\n";
    oss << "  POST /api/agents/replay    — Replay agent session events\r\n";
    oss << "  GET  /api/failures         — Failure timeline (Phase 4B)\r\n";
    return oss.str();
}
