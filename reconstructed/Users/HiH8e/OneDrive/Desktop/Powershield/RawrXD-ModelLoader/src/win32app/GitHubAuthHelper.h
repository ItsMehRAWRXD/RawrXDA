#pragma once

#include <windows.h>
#include <winhttp.h>
#include <wincred.h>
#include <string>
#include <functional>

// GitHub OAuth Device Flow Implementation
// Docs: https://docs.github.com/en/apps/oauth-apps/building-oauth-apps/authorizing-oauth-apps#device-flow

struct GitHubDeviceCode {
    std::string device_code;
    std::string user_code;
    std::string verification_uri;
    int expires_in;
    int interval;
};

struct GitHubUser {
    std::string login;
    std::string name;
    std::string email;
    std::string avatar_url;
    int id;
};

struct CopilotSubscription {
    bool has_subscription;
    std::string plan_type;  // "personal", "business", "enterprise"
    bool is_active;
    std::string expiry_date;
};

class GitHubAuthHelper {
public:
    GitHubAuthHelper();
    ~GitHubAuthHelper();

    // OAuth Device Flow
    bool RequestDeviceCode(GitHubDeviceCode& outDeviceCode);
    bool PollForAuthorization(const std::string& deviceCode, std::string& outAccessToken, 
                              std::function<bool()> cancelCheck = nullptr);
    
    // User Info
    bool GetAuthenticatedUser(const std::string& accessToken, GitHubUser& outUser);
    bool CheckCopilotSubscription(const std::string& accessToken, CopilotSubscription& outSubscription);
    
    // Token Management
    bool StoreToken(const std::string& accessToken, const std::string& username);
    bool LoadToken(std::string& outAccessToken, std::string& outUsername);
    bool DeleteToken();
    bool RefreshToken(std::string& inOutAccessToken);  // GitHub tokens don't expire but we validate
    
    // Error Handling
    std::string GetLastError() const { return m_lastError; }
    int GetLastHttpStatus() const { return m_lastHttpStatus; }

private:
    // HTTP Helper Methods
    bool HttpPost(const std::string& url, const std::string& postData, 
                  std::string& outResponse, const std::string& contentType = "application/json");
    bool HttpGet(const std::string& url, std::string& outResponse, 
                 const std::string& authToken = "");
    
    // JSON Parsing Helpers (simple key-value extraction)
    std::string ExtractJsonValue(const std::string& json, const std::string& key);
    int ExtractJsonInt(const std::string& json, const std::string& key);
    bool ExtractJsonBool(const std::string& json, const std::string& key);
    
    // URL Encoding
    std::string UrlEncode(const std::string& value);
    
    // Credential Manager Helpers
    bool WriteCredential(const std::string& target, const std::string& username, 
                        const std::string& password);
    bool ReadCredential(const std::string& target, std::string& outUsername, 
                       std::string& outPassword);
    bool DeleteCredential(const std::string& target);

private:
    static constexpr const char* CLIENT_ID = "Iv1.b507a08c87ecfe98";
    static constexpr const char* DEVICE_CODE_URL = "https://github.com/login/device/code";
    static constexpr const char* ACCESS_TOKEN_URL = "https://github.com/login/oauth/access_token";
    static constexpr const char* USER_API_URL = "https://api.github.com/user";
    static constexpr const char* COPILOT_API_URL = "https://api.github.com/user/copilot_seats";
    static constexpr const char* CREDENTIAL_TARGET = "RawrXD:GitHub:AccessToken";
    
    std::string m_lastError;
    int m_lastHttpStatus;
};
