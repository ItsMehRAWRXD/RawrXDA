// ============================================================================
// NativeStreamProvider.cpp — Ollama-backed Prediction Provider Implementation
// ============================================================================
// Connects to local Ollama instance for FIM (Fill-in-Middle) completions.
//
// Endpoints used:
//   POST /api/generate  — For FIM/completion predictions (streaming + sync)
//   GET  /api/tags      — For availability check
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "NativeStreamProvider.h"
#include "../ai/inference_retry_shim.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#include <windows.h>
#include <winhttp.h>

// Native Stream Provider and Model List Implementation


#pragma comment(lib, "winhttp.lib")

using RawrXD::Prediction::NativeStreamProvider;
using RawrXD::Prediction::PredictionConfig;
using RawrXD::Prediction::PredictionContext;
using RawrXD::Prediction::PredictionResult;
using RawrXD::Prediction::StreamTokenCallback;
using json = nlohmann::json;

// Global retry shim — circuit breaker + exponential backoff with jitter.
// Policy: 5 retries, 50ms base, circuit opens at 3 consecutive failures.
static rxd::ai::InferenceRetryShim g_inference_retry{[](){
    rxd::ai::RetryPolicy p;
    p.max_retries = 5;
    p.base_ms = 50;
    p.max_backoff_ms = 5000;
    p.circuit_threshold = 3;
    p.circuit_reset_ms = 30000;
    p.jitter_frac = 0.25;
    return p;
}()};

namespace
{

std::wstring ToWide(const std::string& s)
{
    if (s.empty())
        return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0')
        out.pop_back();
    return out;
}

uint64_t NowMs()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// Parse "http://host:port" into components
void ParseUrl(const std::string& baseUrl, std::wstring& host, INTERNET_PORT& port)
{
    host = L"localhost";
    port = 11434;

    size_t colonSlash = baseUrl.find("://");
    if (colonSlash != std::string::npos)
    {
        std::string hostPort = baseUrl.substr(colonSlash + 3);
        // Remove trailing slash
        if (!hostPort.empty() && hostPort.back() == '/')
            hostPort.pop_back();

        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos)
        {
            host = ToWide(hostPort.substr(0, colonPos));
            try
            {
                port = static_cast<INTERNET_PORT>(std::stoi(hostPort.substr(colonPos + 1)));
            }
            catch (...)
            {
            }
        }
        else
        {
            host = ToWide(hostPort);
        }
    }
}

// Trim trailing whitespace and incomplete lines
std::string TrimCompletion(const std::string& raw, int maxLines)
{
    std::string result;
    int lineCount = 0;

    for (size_t i = 0; i < raw.size(); ++i)
    {
        if (raw[i] == '\n')
        {
            lineCount++;
            if (lineCount >= maxLines)
                break;
        }
        result += raw[i];
    }

    // Strip trailing whitespace
    while (!result.empty() &&
           (result.back() == ' ' || result.back() == '\n' || result.back() == '\r' || result.back() == '\t'))
    {
        result.pop_back();
    }

    return result;
}

}  // anonymous namespace

// ============================================================================
// RAII WinHTTP handle
// ============================================================================

NativeStreamProvider::WinHttpHandle::~WinHttpHandle()
{
    if (h)
        WinHttpCloseHandle(h);
}

NativeStreamProvider::WinHttpHandle& NativeStreamProvider::WinHttpHandle::operator=(WinHttpHandle&& o) noexcept
{
    if (this != &o)
    {
        if (h)
            WinHttpCloseHandle(h);
        h = o.h;
        o.h = nullptr;
    }
    return *this;
}

void NativeStreamProvider::WinHttpHandle::reset(void* handle)
{
    if (h)
        WinHttpCloseHandle(h);
    h = handle;
}

// ============================================================================
// Construction
// ============================================================================

NativeStreamProvider::NativeStreamProvider() : m_baseUrl("http://localhost:11435") {}

NativeStreamProvider::NativeStreamProvider(const std::string& baseUrl) : m_baseUrl(baseUrl) {}

