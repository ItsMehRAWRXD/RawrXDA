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

bool Win32IDE::testOllamaConnection() {
    if (!m_ollamaClientInitialized) {
        return false;
    }

    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    bool success = false;

    try {
        // Initialize WinHTTP
        hSession = WinHttpOpen(L"RawrXD-IDE/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            m_ollamaStatus = "Failed to initialize HTTP session";
            return false;
        }

        // Connect to Ollama server
        hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
        if (!hConnect) {
            m_ollamaStatus = "Failed to connect to localhost:11434";
            return false;
        }

        // Create request
        hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags", nullptr,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            m_ollamaStatus = "Failed to create HTTP request";
            return false;
        }

        // Send request
        if ((::WinHttpSendRequest)(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            if (WinHttpReceiveResponse(hRequest, nullptr)) {
                DWORD statusCode = 0;
                DWORD statusSize = sizeof(DWORD);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   nullptr, &statusCode, &statusSize, nullptr);

                if (statusCode == 200) {
                    success = true;
                    m_ollamaStatus = "Connected";
                    m_ollamaConnected = true;
                } else {
                    m_ollamaStatus = "HTTP " + std::to_string(statusCode);
                }
            } else {
                m_ollamaStatus = "No response from server";
            }
        } else {
            m_ollamaStatus = "Failed to send request";
        }

    } catch (...) {
        m_ollamaStatus = "Exception during connection test";
    }

    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);

    if (!success) {
        m_ollamaConnected = false;
    }

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
