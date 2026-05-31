#include "ollama_client.h"
#include "json_parse_guard.hpp"
#include "json_sanitizer.hpp"
#include "json_schema_validator.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>

// WinHTTP must be outside namespace
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace RawrXD {
namespace Backend {

using json = nlohmann::json;
using JSONGuard = JSON::JSONParseGuard;
using JSONSanitizer = JSON::JSONSanitizer;
using JSONValidator = JSON::JSONSchemaValidator;
using JSONRecovery = JSON::JSONParseRecovery;

// ═══════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], needed);
    return out;
}

// RAII WinHTTP handle wrapper
struct WinHttpHandle {
    HINTERNET h = nullptr;
    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle) : h(handle) {}
    ~WinHttpHandle() { if (h) WinHttpCloseHandle(h); }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    WinHttpHandle(WinHttpHandle&& o) noexcept : h(o.h) { o.h = nullptr; }
    WinHttpHandle& operator=(WinHttpHandle&& o) noexcept { if (h) WinHttpCloseHandle(h); h = o.h; o.h = nullptr; return *this; }
    operator HINTERNET() const { return h; }
    explicit operator bool() const { return h != nullptr; }
};

// ═══════════════════════════════════════════════════════════════════
// Constructor / Destructor / Configuration
// ═══════════════════════════════════════════════════════════════════

NativeClient::NativeClient(const std::string& base_url)
    : m_base_url(base_url) {
}

NativeClient::~NativeClient() {
    m_cancelled.store(true);
}

void NativeClient::setBaseUrl(const std::string& url) {
    m_base_url = url;
}

void NativeClient::setTimeoutSeconds(int seconds) {
    m_timeout_seconds = (seconds > 0 && seconds <= 3600) ? seconds : 300;
}

void NativeClient::setRetryConfig(const RetryConfig& config) {
    m_retry_config = config;
}

// ═══════════════════════════════════════════════════════════════════
// URL Parsing
// ═══════════════════════════════════════════════════════════════════

NativeClient::ParsedUrl NativeClient::parseBaseUrl() const {
    ParsedUrl result;
    std::string url = m_base_url;

    // Strip protocol
    if (url.rfind("https://", 0) == 0) {
        result.https = true;
        url = url.substr(8);
        result.port = 443;
    } else if (url.rfind("http://", 0) == 0) {
        url = url.substr(7);
    }

    // Strip trailing slash
    while (!url.empty() && url.back() == '/') url.pop_back();

    // Split host:port
    auto colon = url.find(':');
    if (colon != std::string::npos) {
        result.host = url.substr(0, colon);
        try {
            unsigned long p = std::stoul(url.substr(colon + 1));
            if (p > 0 && p <= 65535) result.port = static_cast<uint16_t>(p);
        } catch (...) {}
    } else {
        result.host = url;
    }

    if (result.host.empty()) result.host = "localhost";
    return result;
}

// ═══════════════════════════════════════════════════════════════════
// Connection & Health
// ═══════════════════════════════════════════════════════════════════

bool NativeClient::testConnection() {
    std::string response = makeGetRequest("/api/tags");
    return !response.empty();
}

std::string NativeClient::getVersion() {
    std::string response = makeGetRequest("/api/version");
    if (response.empty()) return "";

    json j = JSONGuard::SafeParse(response);
    if (j.empty() || !j.is_object()) return "";

    return JSONValidator::GetStringField(j, "version", "");
}

bool NativeClient::isRunning() {
    return testConnection();
}

ConnectionHealth NativeClient::healthCheck() {
    ConnectionHealth health;
    ULONGLONG start = GetTickCount64();

    std::string response = makeGetRequest("/api/tags");
    health.latency_ms = GetTickCount64() - start;
    health.last_check_ms = GetTickCount64();

    if (response.empty()) {
        health.connected = false;
        health.status_text = "Connection failed";
        return health;
    }

    health.connected = true;

    // Parse model count
    json j = JSONGuard::SafeParse(response);
    if (j.is_object() && j.contains("models") && j["models"].is_array()) {
        health.model_count = static_cast<uint32_t>(j["models"].size());
    }

    // Get version
    health.version = getVersion();
    health.status_text = "Connected (" + std::to_string(health.model_count) +
                         " models, " + std::to_string(health.latency_ms) + "ms)";
    return health;
}

// ═══════════════════════════════════════════════════════════════════
// Cancellation
// ═══════════════════════════════════════════════════════════════════