NativeStreamProvider::~NativeStreamProvider() = default;

// ============================================================================
// Configuration
// ============================================================================

void NativeStreamProvider::Configure(const PredictionConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

bool NativeStreamProvider::IsAvailable() const
{
    return CheckConnection();
}

bool NativeStreamProvider::CheckConnection() const
{
    bool success = false;
    PostJson("/api/tags", "", success);
    return success;
}

// ============================================================================
// Synchronous prediction
// ============================================================================

// Build FIM (Fill-in-Middle) prompt for code completion
static std::string BuildFIMPrompt(const PredictionContext& ctx)
{
    // Qwen2.5-Coder FIM format: <|fim_prefix|>{prefix}<|fim_suffix|>{suffix}<|fim_middle|>
    std::string prompt = "<|fim_prefix|>" + ctx.prefix + "<|fim_suffix|>" + ctx.suffix + "<|fim_middle|>";
    return prompt;
}

// Build standard completion prompt
static std::string BuildCompletionPrompt(const PredictionContext& ctx)
{
    std::string prompt = ctx.prefix;
    if (!ctx.suffix.empty())
    {
        prompt += "\n\n[Complete the code. Suffix context: " + ctx.suffix + "]";
    }
    return prompt;
}

PredictionResult NativeStreamProvider::Predict(const PredictionContext& ctx)
{
    m_cancelled.store(false);
    auto startMs = NowMs();

    // Build prompt
    std::string prompt;
    if (m_config.useFIM)
    {
        prompt = BuildFIMPrompt(ctx);
    }
    else
    {
        prompt = BuildCompletionPrompt(ctx);
    }

    // Build request body
    json body;
    body["model"] = m_config.model;
    body["prompt"] = prompt;
    body["stream"] = false;
    body["raw"] = m_config.useFIM;  // Raw mode for FIM (no chat template wrapping)
    body["options"] = {{"temperature", m_config.temperature},
                       {"num_predict", m_config.maxTokens},
                       {"top_p", 0.9},
                       {"repeat_penalty", 1.1}};

    // Add stop sequences
    if (!m_config.stopSequences.empty())
    {
        json stops = json::array();
        std::istringstream ss(m_config.stopSequences);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            if (!token.empty())
                stops.push_back(token);
        }
        body["options"]["stop"] = stops;
    }

    bool success = false;
    std::string responseStr = PostJson("/api/generate", body.dump(), success);

    if (!success)
    {
        return PredictionResult::Error("Ollama request failed: " + responseStr);
    }

    if (m_cancelled.load())
    {
        return PredictionResult::Cancelled();
    }

    // Parse response
    try
    {
        json resp = json::parse(responseStr);

        std::string completion;
        if (resp.contains("response") && resp["response"].is_string())
        {
            completion = resp["response"].get<std::string>();
        }

        // Trim and cap
        completion = TrimCompletion(completion, m_config.maxLines);

        int tokens = 0;
        if (resp.contains("eval_count"))
        {
            tokens = resp["eval_count"].get<int>();
        }

        auto elapsed = static_cast<int64_t>(NowMs() - startMs);
        return PredictionResult::Ok(completion, tokens, elapsed);
    }
    catch (const std::exception& ex)
    {
        return PredictionResult::Error(std::string("Parse error: ") + ex.what());
    }
}

// ============================================================================
// Streaming prediction
// ============================================================================

