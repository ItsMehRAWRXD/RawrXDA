/**
 * @file llm_http_client.cpp
 * @brief WinHTTP/curl HTTP client implementation (Qt-free)
 *
 * Post-Qt replacement for QNetworkAccessManager.
 * Three key components:
 *   1. StlHttpClient — platform HTTP transport (WinHTTP / curl)
 *   2. ChainStep     — single async LLM call with future retention
 *   3. ChainExecutor — sequential step chain with callbacks
 *
 * Compatible with both CLI (RawrEngine) and GUI (RawrXD-Win32IDE).
 * No exceptions in hot paths. Structured PatchResult-style returns.
 */

#include "llm_http_client.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winhttp.h>
#  pragma comment(lib, "winhttp.lib")
#else
#  include <cstdlib>
#  include <filesystem>
#  include <fstream>
#endif

// ============================================================================
// URL parsing helper
// ============================================================================
namespace {

struct ParsedUrl {
    std::wstring host;
    std::wstring path;
    uint16_t     port   = 80;
    bool         isTls  = false;
};

#ifdef _WIN32
ParsedUrl parseUrl(const std::string& url) {
    ParsedUrl p;
    p.isTls = (url.rfind("https", 0) == 0);
    p.port  = p.isTls ? 443 : 80;

    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        p.host = L"localhost";
        p.path = std::wstring(url.begin(), url.end());
        return p;
    }
    schemeEnd += 3;

    size_t pathStart = url.find('/', schemeEnd);
    if (pathStart == std::string::npos) pathStart = url.size();

    std::string hostPort(url.begin() + schemeEnd, url.begin() + pathStart);
    std::string pathStr(url.begin() + pathStart, url.end());
    if (pathStr.empty()) pathStr = "/";

    // Check for explicit port
    auto colon = hostPort.find(':');
    if (colon != std::string::npos) {
        std::string portStr(hostPort.begin() + colon + 1, hostPort.end());
        p.port = static_cast<uint16_t>(std::stoi(portStr));
        hostPort = hostPort.substr(0, colon);
    }

    p.host = std::wstring(hostPort.begin(), hostPort.end());
    p.path = std::wstring(pathStr.begin(), pathStr.end());
    return p;
}
#endif

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // anonymous namespace

// ============================================================================
// StlHttpClient — construction / destruction
// ============================================================================
StlHttpClient::StlHttpClient() = default;

StlHttpClient::~StlHttpClient() {
    cancelAll();
}

StlHttpClient& StlHttpClient::instance() {
    static StlHttpClient s_instance;
    return s_instance;
}

// ============================================================================
// StlHttpClient — synchronous send
// ============================================================================
HttpResponse StlHttpClient::send(const HttpRequest& req) {
    m_stats.totalRequests++;
    m_stats.activeRequests++;

    if (onRequestStarted) {
        onRequestStarted(req.url);
    }

    auto resp = platformSend(req);

    m_stats.activeRequests--;
    m_stats.totalLatencyMs += resp.latencyMs;
    if (resp.success) m_stats.successCount++;
    else              m_stats.failCount++;

    if (onRequestCompleted) {
        onRequestCompleted(resp);
    }
    if (!resp.success && onError) {
        onError(resp.error, true);
    }

    return resp;
}

HttpResponse StlHttpClient::postJson(const std::string& url,
                                      const std::string& jsonBody,
                                      int timeoutMs) {
    HttpRequest req;
    req.method    = "POST";
    req.url       = url;
    req.body      = jsonBody;
    req.timeoutMs = timeoutMs > 0 ? timeoutMs : m_defaultTimeout;
    return send(req);
}

HttpResponse StlHttpClient::get(const std::string& url, int timeoutMs) {
    HttpRequest req;
    req.method    = "GET";
    req.url       = url;
    req.timeoutMs = timeoutMs > 0 ? timeoutMs : m_defaultTimeout;
    return send(req);
}

