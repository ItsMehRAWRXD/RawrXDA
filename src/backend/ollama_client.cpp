#include "ollama_client.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

using json = nlohmann::json;

// ─── Helpers (outside namespace, before RawrXD) ─────────────────────

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], needed);
    return out;
}

// RAII handle wrapper for WinHTTP
struct WinHttpHandle {
    HINTERNET h = nullptr;
    explicit WinHttpHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~WinHttpHandle() { if (h) WinHttpCloseHandle(h); }
    WinHttpHandle(WinHttpHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    WinHttpHandle& operator=(WinHttpHandle&& o) noexcept {
        if (this != &o) { if (h) WinHttpCloseHandle(h); h = o.h; o.h = nullptr; }
        return *this;
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    explicit operator bool() const { return h != nullptr; }
};

// Parse tool_calls from a JSON message object
static std::vector<RawrXD::Backend::ToolCall> parseToolCallsFromJson(const json& message) {
    std::vector<RawrXD::Backend::ToolCall> result;
    if (!message.contains("tool_calls") || !message["tool_calls"].is_array()) return result;
    for (const auto& tc : message["tool_calls"]) {
        RawrXD::Backend::ToolCall call;
        if (tc.contains("id")) call.id = tc["id"].get<std::string>();
        if (tc.contains("type")) call.type = tc["type"].get<std::string>();
        if (tc.contains("function") && tc["function"].is_object()) {
            const auto& fn = tc["function"];
            if (fn.contains("name")) call.function.name = fn["name"].get<std::string>();
            if (fn.contains("arguments")) {
                if (fn["arguments"].is_string())
                    call.function.arguments = fn["arguments"].get<std::string>();
                else
                    call.function.arguments = fn["arguments"].dump();
            }
        }
        result.push_back(std::move(call));
    }
    return result;
}

namespace RawrXD {
namespace Backend {

// ─── Constructor / Destructor ───────────────────────────────────────

OllamaClient::OllamaClient(const std::string& base_url)
    : m_base_url(base_url), m_timeout_seconds(300) {
}

OllamaClient::~OllamaClient() {
}

// ─── Configuration ──────────────────────────────────────────────────

void OllamaClient::setBaseUrl(const std::string& url) {
    m_base_url = url;
}

void OllamaClient::setTimeoutSeconds(int seconds) {
    m_timeout_seconds = (seconds > 0) ? seconds : 300;
}

void OllamaClient::setRetryConfig(const RetryConfig& config) {
    m_retry_config = config;
}

// ─── Connection ─────────────────────────────────────────────────────

bool OllamaClient::testConnection() {
    try {
        std::string version = getVersion();
        return !version.empty();
    } catch (...) {
        return false;
    }
}

std::string OllamaClient::getVersion() {
    std::string response = makeGetRequest("/api/version");
    try {
        json j = json::parse(response);
        return j.value("version", std::string());
    } catch (...) {
        return "";
    }
}

bool OllamaClient::isRunning() {
    return testConnection();
}

ConnectionHealth OllamaClient::healthCheck() {
    ConnectionHealth health;
    auto start = std::chrono::steady_clock::now();
    try {
        // Use std::once_flag to deduplicate redundant /api/tags calls during startup.
        // Multiple subsystems (agentic bridge, model selector, status bar) all call
        // healthCheck() concurrently; this ensures only one GET /api/tags is issued.
        std::call_once(m_health_check_once, [this]() {
            m_health_check_tags_response = makeGetRequest("/api/tags");
        });
        std::string response = m_health_check_tags_response;

        auto elapsed = std::chrono::steady_clock::now() - start;
        health.latency_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

        json j = json::parse(response);
        health.connected = true;
        if (j.contains("models") && j["models"].is_array())
            health.model_count = static_cast<uint32_t>(j["models"].size());

        // Get version separately (lightweight, not cached)
        std::string ver_resp = makeGetRequest("/api/version");
        json vj = json::parse(ver_resp);
        health.version = vj.value("version", std::string());
        health.status_text = "OK";
    } catch (...) {
        health.connected = false;
        health.status_text = "Connection failed";
    }
    return health;
}

// ─── Cancellation ───────────────────────────────────────────────────

void OllamaClient::cancelStream() {
    m_cancelled.store(true, std::memory_order_release);
}

bool OllamaClient::isCancelled() const {
    return m_cancelled.load(std::memory_order_acquire);
}

// ─── Model Listing ──────────────────────────────────────────────────

std::vector<OllamaModel> OllamaClient::listModels() {
    // Return cached model list if still within TTL (deduplicates redundant /api/tags calls).
    {
        std::lock_guard<std::mutex> lock(m_model_cache_mutex);
        auto now = std::chrono::steady_clock::now();
        if (!m_cached_models.empty() &&
            (now - m_cache_timestamp) < k_model_cache_ttl)
        {
            return m_cached_models;
        }
    }

    std::string response = makeGetRequestWithRetry("/api/tags");
    std::vector<OllamaModel> models = parseModels(response);

    {
        std::lock_guard<std::mutex> lock(m_model_cache_mutex);
        m_cached_models = models;
        m_cache_timestamp = std::chrono::steady_clock::now();
    }
    return models;
}

// ─── Synchronous Generation ─────────────────────────────────────────

NativeInferenceResponse OllamaClient::generateSync(const OllamaGenerateRequest& request) {
    OllamaGenerateRequest sync_req = request;
    sync_req.stream = false;
    std::string body = createGenerateRequestJson(sync_req);
    std::string response = makePostRequestWithRetry("/api/generate", body);
    return parseResponse(response);
}

NativeInferenceResponse OllamaClient::chatSync(const OllamaChatRequest& request) {
    OllamaChatRequest sync_req = request;
    sync_req.stream = false;
    std::string body = createChatRequestJson(sync_req);
    std::string response = makePostRequestWithRetry("/api/chat", body);
    return parseResponse(response);
}

// ─── Streaming Generation ───────────────────────────────────────────

bool OllamaClient::generate(const OllamaGenerateRequest& request,
                            StreamCallback on_chunk,
                            ErrorCallback on_error,
                            CompletionCallback on_complete) {
    m_cancelled.store(false, std::memory_order_release);
    std::string body = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", body, on_chunk, on_error, on_complete);
}

bool OllamaClient::chat(const OllamaChatRequest& request,
                        StreamCallback on_chunk,
                        ErrorCallback on_error,
                        CompletionCallback on_complete) {
    m_cancelled.store(false, std::memory_order_release);
    std::string body = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", body, on_chunk, on_error, on_complete);
}

// ─── Tool-Augmented Chat ────────────────────────────────────────────

NativeInferenceResponse OllamaClient::chatWithTools(
    const OllamaChatRequest& request,
    ToolExecutor executor,
    int max_tool_rounds) {

    OllamaChatRequest working = request;
    const int limit = (max_tool_rounds > 0 && max_tool_rounds <= 20) ? max_tool_rounds : 5;

    for (int round = 0; round < limit; ++round) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            NativeInferenceResponse aborted;
            aborted.error = true;
            aborted.error_message = "Cancelled";
            return aborted;
        }

        // On final round, strip tools to force a text answer
        if (round == limit - 1)
            working.tools.clear();

        NativeInferenceResponse resp = chatSync(working);
        if (resp.error) return resp;
        if (!resp.has_tool_calls || resp.tool_calls.empty()) return resp;

        // Assistant message with tool calls
        OllamaChatMessage assistant_msg;
        assistant_msg.role = "assistant";
        assistant_msg.content = resp.message.content;
        assistant_msg.tool_calls = resp.tool_calls;
        working.messages.push_back(std::move(assistant_msg));

        // Execute each tool and feed results back
        for (const auto& tc : resp.tool_calls) {
            std::string result;
            try {
                result = executor(tc.function.name, tc.function.arguments);
            } catch (const std::exception& e) {
                result = std::string("{\"error\":\"") + e.what() + "\"}";
            }

            OllamaChatMessage tool_msg;
            tool_msg.role = "tool";
            tool_msg.content = result;
            tool_msg.tool_call_id = tc.id;
            working.messages.push_back(std::move(tool_msg));
        }
    }

    // Exhausted rounds — return last response
    return chatSync(working);
}

// ─── Embeddings ─────────────────────────────────────────────────────

std::vector<float> OllamaClient::embeddings(const std::string& model, const std::string& prompt) {
    json req_json;
    req_json["model"] = model;
    req_json["prompt"] = prompt;
    std::string response = makePostRequestWithRetry("/api/embeddings", req_json.dump());

    std::vector<float> result;
    try {
        json j = json::parse(response);
        if (j.contains("embedding") && j["embedding"].is_array()) {
            for (const auto& v : j["embedding"]) {
                result.push_back(v.get<float>());
            }
        }
    } catch (...) {
        // JSON parse failed for embedding response — return empty result
        return {};
    }
}

// ─── URL Parsing ────────────────────────────────────────────────────

OllamaClient::ParsedUrl OllamaClient::parseBaseUrl() const {
    ParsedUrl parsed;
    std::string url = m_base_url;

    // Strip protocol
    if (url.rfind("https://", 0) == 0) {
        parsed.https = true;
        url = url.substr(8);
        parsed.port = 443;
    } else if (url.rfind("http://", 0) == 0) {
        url = url.substr(7);
    }

    // Strip trailing slash
    while (!url.empty() && url.back() == '/') url.pop_back();

    // Extract host:port
    auto colon = url.find(':');
    if (colon != std::string::npos) {
        parsed.host = url.substr(0, colon);
        try { parsed.port = static_cast<uint16_t>(std::stoi(url.substr(colon + 1))); }
        catch (const std::exception& e) {
            OutputDebugStringA(("[ollama_client] port parse exception: " + std::string(e.what()) + "\n").c_str());
        } catch (...) {
            OutputDebugStringA("[ollama_client] port parse unknown exception\n");
        }
    } else {
        parsed.host = url;
    }

    if (parsed.host.empty()) parsed.host = "localhost";
    return parsed;
}

// ─── JSON Building ──────────────────────────────────────────────────

std::string OllamaClient::createGenerateRequestJson(const OllamaGenerateRequest& req) {
    json j;
    j["model"] = req.model;
    j["prompt"] = req.prompt;
    j["stream"] = req.stream;
    if (!req.options.empty()) {
        json opts;
        for (const auto& [k, v] : req.options) opts[k] = v;
        j["options"] = opts;
    }
    return j.dump();
}

std::string OllamaClient::createChatRequestJson(const OllamaChatRequest& req) {
    json j;
    j["model"] = req.model;
    j["stream"] = req.stream;

    json msgs = json::array();
    for (const auto& m : req.messages) {
        json msg;
        msg["role"] = m.role;
        msg["content"] = m.content;

        // Serialize tool_calls for assistant messages
        if (!m.tool_calls.empty()) {
            json tcs = json::array();
            for (const auto& tc : m.tool_calls) {
                json tcj;
                if (!tc.id.empty()) tcj["id"] = tc.id;
                tcj["type"] = tc.type;
                json fn;
                fn["name"] = tc.function.name;
                // Try to parse arguments as JSON, fall back to string
                try {
                    fn["arguments"] = json::parse(tc.function.arguments);
                } catch (...) {
                    fn["arguments"] = tc.function.arguments;
                }
                tcj["function"] = fn;
                tcs.push_back(tcj);
            }
            msg["tool_calls"] = tcs;
        }

        // Serialize tool_call_id for tool messages
        if (!m.tool_call_id.empty())
            msg["tool_call_id"] = m.tool_call_id;

        msgs.push_back(msg);
    }
    j["messages"] = msgs;

    // Serialize tool definitions
    if (!req.tools.empty()) {
        json tools = json::array();
        for (const auto& td : req.tools) {
            json tool;
            tool["type"] = td.type;
            json fn;
            fn["name"] = td.function.name;
            fn["description"] = td.function.description;

            json params;
            params["type"] = "object";
            if (!td.function.properties.empty()) {
                json props;
                for (const auto& [pname, pprop] : td.function.properties) {
                    json p;
                    p["type"] = pprop.type;
                    p["description"] = pprop.description;
                    props[pname] = p;
                }
                params["properties"] = props;
            }
            if (!td.function.required.empty()) {
                params["required"] = td.function.required;
            }
            fn["parameters"] = params;
            tool["function"] = fn;
            tools.push_back(tool);
        }
        j["tools"] = tools;
    }

    if (!req.options.empty()) {
        json opts;
        for (const auto& [k, v] : req.options) opts[k] = v;
        j["options"] = opts;
    }

    return j.dump();
}

// ─── Response Parsing ───────────────────────────────────────────────

NativeInferenceResponse OllamaClient::parseResponse(const std::string& json_str) {
    NativeInferenceResponse resp;

    try {
        json j = json::parse(json_str);

        // Check for server error
        if (j.contains("error")) {
            resp.error = true;
            resp.error_message = j["error"].get<std::string>();
            return resp;
        }

        resp.model = j.value("model", std::string());
        resp.response = j.value("response", std::string());
        resp.done = j.value("done", false);

        // Parse message (for /api/chat)
        if (j.contains("message") && j["message"].is_object()) {
            const auto& msg = j["message"];
            resp.message.role = msg.value("role", std::string());
            resp.message.content = msg.value("content", std::string());

            // Extract tool calls
            auto tool_calls = parseToolCallsFromJson(msg);
            if (!tool_calls.empty()) {
                resp.has_tool_calls = true;
                resp.tool_calls = std::move(tool_calls);
                resp.message.tool_calls = resp.tool_calls;
            }
        }

        // Timing metrics
        resp.total_duration = j.value("total_duration", uint64_t(0));
        resp.prompt_eval_count = j.value("prompt_eval_count", uint64_t(0));
        resp.eval_count = j.value("eval_count", uint64_t(0));
        if (j.contains("load_duration"))
            resp.load_duration = j["load_duration"].get<uint64_t>();
        if (j.contains("prompt_eval_duration"))
            resp.prompt_eval_duration = j["prompt_eval_duration"].get<uint64_t>();
        if (j.contains("eval_duration"))
            resp.eval_duration = j["eval_duration"].get<uint64_t>();

    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error in parseResponse: " << e.what() << std::endl;
    }

    return resp;
}

std::vector<OllamaModel> OllamaClient::parseModels(const std::string& json_str) {
    std::vector<OllamaModel> models;
    try {
        json j = json::parse(json_str);
        if (j.contains("models") && j["models"].is_array()) {
            for (const auto& mj : j["models"]) {
                OllamaModel model;
                model.name = mj.value("name", std::string());
                model.modified_at = mj.value("modified_at", std::string());
                model.size = mj.value("size", uint64_t(0));
                model.digest = mj.value("digest", std::string());

                if (mj.contains("details") && mj["details"].is_object()) {
                    const auto& d = mj["details"];
                    model.format = d.value("format", std::string());
                    model.family = d.value("family", std::string());
                    model.parameter_size = d.value("parameter_size", std::string());
                    model.quantization_level = d.value("quantization_level", std::string());
                }
                models.push_back(std::move(model));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error in parseModels: " << e.what() << std::endl;
    }
    return models;
}

// ─── Retry Wrappers ─────────────────────────────────────────────────

std::string OllamaClient::makeGetRequestWithRetry(const std::string& endpoint) {
    int delay_ms = m_retry_config.base_delay_ms;
    for (int attempt = 0; attempt <= m_retry_config.max_retries; ++attempt) {
        std::string result = makeGetRequest(endpoint);
        if (!result.empty()) return result;
        if (attempt < m_retry_config.max_retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = (std::min)(
                static_cast<int>(delay_ms * m_retry_config.backoff_multiplier),
                m_retry_config.max_delay_ms);
        }
    }
    return "";
}

std::string OllamaClient::makePostRequestWithRetry(const std::string& endpoint, const std::string& json_body) {
    int delay_ms = m_retry_config.base_delay_ms;
    for (int attempt = 0; attempt <= m_retry_config.max_retries; ++attempt) {
        std::string result = makePostRequest(endpoint, json_body);
        if (!result.empty()) return result;
        if (attempt < m_retry_config.max_retries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms = (std::min)(
                static_cast<int>(delay_ms * m_retry_config.backoff_multiplier),
                m_retry_config.max_delay_ms);
        }
    }
    return "";
}

// ═══════════════════════════════════════════════════════════════════
// Platform-specific HTTP transport
// ═══════════════════════════════════════════════════════════════════

#ifdef _WIN32

std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    ParsedUrl url = parseBaseUrl();
    std::wstring whost = ToWide(url.host);
    std::wstring wpath = ToWide(endpoint);
    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD-Backend/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return "";

    // Set timeouts: resolve, connect, send, receive (all in ms)
    int tms = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession.h, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(hSession.h, whost.c_str(), url.port, 0));
    if (!hConnect) return "";

    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.h, L"GET", wpath.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) return "";

    if (!WinHttpSendRequest(hRequest.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        return "";

    if (!WinHttpReceiveResponse(hRequest.h, NULL))
        return "";

    // Check HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode >= 400) return "";

    // Read response body (64KB chunks, 64MB cap)
    std::string body;
    constexpr DWORD kChunkSize = 65536;
    constexpr size_t kMaxBody = 64 * 1024 * 1024;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest.h, &avail) && avail > 0) {
        DWORD toRead = (avail > kChunkSize) ? kChunkSize : avail;
        char buf[65536];
        DWORD read = 0;
        if (WinHttpReadData(hRequest.h, buf, toRead, &read) && read > 0) {
            body.append(buf, read);
            if (body.size() > kMaxBody) break;
        } else break;
    }
    return body;
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    ParsedUrl url = parseBaseUrl();
    std::wstring whost = ToWide(url.host);
    std::wstring wpath = ToWide(endpoint);
    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD-Backend/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return "";

    int tms = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession.h, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(hSession.h, whost.c_str(), url.port, 0));
    if (!hConnect) return "";

    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.h, L"POST", wpath.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) return "";

    const wchar_t* hdrs = L"Content-Type: application/json\r\n";
    DWORD bodyLen = static_cast<DWORD>(json_body.size());
    if (!WinHttpSendRequest(hRequest.h, hdrs, (DWORD)-1,
                            (LPVOID)json_body.data(), bodyLen, bodyLen, 0))
        return "";

    if (!WinHttpReceiveResponse(hRequest.h, NULL))
        return "";

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode >= 400) return "";

    std::string body;
    constexpr DWORD kChunkSize = 65536;
    constexpr size_t kMaxBody = 64 * 1024 * 1024;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest.h, &avail) && avail > 0) {
        DWORD toRead = (avail > kChunkSize) ? kChunkSize : avail;
        char buf[65536];
        DWORD read = 0;
        if (WinHttpReadData(hRequest.h, buf, toRead, &read) && read > 0) {
            body.append(buf, read);
            if (body.size() > kMaxBody) break;
        } else break;
    }
    return body;
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint,
                                            const std::string& json_body,
                                            StreamCallback on_chunk,
                                            ErrorCallback on_error,
                                            CompletionCallback on_complete) {
    ParsedUrl url = parseBaseUrl();
    std::wstring whost = ToWide(url.host);
    std::wstring wpath = ToWide(endpoint);
    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD-Backend/1.0",
                                       WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                       WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) {
        if (on_error) on_error("WinHTTP session creation failed");
        return false;
    }

    int tms = m_timeout_seconds * 1000;
    WinHttpSetTimeouts(hSession.h, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(hSession.h, whost.c_str(), url.port, 0));
    if (!hConnect) {
        if (on_error) on_error("WinHTTP connect failed to " + url.host + ":" + std::to_string(url.port));
        return false;
    }

    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.h, L"POST", wpath.c_str(),
                                              NULL, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) {
        if (on_error) on_error("WinHTTP open request failed");
        return false;
    }

    const wchar_t* hdrs = L"Content-Type: application/json\r\n";
    DWORD bodyLen = static_cast<DWORD>(json_body.size());
    if (!WinHttpSendRequest(hRequest.h, hdrs, (DWORD)-1,
                            (LPVOID)json_body.data(), bodyLen, bodyLen, 0)) {
        if (on_error) on_error("WinHTTP send request failed");
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest.h, NULL)) {
        if (on_error) on_error("WinHTTP receive response failed");
        return false;
    }

    // Check HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode >= 400) {
        // Read error body
        std::string errBody;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest.h, &avail) && avail > 0 && errBody.size() < 4096) {
            char buf[4096];
            DWORD read = 0;
            DWORD toRead = (avail > 4096) ? 4096 : avail;
            if (WinHttpReadData(hRequest.h, buf, toRead, &read) && read > 0)
                errBody.append(buf, read);
            else break;
        }
        if (on_error) on_error("HTTP " + std::to_string(statusCode) + ": " + errBody);
        return false;
    }

    // Stream NDJSON line by line
    std::string line_buffer;
    NativeInferenceResponse final_resp;
    constexpr size_t kMaxLine = 1024 * 1024; // 1MB max per line

    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest.h, &avail) && avail > 0) {
        if (m_cancelled.load(std::memory_order_acquire)) {
            if (on_error) on_error("Stream cancelled");
            return false;
        }

        char buf[8192];
        DWORD toRead = (avail > sizeof(buf)) ? static_cast<DWORD>(sizeof(buf)) : avail;
        DWORD read = 0;
        if (!WinHttpReadData(hRequest.h, buf, toRead, &read) || read == 0) break;

        // Append to buffer and process lines
        for (DWORD i = 0; i < read; ++i) {
            if (buf[i] == '\n') {
                // Process complete NDJSON line
                if (!line_buffer.empty()) {
                    try {
                        json j = json::parse(line_buffer);

                        // Check for error
                        if (j.contains("error")) {
                            if (on_error) on_error(j["error"].get<std::string>());
                            return false;
                        }

                        // Extract content token for streaming callback
                        std::string token;
                        if (j.contains("response")) {
                            // /api/generate format
                            token = j["response"].get<std::string>();
                        } else if (j.contains("message") && j["message"].is_object()) {
                            // /api/chat format
                            token = j["message"].value("content", std::string());
                        }

                        if (!token.empty() && on_chunk)
                            on_chunk(token);

                        // Check for done
                        if (j.value("done", false)) {
                            final_resp = parseResponse(line_buffer);

                            // Extract tool_calls from final message
                            if (j.contains("message") && j["message"].is_object()) {
                                auto tcs = parseToolCallsFromJson(j["message"]);
                                if (!tcs.empty()) {
                                    final_resp.has_tool_calls = true;
                                    final_resp.tool_calls = std::move(tcs);
                                    final_resp.message.tool_calls = final_resp.tool_calls;
                                }
                            }

                            if (on_complete) on_complete(final_resp);
                            return true;
                        }
                    } catch (...) {
                        // Skip malformed lines
                    }
                }
                line_buffer.clear();
            } else {
                line_buffer += buf[i];
                if (line_buffer.size() > kMaxLine) {
                    if (on_error) on_error("NDJSON line exceeds 1MB limit");
                    return false;
                }
            }
        }
    }

    // Stream ended without done=true — send what we have
    if (on_complete) on_complete(final_resp);
    return true;
}

#else
// Linux/Mac fallback: return explicit JSON error payloads instead of silent empty strings.
std::string OllamaClient::makeGetRequest(const std::string& endpoint) {
    std::ostringstream oss;
    oss << "{\"error\":\"platform_transport_unavailable\",\"endpoint\":\"" << endpoint << "\"}";
    return oss.str();
}

std::string OllamaClient::makePostRequest(const std::string& endpoint, const std::string&) {
    std::ostringstream oss;
    oss << "{\"error\":\"platform_transport_unavailable\",\"endpoint\":\"" << endpoint << "\"}";
    return oss.str();
}

bool OllamaClient::makeStreamingPostRequest(const std::string& endpoint, const std::string&,
                                            StreamCallback, ErrorCallback on_error, CompletionCallback on_complete) {
    if (on_error) {
        on_error("Streaming transport unavailable on this platform for endpoint: " + endpoint);
    }
    if (on_complete) {
        NativeInferenceResponse r;
        r.success = false;
        r.error = "Streaming transport unavailable on this platform";
        on_complete(r);
    }
    return false;
}
#endif

} // namespace Backend
} // namespace RawrXD