void NativeClient::cancelStream() {
    m_cancelled.store(true);
}

bool NativeClient::isCancelled() const {
    return m_cancelled.load();
}

std::vector<OllamaModel> NativeClient::listModels() {
    std::string response = makeGetRequestWithRetry("/api/tags");
    if (response.empty()) return {};

    return parseModels(response);
}

std::vector<OllamaModel> NativeClient::filterModels(
    const std::vector<OllamaModel>& models,
    std::function<bool(const OllamaModel&)> predicate) const {

    std::vector<OllamaModel> filtered;
    for (const auto& model : models) {
        if (predicate(model)) {
            filtered.push_back(model);
        }
    }
    return filtered;
}

const OllamaModel* NativeClient::findModelById(
    const std::vector<OllamaModel>& models,
    const std::string& targetId) const {

    for (const auto& model : models) {
        if (model.id == targetId) {
            return &model;
        }
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════
// Synchronous Generation
// ═══════════════════════════════════════════════════════════════════

NativeInferenceResponse NativeClient::generateSync(const OllamaGenerateRequest& request) {
    OllamaGenerateRequest non_stream = request;
    non_stream.stream = false;
    std::string json_body = createGenerateRequestJson(non_stream);
    std::string response = makePostRequestWithRetry("/api/generate", json_body);

    if (response.empty()) {
        NativeInferenceResponse res;
        res.error = true;
        res.error_message = "Empty response from server";
        return res;
    }

    return parseResponse(response);
}

NativeInferenceResponse NativeClient::chatSync(const OllamaChatRequest& request) {
    OllamaChatRequest non_stream = request;
    non_stream.stream = false;
    std::string json_body = createChatRequestJson(non_stream);
    std::string response = makePostRequestWithRetry("/api/chat", json_body);

    if (response.empty()) {
        NativeInferenceResponse res;
        res.error = true;
        res.error_message = "Empty response from server";
        return res;
    }

    return parseResponse(response);
}

// ═══════════════════════════════════════════════════════════════════
// Streaming Generation
// ═══════════════════════════════════════════════════════════════════

bool NativeClient::generate(const OllamaGenerateRequest& request,
                            StreamCallback on_chunk,
                            ErrorCallback on_error,
                            CompletionCallback on_complete) {
    m_cancelled.store(false);
    std::string json_body = createGenerateRequestJson(request);
    return makeStreamingPostRequest("/api/generate", json_body, on_chunk, on_error, on_complete);
}

bool NativeClient::chat(const OllamaChatRequest& request,
                        StreamCallback on_chunk,
                        ErrorCallback on_error,
                        CompletionCallback on_complete) {
    m_cancelled.store(false);
    std::string json_body = createChatRequestJson(request);
    return makeStreamingPostRequest("/api/chat", json_body, on_chunk, on_error, on_complete);
}

// ═══════════════════════════════════════════════════════════════════
// Tool-Augmented Chat (multi-turn tool loop)
// ═══════════════════════════════════════════════════════════════════

NativeInferenceResponse NativeClient::chatWithTools(
    const OllamaChatRequest& request,
    ToolExecutor executor,
    int max_tool_rounds) {

    if (max_tool_rounds < 1) max_tool_rounds = 1;
    if (max_tool_rounds > 20) max_tool_rounds = 20;

    OllamaChatRequest working = request;
    working.stream = false;  // Tool loop uses sync responses

    for (int round = 0; round < max_tool_rounds; ++round) {
        if (m_cancelled.load()) {
            NativeInferenceResponse cancelled;
            cancelled.error = true;
            cancelled.error_message = "Cancelled";
            return cancelled;
        }

        NativeInferenceResponse resp = chatSync(working);

        if (resp.error || !resp.has_tool_calls || resp.tool_calls.empty()) {
            return resp;  // No tools requested or error — done
        }

        // Assistant message with tool_calls goes into history
        OllamaChatMessage assistant_msg;
        assistant_msg.role = "assistant";
        assistant_msg.content = resp.message.content;
        assistant_msg.tool_calls = resp.tool_calls;
        working.messages.push_back(assistant_msg);

        // Execute each tool and feed results back
        for (const auto& tc : resp.tool_calls) {
            std::string tool_result;
            try {
                tool_result = executor(tc.function.name, tc.function.arguments);
            } catch (const std::exception& e) {
                tool_result = std::string("{\"error\":\"Tool execution failed: ") + e.what() + "\"}";
            }

            OllamaChatMessage tool_msg;
            tool_msg.role = "tool";
            tool_msg.content = tool_result;
            tool_msg.tool_call_id = tc.id;
            working.messages.push_back(tool_msg);
        }
    }

    // Exhausted tool rounds — make one final call without tools
    working.tools.clear();
    return chatSync(working);
}

// ═══════════════════════════════════════════════════════════════════
// Embeddings
// ═══════════════════════════════════════════════════════════════════

std::vector<float> NativeClient::embeddings(const std::string& model, const std::string& prompt) {
    json request_json = {
        {"model", model},
        {"prompt", prompt}
    };

    std::string json_body = request_json.dump();
    std::string response = makePostRequestWithRetry("/api/embeddings", json_body);

    if (response.empty()) return {};

    json j = JSONGuard::SafeParse(response);
    if (j.empty() || !j.is_object()) {
        return {};
    }

    try {
        json embedding_field = JSONValidator::GetArrayField(j, "embedding");
        if (!embedding_field.empty() && embedding_field.is_array()) {
            return embedding_field.get<std::vector<float>>();
        }
    } catch (const std::exception& e) {
        (void)e;
    }

    return {};
}

// ═══════════════════════════════════════════════════════════════════
// JSON Serialization
// ═══════════════════════════════════════════════════════════════════

std::string NativeClient::createGenerateRequestJson(const OllamaGenerateRequest& req) {
    json j = {
        {"model", req.model},
        {"prompt", req.prompt},
        {"stream", req.stream}
    };

    if (!req.options.empty()) {
        json options = json::object();
        for (const auto& [key, value] : req.options) {
            options[key] = value;
        }
        j["options"] = options;
    }

    return j.dump();
}

std::string NativeClient::createChatRequestJson(const OllamaChatRequest& req) {
    json j = {
        {"model", req.model},
        {"stream", req.stream},
        {"messages", json::array()}
    };

    for (const auto& msg : req.messages) {
        json msg_json = {
            {"role", msg.role},
            {"content", msg.content}
        };

        // Serialize tool_calls on assistant messages
        if (!msg.tool_calls.empty()) {
            json tc_arr = json::array();
            for (const auto& tc : msg.tool_calls) {
                json tc_json = {
                    {"type", tc.type},
                    {"function", {
                        {"name", tc.function.name},
                        {"arguments", json::parse(tc.function.arguments, nullptr, false)}
                    }}
                };
                if (!tc.id.empty()) tc_json["id"] = tc.id;
                tc_arr.push_back(tc_json);
            }
            msg_json["tool_calls"] = tc_arr;
        }

        // Tool result messages include tool_call_id
        if (!msg.tool_call_id.empty()) {
            msg_json["tool_call_id"] = msg.tool_call_id;
        }

        j["messages"].push_back(msg_json);
    }

    // Tool definitions
    if (!req.tools.empty()) {
        json tools_arr = json::array();
        for (const auto& td : req.tools) {
            json props = json::object();
            for (const auto& [pname, pprop] : td.function.properties) {
                props[pname] = {{"type", pprop.type}, {"description", pprop.description}};
            }

            json tool_json = {
                {"type", td.type},
                {"function", {
                    {"name", td.function.name},
                    {"description", td.function.description},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", props},
                        {"required", td.function.required}
                    }}
                }}
            };
            tools_arr.push_back(tool_json);
        }
        j["tools"] = tools_arr;
    }

    if (!req.options.empty()) {
        json options = json::object();
        for (const auto& [key, value] : req.options) {
            options[key] = value;
        }
        j["options"] = options;
    }

    return j.dump();
}