// ============================================================================
// StlHttpClient — async (future-based)
// ============================================================================
std::future<HttpResponse> StlHttpClient::sendAsync(const HttpRequest& req) {
    // CRITICAL FIX: std::async with launch::async creates a new thread.
    // The returned future MUST be held alive by the caller.
    // If the future is destroyed, the destructor blocks until the async
    // operation completes (C++ standard behavior for std::async futures).
    // This is the correct behavior — Qt's deleteLater() hid this requirement.
    return std::async(std::launch::async, [this, req]() -> HttpResponse {
        return this->send(req);
    });
}

std::future<HttpResponse> StlHttpClient::postJsonAsync(const std::string& url,
                                                        const std::string& jsonBody,
                                                        int timeoutMs) {
    HttpRequest req;
    req.method    = "POST";
    req.url       = url;
    req.body      = jsonBody;
    req.timeoutMs = timeoutMs > 0 ? timeoutMs : m_defaultTimeout;
    return sendAsync(req);
}

void StlHttpClient::cancelAll() {
    m_cancelled.store(true);
}

void StlHttpClient::resetStats() {
    m_stats.totalRequests.store(0);
    m_stats.successCount.store(0);
    m_stats.failCount.store(0);
    m_stats.totalLatencyMs.store(0);
    // Don't reset activeRequests — they may still be in flight
}

// ============================================================================
// StlHttpClient — platform-specific HTTP implementation
// ============================================================================
#ifdef _WIN32
HttpResponse StlHttpClient::platformSend(const HttpRequest& req) {
    auto startT = nowMs();

    if (m_cancelled.load()) {
        return HttpResponse::fail("Cancelled", 0);
    }

    auto parsed = parseUrl(req.url);

    // Open session
    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-Agent/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        return HttpResponse::fail("WinHttpOpen failed (err=" +
            std::to_string(GetLastError()) + ")", static_cast<int>(nowMs() - startT));
    }

    // Set timeouts
    int tms = req.timeoutMs > 0 ? req.timeoutMs : m_defaultTimeout;
    WinHttpSetTimeouts(hSession, tms, tms, tms, tms);

    // Connect
    HINTERNET hConnect = WinHttpConnect(hSession, parsed.host.c_str(), parsed.port, 0);
    if (!hConnect) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hSession);
        return HttpResponse::fail("WinHttpConnect failed to " +
            std::string(parsed.host.begin(), parsed.host.end()) + ":" +
            std::to_string(parsed.port) + " (err=" + std::to_string(err) + ")",
            static_cast<int>(nowMs() - startT));
    }

    // Open request
    DWORD flags = parsed.isTls ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wMethod(req.method.begin(), req.method.end());
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, wMethod.c_str(), parsed.path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return HttpResponse::fail("WinHttpOpenRequest failed (err=" +
            std::to_string(err) + ")", static_cast<int>(nowMs() - startT));
    }

    // Build headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!req.extraHeaderKey.empty()) {
        std::wstring ek(req.extraHeaderKey.begin(), req.extraHeaderKey.end());
        std::wstring ev(req.extraHeaderVal.begin(), req.extraHeaderVal.end());
        headers += ek + L": " + ev + L"\r\n";
    }
    if (!req.apiKey.empty()) {
        std::wstring key(req.apiKey.begin(), req.apiKey.end());
        headers += L"Authorization: Bearer " + key + L"\r\n";
    }

    // Send
    LPVOID bodyData = req.body.empty()
        ? WINHTTP_NO_REQUEST_DATA
        : const_cast<char*>(req.body.c_str());
    DWORD bodyLen = static_cast<DWORD>(req.body.size());

    BOOL sent = WinHttpSendRequest(
        hRequest,
        headers.c_str(), static_cast<DWORD>(headers.size()),
        bodyData, bodyLen, bodyLen, 0);

    if (!sent) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return HttpResponse::fail("WinHttpSendRequest failed (err=" +
            std::to_string(err) + ")", static_cast<int>(nowMs() - startT));
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return HttpResponse::fail("WinHttpReceiveResponse failed (err=" +
            std::to_string(err) + ")", static_cast<int>(nowMs() - startT));
    }

    // Read status code
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX);

    // Read body
    std::string responseBody;
    DWORD bytesAvail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
        if (m_cancelled.load()) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return HttpResponse::fail("Cancelled during read",
                static_cast<int>(nowMs() - startT));
        }

        size_t pos = responseBody.size();
        responseBody.resize(pos + bytesAvail);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, responseBody.data() + pos, bytesAvail, &bytesRead);
        responseBody.resize(pos + bytesRead);
    }

    int latency = static_cast<int>(nowMs() - startT);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // HTTP 2xx is success
    bool isOk = (statusCode >= 200 && statusCode < 300);
    if (isOk) {
        return HttpResponse::ok(static_cast<int>(statusCode), responseBody, latency);
    } else {
        HttpResponse resp;
        resp.success    = false;
        resp.statusCode = static_cast<int>(statusCode);
        resp.body       = responseBody;
        resp.error      = "HTTP " + std::to_string(statusCode);
        resp.latencyMs  = latency;
        return resp;
    }
}

