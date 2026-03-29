#include "AgentOllamaClient.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>

using RawrXD::Agent::AgentOllamaClient;
using RawrXD::Agent::InferenceResult;
using RawrXD::Agent::OllamaHealth;

namespace {
constexpr int kMaxRetries = 3;
constexpr int kRetryBaseDelayMs = 100;

#pragma comment(lib, "winhttp.lib")

std::wstring ToWide(const std::string& s) {
    if (s.empty()) {
        return L"";
    }
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

struct HttpResponse {
    bool ok = false;
    DWORD status = 0;
    std::string body;
    std::string error;
};

HttpResponse SendOllamaRequest(
    const RawrXD::Agent::OllamaConfig& cfg,
    const std::wstring& method,
    const std::wstring& path,
    const std::string* body,
    const std::function<bool(const std::string&)>& onLine,
    const std::atomic<bool>* cancelFlag) {

    HttpResponse resp;
    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-AgentOllamaClient/2.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        resp.error = "WinHttpOpen failed";
        return resp;
    }

    std::wstring host = ToWide(cfg.host.empty() ? "127.0.0.1" : cfg.host);
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), cfg.port, 0);
    if (!hConnect) {
        resp.error = "WinHttpConnect failed";
        WinHttpCloseHandle(hSession);
        return resp;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        method.c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);
    if (!hRequest) {
        resp.error = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    int timeout = cfg.timeout_ms > 0 ? cfg.timeout_ms : 120000;
    WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);

    BOOL sent = FALSE;
    if (body != nullptr) {
        const std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json";
        sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            const_cast<char*>(body->data()),
            static_cast<DWORD>(body->size()),
            static_cast<DWORD>(body->size()),
            0);
    } else {
        sent = WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0);
    }

    if (!sent) {
        resp.error = "WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        resp.error = "WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return resp;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        nullptr,
        &statusCode,
        &statusSize,
        nullptr);
    resp.status = statusCode;

    std::string lineBuffer;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        if (cancelFlag != nullptr && cancelFlag->load(std::memory_order_relaxed)) {
            break;
        }

        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            break;
        }

        if (bytesRead == 0) {
            continue;
        }

        if (!onLine) {
            resp.body.append(buffer.data(), bytesRead);
            continue;
        }

        for (DWORD i = 0; i < bytesRead; ++i) {
            const char c = buffer[i];
            if (c == '\n') {
                if (!lineBuffer.empty()) {
                    const bool keepGoing = onLine(lineBuffer);
                    lineBuffer.clear();
                    if (!keepGoing) {
                        break;
                    }
                }
            } else if (c != '\r') {
                lineBuffer.push_back(c);
            }
        }
    }

    if (onLine && !lineBuffer.empty()) {
        (void)onLine(lineBuffer);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (resp.status >= 200 && resp.status < 300) {
        resp.ok = true;
    } else if (resp.error.empty()) {
        resp.error = "HTTP " + std::to_string(resp.status);
    }

    return resp;
}
}

AgentOllamaClient::AgentOllamaClient(const OllamaConfig& config)
    : m_config(config) {
}

AgentOllamaClient::~AgentOllamaClient() {
    CancelStream();
}

std::string AgentOllamaClient::BuildPromptFromMessages(const std::vector<ChatMessage>& messages,
                                                       const nlohmann::json& tools) const {
    std::string prompt;

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            prompt += "System: " + msg.content + "\n\n";
            break;
        }
    }

    for (const auto& msg : messages) {
        if (msg.role == "user") {
            prompt += "User: " + msg.content + "\n";
        } else if (msg.role == "assistant") {
            prompt += "Assistant: " + msg.content + "\n";
        } else if (msg.role == "tool") {
            prompt += "Tool result: " + msg.content + "\n";
        }
    }

    if (!tools.empty() && tools.is_array()) {
        prompt += "\nAvailable tools:\n";
        for (const auto& tool : tools) {
            if (tool.contains("function")) {
                const auto& func = tool["function"];
                prompt += "- " + func.value("name", "") + ": " +
                          func.value("description", "") + "\n";
            }
        }
        prompt += "\nTo call a tool, respond with JSON: {\"tool_call\": {\"name\": \"tool_name\", \"arguments\": {...}}}\n";
    }

    prompt += "Assistant: ";
    return prompt;
}

