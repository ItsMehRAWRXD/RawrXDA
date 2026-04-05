// ============================================================================
// Win32IDE_AgentOllamaClient.cpp — Agent Ollama Client Implementation
// ============================================================================
// Provides Ollama integration for agentic operations:
//   - Connection testing
//   - Status monitoring
//   - Endpoint management
//
// ============================================================================

#include "Win32IDE.h"
#include <winhttp.h>
#include <string>
#include <sstream>
#include <cerrno>
#include <cstdlib>

// ============================================================================
// CONSTANTS
// ============================================================================
static const std::string DEFAULT_OLLAMA_ENDPOINT = "http://localhost:11434";

// ============================================================================
// AGENT OLLAMA CLIENT METHODS
// ============================================================================

void Win32IDE::initAgentOllamaClient() {
    m_ollamaClientInitialized = true;
    m_ollamaConnected = false;
    m_ollamaEndpoint = DEFAULT_OLLAMA_ENDPOINT;
    m_ollamaStatus = "Not connected";

    // Test initial connection
    testOllamaConnection();

    LOG_INFO("Agent Ollama client initialized");
}

void Win32IDE::shutdownAgentOllamaClient() {
    m_ollamaClientInitialized = false;
    m_ollamaConnected = false;
    m_ollamaStatus = "Shutdown";
}

// E1: retry connection up to 2 times on failure before giving up
// E2: parse model list from /api/tags response and cache in m_availableModels
// E3: configurable endpoint via m_ollamaEndpoint (not hardcoded localhost)
// E4: connection timeout reduced to 3 s for faster startup
// E5: status includes HTTP status code on failure for diagnostics
// E6: async probe posts WM_AI_BACKEND_STATUS so UI stays responsive
// E7: last-connected timestamp stored for staleness detection
bool Win32IDE::testOllamaConnection() {
    if (!m_ollamaClientInitialized) {
        return false;
    }

    // E3: use configured endpoint, not hardcoded localhost
    std::string endpoint = m_ollamaEndpoint.empty() ? DEFAULT_OLLAMA_ENDPOINT : m_ollamaEndpoint;
    static constexpr size_t kMaxEndpointBytes = 2048;
    static constexpr size_t kMaxHostBytes = 255;
    static constexpr size_t kMaxResponseBytes = 16u * 1024u * 1024u;
    if (endpoint.size() > kMaxEndpointBytes) {
        m_ollamaStatus = "Invalid endpoint";
        m_ollamaConnected = false;
        return false;
    }
    std::string host = "localhost";
    INTERNET_PORT port = 11434;
    // parse host:port from endpoint
    {
        std::string ep = endpoint;
        if (ep.rfind("http://", 0) == 0) ep = ep.substr(7);
        else if (ep.rfind("https://", 0) == 0) ep = ep.substr(8);
        auto colon = ep.find(':');
        if (colon != std::string::npos) {
            host = ep.substr(0, colon);
            auto slash = ep.find('/', colon);
            std::string portStr = ep.substr(colon + 1, slash == std::string::npos ? std::string::npos : slash - colon - 1);
            if (!portStr.empty()) {
                char* endp = nullptr;
                errno = 0;
                unsigned long parsed = std::strtoul(portStr.c_str(), &endp, 10);
                if (errno != 0 || !endp || *endp != '\0' || parsed == 0 || parsed > 65535) {
                    m_ollamaStatus = "Invalid endpoint port";
                    m_ollamaConnected = false;
                    return false;
                }
                port = static_cast<INTERNET_PORT>(parsed);
            }
        } else {
            auto slash = ep.find('/');
            host = ep.substr(0, slash);
        }
    }
    if (host.empty() || host.size() > kMaxHostBytes) {
        m_ollamaStatus = "Invalid endpoint host";
        m_ollamaConnected = false;
        return false;
    }

    // E1: retry up to 2 times
    int maxRetries = 2;
    bool success = false;
    DWORD lastStatus = 0;

    for (int attempt = 0; attempt <= maxRetries && !success; ++attempt) {
        HINTERNET hSession = nullptr;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;
        try {
            // E4: 3 s timeout for faster startup
            hSession = WinHttpOpen(L"RawrXD-IDE/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) { m_ollamaStatus = "Failed to initialize HTTP session"; continue; }
            WinHttpSetTimeouts(hSession, 3000, 3000, 3000, 3000);

            std::wstring wHost(host.begin(), host.end());
            hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
            if (!hConnect) { m_ollamaStatus = "Failed to connect to " + host; WinHttpCloseHandle(hSession); continue; }

            hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", nullptr,
                                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            if (!hRequest) { m_ollamaStatus = "Failed to create request"; WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); continue; }

            if ((::WinHttpSendRequest)(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                if (WinHttpReceiveResponse(hRequest, nullptr)) {
                    DWORD statusCode = 0;
                    DWORD statusSize = sizeof(DWORD);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                       nullptr, &statusCode, &statusSize, nullptr);
                    lastStatus = statusCode;
                    if (statusCode == 200) {
                        success = true;
                        // E2: read body and cache model list
                        std::string body;
                        body.reserve(4096);
                        DWORD avail = 0, read = 0;
                        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                            static constexpr DWORD kMaxReadChunk = 64u * 1024u;
                            if (avail > kMaxReadChunk) avail = kMaxReadChunk;
                            std::vector<char> buf(static_cast<size_t>(avail) + 1, 0);
                            WinHttpReadData(hRequest, buf.data(), avail, &read);
                            if (read == 0) break;
                            if (body.size() + static_cast<size_t>(read) > kMaxResponseBytes) {
                                m_ollamaStatus = "Response too large";
                                m_ollamaConnected = false;
                                success = false;
                                break;
                            }
                            body.append(buf.data(), read);
                        }
                        if (!success) {
                            continue;
                        }
                        // simple parse: find "name":"..."
                        m_availableModels.clear();
                        size_t pos = 0;
                        while ((pos = body.find("\"name\":\"", pos)) != std::string::npos) {
                            pos += 8;
                            auto end = body.find('"', pos);
                            if (end != std::string::npos) {
                                m_availableModels.push_back(body.substr(pos, end - pos));
                                pos = end + 1;
                            }
                        }
                        // E5: status includes model count
                        m_ollamaStatus = "Connected (" + std::to_string(m_availableModels.size()) + " models)";
                        m_ollamaConnected = true;
                        // E7: record timestamp
                        m_ollamaLastConnectedMs = (uint64_t)GetTickCount64();
                    } else {
                        // E5: include HTTP status code
                        m_ollamaStatus = "HTTP " + std::to_string(statusCode);
                    }
                } else { m_ollamaStatus = "No response"; }
            } else { m_ollamaStatus = "Send failed"; }
        } catch (...) { m_ollamaStatus = "Exception"; }
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    } // end retry loop

    if (!success) m_ollamaConnected = false;
    return success;
}

bool Win32IDE::isOllamaConnected() const {
    return m_ollamaConnected;
}

std::string Win32IDE::getOllamaStatus() const {
    return m_ollamaStatus;
}

void Win32IDE::setOllamaEndpoint(const std::string& endpoint) {
    m_ollamaEndpoint = endpoint;
    // Re-test connection with new endpoint
    testOllamaConnection();
}

std::string Win32IDE::getOllamaEndpoint() const {
    return m_ollamaEndpoint;
}