// ═══════════════════════════════════════════════════════════════════
// Response Parsing
// ═══════════════════════════════════════════════════════════════════

static std::vector<ToolCall> parseToolCallsFromJson(const json& message_json) {
    std::vector<ToolCall> calls;

    if (!message_json.contains("tool_calls") || !message_json["tool_calls"].is_array()) {
        return calls;
    }

    for (const auto& tc_json : message_json["tool_calls"]) {
        if (!tc_json.is_object()) continue;

        ToolCall tc;
        tc.id = tc_json.value("id", "");
        tc.type = tc_json.value("type", "function");

        if (tc_json.contains("function") && tc_json["function"].is_object()) {
            const auto& fn = tc_json["function"];
            tc.function.name = fn.value("name", "");
            if (fn.contains("arguments")) {
                if (fn["arguments"].is_string()) {
                    tc.function.arguments = fn["arguments"].get<std::string>();
                } else {
                    tc.function.arguments = fn["arguments"].dump();
                }
            }
        }

        if (!tc.function.name.empty()) {
            calls.push_back(tc);
        }
    }
    return calls;
}

NativeInferenceResponse NativeClient::parseResponse(const std::string& json_str) {
    NativeInferenceResponse response;

    std::string parse_error;
    json j = JSONGuard::SafeParse(json_str,
        [&parse_error](const std::string& err) {
            parse_error = err;
        });

    if (j.empty() || !j.is_object()) {
        response.error = true;
        response.error_message = "Failed to parse response JSON. Original error: " + parse_error;
        if (json_str.length() < 500) {
            response.error_message += ". Raw input: " + json_str;
        }
        return response;
    }

    // Check for server-side error
    if (j.contains("error") && j["error"].is_string()) {
        response.error = true;
        response.error_message = j["error"].get<std::string>();
        return response;
    }

    try {
        response.model = JSONValidator::GetStringField(j, "model", "");
        response.response = JSONValidator::GetStringField(j, "response", "");
        response.done = j.value("done", false);

        if (j.contains("message") && j["message"].is_object()) {
            const auto& msg = j["message"];
            response.message.role = JSONValidator::GetStringField(msg, "role", "");
            response.message.content = JSONValidator::GetStringField(msg, "content", "");

            // Parse tool calls from message
            response.tool_calls = parseToolCallsFromJson(msg);
            response.has_tool_calls = !response.tool_calls.empty();
            response.message.tool_calls = response.tool_calls;
        }

        response.total_duration = j.value("total_duration", 0ULL);
        response.prompt_eval_count = j.value("prompt_eval_count", 0ULL);
        response.eval_count = j.value("eval_count", 0ULL);
        response.load_duration = j.value("load_duration", 0ULL);
        response.prompt_eval_duration = j.value("prompt_eval_duration", 0ULL);
        response.eval_duration = j.value("eval_duration", 0ULL);

        response.error = false;
    } catch (const std::exception& e) {
        response.error = true;
        response.error_message = std::string("Field extraction error: ") + e.what();
    }

    return response;
}