void AgentOllamaClient::ParseToolCallsFromResponse(const std::string& response,
                                                   InferenceResult& result) const {
    size_t json_start = response.find("{");
    if (json_start == std::string::npos) {
        return;
    }

    size_t json_end = response.rfind("}");
    if (json_end == std::string::npos || json_end < json_start) {
        return;
    }

    std::string json_str = response.substr(json_start, json_end - json_start + 1);
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        if (j.contains("tool_call") && j["tool_call"].is_object()) {
            const auto& tc = j["tool_call"];
            std::string name = tc.value("name", "");
            nlohmann::json args = tc.value("arguments", nlohmann::json::object());
            if (!name.empty()) {
                result.has_tool_calls = true;
                result.tool_calls.emplace_back(name, args);
                result.response = response.substr(0, json_start);
            }
        }
    } catch (...) {
    }
}

bool AgentOllamaClient::TestConnection() {
    return TestConnectionWithStats().ok;
}

OllamaHealth AgentOllamaClient::TestConnectionWithStats() {
    OllamaHealth h;
    auto t0 = std::chrono::steady_clock::now();

    auto models = ListModels();
    h.model_count = static_cast<int>(models.size());
    h.ok = h.model_count > 0;

    const std::string version = GetVersion();
    if (!version.empty()) {
        h.version = version;
    }

    auto t1 = std::chrono::steady_clock::now();
    h.latency_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    if (h.version.empty()) {
        h.version = "unknown";
    }

    return h;
}

std::string AgentOllamaClient::GetVersion() {
    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"GET",
        L"/api/version",
        nullptr,
        {},
        nullptr);
    if (!resp.ok) {
        return "";
    }

    try {
        nlohmann::json j = nlohmann::json::parse(resp.body);
        if (j.contains("version") && j["version"].is_string()) {
            return j["version"].get<std::string>();
        }
    } catch (...) {
    }
    return "";
}

std::vector<std::string> AgentOllamaClient::ListModels() {
    std::vector<std::string> models;

    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"GET",
        L"/api/tags",
        nullptr,
        {},
        nullptr);
    if (!resp.ok) {
        return models;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(resp.body);
        if (j.contains("models") && j["models"].is_array()) {
            for (const auto& model : j["models"]) {
                if (model.contains("name") && model["name"].is_string()) {
                    models.push_back(model["name"].get<std::string>());
                }
            }
        }
    } catch (...) {
    }

    return models;
}

InferenceResult AgentOllamaClient::ChatSync(const std::vector<ChatMessage>& messages,
                                            const nlohmann::json& tools) {
    auto start_time = std::chrono::steady_clock::now();

    nlohmann::json body;
    body["model"] = m_config.chat_model.empty() ? "llama3" : m_config.chat_model;
    body["stream"] = false;
    body["messages"] = nlohmann::json::array();
    body["options"] = {
        {"temperature", m_config.temperature},
        {"top_p", m_config.top_p},
        {"num_predict", m_config.max_tokens},
        {"num_ctx", m_config.num_ctx}
    };

    if (!tools.empty() && tools.is_array()) {
        body["tools"] = tools;
    }

    for (const auto& msg : messages) {
        nlohmann::json outMsg;
        outMsg["role"] = msg.role;
        outMsg["content"] = msg.content;
        if (!msg.tool_call_id.empty()) {
            outMsg["tool_call_id"] = msg.tool_call_id;
        }
        if (!msg.tool_calls.is_null() && !msg.tool_calls.empty()) {
            outMsg["tool_calls"] = msg.tool_calls;
        }
        body["messages"].push_back(outMsg);
    }

    const std::string bodyStr = body.dump();
    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"POST",
        L"/api/chat",
        &bodyStr,
        {},
        nullptr);

    if (!resp.ok) {
        return InferenceResult::error(resp.error.empty() ? "chat request failed" : resp.error);
    }

    InferenceResult result;
    result.success = true;
    result.has_tool_calls = false;
    result.prompt_tokens = 0;
    result.completion_tokens = 0;
    result.tokens_per_sec = 0.0;

    try {
        nlohmann::json j = nlohmann::json::parse(resp.body);
        if (j.contains("message") && j["message"].is_object()) {
            const auto& message = j["message"];
            if (message.contains("content") && message["content"].is_string()) {
                result.response = message["content"].get<std::string>();
            }
            if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
                for (const auto& tc : message["tool_calls"]) {
                    if (!tc.contains("function") || !tc["function"].is_object()) {
                        continue;
                    }
                    const auto& func = tc["function"];
                    std::string name = func.value("name", "");
                    nlohmann::json args = nlohmann::json::object();
                    if (func.contains("arguments")) {
                        args = func["arguments"];
                        if (args.is_string()) {
                            try {
                                args = nlohmann::json::parse(args.get<std::string>());
                            } catch (...) {
                                args = nlohmann::json::object();
                            }
                        }
                    }
                    if (!name.empty()) {
                        result.has_tool_calls = true;
                        result.tool_calls.emplace_back(name, args);
                    }
                }
            }
        }
        if (result.response.empty() && j.contains("response") && j["response"].is_string()) {
            result.response = j["response"].get<std::string>();
        }
        if (j.contains("prompt_eval_count")) {
            result.prompt_tokens = j["prompt_eval_count"].get<uint64_t>();
        }
        if (j.contains("eval_count")) {
            result.completion_tokens = j["eval_count"].get<uint64_t>();
        }
        if (j.contains("eval_duration") && result.completion_tokens > 0) {
            uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
            result.tokens_per_sec = static_cast<double>(result.completion_tokens) /
                                    (static_cast<double>(eval_ns) / 1e9);
        }
    } catch (...) {
        result.response = resp.body;
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_duration_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

    ParseToolCallsFromResponse(result.response, result);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalDurationMs += result.total_duration_ms;
    }
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);
    m_totalTokens.fetch_add(result.completion_tokens, std::memory_order_relaxed);
    m_consecutiveErrors = 0;

    if (result.response.empty()) {
        return InferenceResult::error("empty chat response");
    }

    return result;
}