#else
// ============================================================================
// POSIX fallback — shell out to curl
// ============================================================================
HttpResponse StlHttpClient::platformSend(const HttpRequest& req) {
    namespace fs = std::filesystem;
    auto startT = nowMs();

    if (m_cancelled.load()) {
        return HttpResponse::fail("Cancelled", 0);
    }

    // Write body to temp file
    auto tmpBody = fs::temp_directory_path() / "rawrxd_http_body.json";
    if (!req.body.empty()) {
        std::ofstream f(tmpBody, std::ios::trunc);
        if (!f) return HttpResponse::fail("Failed to write temp body", 0);
        f << req.body;
        f.close();
    }

    std::string cmd = "curl -s -w '\\n%{http_code}' -X " + req.method;
    cmd += " -H 'Content-Type: application/json'";

    if (!req.apiKey.empty())
        cmd += " -H 'Authorization: Bearer " + req.apiKey + "'";
    if (!req.extraHeaderKey.empty())
        cmd += " -H '" + req.extraHeaderKey + ": " + req.extraHeaderVal + "'";

    if (req.timeoutMs > 0)
        cmd += " --max-time " + std::to_string(req.timeoutMs / 1000);

    if (!req.body.empty())
        cmd += " -d @" + tmpBody.string();

    cmd += " '" + req.url + "' 2>/dev/null";

    FILE* p = popen(cmd.c_str(), "r");
    if (!p) {
        fs::remove(tmpBody);
        return HttpResponse::fail("popen failed", static_cast<int>(nowMs() - startT));
    }

    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), p)) output += buf;
    int exitCode = pclose(p);
    fs::remove(tmpBody);

    int latency = static_cast<int>(nowMs() - startT);

    if (exitCode != 0) {
        return HttpResponse::fail("curl exit code " + std::to_string(exitCode), latency);
    }

    // Last line is the HTTP status code
    size_t lastNL = output.rfind('\n');
    int statusCode = 200;
    std::string body = output;
    if (lastNL != std::string::npos && lastNL > 0) {
        std::string codeStr = output.substr(lastNL + 1);
        body = output.substr(0, lastNL);
        try { statusCode = std::stoi(codeStr); } catch (...) {}
    }

    bool isOk = (statusCode >= 200 && statusCode < 300);
    if (isOk) {
        return HttpResponse::ok(statusCode, body, latency);
    } else {
        HttpResponse resp;
        resp.success    = false;
        resp.statusCode = statusCode;
        resp.body       = body;
        resp.error      = "HTTP " + std::to_string(statusCode);
        resp.latencyMs  = latency;
        return resp;
    }
}
#endif

// ============================================================================
// ChainStep — async LLM call with future retention
// ============================================================================
ChainStep::~ChainStep() {
    // If the future is valid and the step is running, wait for it
    // This prevents crash from destroying an in-flight async operation
    if (m_future.valid() && m_state == State::Running) {
        try {
            m_future.wait();
        } catch (...) {
            // Absorb — we're in destructor
        }
    }
}

void ChainStep::start(const std::string& url,
                       const std::string& jsonBody,
                       int timeoutMs) {
    if (m_state == State::Running) {
        fprintf(stderr, "[WARN] [ChainStep:%s] Already running\n", m_name.c_str());
        return;
    }

    m_state  = State::Running;
    m_result = {};

    // CRITICAL: m_future MUST be stored as member to keep the async alive
    m_future = StlHttpClient::instance().postJsonAsync(url, jsonBody, timeoutMs);

    fprintf(stderr, "[INFO] [ChainStep:%s] Started async POST %s\n",
            m_name.c_str(), url.c_str());
}

