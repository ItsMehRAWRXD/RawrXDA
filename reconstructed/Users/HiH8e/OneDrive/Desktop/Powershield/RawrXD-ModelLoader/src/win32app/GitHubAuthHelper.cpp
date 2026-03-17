#include "GitHubAuthHelper.h"
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

GitHubAuthHelper::GitHubAuthHelper() 
    : m_lastHttpStatus(0) {
}

GitHubAuthHelper::~GitHubAuthHelper() {
}

bool GitHubAuthHelper::RequestDeviceCode(GitHubDeviceCode& outDeviceCode) {
    m_lastError.clear();
    
    // Prepare POST data
    std::string postData = "client_id=" + UrlEncode(CLIENT_ID) + 
                          "&scope=user:email%20copilot";
    
    std::string response;
    if (!HttpPost(DEVICE_CODE_URL, postData, response, "application/x-www-form-urlencoded")) {
        return false;
    }
    
    // Parse JSON response
    outDeviceCode.device_code = ExtractJsonValue(response, "device_code");
    outDeviceCode.user_code = ExtractJsonValue(response, "user_code");
    outDeviceCode.verification_uri = ExtractJsonValue(response, "verification_uri");
    outDeviceCode.expires_in = ExtractJsonInt(response, "expires_in");
    outDeviceCode.interval = ExtractJsonInt(response, "interval");
    
    if (outDeviceCode.device_code.empty() || outDeviceCode.user_code.empty()) {
        m_lastError = "Failed to parse device code response";
        return false;
    }
    
    return true;
}

bool GitHubAuthHelper::PollForAuthorization(const std::string& deviceCode, 
                                           std::string& outAccessToken,
                                           std::function<bool()> cancelCheck) {
    m_lastError.clear();
    
    // Poll every 5 seconds (GitHub recommends respecting the interval)
    int pollInterval = 5;
    int maxAttempts = 60;  // 5 minutes max (5s * 60 = 300s)
    
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        // Check if user cancelled
        if (cancelCheck && cancelCheck()) {
            m_lastError = "User cancelled authorization";
            return false;
        }
        
        // Prepare POST data
        std::string postData = "client_id=" + UrlEncode(CLIENT_ID) + 
                              "&device_code=" + UrlEncode(deviceCode) +
                              "&grant_type=urn:ietf:params:oauth:grant-type:device_code";
        
        std::string response;
        if (!HttpPost(ACCESS_TOKEN_URL, postData, response, "application/x-www-form-urlencoded")) {
            // Check for specific error codes
            std::string error = ExtractJsonValue(response, "error");
            
            if (error == "authorization_pending") {
                // User hasn't authorized yet - continue polling
                std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
                continue;
            }
            else if (error == "slow_down") {
                // GitHub asks us to slow down - increase interval
                pollInterval += 5;
                std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
                continue;
            }
            else if (error == "expired_token") {
                m_lastError = "Device code expired. Please try again.";
                return false;
            }
            else if (error == "access_denied") {
                m_lastError = "User declined authorization";
                return false;
            }
            
            // Unknown error - retry
            std::this_thread::sleep_for(std::chrono::seconds(pollInterval));
            continue;
        }
        
        // Success! Extract access token
        outAccessToken = ExtractJsonValue(response, "access_token");
        
        if (outAccessToken.empty()) {
            m_lastError = "Failed to extract access token from response";
            return false;
        }
        
        return true;
    }
    
    m_lastError = "Authorization timeout - no response after 5 minutes";
    return false;
}

bool GitHubAuthHelper::GetAuthenticatedUser(const std::string& accessToken, GitHubUser& outUser) {
    m_lastError.clear();
    
    std::string response;
    if (!HttpGet(USER_API_URL, response, accessToken)) {
        return false;
    }
    
    outUser.login = ExtractJsonValue(response, "login");
    outUser.name = ExtractJsonValue(response, "name");
    outUser.email = ExtractJsonValue(response, "email");
    outUser.avatar_url = ExtractJsonValue(response, "avatar_url");
    outUser.id = ExtractJsonInt(response, "id");
    
    if (outUser.login.empty()) {
        m_lastError = "Failed to parse user info from response";
        return false;
    }
    
    return true;
}