bool AgentOllamaClient::ChatStream(const std::vector<ChatMessage>& messages,
                                   const nlohmann::json& tools,
                                   TokenCallback on_token,
                                   ToolCallCallback on_tool_call,
                                   DoneCallback on_done,
                                   ErrorCallback on_error) {
    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();

    auto full_response = std::make_shared<std::string>();
    auto prompt_tokens = std::make_shared<uint64_t>(0);
    auto completion_tokens = std::make_shared<uint64_t>(0);
    auto tps = std::make_shared<double>(0.0);
    auto streamed_tool_calls = std::make_shared<std::vector<std::pair<std::string, nlohmann::json>>>();

    nlohmann::json body;
    body["model"] = m_config.chat_model.empty() ? "llama3" : m_config.chat_model;
    body["stream"] = true;
    body["messages"] = nlohmann::json::array();
    body["options"] = {
        {"temperature", m_config.temperature},
        {"top_p", m_config.top_p},
        {"num_predict", m_config.max_tokens},
        {"num_ctx", m_config.num_ctx}
    };

    if (!tools.empty() && tools.is_array()) {
        body["tools"] = tools;
    }

    for (const auto& msg : messages) {
        nlohmann::json outMsg;
        outMsg["role"] = msg.role;
        outMsg["content"] = msg.content;
        if (!msg.tool_call_id.empty()) {
            outMsg["tool_call_id"] = msg.tool_call_id;
        }
        if (!msg.tool_calls.is_null() && !msg.tool_calls.empty()) {
            outMsg["tool_calls"] = msg.tool_calls;
        }
        body["messages"].push_back(outMsg);
    }

    const std::string bodyStr = body.dump();
    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"POST",
        L"/api/chat",
        &bodyStr,
        [this, on_token, full_response, prompt_tokens, completion_tokens, tps, streamed_tool_calls](const std::string& line) {
            if (m_cancelRequested.load(std::memory_order_relaxed)) {
                return false;
            }

            try {
                nlohmann::json j = nlohmann::json::parse(line);
                if (j.contains("message") && j["message"].is_object()) {
                    const auto& message = j["message"];
                    if (message.contains("content") && message["content"].is_string()) {
                        const std::string token = message["content"].get<std::string>();
                        if (!token.empty()) {
                            full_response->append(token);
                            if (on_token) {
                                on_token(token);
                            }
                        }
                    }
                    if (message.contains("tool_calls") && message["tool_calls"].is_array()) {
                        for (const auto& tc : message["tool_calls"]) {
                            if (!tc.contains("function") || !tc["function"].is_object()) {
                                continue;
                            }
                            const auto& func = tc["function"];
                            const std::string name = func.value("name", "");
                            nlohmann::json args = nlohmann::json::object();
                            if (func.contains("arguments")) {
                                args = func["arguments"];
                                if (args.is_string()) {
                                    try {
                                        args = nlohmann::json::parse(args.get<std::string>());
                                    } catch (...) {
                                        args = nlohmann::json::object();
                                    }
                                }
                            }
                            if (!name.empty()) {
                                streamed_tool_calls->emplace_back(name, args);
                            }
                        }
                    }
                }

                if (j.contains("prompt_eval_count")) {
                    *prompt_tokens = j["prompt_eval_count"].get<uint64_t>();
                }
                if (j.contains("eval_count")) {
                    *completion_tokens = j["eval_count"].get<uint64_t>();
                }
                if (j.contains("eval_duration") && *completion_tokens > 0) {
                    const uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
                    if (eval_ns > 0) {
                        *tps = static_cast<double>(*completion_tokens) /
                               (static_cast<double>(eval_ns) / 1e9);
                    }
                }

                if (j.value("done", false)) {
                    return false;
                }
            } catch (...) {
                // Ignore malformed incremental chunks.
            }
            return true;
        },
        &m_cancelRequested);

    if (!resp.ok && !m_cancelRequested.load(std::memory_order_relaxed)) {
        m_streaming.store(false);
        if (on_error) {
            on_error(resp.error.empty() ? "stream request failed" : resp.error);
        }
        return false;
    }

    if (!streamed_tool_calls->empty() && on_tool_call) {
        for (const auto& tc : *streamed_tool_calls) {
            on_tool_call(tc.first, tc.second);
        }
    }

    if (full_response->empty() && !resp.body.empty()) {
        try {
            nlohmann::json j = nlohmann::json::parse(resp.body);
            if (j.contains("message") && j["message"].is_object() &&
                j["message"].contains("content") && j["message"]["content"].is_string()) {
                *full_response = j["message"]["content"].get<std::string>();
            }
        } catch (...) {
            // Best-effort fallback only.
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalDurationMs += elapsed_ms;
    }
    m_totalTokens.fetch_add(*completion_tokens, std::memory_order_relaxed);
    m_streaming.store(false);
    m_consecutiveErrors = 0;

    if (on_done) {
        on_done(*full_response, *prompt_tokens, *completion_tokens, *tps);
    }

    return true;
}

