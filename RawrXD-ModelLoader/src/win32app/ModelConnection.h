// ============================================================================
// Model Connection Handler - HTTP Communication with Model Loader
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// RESPONSE CALLBACK TYPES
// ============================================================================

typedef std::function<void(const std::string&)> ResponseCallback;
typedef std::function<void(const std::string&)> ErrorCallback;
typedef std::function<void()> CompleteCallback;

// ============================================================================
// MODEL CONNECTION CLASS
// ============================================================================

class ModelConnection
{
public:
    ModelConnection(const std::string& endpoint = "http://localhost:11434")
        : m_endpoint(endpoint), m_connected(false), m_isProcessing(false),
          m_workerThread(nullptr), m_stopWorker(false)
    {
        // Start worker thread for background I/O
        m_workerThread = new std::thread(&ModelConnection::workerLoop, this);
    }

    ~ModelConnection()
    {
        shutdown();
        if (m_workerThread) {
            m_workerThread->join();
            delete m_workerThread;
        }
    }

    // Check connection to model endpoint
    bool checkConnection()
    {
        // Try to reach /api/version endpoint
        HINTERNET hSession = WinHttpOpen(L"RawrXD-Chat/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) {
            m_connected = false;
            return false;
        }

        // Parse endpoint
        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);

        std::wstring endpoint_wide(m_endpoint.begin(), m_endpoint.end());
        wchar_t host[256] = L"";
        wchar_t path[256] = L"/api/version";

        urlComp.lpszHostName = host;
        urlComp.dwHostNameLength = sizeof(host) / sizeof(host[0]);
        urlComp.lpszUrlPath = path;
        urlComp.dwUrlPathLength = sizeof(path) / sizeof(path[0]);

        if (!WinHttpCrackUrl(endpoint_wide.c_str(), 0, 0, &urlComp)) {
            WinHttpCloseHandle(hSession);
            m_connected = false;
            return false;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName,
            urlComp.nPort, 0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            m_connected = false;
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
            urlComp.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            m_connected = false;
            return false;
        }

        BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
            0, WINHTTP_NO_REQUEST_BODY, 0, 0, 0);

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        m_connected = bResults;
        return bResults;
    }

    // Send prompt to model (non-blocking)
    void sendPrompt(const std::string& model, const std::string& prompt,
                    const std::vector<std::string>& context = {},
                    ResponseCallback onResponse = nullptr,
                    ErrorCallback onError = nullptr,
                    CompleteCallback onComplete = nullptr)
    {
        if (m_isProcessing) {
            if (onError) {
                onError("Already processing a request");
            }
            return;
        }

        // Queue request
        Request req;
        req.model = model;
        req.prompt = prompt;
        req.context = context;
        req.onResponse = onResponse;
        req.onError = onError;
        req.onComplete = onComplete;

        {
            std::lock_guard<std::mutex> lock(m_requestMutex);
            m_requestQueue.push(req);
        }

        m_requestCV.notify_one();
    }

    // Get list of available models
    std::vector<std::string> getAvailableModels()
    {
        std::vector<std::string> models;
        // TODO: Implement model list retrieval
        return models;
    }

    // Check if connection active
    bool isConnected() const
    {
        return m_connected;
    }

    // Check if currently processing
    bool isProcessing() const
    {
        return m_isProcessing;
    }

    // Set custom endpoint
    void setEndpoint(const std::string& endpoint)
    {
        m_endpoint = endpoint;
        m_connected = false;
    }

    // Get current endpoint
    std::string getEndpoint() const
    {
        return m_endpoint;
    }

    // Shutdown connection
    void shutdown()
    {
        m_stopWorker = true;
        m_requestCV.notify_one();
    }