void NativeStreamProvider::PredictStreaming(const PredictionContext& ctx, StreamTokenCallback callback)
{
    m_cancelled.store(false);

    // Build prompt
    std::string prompt;
    if (m_config.useFIM)
    {
        prompt = BuildFIMPrompt(ctx);
    }
    else
    {
        prompt = BuildCompletionPrompt(ctx);
    }

    // Build request body
    json body;
    body["model"] = m_config.model;
    body["prompt"] = prompt;
    body["stream"] = true;
    body["raw"] = m_config.useFIM;
    body["options"] = {{"temperature", m_config.temperature},
                       {"num_predict", m_config.maxTokens},
                       {"top_p", 0.9},
                       {"repeat_penalty", 1.1}};

    int lineCount = 0;
    int tokenCount = 0;

    PostJsonStreaming("/api/generate", body.dump(),
                      [this, &callback, &lineCount, &tokenCount](const std::string& chunk) -> bool
                      {
                          if (m_cancelled.load())
                              return false;

                          try
                          {
                              json j = json::parse(chunk);

                              bool done = j.value("done", false);
                              std::string token;
                              if (j.contains("response") && j["response"].is_string())
                              {
                                  token = j["response"].get<std::string>();
                              }

                              ++tokenCount;

                              // Count newlines for line cap
                              for (char c : token)
                              {
                                  if (c == '\n')
                                      ++lineCount;
                              }
                              if (lineCount >= m_config.maxLines)
                              {
                                  callback("", true);
                                  return false;  // Stop streaming
                              }

                              callback(token, done);
                              return !done;
                          }
                          catch (...)
                          {
                              return true;  // Skip malformed chunks
                          }
                      });
}

// ============================================================================
// Cancellation
// ============================================================================

void NativeStreamProvider::Cancel()
{
    m_cancelled.store(true);
}

// ============================================================================
// HTTP helpers (WinHTTP-based, no external dependencies)
// ============================================================================

std::string NativeStreamProvider::PostJson(const std::string& endpoint, const std::string& body, bool& success) const
{
    success = false;
    if (m_cancelled.load())
        return "Cancelled";

    std::string lastResult;

    auto status = g_inference_retry.Execute([&]() -> rxd::ai::InferenceStatus {
        if (m_cancelled.load())
            return rxd::ai::InferenceStatus::NonRetryable;

        bool ok = false;
        lastResult = PostJsonOnce(endpoint, body, ok);
        if (ok) {
            success = true;
            return rxd::ai::InferenceStatus::OK;
        }
        // Connection-level failures are retryable; leave non-retryable for 4xx HTTP
        return rxd::ai::InferenceStatus::Retryable;
    }, endpoint);

    if (status == rxd::ai::InferenceStatus::CircuitOpen)
        lastResult = "Circuit open: too many consecutive failures for " + endpoint;

    return lastResult;
}

std::string NativeStreamProvider::PostJsonOnce(const std::string& endpoint, const std::string& body, bool& success) const
{
    success = false;
    std::wstring host;
    INTERNET_PORT port;
    ParseUrl(m_baseUrl, host, port);

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD-GhostText/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                       WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession)
        return "WinHttpOpen failed";

    WinHttpHandle hConnect(WinHttpConnect(hSession.get(), host.c_str(), port, 0));
    if (!hConnect)
        return "WinHttpConnect failed";

    std::wstring method = body.empty() ? L"GET" : L"POST";
    std::wstring path = ToWide(endpoint);

    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.get(), method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    if (!hRequest)
        return "WinHttpOpenRequest failed";

    DWORD timeout = 60000;
    WinHttpSetTimeouts(hRequest.get(), timeout, timeout, timeout, timeout);

    BOOL sent;
    if (body.empty())
    {
        sent = WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    }
    else
    {
        std::wstring headers = L"Content-Type: application/json";
        sent = WinHttpSendRequest(hRequest.get(), headers.c_str(), (DWORD)headers.size(), (LPVOID)body.data(),
                                  (DWORD)body.size(), (DWORD)body.size(), 0);
    }

    if (!sent)
        return "Send failed (is native inference server running?)";

    if (!WinHttpReceiveResponse(hRequest.get(), nullptr))
        return "No response from Ollama";

    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest.get(), &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest.get(), buffer.data(), bytesAvailable, &bytesRead))
        {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    success = true;
    return responseBody;
}

