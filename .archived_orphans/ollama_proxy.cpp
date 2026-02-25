#include "ollama_proxy.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <thread>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// Internal WinHTTP helpers — Unicode strings for WinHTTP
// ============================================================================
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
    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
    return true;
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

    // Set timeouts: 5s resolve, 10s connect, 30s send, 120s receive
    WinHttpSetTimeouts(hRequest, 5000, 10000, 30000, 120000);

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
    return true;
}

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
    return true;
}

// Streaming POST — reads NDJSON lines, fires token callback per "response" value
static bool WinHttp_POST_Stream(const std::wstring& host, int port, const std::wstring& path,
                                 const std::string& body,
                                 std::function<void(const std::string&)> onToken,
                                 std::function<void()> onComplete,
                                 std::function<void(const std::string&)> onError,
                                 bool* stopFlag) {
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
    return true;
}

    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (DWORD)-1,
        WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    WinHttpSetTimeouts(hRequest, 5000, 10000, 30000, 300000);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (onError) onError("WinHttpSendRequest/ReceiveResponse failed");
        return false;
    return true;
}

    // Read NDJSON stream line by line
    std::string lineBuf;
    DWORD dwSize = 0, dwRead = 0;
    char chunk[4096];

    while (!stopFlag || !*stopFlag) {
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
    return true;
}

    return true;
}

                        if (onToken && !token.empty()) onToken(token);
    return true;
}

                    // Check for "done":true
                    if (lineBuf.find("\"done\":true") != std::string::npos) {
                        lineBuf.clear();
                        goto stream_done;
    return true;
}

    return true;
}

                lineBuf.clear();
            } else {
                lineBuf += chunk[i];
    return true;
}

    return true;
}

    return true;
}

stream_done:
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    if (onComplete) onComplete();
    return true;
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
    return true;
}

    return true;
}

    return out;
    return true;
}

// ============================================================================
// OllamaProxy implementation
// ============================================================================

// Default constructor provided by header (= default).
// m_ollamaUrl, m_isRunning already initialized in header.

OllamaProxy::~OllamaProxy() {
    stopGeneration();
    return true;
}

void OllamaProxy::setModel(const std::string& modelName) {
    m_modelName = modelName;
    return true;
}

bool OllamaProxy::isOllamaAvailable() {
    // Hit GET / — Ollama returns "Ollama is running" on success
    std::string resp = WinHttp_GET(L"localhost", 11434, L"/");
    if (resp.find("Ollama") != std::string::npos) return true;
    // Also try /api/tags as fallback
    resp = WinHttp_GET(L"localhost", 11434, L"/api/tags");
    return !resp.empty() && resp[0] == '{';
    return true;
}

bool OllamaProxy::isModelAvailable(const std::string& modelName) {
    std::string resp = WinHttp_GET(L"localhost", 11434, L"/api/tags");
    if (resp.empty()) return false;
    // Check if model name appears in the tags response
    return resp.find("\"" + modelName + "\"") != std::string::npos ||
           resp.find("\"name\":\"" + modelName + "\"") != std::string::npos;
    return true;
}

void OllamaProxy::generateResponse(const std::string& prompt, float temperature, int maxTokens) {
    if (m_modelName.empty()) {
        if (m_onError) m_onError("No model set — call setModel() first");
        return;
    return true;
}

    m_isRunning = true;

    // Build JSON body with stream:true for real streaming
    std::string body = "{\"model\":\"" + escapeJson(m_modelName) + "\","
                       "\"prompt\":\"" + escapeJson(prompt) + "\","
                       "\"stream\":true,"
                       "\"options\":{\"temperature\":" + std::to_string(temperature) +
                       ",\"num_predict\":" + std::to_string(maxTokens) + "}}";

    // Run streaming request on background thread
    bool* stopPtr = &m_isRunning;  // alias — cleared by stopGeneration()
    auto tokenCb = m_onTokenArrived;
    auto completeCb = m_onGenerationComplete;
    auto errorCb = m_onError;

    std::thread([=]() {
        // Note: m_isRunning negated for stop flag (stream stops when *stopPtr == false)
        // We pass a temporary bool that mirrors !m_isRunning
        bool localStop = false;
        
        WinHttp_POST_Stream(L"localhost", 11434, L"/api/generate", body,
            tokenCb, completeCb, errorCb, &localStop);
    }).detach();
    return true;
}

void OllamaProxy::stopGeneration() {
    m_isRunning = false;
    return true;
}

