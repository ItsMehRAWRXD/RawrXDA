#include "ollama_proxy.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <atomic>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// Internal WinHTTP helpers — Unicode strings for WinHTTP
// ============================================================================
static void ParseOllamaUrl(const std::string& url, std::wstring& hostOut, int& portOut) {
    std::string u = url;
    if (u.empty()) u = "http://127.0.0.1:11434";

    // Strip scheme
    auto scheme = u.find("://");
    if (scheme != std::string::npos) u = u.substr(scheme + 3);
    // Strip path
    auto slash = u.find('/');
    if (slash != std::string::npos) u = u.substr(0, slash);
    // Strip trailing slash
    while (!u.empty() && u.back() == '/') u.pop_back();

    std::string hostStr = u;
    int port = 11434;
    auto colon = u.rfind(':');
    if (colon != std::string::npos) {
        hostStr = u.substr(0, colon);
        try { port = std::stoi(u.substr(colon + 1)); } catch (...) { port = 11434; }
    }
    if (hostStr.empty()) hostStr = "127.0.0.1";
    if (hostStr == "localhost") hostStr = "127.0.0.1";
    portOut = port;

    int wlen = MultiByteToWideChar(CP_UTF8, 0, hostStr.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        hostOut = L"127.0.0.1";
        portOut = 11434;
        return;
    }
    hostOut.resize((size_t)wlen - 1);
    MultiByteToWideChar(CP_UTF8, 0, hostStr.c_str(), -1, hostOut.data(), wlen);
}

static std::string WinHttp_GET(const std::wstring& host, int port, const std::wstring& path) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD-OllamaProxy/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwSize = 0, dwRead = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
            std::vector<char> buf(dwSize + 1);
            if (WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead))
                response.append(buf.data(), dwRead);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static std::string WinHttp_POST(const std::wstring& host, int port, const std::wstring& path,
                                 const std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD-OllamaProxy/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    // Add content-type header
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

    // Timeouts: allow long cold-loads on first request.
    WinHttpSetTimeouts(hRequest, 5000, 15000, 30000, 600000);

    std::string response;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD dwSize = 0, dwRead = 0;
        do {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
            std::vector<char> buf(dwSize + 1);
            if (WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead))
                response.append(buf.data(), dwRead);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

// Streaming POST — reads NDJSON lines, fires token callback per "response" value
static bool WinHttp_POST_Stream(const std::wstring& host, int port, const std::wstring& path,
                                 const std::string& body,
                                 std::function<void(const std::string&)> onToken,
                                 std::function<void()> onComplete,
                                 std::function<void(const std::string&)> onError,
                                 std::atomic_bool* runningFlag) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD-OllamaProxy/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { if (onError) onError("WinHttpOpen failed"); return false; }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); if (onError) onError("WinHttpConnect failed"); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (onError) onError("WinHttpOpenRequest failed"); return false;
    }

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    // Streaming: long receive timeout to avoid cutting off long generations.
    // (If the model emits no tokens for a while, WinHTTP receive timeout is what kills the stream.)
    WinHttpSetTimeouts(hRequest, 5000, 15000, 30000, 3600000);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (onError) onError("WinHttpSendRequest/ReceiveResponse failed");
        return false;
    }

    // Read NDJSON stream line by line
    std::string lineBuf;
    DWORD dwSize = 0, dwRead = 0;
    char chunk[4096];

    while (!runningFlag || runningFlag->load(std::memory_order_relaxed)) {
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
        DWORD toRead = (dwSize < sizeof(chunk)) ? dwSize : sizeof(chunk) - 1;
        if (!WinHttpReadData(hRequest, chunk, toRead, &dwRead) || dwRead == 0) break;

        for (DWORD i = 0; i < dwRead; i++) {
            if (chunk[i] == '\n') {
                // Process complete NDJSON line
                if (!lineBuf.empty()) {
                    // Extract "response":"..." value
                    size_t rpos = lineBuf.find("\"response\":\"");
                    if (rpos != std::string::npos) {
                        rpos += 12;
                        std::string token;
                        for (size_t j = rpos; j < lineBuf.size(); j++) {
                            if (lineBuf[j] == '\\' && j + 1 < lineBuf.size()) {
                                char esc = lineBuf[j + 1];
                                if (esc == '"') { token += '"'; j++; }
                                else if (esc == '\\') { token += '\\'; j++; }
                                else if (esc == 'n') { token += '\n'; j++; }
                                else if (esc == 't') { token += '\t'; j++; }
                                else if (esc == 'r') { token += '\r'; j++; }
                                else { token += lineBuf[j]; }
                            } else if (lineBuf[j] == '"') {
                                break; // end of response string
                            } else {
                                token += lineBuf[j];
                            }
                        }
                        if (onToken && !token.empty()) onToken(token);
                    }
                    // Check for "done":true
                    if (lineBuf.find("\"done\":true") != std::string::npos) {
                        lineBuf.clear();
                        goto stream_done;
                    }
                }
                lineBuf.clear();
            } else {
                lineBuf += chunk[i];
            }
        }
    }

