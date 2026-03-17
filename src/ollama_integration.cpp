#include "ollama_integration.h"
#include <winsock2.h>
#include <winhttp.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;

namespace OllamaIntegration {

// Global state for async suggester
static std::thread g_suggester_thread;
static bool g_suggester_running = false;
static std::mutex g_suggester_mutex;

//==============================================================================
// HTTP CLIENT IMPLEMENTATION
//==============================================================================

CompletionResponse QueryCompletion(const CompletionRequest& req) {
    CompletionResponse result;
    result.success = false;

    try {
        // Initialize WinHTTP
        HINTERNET hSession = WinHttpOpen(
            L"RawrXD_IDE/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession) {
            result.error_message = "Failed to create WinHTTP session";
            return result;
        }

        // Connect to Ollama
        HINTERNET hConnect = WinHttpConnect(
            hSession,
            L"localhost",
            11434,
            0);

        if (!hConnect) {
            result.error_message = "Failed to connect to Ollama (localhost:11434 unreachable)";
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Create HTTP request
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            L"/api/generate",
            NULL,
            WINHTTP_NO_REFERER,
            NULL,
            WINHTTP_FLAG_REFRESH);

        if (!hRequest) {
            result.error_message = "Failed to create HTTP request";
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Add headers
        if (!WinHttpAddRequestHeaders(
            hRequest,
            L"Content-Type: application/json",
            (ULONG)-1L,
            WINHTTP_ADDREQ_FLAG_ADD)) {
            result.error_message = "Failed to add HTTP headers";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Build JSON request
        json request_json;
        request_json["model"] = req.model;
        request_json["prompt"] = req.prompt;
        request_json["temperature"] = req.temperature;
        request_json["top_p"] = req.top_p;
        request_json["num_predict"] = req.num_predict;
        request_json["stream"] = req.stream;

        std::string body = request_json.dump();

        // Send request
        if (!WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            (LPVOID)body.c_str(),
            (DWORD)body.length(),
            (DWORD)body.length(),
            0)) {
            result.error_message = "Failed to send HTTP request";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Receive response
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            result.error_message = "Failed to receive HTTP response";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Get HTTP status code
        DWORD status_code = 0;
        DWORD status_size = sizeof(status_code);
        if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            NULL,
            &status_code,
            &status_size,
            NULL)) {
            result.error_message = "Failed to query HTTP status";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        if (status_code != 200) {
            char error_str[256];
            sprintf_s(error_str, "HTTP Error: %lu", status_code);
            result.error_message = error_str;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return result;
        }

        // Read response body
        std::string response_body;
        DWORD bytes_available = 0;

        while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
            std::vector<char> buffer(bytes_available + 1, 0);
            DWORD bytes_read = 0;

            if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
                response_body.append(buffer.data(), bytes_read);
            } else {
                break;
            }
        }

        // Parse response JSON
        if (!response_body.empty()) {
            try {
                json response = json::parse(response_body);
                if (response.contains("response")) {
                    result.text = response["response"];
                    result.success = true;
                }
            } catch (const std::exception& e) {
                result.error_message = std::string("JSON parse error: ") + e.what();
            }
        }

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
    }

    return result;
}

//==============================================================================
// CONNECTIVITY CHECK
//==============================================================================

bool IsOllamaAvailable(const std::string& host, int port) {
    try {
        std::string host_wide_temp = host;
        std::wstring host_wide(host_wide_temp.begin(), host_wide_temp.end());

        HINTERNET hSession = WinHttpOpen(
            L"RawrXD_IDE/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(
            hSession,
            host_wide.c_str(),
            port,
            0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            L"/api/tags",
            NULL,
            WINHTTP_NO_REFERER,
            NULL,
            0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        BOOL success = WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_BODY,
            0,
            0,
            0);

        if (success) {
            success = WinHttpReceiveResponse(hRequest, NULL);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return success ? true : false;

    } catch (...) {
        return false;
    }
}

//==============================================================================
// MODEL LIST RETRIEVAL
//==============================================================================

std::vector<std::string> GetAvailableModels() {
    std::vector<std::string> models;

    try {
        HINTERNET hSession = WinHttpOpen(
            L"RawrXD_IDE/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!hSession) return models;

        HINTERNET hConnect = WinHttpConnect(
            hSession,
            L"localhost",
            11434,
            0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return models;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            L"/api/tags",
            NULL,
            WINHTTP_NO_REFERER,
            NULL,
            0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return models;
        }

        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_BODY, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {

            std::string response_body;
            DWORD bytes_available = 0;

            while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
                std::vector<char> buffer(bytes_available + 1, 0);
                DWORD bytes_read = 0;

                if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read)) {
                    response_body.append(buffer.data(), bytes_read);
                }
            }

            if (!response_body.empty()) {
                try {
                    json response = json::parse(response_body);
                    if (response.contains("models") && response["models"].is_array()) {
                        for (const auto& model : response["models"]) {
                            if (model.contains("name")) {
                                models.push_back(model["name"]);
                            }
                        }
                    }
                } catch (...) {
                    // Silent fail
                }
            }
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

    } catch (...) {
        // Silent fail
    }

    return models;
}

//==============================================================================
// ASYNC SUGGESTER
//==============================================================================

void StartAsyncSuggester(std::function<void(const std::string&)> on_suggestion) {
    std::lock_guard<std::mutex> lock(g_suggester_mutex);
    if (g_suggester_running) return;

    g_suggester_running = true;
    // Placeholder: would implement background thread for real-time suggestions
}

void StopAsyncSuggester() {
    std::lock_guard<std::mutex> lock(g_suggester_mutex);
    g_suggester_running = false;
    if (g_suggester_thread.joinable()) {
        g_suggester_thread.join();
    }
}

} // namespace OllamaIntegration
