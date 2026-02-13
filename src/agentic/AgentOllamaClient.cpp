// =============================================================================
// AgentOllamaClient.cpp — Streaming Ollama Client Implementation
// =============================================================================
#include "AgentOllamaClient.h"
#include <chrono>
#include <sstream>

#ifdef _WIN32
#pragma comment(lib, "winhttp.lib")
#endif

using RawrXD::Agent::AgentOllamaClient;
using RawrXD::Agent::InferenceResult;

namespace {
    const char* kChatEndpoint = "/api/chat";
    const char* kGenerateEndpoint = "/api/generate";
    const char* kTagsEndpoint = "/api/tags";
    const char* kVersionEndpoint = "/api/version";

    std::wstring ToWide(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
        if (!out.empty() && out.back() == L'\0') out.pop_back();
        return out;
    }
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AgentOllamaClient::AgentOllamaClient(const OllamaConfig& config)
    : m_config(config)
{
#ifdef _WIN32
    InitWinHTTP();
#endif
}

AgentOllamaClient::~AgentOllamaClient() {
    CancelStream();
#ifdef _WIN32
    CleanupWinHTTP();
#endif
}

#ifdef _WIN32
void AgentOllamaClient::InitWinHTTP() {
    m_hSession = WinHttpOpen(L"RawrXD-Agent/1.0",
                             WINHTTP_ACCESS_TYPE_NO_PROXY,
                             WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0);
    if (m_hSession) {
        DWORD timeout = static_cast<DWORD>(m_config.timeout_ms);
        WinHttpSetTimeouts(m_hSession, timeout, timeout, timeout, timeout);
    }
}

void AgentOllamaClient::CleanupWinHTTP() {
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
    }
}
#endif

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

bool AgentOllamaClient::TestConnection() {
    std::string ver = GetVersion();
    return !ver.empty();
}

std::string AgentOllamaClient::GetVersion() {
    std::string resp = MakeGetRequest(kVersionEndpoint);
    if (resp.empty()) return "";
    try {
        json j = json::parse(resp);
        return j.value("version", "");
    } catch (...) {
        return "";
    }
}

std::vector<std::string> AgentOllamaClient::ListModels() {
    std::vector<std::string> models;
    std::string resp = MakeGetRequest(kTagsEndpoint);
    if (resp.empty()) return models;

    try {
        json j = json::parse(resp);
        if (j.contains("models") && j["models"].is_array()) {
            auto& arr = j["models"];
            for (size_t i = 0; i < arr.size(); ++i) {
                models.push_back(arr[i].value("name", ""));
            }
        }
    } catch (...) {}

    return models;
}

// ---------------------------------------------------------------------------
// Chat API
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::ChatSync(
    const std::vector<ChatMessage>& messages,
    const json& tools)
{
    json payload = BuildChatPayload(messages, tools, false);
    std::string resp = MakePostRequest(kChatEndpoint, payload.dump());
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    if (resp.empty()) return InferenceResult::error("Empty response from Ollama");
    return ParseChatResponse(resp);
}

bool AgentOllamaClient::ChatStream(
    const std::vector<ChatMessage>& messages,
    const json& tools,
    TokenCallback on_token,
    ToolCallCallback on_tool_call,
    DoneCallback on_done,
    ErrorCallback on_error)
{
    json payload = BuildChatPayload(messages, tools, true);
    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    std::string fullResponse;
    uint64_t promptTokens = 0, completionTokens = 0;
    double tps = 0.0;

    bool result = MakeStreamingPost(kChatEndpoint, payload.dump(),
        [&](const std::string& line) -> bool {
            if (m_cancelRequested.load()) return false;
            if (line.empty()) return true;

            try {
                json j = json::parse(line);
                bool done = j.value("done", false);

                if (j.contains("message")) {
                    auto& msg = j["message"];
                    std::string content = msg.value("content", "");
                    if (!content.empty()) {
                        fullResponse += content;
                        if (on_token) on_token(content);
                    }

                    // Check for tool calls
                    if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
                        auto& tcs = msg["tool_calls"];
                        for (size_t ti = 0; ti < tcs.size(); ++ti) {
                            auto& tc = tcs[ti];
                            if (tc.contains("function")) {
                                std::string fname = tc["function"].value("name", "");
                                json fargs = tc["function"].value("arguments", json::object());
                                if (on_tool_call) on_tool_call(fname, fargs);
                            }
                        }
                    }
                }

                if (done) {
                    promptTokens = j.value("prompt_eval_count", 0ULL);
                    completionTokens = j.value("eval_count", 0ULL);
                    uint64_t evalDuration = j.value("eval_duration", 0ULL);
                    if (evalDuration > 0) {
                        tps = static_cast<double>(completionTokens) /
                              (static_cast<double>(evalDuration) / 1e9);
                    }
                    return false; // Stop reading
                }
            } catch (...) {
                // Non-JSON line, skip
            }
            return true;
        },
        on_error);

    m_streaming.store(false);

    if (result && on_done) {
        on_done(fullResponse, promptTokens, completionTokens, tps);
    }

    m_totalTokens.fetch_add(completionTokens, std::memory_order_relaxed);
    return result;
}