bool GitHubAuthHelper::CheckCopilotSubscription(const std::string& accessToken, 
                                               CopilotSubscription& outSubscription) {
    m_lastError.clear();
    
    std::string response;
    if (!HttpGet(COPILOT_API_URL, response, accessToken)) {
        // API might return 404 if no Copilot subscription
        if (m_lastHttpStatus == 404) {
            outSubscription.has_subscription = false;
            outSubscription.is_active = false;
            return true;  // Not an error - just no subscription
        }
        return false;
    }
    
    // Parse subscription details
    outSubscription.has_subscription = true;
    outSubscription.is_active = ExtractJsonBool(response, "is_active");
    outSubscription.plan_type = ExtractJsonValue(response, "plan_type");
    outSubscription.expiry_date = ExtractJsonValue(response, "expires_at");
    
    return true;
}

bool GitHubAuthHelper::StoreToken(const std::string& accessToken, const std::string& username) {
    return WriteCredential(CREDENTIAL_TARGET, username, accessToken);
}

bool GitHubAuthHelper::LoadToken(std::string& outAccessToken, std::string& outUsername) {
    return ReadCredential(CREDENTIAL_TARGET, outUsername, outAccessToken);
}

bool GitHubAuthHelper::DeleteToken() {
    return DeleteCredential(CREDENTIAL_TARGET);
}

bool GitHubAuthHelper::RefreshToken(std::string& inOutAccessToken) {
    // GitHub personal access tokens don't expire, but we can validate them
    GitHubUser user;
    if (!GetAuthenticatedUser(inOutAccessToken, user)) {
        m_lastError = "Token validation failed - may be invalid or revoked";
        return false;
    }
    
    return true;  // Token is still valid
}

// ============================================================================
// HTTP Helper Methods
// ============================================================================

bool GitHubAuthHelper::HttpPost(const std::string& url, const std::string& postData,
                               std::string& outResponse, const std::string& contentType) {
    m_lastError.clear();
    m_lastHttpStatus = 0;
    
    // Parse URL components
    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    char hostname[256] = {0};
    char urlPath[1024] = {0};
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = sizeof(hostname);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);
    
    if (!WinHttpCrackUrl(std::wstring(url.begin(), url.end()).c_str(), 0, 0, 
                        reinterpret_cast<URL_COMPONENTS*>(&urlComp))) {
        m_lastError = "Failed to parse URL";
        return false;
    }
    
    // Open session
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_lastError = "Failed to open HTTP session";
        return false;
    }
    
    // Connect
    std::wstring wHostname(hostname, hostname + strlen(hostname));
    HINTERNET hConnect = WinHttpConnect(hSession, wHostname.c_str(),
                                       INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        m_lastError = "Failed to connect to server";
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Open request
    std::wstring wUrlPath(urlPath, urlPath + strlen(urlPath));
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wUrlPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        m_lastError = "Failed to create request";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Set headers
    std::wstring headers = L"Content-Type: " + std::wstring(contentType.begin(), contentType.end()) + L"\r\n";
    headers += L"Accept: application/json\r\n";
    headers += L"User-Agent: RawrXD-IDE/1.0\r\n";
    
    if (!WinHttpSendRequest(hRequest, headers.c_str(), -1,
                           (LPVOID)postData.data(), postData.size(),
                           postData.size(), 0)) {
        m_lastError = "Failed to send request";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        m_lastError = "Failed to receive response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Get status code
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);
    m_lastHttpStatus = statusCode;
    
    // Read data
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    outResponse = response;
    
    // Check for HTTP errors
    if (statusCode < 200 || statusCode >= 300) {
        m_lastError = "HTTP error " + std::to_string(statusCode) + ": " + response;
        return false;
    }
    
    return true;
}