std::vector<OllamaModel> NativeClient::parseModels(const std::string& json_str) {
    std::vector<OllamaModel> models;

    json j = JSONGuard::SafeParse(json_str, [](const std::string& err) {
        (void)err;
    });

    if (j.empty() || !j.is_object()) {
        return models;
    }

    try {
        json models_array = JSONValidator::GetArrayField(j, "models");

        for (const auto& model_json : models_array) {
            if (!model_json.is_object()) continue;

            OllamaModel model;
            model.name = JSONValidator::GetStringField(model_json, "name", "");

            if (model.name.empty()) continue;

            model.id = model.name;
            model.size = model_json.value("size", 0ULL);
            model.digest = JSONValidator::GetStringField(model_json, "digest", "");
            model.modified_at = JSONValidator::GetStringField(model_json, "modified_at", "");

            json details = JSONValidator::GetObjectField(model_json, "details");
            if (!details.empty()) {
                model.format = JSONValidator::GetStringField(details, "format", "");
                model.family = JSONValidator::GetStringField(details, "family", "");
                model.parameter_size = JSONValidator::GetStringField(details, "parameter_size", "");
                model.quantization_level = JSONValidator::GetStringField(details, "quantization_level", "");
            }

            models.push_back(model);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing models (recovered): " << e.what() << std::endl;
    }

    return models;
}

// ═══════════════════════════════════════════════════════════════════
// WinHTTP Transport — Production Implementation
// ═══════════════════════════════════════════════════════════════════

static constexpr DWORD kMaxResponseBytes = 64u * 1024u * 1024u;  // 64 MB response cap
static constexpr DWORD kReadChunkSize = 65536u;                   // 64 KB read chunks

std::string NativeClient::makeGetRequest(const std::string& endpoint) {
    ParsedUrl url = parseBaseUrl();
    DWORD tms = static_cast<DWORD>(m_timeout_seconds) * 1000;

    WinHttpHandle hSession(WinHttpOpen(
        L"RawrXD-NativeClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return {};

    WinHttpSetTimeouts(hSession, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(
        hSession, ToWide(url.host).c_str(),
        static_cast<INTERNET_PORT>(url.port), 0));
    if (!hConnect) return {};

    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect, L"GET",
        ToWide(endpoint).c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) return {};

    if (!WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        return {};
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        return {};
    }

    // Check status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode >= 400) return {};

    // Read response body
    std::string body;
    body.reserve(4096);
    DWORD dwSize = 0;

    while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
        DWORD toRead = (std::min)(dwSize, kReadChunkSize);
        std::vector<char> buf(static_cast<size_t>(toRead));
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), toRead, &downloaded)) break;
        if (downloaded == 0) break;
        if (body.size() + downloaded > kMaxResponseBytes) break;
        body.append(buf.data(), downloaded);
    }

    return body;
}

