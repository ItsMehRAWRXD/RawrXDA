/**
 * @file win_http_client.cpp
 * @brief WinHTTP-based HTTP client for RawrXD IDE
 * 
 * Provides HTTP client functionality using Windows WinHTTP API.
 * No external dependencies (libcurl not required).
 * 
 * Features:
 * - Synchronous GET/POST requests
 * - Streaming response support with chunked callbacks
 * - Connection timeout handling
 * - HTTPS support (system TLS)
 */

#include "ai_implementation.h"
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <mutex>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

//=============================================================================
// URL parsing helper
//=============================================================================
struct ParsedURL {
    std::wstring scheme;
    std::wstring host;
    INTERNET_PORT port = 80;
    std::wstring path;
    bool https = false;
    bool valid = false;
};

static std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

static std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

static ParsedURL parseURL(const std::string& url) {
    ParsedURL result;
    std::wstring wurl = utf8_to_wstring(url);
    
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;
    
    if (WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &urlComp)) {
        result.scheme = std::wstring(urlComp.lpszScheme, urlComp.dwSchemeLength);
        result.host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
        result.port = urlComp.nPort;
        result.path = std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        if (urlComp.dwExtraInfoLength > 0) {
            result.path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        }
        result.https = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        result.valid = true;
    }
    return result;
}

//=============================================================================
// WinHTTP Client implementation
//=============================================================================
class WinHTTPClientImpl : public HTTPClient {
public:
    WinHTTPClientImpl() : m_timeoutConnect(10000), m_timeoutSend(30000), m_timeoutReceive(60000) {}
    ~WinHTTPClientImpl() override = default;

    void setTimeout(int connectMs, int sendMs, int receiveMs) {
        m_timeoutConnect = connectMs;
        m_timeoutSend = sendMs;
        m_timeoutReceive = receiveMs;
    }

    HTTPResponse sendRequest(const HTTPRequest& request) override {
        return sendRequestImpl(request, nullptr);
    }

    HTTPResponse sendRequest(const HTTPRequest& request,
                             std::function<void(const std::string&)> chunkCallback) override {
        return sendRequestImpl(request, chunkCallback);
    }

    HTTPResponse get(const std::string& url) override {
        HTTPRequest req;
        req.method = "GET";
        req.url = url;
        return sendRequest(req);
    }

private:
    int m_timeoutConnect;
    int m_timeoutSend;
    int m_timeoutReceive;
    std::mutex m_mutex;

    HTTPResponse sendRequestImpl(const HTTPRequest& request,
                                 std::function<void(const std::string&)> chunkCallback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        HTTPResponse response;
        
        ParsedURL parsed = parseURL(request.url);
        if (!parsed.valid) {
            response.errorMessage = "Invalid URL: " + request.url;
            return response;
        }

        // Open WinHTTP session
        HINTERNET hSession = WinHttpOpen(L"RawrXD-HTTPClient/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);
        if (!hSession) {
            response.errorMessage = "WinHttpOpen failed: " + std::to_string(GetLastError());
            return response;
        }

        // Connect to server
        HINTERNET hConnect = WinHttpConnect(hSession, parsed.host.c_str(), parsed.port, 0);
        if (!hConnect) {
            response.errorMessage = "WinHttpConnect failed: " + std::to_string(GetLastError());
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Create request
        std::wstring wmethod = utf8_to_wstring(request.method);
        std::wstring wpath = parsed.path.empty() ? L"/" : parsed.path;
        
        DWORD flags = parsed.https ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod.c_str(), wpath.c_str(),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest) {
            response.errorMessage = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Set timeouts
        WinHttpSetTimeouts(hRequest, m_timeoutConnect, m_timeoutConnect, 
                          m_timeoutSend, m_timeoutReceive);

        // Build headers
        std::wstring headerString;
        for (const auto& [name, value] : request.headers) {
            headerString += utf8_to_wstring(name) + L": " + utf8_to_wstring(value) + L"\r\n";
        }

        // Add Content-Type if not present and we have a body
        if (!request.body.empty()) {
            bool hasContentType = false;
            for (const auto& [name, value] : request.headers) {
                if (_stricmp(name.c_str(), "Content-Type") == 0) {
                    hasContentType = true;
                    break;
                }
            }
            if (!hasContentType) {
                headerString += L"Content-Type: application/json\r\n";
            }
        }

        // Send request
        BOOL sendResult;
        if (!request.body.empty()) {
            sendResult = WinHttpSendRequest(hRequest,
                                           headerString.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerString.c_str(),
                                           headerString.empty() ? 0 : -1L,
                                           (LPVOID)request.body.c_str(),
                                           (DWORD)request.body.size(),
                                           (DWORD)request.body.size(),
                                           0);
        } else {
            sendResult = WinHttpSendRequest(hRequest,
                                           headerString.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerString.c_str(),
                                           headerString.empty() ? 0 : -1L,
                                           WINHTTP_NO_REQUEST_DATA,
                                           0, 0, 0);
        }

        if (!sendResult) {
            DWORD err = GetLastError();
            response.errorMessage = "WinHttpSendRequest failed: " + std::to_string(err);
            if (err == ERROR_WINHTTP_CANNOT_CONNECT) {
                response.errorMessage += " (Cannot connect to server)";
            }
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Receive response
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            response.errorMessage = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Get status code
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode, &statusSize,
                           WINHTTP_NO_HEADER_INDEX);
        response.statusCode = (int)statusCode;

        // Read response body
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        std::string responseBody;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                break;
            }
            if (dwSize == 0) {
                break;
            }

            std::vector<char> buffer(dwSize + 1, 0);
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                std::string chunk(buffer.data(), dwDownloaded);
                responseBody.append(chunk);
                
                // Invoke streaming callback if provided
                if (chunkCallback) {
                    chunkCallback(chunk);
                }
            }
        } while (dwSize > 0);

        response.body = responseBody;
        response.success = (statusCode >= 200 && statusCode < 300);

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        return response;
    }
};

//=============================================================================
// Factory function to create HTTP client
//=============================================================================
std::shared_ptr<HTTPClient> CreateWinHTTPClient() {
    return std::make_shared<WinHTTPClientImpl>();
}

} // namespace RawrXD

//=============================================================================
// Global HTTP client instance for ai_implementation.cpp
//=============================================================================
namespace {
    std::shared_ptr<RawrXD::WinHTTPClientImpl> g_httpClient;
    std::once_flag g_httpInitFlag;
}

HTTPResponse WinHTTPClient::sendRequest(const HTTPRequest& request) {
    std::call_once(g_httpInitFlag, []() {
        g_httpClient = std::make_shared<RawrXD::WinHTTPClientImpl>();
    });
    return g_httpClient->sendRequest(request);
}

HTTPResponse WinHTTPClient::sendRequest(const HTTPRequest& request,
                                        std::function<void(const std::string&)> chunkCallback) {
    std::call_once(g_httpInitFlag, []() {
        g_httpClient = std::make_shared<RawrXD::WinHTTPClientImpl>();
    });
    return g_httpClient->sendRequest(request, chunkCallback);
}

HTTPResponse WinHTTPClient::get(const std::string& url) {
    std::call_once(g_httpInitFlag, []() {
        g_httpClient = std::make_shared<RawrXD::WinHTTPClientImpl>();
    });
    return g_httpClient->get(url);
}