void NativeStreamProvider::PostJsonStreaming(const std::string& endpoint, const std::string& jsonBody,
                                       std::function<bool(const std::string& chunk)> onChunk) const
{
    if (m_cancelled.load())
        return;

    g_inference_retry.Execute([&]() -> rxd::ai::InferenceStatus {
        if (m_cancelled.load())
            return rxd::ai::InferenceStatus::NonRetryable;
        bool connected = PostJsonStreamingOnce(endpoint, jsonBody, onChunk);
        return connected ? rxd::ai::InferenceStatus::OK : rxd::ai::InferenceStatus::Retryable;
    }, endpoint + ":stream");
}

bool NativeStreamProvider::PostJsonStreamingOnce(const std::string& endpoint, const std::string& body,
                                           std::function<bool(const std::string& chunk)> onChunk) const
{
    std::wstring host;
    INTERNET_PORT port;
    ParseUrl(m_baseUrl, host, port);

    WinHttpHandle hSession(WinHttpOpen(L"RawrXD-GhostText-Stream/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                       WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession)
        return false;

    WinHttpHandle hConnect(WinHttpConnect(hSession.get(), host.c_str(), port, 0));
    if (!hConnect)
        return false;

    std::wstring path = ToWide(endpoint);
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect.get(), L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
    if (!hRequest)
        return false;

    DWORD timeout = 300000;
    WinHttpSetTimeouts(hRequest.get(), timeout, timeout, timeout, timeout);

    std::wstring headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest.get(), headers.c_str(), (DWORD)headers.size(), (LPVOID)body.data(),
                                   (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent)
        return false;

    if (!WinHttpReceiveResponse(hRequest.get(), nullptr))
        return false;

    // Connection established — stream NDJSON lines
    std::string lineBuffer;
    DWORD bytesAvailable = 0;

    while (!m_cancelled.load() && WinHttpQueryDataAvailable(hRequest.get(), &bytesAvailable) && bytesAvailable > 0)
    {

        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest.get(), buffer.data(), bytesAvailable, &bytesRead))
            break;

        for (DWORD i = 0; i < bytesRead; ++i)
        {
            char c = buffer[i];
            if (c == '\n')
            {
                if (!lineBuffer.empty())
                {
                    bool keepGoing = onChunk(lineBuffer);
                    lineBuffer.clear();
                    if (!keepGoing)
                        return true;
                }
            }
            else
            {
                lineBuffer += c;
            }
        }
    }

    // Flush remaining
    if (!lineBuffer.empty() && !m_cancelled.load())
    {
        onChunk(lineBuffer);
    }

    return true;  // Connected successfully (even if cancelled mid-stream)
}

// ============================================================================
// Shared HTTP surface (agent / other callers — single WinHTTP implementation)
// ============================================================================

std::string NativeStreamProvider::SyncHttpJson(const std::string& endpoint, const std::string& body, bool& success) const
{
    return PostJson(endpoint, body, success);
}

void NativeStreamProvider::StreamHttpJsonLines(const std::string& endpoint, const std::string& jsonBody,
                                         std::function<bool(const std::string& line)> onLine) const
{
    PostJsonStreaming(endpoint, jsonBody, std::move(onLine));
}

std::string NativeStreamProvider::GetFirstModelTag() const
{
    bool ok = false;
    const std::string raw = PostJson("/api/tags", "", ok);
    if (!ok || raw.empty())
    {
        return {};
    }
    try
    {
        const json j = json::parse(raw);
        if (!j.contains("models") || !j["models"].is_array())
        {
            return {};
        }
        for (const auto& m : j["models"])
        {
            if (!m.is_object() || !m.contains("name") || !m["name"].is_string())
            {
                continue;
            }
            const std::string name = m["name"].get<std::string>();
            if (!name.empty())
            {
                return name;
            }
        }
    }
    catch (...)
    {
    }
    return {};
}

// Deleter implementation for forward-declared NativeStreamProvider in Win32IDE.h
void NativeStreamProviderDeleter::operator()(RawrXD::Prediction::NativeStreamProvider* p) noexcept
{
    delete p;
}