stream_done:
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    if (onComplete) onComplete();
    return true;
}

// Helper: escape JSON string
static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// ============================================================================
// OllamaProxy implementation
// ============================================================================

// Default constructor provided by header (= default).
// m_ollamaUrl, m_isRunning already initialized in header.

OllamaProxy::~OllamaProxy() {
    stopGeneration();
}

void OllamaProxy::setModel(const std::string& modelName) {
    m_modelName = modelName;
}

bool OllamaProxy::isOllamaAvailable() {
    // Hit GET / — Ollama returns "Ollama is running" on success
    std::wstring host;
    int port = 11434;
    ParseOllamaUrl(m_ollamaUrl, host, port);
    std::string resp = WinHttp_GET(host, port, L"/");
    if (resp.find("Ollama") != std::string::npos) return true;
    // Also try /api/tags as fallback
    resp = WinHttp_GET(host, port, L"/api/tags");
    return !resp.empty() && resp[0] == '{';
}

bool OllamaProxy::isModelAvailable(const std::string& modelName) {
    std::wstring host;
    int port = 11434;
    ParseOllamaUrl(m_ollamaUrl, host, port);
    std::string resp = WinHttp_GET(host, port, L"/api/tags");
    if (resp.empty()) return false;
    // Check if model name appears in the tags response
    return resp.find("\"" + modelName + "\"") != std::string::npos ||
           resp.find("\"name\":\"" + modelName + "\"") != std::string::npos;
}

void OllamaProxy::generateResponse(const std::string& prompt, float temperature, int maxTokens) {
    if (m_modelName.empty()) {
        if (m_onError) m_onError("No model set — call setModel() first");
        return;
    }

    m_isRunning.store(true, std::memory_order_relaxed);

    // Build JSON body with stream:true for real streaming
    std::string body = "{\"model\":\"" + escapeJson(m_modelName) + "\","
                       "\"prompt\":\"" + escapeJson(prompt) + "\","
                       "\"stream\":true,"
                       "\"options\":{\"temperature\":" + std::to_string(temperature) +
                       ",\"num_predict\":" + std::to_string(maxTokens) + "}}";

    // Run streaming request on background thread
    auto tokenCb = m_onTokenArrived;
    auto completeCb = m_onGenerationComplete;
    auto errorCb = m_onError;

    std::wstring host;
    int port = 11434;
    ParseOllamaUrl(m_ollamaUrl, host, port);

    std::thread([this, host, port, body, tokenCb, completeCb, errorCb]() {
        WinHttp_POST_Stream(host, port, L"/api/generate", body,
            tokenCb, completeCb, errorCb, &this->m_isRunning);
    }).detach();
}

void OllamaProxy::stopGeneration() {
    m_isRunning.store(false, std::memory_order_relaxed);
}