// ---------------------------------------------------------------------------
// FIM API (Ghost Text)
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::FIMSync(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename)
{
    json payload = BuildFIMPayload(prefix, suffix, filename, false);
    std::string resp = MakePostRequest(kGenerateEndpoint, payload.dump());
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    if (resp.empty()) return InferenceResult::error("Empty FIM response");
    return ParseFIMResponse(resp);
}

bool AgentOllamaClient::FIMStream(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename,
    TokenCallback on_token,
    DoneCallback on_done,
    ErrorCallback on_error)
{
    json payload = BuildFIMPayload(prefix, suffix, filename, true);
    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    std::string fullResponse;
    uint64_t completionTokens = 0;
    double tps = 0.0;

    bool result = MakeStreamingPost(kGenerateEndpoint, payload.dump(),
        [&](const std::string& line) -> bool {
            if (m_cancelRequested.load()) return false;
            if (line.empty()) return true;

            try {
                json j = json::parse(line);
                bool done = j.value("done", false);

                std::string token = j.value("response", "");
                if (!token.empty()) {
                    fullResponse += token;
                    if (on_token) on_token(token);
                }

                if (done) {
                    completionTokens = j.value("eval_count", 0ULL);
                    uint64_t evalDuration = j.value("eval_duration", 0ULL);
                    if (evalDuration > 0) {
                        tps = static_cast<double>(completionTokens) /
                              (static_cast<double>(evalDuration) / 1e9);
                    }
                    return false;
                }
            } catch (...) {}
            return true;
        },
        on_error);

    m_streaming.store(false);

    if (result && on_done) {
        on_done(fullResponse, 0, completionTokens, tps);
    }

    m_totalTokens.fetch_add(completionTokens, std::memory_order_relaxed);
    return result;
}

// ---------------------------------------------------------------------------
// Cancel
// ---------------------------------------------------------------------------

void AgentOllamaClient::CancelStream() {
    m_cancelRequested.store(true);
}

