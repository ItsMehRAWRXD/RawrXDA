#include "../include/native_http.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

// Parse URL into components
struct ParsedURL {
    std::wstring scheme;
    std::wstring host;
    std::wstring path;
    int port;
    
    bool parse(const std::string& urlStr) {
        // Simple URL parser
        size_t schemeEnd = urlStr.find("://");
        if (schemeEnd == std::string::npos) return false;
        
        std::string schemeStr = urlStr.substr(0, schemeEnd);
        scheme = std::wstring(schemeStr.begin(), schemeStr.end());
        
        size_t hostStart = schemeEnd + 3;
        size_t hostEnd = urlStr.find('/', hostStart);
        if (hostEnd == std::string::npos) {
            hostEnd = urlStr.length();
        }
        
        std::string hostStr = urlStr.substr(hostStart, hostEnd - hostStart);
        host = std::wstring(hostStr.begin(), hostStr.end());
        
        path = hostEnd < urlStr.length() ? 
            std::wstring(urlStr.begin() + hostEnd, urlStr.end()) : 
            std::wstring(L"/");
        
        // Determine port from scheme
        port = (scheme == L"https") ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
        
        // Check for explicit port in host
        size_t colonPos = hostStr.find(':');
        if (colonPos != std::string::npos) {
            host = std::wstring(hostStr.begin(), hostStr.begin() + colonPos);
            port = std::stoi(hostStr.substr(colonPos + 1));
        }
        
        return true;
    }
};

std::optional<std::pair<int, std::string>> NativeHttp_Get(const std::string &url, int timeoutMs) {
    ParsedURL parsed;
    if (!parsed.parse(url)) {
        return std::nullopt;
    }
    
    HINTERNET hSession = WinHttpOpen(
        L"AgenticIDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession) {
        return std::nullopt;
    }
    
    // Set timeout
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        parsed.host.c_str(),
        parsed.port,
        0
    );
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    DWORD openFlags = (parsed.scheme == L"https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        openFlags
    );
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    // Get status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );
    
    // Read response body
    std::string responseBody;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer = nullptr;
    
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        
        pszOutBuffer = new char[dwSize + 1];
        if (!pszOutBuffer) break;
        
        ZeroMemory(pszOutBuffer, dwSize + 1);
        
        if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) break;
        
        responseBody.append(pszOutBuffer, dwDownloaded);
        delete[] pszOutBuffer;
        pszOutBuffer = nullptr;
    } while (dwSize > 0);
    
    if (pszOutBuffer) delete[] pszOutBuffer;
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return std::make_pair((int)statusCode, responseBody);
}

std::optional<std::pair<int, std::string>> NativeHttp_Post(const std::string &url, const std::string &body, 
                                                             const std::vector<std::pair<std::string,std::string>>& headers, 
                                                             int timeoutMs) {
    ParsedURL parsed;
    if (!parsed.parse(url)) {
        return std::nullopt;
    }
    
    HINTERNET hSession = WinHttpOpen(
        L"AgenticIDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    
    if (!hSession) {
        return std::nullopt;
    }
    
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
    
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        parsed.host.c_str(),
        parsed.port,
        0
    );
    
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    DWORD openFlags = (parsed.scheme == L"https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        parsed.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        openFlags
    );
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    // Build header string
    std::wstring headerString;
    for (const auto& h : headers) {
        std::wstring key(h.first.begin(), h.first.end());
        std::wstring value(h.second.begin(), h.second.end());
        headerString += key + L": " + value + L"\r\n";
    }
    
    if (!headerString.empty()) {
        WinHttpAddRequestHeaders(hRequest, headerString.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_REPLACE);
    }
    
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                           (LPVOID)body.c_str(), body.length(), body.length(), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }
    
    // Get status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX
    );
    
    // Read response body
    std::string responseBody;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    LPSTR pszOutBuffer = nullptr;
    
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        
        pszOutBuffer = new char[dwSize + 1];
        if (!pszOutBuffer) break;
        
        ZeroMemory(pszOutBuffer, dwSize + 1);
        
        if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) break;
        
        responseBody.append(pszOutBuffer, dwDownloaded);
        delete[] pszOutBuffer;
        pszOutBuffer = nullptr;
    } while (dwSize > 0);
    
    if (pszOutBuffer) delete[] pszOutBuffer;
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return std::make_pair((int)statusCode, responseBody);
}
