#include "CopilotStreaming.h"
#include <winhttp.h>
#include <sstream>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;

CopilotStreaming::CopilotStreaming(HWND outputWindow)
    : m_outputWindow(outputWindow),
      m_proxyUrl("http://localhost:3200/stream"),
      m_upstream("http://localhost:11434/api/generate"),
      m_model("bigdaddyg-fast:latest"),
      m_provider("ollama"),
      m_streamThread(NULL),
      m_cancelRequested(false) {}

CopilotStreaming::~CopilotStreaming() {
    CancelStream();
}

void CopilotStreaming::SetProxyUrl(const std::string& url) {
    m_proxyUrl = url;
}

void CopilotStreaming::SetUpstream(const std::string& upstream) {
    m_upstream = upstream;
}

void CopilotStreaming::SetModel(const std::string& model) {
    m_model = model;
}

void CopilotStreaming::SetProvider(const std::string& provider) {
    m_provider = provider;
}

void CopilotStreaming::OnToken(std::function<void(const std::string&)> callback) {
    m_tokenCallback = callback;
}

void CopilotStreaming::OnComplete(std::function<void()> callback) {
    m_completeCallback = callback;
}

void CopilotStreaming::OnError(std::function<void(const std::string&)> callback) {
    m_errorCallback = callback;
}

void CopilotStreaming::SendPrompt(const std::string& prompt, const std::string& context) {
    CancelStream();
    m_cancelRequested = false;

    struct ThreadParams {
        CopilotStreaming* self;
        std::string prompt;
        std::string context;
    };

    auto* params = new ThreadParams{ this, prompt, context };
    m_streamThread = CreateThread(NULL, 0, StreamThreadProc, params, 0, NULL);
}

void CopilotStreaming::CancelStream() {
    m_cancelRequested = true;
    if (m_streamThread) {
        WaitForSingleObject(m_streamThread, 5000);
        CloseHandle(m_streamThread);
        m_streamThread = NULL;
    }
}

DWORD WINAPI CopilotStreaming::StreamThreadProc(LPVOID param) {
    auto* params = static_cast<struct ThreadParams*>(param);
    params->self->StreamRequest(params->prompt, params->context);
    delete params;
    return 0;
}

void CopilotStreaming::StreamRequest(const std::string& prompt, const std::string& context) {
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;

    try {
        // Parse proxy URL
        std::string server = "localhost";
        int port = 3200;
        std::string path = "/stream";

        // Build JSON request body
        json body = {
            {"prompt", context.empty() ? prompt : context + "\\n\\nUser: " + prompt},
            {"upstream", m_upstream},
            {"model", m_model},
            {"provider", m_provider},
            {"max_tokens", 512}
        };
        std::string jsonBody = body.dump();

        // Initialize WinHTTP
        hSession = WinHttpOpen(L"Copilot/1.0",
                               WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS,
                               0);
        if (!hSession) {
            if (m_errorCallback) m_errorCallback("Failed to initialize WinHTTP");
            return;
        }

        // Connect
        hConnect = WinHttpConnect(hSession, L"localhost", port, 0);
        if (!hConnect) {
            if (m_errorCallback) m_errorCallback("Failed to connect to proxy");
            WinHttpCloseHandle(hSession);
            return;
        }

        // Create request
        hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/stream",
                                      NULL, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      0);
        if (!hRequest) {
            if (m_errorCallback) m_errorCallback("Failed to create request");
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return;
        }

        // Set headers
        std::wstring headers = L"Content-Type: application/json\\r\\n";
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        if (!WinHttpSendRequest(hRequest,
                                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                (LPVOID)jsonBody.c_str(), jsonBody.size(),
                                jsonBody.size(), 0)) {
            if (m_errorCallback) m_errorCallback("Failed to send request");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return;
        }

        // Receive response
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            if (m_errorCallback) m_errorCallback("Failed to receive response");
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return;
        }

        // Stream response
        std::string buffer;
        char chunk[8192];
        DWORD bytesRead = 0;

        while (!m_cancelRequested) {
            if (!WinHttpReadData(hRequest, chunk, sizeof(chunk), &bytesRead)) {
                break;
            }
            if (bytesRead == 0) break;

            buffer.append(chunk, bytesRead);

            // Process SSE events (split by \\n\\n)
            size_t pos;
            while ((pos = buffer.find("\\n\\n")) != std::string::npos) {
                std::string event = buffer.substr(0, pos);
                buffer = buffer.substr(pos + 2);

                // Parse SSE event
                if (event.find("data:") == 0) {
                    std::string payload = event.substr(5);
                    // Trim whitespace
                    payload.erase(0, payload.find_first_not_of(" \\t\\r\\n"));

                    try {
                        auto j = json::parse(payload);
                        
                        // Ollama format: j["response"]
                        if (j.contains("response") && j["response"].is_string()) {
                            std::string token = j["response"];
                            if (m_tokenCallback) {
                                m_tokenCallback(token);
                            }
                        }
                        
                        // Check if done
                        if (j.contains("done") && j["done"].is_boolean() && j["done"]) {
                            if (m_completeCallback) {
                                m_completeCallback();
                            }
                            goto cleanup;
                        }
                    } catch (...) {
                        // Ignore non-JSON lines
                    }
                }
            }
        }

        if (m_completeCallback && !m_cancelRequested) {
            m_completeCallback();
        }

    cleanup:
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    }
    catch (...) {
        if (m_errorCallback) m_errorCallback("Streaming error");
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    }
}