void AgentOllamaClient::SetConfig(const OllamaConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

double AgentOllamaClient::GetAvgTokensPerSec() const {
    uint64_t tokens = m_totalTokens.load();
    if (tokens == 0 || m_totalDurationMs == 0.0) return 0.0;
    return static_cast<double>(tokens) / (m_totalDurationMs / 1000.0);
}

// ---------------------------------------------------------------------------
// JSON Payload Builders
// ---------------------------------------------------------------------------

json AgentOllamaClient::BuildChatPayload(
    const std::vector<ChatMessage>& messages,
    const json& tools,
    bool stream) const
{
    json payload;
    payload["model"] = m_config.chat_model;
    payload["stream"] = stream;

    // Messages
    json msgs = json::array();
    for (const auto& m : messages) {
        json msg;
        msg["role"] = m.role;
        msg["content"] = m.content;
        if (!m.tool_call_id.empty()) {
            msg["tool_call_id"] = m.tool_call_id;
        }
        if (!m.tool_calls.is_null() && !m.tool_calls.empty()) {
            msg["tool_calls"] = m.tool_calls;
        }
        msgs.push_back(msg);
    }
    payload["messages"] = msgs;

    // Tools (OpenAI function-calling format)
    if (!tools.empty() && tools.is_array()) {
        payload["tools"] = tools;
    }

    // Options
    json options;
    options["temperature"] = m_config.temperature;
    options["top_p"] = m_config.top_p;
    options["num_predict"] = m_config.max_tokens;
    options["num_ctx"] = m_config.num_ctx;
    if (m_config.use_gpu) {
        options["num_gpu"] = m_config.num_gpu;
    }
    payload["options"] = options;

    return payload;
}

json AgentOllamaClient::BuildFIMPayload(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filename,
    bool stream) const
{
    json payload;
    payload["model"] = m_config.fim_model;
    payload["stream"] = stream;
    payload["raw"] = true;

    // Qwen2.5-Coder uses <|fim_prefix|>, <|fim_suffix|>, <|fim_middle|> tokens
    // DeepSeek Coder uses <|fim▁begin|>, <|fim▁hole|>, <|fim▁end|>
    // We use the Qwen format by default
    std::string prompt;
    prompt += "<|fim_prefix|>";
    if (!filename.empty()) {
        prompt += "# " + filename + "\n";
    }
    prompt += prefix;
    prompt += "<|fim_suffix|>";
    prompt += suffix;
    prompt += "<|fim_middle|>";

    payload["prompt"] = prompt;

    // FIM-specific options
    json options;
    options["temperature"] = 0.0f;  // Deterministic for completions
    options["top_p"] = 0.95f;
    options["num_predict"] = m_config.fim_max_tokens;
    options["num_ctx"] = m_config.num_ctx;
    {
        json stopArr = json::array();
        stopArr.push_back("<|fim_pad|>");
        stopArr.push_back("<|endoftext|>");
        stopArr.push_back("\n\n\n");
        options["stop"] = stopArr;
    }
    if (m_config.use_gpu) {
        options["num_gpu"] = m_config.num_gpu;
    }
    payload["options"] = options;

    return payload;
}

// ---------------------------------------------------------------------------
// Response Parsers
// ---------------------------------------------------------------------------

InferenceResult AgentOllamaClient::ParseChatResponse(const std::string& json_str) {
    try {
        json j = json::parse(json_str);

        if (j.contains("error")) {
            return InferenceResult::error(j["error"].get<std::string>());
        }

        InferenceResult result;
        result.success = true;
        result.has_tool_calls = false;

        if (j.contains("message")) {
            result.response = j["message"].value("content", "");

            // Parse tool calls
            if (j["message"].contains("tool_calls") && j["message"]["tool_calls"].is_array()) {
                result.has_tool_calls = true;
                auto& tcs = j["message"]["tool_calls"];
                for (size_t ti = 0; ti < tcs.size(); ++ti) {
                    auto& tc = tcs[ti];
                    if (tc.contains("function")) {
                        std::string tname = tc["function"].value("name", "");
                        json targs = tc["function"].value("arguments", json::object());
                        result.tool_calls.emplace_back(tname, targs);
                    }
                }
            }
        }

        // Perf metrics
        result.prompt_tokens = j.value("prompt_eval_count", 0ULL);
        result.completion_tokens = j.value("eval_count", 0ULL);
        uint64_t evalDuration = j.value("eval_duration", 0ULL);
        result.total_duration_ms = j.value("total_duration", 0ULL) / 1e6;
        if (evalDuration > 0) {
            result.tokens_per_sec = static_cast<double>(result.completion_tokens) /
                                    (static_cast<double>(evalDuration) / 1e9);
        }

        return result;
    } catch (const std::exception& e) {
        return InferenceResult::error(std::string("JSON parse error: ") + e.what());
    } catch (...) {
        return InferenceResult::error("Unknown JSON parse error");
    }
}

InferenceResult AgentOllamaClient::ParseFIMResponse(const std::string& json_str) {
    try {
        json j = json::parse(json_str);

        if (j.contains("error")) {
            return InferenceResult::error(j["error"].get<std::string>());
        }

        InferenceResult result;
        result.success = true;
        result.has_tool_calls = false;
        result.response = j.value("response", "");
        result.completion_tokens = j.value("eval_count", 0ULL);

        uint64_t evalDuration = j.value("eval_duration", 0ULL);
        result.total_duration_ms = j.value("total_duration", 0ULL) / 1e6;
        if (evalDuration > 0) {
            result.tokens_per_sec = static_cast<double>(result.completion_tokens) /
                                    (static_cast<double>(evalDuration) / 1e9);
        }

        return result;
    } catch (const std::exception& e) {
        return InferenceResult::error(std::string("JSON parse error: ") + e.what());
    } catch (...) {
        return InferenceResult::error("Unknown JSON parse error");
    }
}

// ---------------------------------------------------------------------------
// HTTP Implementation (WinHTTP)
// ---------------------------------------------------------------------------

#ifdef _WIN32

std::string AgentOllamaClient::MakeGetRequest(const std::string& endpoint) {
    if (!m_hSession) return "";

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) return "";

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return "";
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                   WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

std::string AgentOllamaClient::MakePostRequest(const std::string& endpoint,
                                                const std::string& body) {
    if (!m_hSession) return "";

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) return "";

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return "";
    }

    const wchar_t* contentType = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                   (LPVOID)body.data(), static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return "";
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