bool GitHubAuthHelper::HttpGet(const std::string& url, std::string& outResponse,
                              const std::string& authToken) {
    m_lastError.clear();
    m_lastHttpStatus = 0;
    
    // Parse URL (similar to HttpPost)
    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    char hostname[256] = {0};
    char urlPath[1024] = {0};
    urlComp.lpszHostName = hostname;
    urlComp.dwHostNameLength = sizeof(hostname);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);
    
    if (!WinHttpCrackUrl(std::wstring(url.begin(), url.end()).c_str(), 0, 0,
                        reinterpret_cast<URL_COMPONENTS*>(&urlComp))) {
        m_lastError = "Failed to parse URL";
        return false;
    }
    
    // Open session, connect, request (similar to HttpPost but GET)
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        m_lastError = "Failed to open HTTP session";
        return false;
    }
    
    std::wstring wHostname(hostname, hostname + strlen(hostname));
    HINTERNET hConnect = WinHttpConnect(hSession, wHostname.c_str(),
                                       INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        m_lastError = "Failed to connect to server";
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    std::wstring wUrlPath(urlPath, urlPath + strlen(urlPath));
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wUrlPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        m_lastError = "Failed to create request";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Set headers with authorization
    std::wstring headers = L"Accept: application/json\r\n";
    headers += L"User-Agent: RawrXD-IDE/1.0\r\n";
    if (!authToken.empty()) {
        headers += L"Authorization: Bearer " + std::wstring(authToken.begin(), authToken.end()) + L"\r\n";
    }
    
    if (!WinHttpSendRequest(hRequest, headers.c_str(), -1,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        m_lastError = "Failed to send request";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        m_lastError = "Failed to receive response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Get status code and read data (same as HttpPost)
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);
    m_lastHttpStatus = statusCode;
    
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1);
        DWORD bytesRead = 0;
        
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    outResponse = response;
    
    if (statusCode < 200 || statusCode >= 300) {
        m_lastError = "HTTP error " + std::to_string(statusCode) + ": " + response;
        return false;
    }
    
    return true;
}

// ============================================================================
// JSON Parsing Helpers (Simple key-value extraction - no library needed)
// ============================================================================

std::string GitHubAuthHelper::ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) return "";
    valueStart++;
    
    size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string::npos) return "";
    
    return json.substr(valueStart, valueEnd - valueStart);
}

int GitHubAuthHelper::ExtractJsonInt(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return 0;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return 0;
    
    size_t valueStart = colonPos + 1;
    while (valueStart < json.length() && std::isspace(json[valueStart])) valueStart++;
    
    size_t valueEnd = valueStart;
    while (valueEnd < json.length() && std::isdigit(json[valueEnd])) valueEnd++;
    
    if (valueEnd == valueStart) return 0;
    
    return std::stoi(json.substr(valueStart, valueEnd - valueStart));
}

bool GitHubAuthHelper::ExtractJsonBool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return false;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return false;
    
    size_t truePos = json.find("true", colonPos);
    size_t falsePos = json.find("false", colonPos);
    
    if (truePos != std::string::npos && (falsePos == std::string::npos || truePos < falsePos)) {
        return true;
    }
    
    return false;
}

// ============================================================================
// URL Encoding
// ============================================================================

std::string GitHubAuthHelper::UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::uppercase;
            escaped << std::setw(2) << int((unsigned char)c);
            escaped << std::nouppercase;
        }
    }
    
    return escaped.str();
}

// ============================================================================
// Windows Credential Manager Integration
// ============================================================================

bool GitHubAuthHelper::WriteCredential(const std::string& target, const std::string& username,
                                      const std::string& password) {
    CREDENTIALA cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<char*>(target.c_str());
    cred.CredentialBlobSize = password.size();
    cred.CredentialBlob = (LPBYTE)password.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<char*>(username.c_str());
    
    if (!CredWriteA(&cred, 0)) {
        m_lastError = "Failed to write credential: " + std::to_string(GetLastError());
        return false;
    }
    
    return true;
}

bool GitHubAuthHelper::ReadCredential(const std::string& target, std::string& outUsername,
                                     std::string& outPassword) {
    PCREDENTIALA pCred = nullptr;
    
    if (!CredReadA(target.c_str(), CRED_TYPE_GENERIC, 0, &pCred)) {
        DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND) {
            m_lastError = "Credential not found";
        } else {
            m_lastError = "Failed to read credential: " + std::to_string(error);
        }
        return false;
    }
    
    outUsername = pCred->UserName ? std::string(pCred->UserName) : "";
    outPassword = std::string((char*)pCred->CredentialBlob, pCred->CredentialBlobSize);
    
    CredFree(pCred);
    return true;
}

bool GitHubAuthHelper::DeleteCredential(const std::string& target) {
    if (!CredDeleteA(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            m_lastError = "Failed to delete credential: " + std::to_string(error);
            return false;
        }
    }
    
    return true;
}