private:
    struct Request {
        std::string model;
        std::string prompt;
        std::vector<std::string> context;
        ResponseCallback onResponse;
        ErrorCallback onError;
        CompleteCallback onComplete;
    };

    // Worker thread loop
    void workerLoop()
    {
        while (!m_stopWorker) {
            Request req;

            {
                std::unique_lock<std::mutex> lock(m_requestMutex);
                m_requestCV.wait(lock, [this] { return !m_requestQueue.empty() || m_stopWorker; });

                if (m_stopWorker || m_requestQueue.empty()) {
                    continue;
                }

                req = m_requestQueue.front();
                m_requestQueue.pop();
            }

            // Process request
            m_isProcessing = true;
            processRequest(req);
            m_isProcessing = false;
        }
    }

    // Process a single request
    void processRequest(const Request& req)
    {
        // Build prompt with context
        std::string fullPrompt = buildContextPrompt(req.prompt, req.context);

        // Build JSON payload (with streaming enabled)
        std::string payload = buildPayload(req.model, fullPrompt);

        // Send HTTP POST request
        HINTERNET hSession = WinHttpOpen(L"RawrXD-Chat/1.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) {
            if (req.onError) req.onError("Failed to open session");
            return;
        }

        // Parse endpoint
        std::wstring endpoint_wide(m_endpoint.begin(), m_endpoint.end());
        HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            if (req.onError) req.onError("Failed to connect to model endpoint");
            return;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
            L"/api/generate", NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (req.onError) req.onError("Failed to open request");
            return;
        }

        // Set headers
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1,
            WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        BOOL bResults = WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            (LPVOID)payload.c_str(), (DWORD)payload.length(),
            (DWORD)payload.length(), 0);

        if (!bResults) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            if (req.onError) req.onError("Failed to send request");
            return;
        }

        // Receive and process response stream
        bResults = WinHttpReceiveResponse(hRequest, NULL);

        if (bResults) {
            DWORD dwSize = 0;
            std::string lineBuffer;      // Buffer for partial lines
            std::string fullResponseText; // Accumulate complete response text

            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    if (req.onError) req.onError("Error in WinHttpQueryDataAvailable");
                    break;
                }

                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1);
                DWORD dwRead = 0;

                if (!WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwRead)) {
                    if (req.onError) req.onError("Error in WinHttpReadData");
                    break;
                }

                buffer[dwRead] = '\0';
                lineBuffer.append(buffer.data(), dwRead);

                // Process the line buffer to extract complete JSON lines (newline-delimited JSON)
                size_t newlinePos = lineBuffer.find('\n');
                while (newlinePos != std::string::npos) {
                    std::string jsonLine = lineBuffer.substr(0, newlinePos);

                    // Parse the individual JSON line
                    try {
                        auto jsonObject = json::parse(jsonLine);
                        if (jsonObject.contains("response")) {
                            std::string responseChunk = jsonObject["response"].get<std::string>();

                            // Call the user's callback immediately for this chunk of text
                            if (req.onResponse && !responseChunk.empty()) {
                                req.onResponse(responseChunk);
                                fullResponseText += responseChunk;
                            }
                        }
                    } catch (const json::exception& e) {
                        // Handle potential parsing errors if a line is incomplete or invalid
                        std::cerr << "JSON parse error: " << e.what() << std::endl;
                    }

                    // Remove the processed line from the buffer
                    lineBuffer.erase(0, newlinePos + 1);
                    newlinePos = lineBuffer.find('\n');
                }

            } while (dwSize > 0);

            // At this point, fullResponseText holds the complete response

        } else {
            if (req.onError) req.onError("Failed to receive response");
        }

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Call completion callback
        if (req.onComplete) {
            req.onComplete();
        }
    }

    // Build context-aware prompt
    std::string buildContextPrompt(const std::string& prompt,
                                    const std::vector<std::string>& context)
    {
        std::string result;

        // Add context if provided
        if (!context.empty()) {
            result += "Context:\n";
            for (const auto& ctx : context) {
                result += "- " + ctx + "\n";
            }
            result += "\n";
        }

        result += "Prompt: " + prompt;
        return result;
    }

    // Build JSON payload for API
    std::string buildPayload(const std::string& model, const std::string& prompt)
    {
        // Use nlohmann/json for robust JSON building with proper escaping
        json payload = {
            {"model", model},
            {"prompt", prompt},
            {"stream", true}  // Enable streaming for real-time responses
        };
        return payload.dump();
    }

private:
    // Member variables
    std::string m_endpoint;
    bool m_connected;
    bool m_isProcessing;
    bool m_stopWorker;
    std::thread* m_workerThread;
    std::queue<Request> m_requestQueue;
    std::mutex m_requestMutex;
    std::condition_variable m_requestCV;
};