bool AgentOllamaClient::MakeStreamingPost(
    const std::string& endpoint,
    const std::string& body,
    std::function<bool(const std::string& line)> on_line,
    ErrorCallback on_error)
{
    if (!m_hSession) {
        if (on_error) on_error("WinHTTP session not initialized");
        return false;
    }

    std::wstring wHost = ToWide(m_config.host);
    HINTERNET hConnect = WinHttpConnect(m_hSession, wHost.c_str(),
                                        static_cast<INTERNET_PORT>(m_config.port), 0);
    if (!hConnect) {
        if (on_error) on_error("Failed to connect to " + m_config.host);
        return false;
    }

    std::wstring wEndpoint = ToWide(endpoint);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wEndpoint.c_str(),
                                            nullptr, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        if (on_error) on_error("Failed to open request");
        return false;
    }

    const wchar_t* contentType = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                   (LPVOID)body.data(), static_cast<DWORD>(body.size()),
                                   static_cast<DWORD>(body.size()), 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        if (on_error) on_error("HTTP request failed: " + std::to_string(err));
        return false;
    }

    // Stream NDJSON line by line
    std::string lineBuffer;
    char buffer[4096];
    DWORD bytesRead;
    bool success = true;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (m_cancelRequested.load()) {
            success = false;
            break;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            if (buffer[i] == '\n') {
                if (!lineBuffer.empty()) {
                    bool cont = on_line(lineBuffer);
                    lineBuffer.clear();
                    if (!cont) goto done;
                }
            } else if (buffer[i] != '\r') {
                lineBuffer += buffer[i];
            }
        }
    }

    // Process any remaining data
    if (!lineBuffer.empty()) {
        on_line(lineBuffer);
    }

done:
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return success;
}

#else
// POSIX implementation using libcurl or raw sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

static int posix_connect(const std::string& host, int port) {
    struct addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) return -1;
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) { freeaddrinfo(result); return -1; }
    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        close(sock); freeaddrinfo(result); return -1;
    }
    freeaddrinfo(result);
    return sock;
}

std::string AgentOllamaClient::MakeGetRequest(const std::string& path) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) return "";

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + m_host + "\r\nConnection: close\r\n\r\n";
    send(sock, req.c_str(), req.size(), 0);

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        response += buf;
    }
    close(sock);

    // Strip HTTP headers
    size_t bodyStart = response.find("\r\n\r\n");
    return (bodyStart != std::string::npos) ? response.substr(bodyStart + 4) : response;
}

std::string AgentOllamaClient::MakePostRequest(const std::string& path, const std::string& body) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) return "";

    std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + m_host +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    send(sock, req.c_str(), req.size(), 0);

    std::string response;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';
        response += buf;
    }
    close(sock);

    size_t bodyStart = response.find("\r\n\r\n");
    return (bodyStart != std::string::npos) ? response.substr(bodyStart + 4) : response;
}

bool AgentOllamaClient::MakeStreamingPost(const std::string& path, const std::string& body,
    std::function<bool(const std::string&)> on_line, ErrorCallback on_error) {
    int sock = posix_connect(m_host, m_port);
    if (sock < 0) {
        if (on_error) on_error("Failed to connect");
        return false;
    }

    std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + m_host +
        "\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(body.size()) + "\r\n\r\n" + body;
    send(sock, req.c_str(), req.size(), 0);

    // Skip HTTP headers
    std::string headerBuf;
    char c;
    while (recv(sock, &c, 1, 0) == 1) {
        headerBuf += c;
        if (headerBuf.size() >= 4 && headerBuf.substr(headerBuf.size()-4) == "\r\n\r\n") break;
    }

    // Read body line by line
    std::string lineBuf;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                if (!lineBuf.empty()) {
                    if (!on_line(lineBuf)) { close(sock); return true; }
                    lineBuf.clear();
                }
            } else if (buf[i] != '\r') {
                lineBuf += buf[i];
            }
        }
    }
    if (!lineBuf.empty()) on_line(lineBuf);
    close(sock);
    return true;
}
#endif