std::string NativeClient::makePostRequest(const std::string& endpoint, const std::string& json_body) {
    ParsedUrl url = parseBaseUrl();
    DWORD tms = static_cast<DWORD>(m_timeout_seconds) * 1000;

    WinHttpHandle hSession(WinHttpOpen(
        L"RawrXD-NativeClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return {};

    WinHttpSetTimeouts(hSession, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(
        hSession, ToWide(url.host).c_str(),
        static_cast<INTERNET_PORT>(url.port), 0));
    if (!hConnect) return {};

    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect, L"POST",
        ToWide(endpoint).c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) return {};

    std::wstring headers = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hRequest,
            headers.c_str(), static_cast<DWORD>(headers.size()),
            (LPVOID)json_body.data(), static_cast<DWORD>(json_body.size()),
            static_cast<DWORD>(json_body.size()), 0)) {
        return {};
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        return {};
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode >= 400) return {};

    std::string body;
    body.reserve(8192);
    DWORD dwSize = 0;

    while (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
        DWORD toRead = (std::min)(dwSize, kReadChunkSize);
        std::vector<char> buf(static_cast<size_t>(toRead));
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), toRead, &downloaded)) break;
        if (downloaded == 0) break;
        if (body.size() + downloaded > kMaxResponseBytes) break;
        body.append(buf.data(), downloaded);
    }

    return body;
}