InferenceResult AgentOllamaClient::FIMSync(const std::string& prefix,
                                           const std::string& suffix,
                                           const std::string& filename) {
    (void)filename;

    std::string prompt = prefix + "<FILL>" + suffix;

    nlohmann::json body;
    body["model"] = m_config.fim_model.empty()
        ? (m_config.chat_model.empty() ? "llama3" : m_config.chat_model)
        : m_config.fim_model;
    body["prompt"] = prompt;
    body["stream"] = false;
    body["raw"] = true;
    body["options"] = {
        {"temperature", m_config.temperature},
        {"top_p", m_config.top_p},
        {"num_predict", m_config.fim_max_tokens},
        {"num_ctx", m_config.num_ctx}
    };

    const std::string bodyStr = body.dump();
    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"POST",
        L"/api/generate",
        &bodyStr,
        {},
        nullptr);

    if (!resp.ok) {
        return InferenceResult::error(resp.error.empty() ? "FIM request failed" : resp.error);
    }

    std::string response;
    try {
        nlohmann::json j = nlohmann::json::parse(resp.body);
        if (j.contains("response") && j["response"].is_string()) {
            response = j["response"].get<std::string>();
        }
    } catch (...) {
        response = resp.body;
    }

    size_t fill_pos = response.find("<FILL>");
    if (fill_pos != std::string::npos) {
        std::string fill = response.substr(fill_pos + 6);
        if (!suffix.empty() && fill.size() >= suffix.size() &&
            fill.compare(fill.size() - suffix.size(), suffix.size(), suffix) == 0) {
            fill.resize(fill.size() - suffix.size());
        }
        return InferenceResult::ok(fill);
    }

    return InferenceResult::ok(response);
}