bool ChainStep::poll() {
    if (m_state != State::Running || !m_future.valid()) {
        return m_state == State::Completed || m_state == State::Failed;
    }

    auto status = m_future.wait_for(std::chrono::milliseconds(0));
    if (status == std::future_status::ready) {
        try {
            m_result = m_future.get();
            m_state  = m_result.success ? State::Completed : State::Failed;
        } catch (const std::exception& e) {
            m_result = HttpResponse::fail(std::string("Future exception: ") + e.what());
            m_state  = State::Failed;
        }
        fprintf(stderr, "[INFO] [ChainStep:%s] %s (%dms)\n",
                m_name.c_str(),
                m_state == State::Completed ? "Completed" : "Failed",
                m_result.latencyMs);
        return true;
    }
    return false;
}

bool ChainStep::waitFor(int timeoutMs) {
    if (m_state != State::Running || !m_future.valid()) {
        return m_state == State::Completed;
    }

    if (timeoutMs > 0) {
        auto status = m_future.wait_for(std::chrono::milliseconds(timeoutMs));
        if (status != std::future_status::ready) {
            fprintf(stderr, "[WARN] [ChainStep:%s] Wait timed out after %dms\n",
                    m_name.c_str(), timeoutMs);
            return false;
        }
    } else {
        m_future.wait();
    }

    try {
        m_result = m_future.get();
        m_state  = m_result.success ? State::Completed : State::Failed;
    } catch (const std::exception& e) {
        m_result = HttpResponse::fail(std::string("Future exception: ") + e.what());
        m_state  = State::Failed;
    }
    return m_state == State::Completed;
}

void ChainStep::cancel() {
    m_state = State::Cancelled;
    // Note: the underlying WinHTTP call may still complete, but result is discarded
}

// ============================================================================
// ChainExecutor — sequential step chain
// ============================================================================
ChainExecutor::~ChainExecutor() {
    cancel();
}

void ChainExecutor::addStep(const std::string& name,
                             const std::string& url,
                             const std::string& jsonBody,
                             int timeoutMs) {
    m_pending.push_back({name, url, jsonBody, timeoutMs});
}

bool ChainExecutor::executeAll(
    std::function<std::string(const std::string& prevResult, int stepIndex)> bodyBuilder) {

    m_results.clear();
    m_cancelled.store(false);

    fprintf(stderr, "[INFO] [ChainExecutor] Executing %zu steps\n", m_pending.size());

    std::string prevResult;

    for (size_t i = 0; i < m_pending.size(); ++i) {
        if (m_cancelled.load()) {
            fprintf(stderr, "[WARN] [ChainExecutor] Cancelled at step %zu\n", i);
            return false;
        }

        auto& step = m_pending[i];

        if (onStepStarted) onStepStarted(static_cast<int>(i), step.name);

        // If bodyBuilder is provided, let it modify the request body
        // based on previous step's result
        std::string body = step.jsonBody;
        if (bodyBuilder && i > 0) {
            body = bodyBuilder(prevResult, static_cast<int>(i));
        }

        // Execute via ChainStep with future retention
        ChainStep cs(step.name);
        cs.start(step.url, body, step.timeoutMs);

        // Wait for completion — future stays alive in ChainStep member
        bool ok = cs.waitFor(step.timeoutMs + 5000); // pad timeout for net overhead

        StepResult sr;
        sr.name     = step.name;
        sr.response = cs.result();
        sr.index    = static_cast<int>(i);
        m_results.push_back(sr);

        if (onStepCompleted) onStepCompleted(static_cast<int>(i), sr);

        if (ok) {
            prevResult = sr.response.body;
        } else {
            fprintf(stderr, "[ERROR] [ChainExecutor] Step '%s' failed: %s\n",
                    step.name.c_str(), sr.response.error.c_str());
            return false;
        }
    }

    fprintf(stderr, "[INFO] [ChainExecutor] All %zu steps completed\n", m_pending.size());
    return true;
}

void ChainExecutor::cancel() {
    m_cancelled.store(true);
}