bool NativeClient::makeStreamingPostRequest(const std::string& endpoint,
                                           const std::string& json_body,
                                           StreamCallback on_chunk,
                                           ErrorCallback on_error,
                                           CompletionCallback on_complete) {
    ParsedUrl url = parseBaseUrl();

    // Streaming uses long timeout (5 min default, or configured)
    DWORD tms = static_cast<DWORD>(m_timeout_seconds) * 1000;

    WinHttpHandle hSession(WinHttpOpen(
        L"RawrXD-NativeClient/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) {
        if (on_error) on_error("WinHTTP session creation failed");
        return false;
    }

    WinHttpSetTimeouts(hSession, tms, tms, tms, tms);

    WinHttpHandle hConnect(WinHttpConnect(
        hSession, ToWide(url.host).c_str(),
        static_cast<INTERNET_PORT>(url.port), 0));
    if (!hConnect) {
        if (on_error) on_error("WinHTTP connection failed to " + url.host + ":" + std::to_string(url.port));
        return false;
    }

    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(
        hConnect, L"POST",
        ToWide(endpoint).c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) {
        if (on_error) on_error("WinHTTP request creation failed");
        return false;
    }

    std::wstring headers = L"Content-Type: application/json";
    if (!WinHttpSendRequest(hRequest,
            headers.c_str(), static_cast<DWORD>(headers.size()),
            (LPVOID)json_body.data(), static_cast<DWORD>(json_body.size()),
            static_cast<DWORD>(json_body.size()), 0)) {
        if (on_error) on_error("WinHTTP send request failed");
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        if (on_error) on_error("WinHTTP receive response failed — is native inference server running?");
        return false;
    }

    // Check status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode >= 400) {
        // Read error body
        std::string errBody;
        DWORD avail = 0;
        if (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            DWORD toRead = (std::min)(avail, static_cast<DWORD>(4096));
            std::vector<char> buf(toRead);
            DWORD read = 0;
            WinHttpReadData(hRequest, buf.data(), toRead, &read);
            errBody.assign(buf.data(), read);
        }
        if (on_error) on_error("HTTP " + std::to_string(statusCode) + ": " + errBody);
        return false;
    }

    // Stream NDJSON response line-by-line
    std::string lineBuffer;
    std::string accumulated_response;
    DWORD bytesAvailable = 0;

    while (!m_cancelled.load() &&
           WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {

        DWORD toRead = (std::min)(bytesAvailable, kReadChunkSize);
        std::vector<char> buffer(static_cast<size_t>(toRead));
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), toRead, &bytesRead)) break;
        if (bytesRead == 0) break;

        // Parse NDJSON: split on newlines, parse each complete line
        for (DWORD i = 0; i < bytesRead; ++i) {
            char c = buffer[i];
            if (c == '\n') {
                if (!lineBuffer.empty()) {
                    // Parse this NDJSON line
                    json line_json = JSONGuard::SafeParseStreamChunk(lineBuffer,
                        [&on_error](const std::string& err) {
                            if (on_error) on_error("Stream parse error: " + err);
                        });

                    if (!line_json.is_null() && line_json.is_object()) {
                        try {
                            // Extract token from either "response" (generate) or "message.content" (chat)
                            std::string token;
                            if (line_json.contains("response") && line_json["response"].is_string()) {
                                token = line_json["response"].get<std::string>();
                            } else if (line_json.contains("message") && line_json["message"].is_object()) {
                                const auto& msg = line_json["message"];
                                if (msg.contains("content") && msg["content"].is_string()) {
                                    token = msg["content"].get<std::string>();
                                }
                            }

                            if (!token.empty()) {
                                accumulated_response += token;
                                if (on_chunk) on_chunk(token);
                            }

                            // Check for completion
                            if (line_json.contains("done") && line_json["done"].get<bool>()) {
                                NativeInferenceResponse final_response;
                                final_response.done = true;
                                final_response.model = line_json.value("model", "");
                                final_response.response = accumulated_response;
                                final_response.total_duration = line_json.value("total_duration", 0ULL);
                                final_response.prompt_eval_count = line_json.value("prompt_eval_count", 0ULL);
                                final_response.eval_count = line_json.value("eval_count", 0ULL);
                                final_response.load_duration = line_json.value("load_duration", 0ULL);
                                final_response.prompt_eval_duration = line_json.value("prompt_eval_duration", 0ULL);
                                final_response.eval_duration = line_json.value("eval_duration", 0ULL);

                                // Check final message for tool calls
                                if (line_json.contains("message") && line_json["message"].is_object()) {
                                    final_response.message.role = line_json["message"].value("role", "assistant");
                                    final_response.message.content = accumulated_response;
                                    final_response.tool_calls = parseToolCallsFromJson(line_json["message"]);
                                    final_response.has_tool_calls = !final_response.tool_calls.empty();
                                }

                                if (on_complete) on_complete(final_response);
                                return true;
                            }
                        } catch (const std::exception& e) {
                            if (on_error) on_error(std::string("Stream field error: ") + e.what());
                        }
                    }
                    lineBuffer.clear();
                }
            } else {
                lineBuffer += c;
            }
        }
    }

    if (m_cancelled.load()) {
        if (on_error) on_error("Stream cancelled by user");
        return false;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
// Retry Wrappers (exponential backoff)
// ═══════════════════════════════════════════════════════════════════

std::string NativeClient::makeGetRequestWithRetry(const std::string& endpoint) {
    int delay_ms = m_retry_config.base_delay_ms;

    for (int attempt = 0; attempt <= m_retry_config.max_retries; ++attempt) {
        if (m_cancelled.load()) return {};

        std::string result = makeGetRequest(endpoint);
        if (!result.empty()) return result;

        if (attempt < m_retry_config.max_retries) {
            Sleep(static_cast<DWORD>(delay_ms));
            delay_ms = (std::min)(
                static_cast<int>(delay_ms * m_retry_config.backoff_multiplier),
                m_retry_config.max_delay_ms);
        }
    }
    return {};
}

std::string NativeClient::makePostRequestWithRetry(const std::string& endpoint, const std::string& json_body) {
    int delay_ms = m_retry_config.base_delay_ms;

    for (int attempt = 0; attempt <= m_retry_config.max_retries; ++attempt) {
        if (m_cancelled.load()) return {};

        std::string result = makePostRequest(endpoint, json_body);
        if (!result.empty()) return result;

        if (attempt < m_retry_config.max_retries) {
            Sleep(static_cast<DWORD>(delay_ms));
            delay_ms = (std::min)(
                static_cast<int>(delay_ms * m_retry_config.backoff_multiplier),
                m_retry_config.max_delay_ms);
        }
    }
    return {};
}

} // namespace Backend
} // namespace RawrXD