bool AgentOllamaClient::FIMStream(const std::string& prefix,
                                  const std::string& suffix,
                                  const std::string& filename,
                                  TokenCallback on_token,
                                  DoneCallback on_done,
                                  ErrorCallback on_error) {
    (void)filename;

    std::string prompt = prefix + "<FILL>" + suffix;

    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();
    auto completion_tokens = std::make_shared<uint64_t>(0);
    auto tps = std::make_shared<double>(0.0);

    nlohmann::json body;
    body["model"] = m_config.fim_model.empty()
        ? (m_config.chat_model.empty() ? "llama3" : m_config.chat_model)
        : m_config.fim_model;
    body["prompt"] = prompt;
    body["stream"] = true;
    body["raw"] = true;
    body["options"] = {
        {"temperature", m_config.temperature},
        {"top_p", m_config.top_p},
        {"num_predict", m_config.fim_max_tokens},
        {"num_ctx", m_config.num_ctx}
    };

    std::string completion;
    const std::string bodyStr = body.dump();
    HttpResponse resp = SendOllamaRequest(
        m_config,
        L"POST",
        L"/api/generate",
        &bodyStr,
        [this, on_token, completion_tokens, tps, &completion](const std::string& line) {
            if (m_cancelRequested.load(std::memory_order_relaxed)) {
                return false;
            }

            try {
                nlohmann::json j = nlohmann::json::parse(line);
                if (j.contains("response") && j["response"].is_string()) {
                    const std::string tok = j["response"].get<std::string>();
                    completion.append(tok);
                    if (on_token && !tok.empty()) {
                        on_token(tok);
                    }
                }
                if (j.contains("eval_count")) {
                    *completion_tokens = j["eval_count"].get<uint64_t>();
                }
                if (j.contains("eval_duration") && *completion_tokens > 0) {
                    const uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
                    if (eval_ns > 0) {
                        *tps = static_cast<double>(*completion_tokens) /
                               (static_cast<double>(eval_ns) / 1e9);
                    }
                }
                if (j.value("done", false)) {
                    return false;
                }
            } catch (...) {
                // Ignore malformed incremental chunks.
            }
            return true;
        },
        &m_cancelRequested);

    if (!resp.ok && !m_cancelRequested.load(std::memory_order_relaxed)) {
        m_streaming.store(false);
        if (on_error) {
            on_error(resp.error.empty() ? "FIM stream failed" : resp.error);
        }
        return false;
    }

    auto end_time = std::chrono::steady_clock::now();
    double elapsed_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalDurationMs += elapsed_ms;
    }
    m_totalTokens.fetch_add(*completion_tokens, std::memory_order_relaxed);
    m_streaming.store(false);
    m_consecutiveErrors = 0;

    if (on_done) {
        on_done(completion, 0, *completion_tokens, *tps);
    }

    return true;
}

void AgentOllamaClient::CancelStream() {
    m_cancelRequested.store(true);
}

bool AgentOllamaClient::WarmupConnection() {
    auto t0 = std::chrono::steady_clock::now();
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        if (TestConnection()) {
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cerr << "[AgentOllamaClient] Native warmup succeeded in " << ms
                      << "ms (attempt " << (attempt + 1) << ")\n";
            return true;
        }
        if (attempt < kMaxRetries - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(kRetryBaseDelayMs * (1 << attempt)));
        }
    }
    std::cerr << "[AgentOllamaClient] Native warmup failed after " << kMaxRetries
              << " attempts\n";
    return false;
}

bool AgentOllamaClient::CheckModelHealth(const std::string& modelName) {
    auto models = ListModels();
    for (const auto& m : models) {
        if (m == modelName || m.find(modelName) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool AgentOllamaClient::ShouldEmitError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    while (m_recentErrors.size() > 10) {
        m_recentErrors.pop_front();
    }

    for (const auto& recent : m_recentErrors) {
        if (recent == msg) {
            return false;
        }
    }

    m_recentErrors.push_back(msg);
    m_consecutiveErrors++;
    return m_consecutiveErrors <= 10;
}

void AgentOllamaClient::SetConfig(const OllamaConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

double AgentOllamaClient::GetAvgTokensPerSec() const {
    uint64_t tokens = m_totalTokens.load(std::memory_order_relaxed);
    if (tokens == 0 || m_totalDurationMs <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(tokens) / (m_totalDurationMs / 1000.0);
}

AgentOllamaClient::MetricsSnapshot AgentOllamaClient::GetMetricsSnapshot() const {
    MetricsSnapshot snap;
    snap.totalRequests = m_totalRequests.load(std::memory_order_relaxed);
    snap.totalTokens = m_totalTokens.load(std::memory_order_relaxed);
    snap.avgTokensPerSec = GetAvgTokensPerSec();
    snap.isStreaming = m_streaming.load(std::memory_order_relaxed);
    snap.consecutiveErrors = m_consecutiveErrors;
    snap.chatModel = m_config.chat_model;
    snap.fimModel = m_config.fim_model;
    snap.host = m_config.host;
    snap.port = m_config.port;
    return snap;
}

InferenceResult AgentOllamaClient::ChatSyncWithRetry(const std::vector<ChatMessage>& messages,
                                                     const nlohmann::json& tools,
                                                     int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        InferenceResult result = ChatSync(messages, tools);
        if (result.success) {
            m_consecutiveErrors = 0;
            return result;
        }

        if (result.error_message.find("model not found") != std::string::npos ||
            result.error_message.find("invalid") != std::string::npos) {
            return result;
        }

        if (attempt < maxRetries - 1) {
            int delayMs = kRetryBaseDelayMs * (1 << attempt);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }

    return InferenceResult::error("ChatSync failed after retries");
}
