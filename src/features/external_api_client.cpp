// ============================================================================
// external_api_client.cpp — External Model API Support
// ============================================================================
// Unified client for OpenAI, Anthropic, Claude, and other external APIs
// ============================================================================

#include "external_api_client.h"
#include "logging/logger.h"
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <curl/curl.h>
#endif

static Logger s_logger("ExternalAPI");

class ExternalAPIClient::Impl {
public:
    std::string m_apiKey;
    std::string m_baseUrl;
    APIProvider m_provider;
    int m_timeout;
    std::mutex m_mutex;
    
    Impl() : m_provider(APIProvider::OpenAI), m_timeout(30000) {}
    
    bool httpPost(const std::string& endpoint, 
                  const std::string& jsonPayload,
                  std::string& response) {
#ifdef _WIN32
        return httpPostWindows(endpoint, jsonPayload, response);
#else
        return httpPostCurl(endpoint, jsonPayload, response);
#endif
    }
    
#ifdef _WIN32
    bool httpPostWindows(const std::string& endpoint,
                        const std::string& jsonPayload,
                        std::string& response) {
        HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
        bool success = false;
        
        // Parse URL
        std::regex urlPattern(R"(https?://([^/]+)(/.*)?)");
        std::smatch match;
        if (!std::regex_match(m_baseUrl + endpoint, match, urlPattern)) {
            s_logger.error("Invalid URL: {}", m_baseUrl + endpoint);
            return false;
        }
        
        std::string host = match[1].str();
        std::string path = match[2].str();
        
        hSession = WinHttpOpen(L"RawrXD/1.0",
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            s_logger.error("WinHttpOpen failed");
            return false;
        }
        
        std::wstring whost(host.begin(), host.end());
        hConnect = WinHttpConnect(hSession, whost.c_str(), 
                                 INTERNET_DEFAULT_HTTPS_PORT, 0);
        
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        std::wstring wpath(path.begin(), path.end());
        hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.c_str(),
                                     NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     WINHTTP_FLAG_SECURE);
        
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        // Set headers
        std::wstring headers = L"Content-Type: application/json\r\n";
        headers += L"Authorization: Bearer ";
        headers += std::wstring(m_apiKey.begin(), m_apiKey.end());
        headers += L"\r\n";
        
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1,
                                WINHTTP_ADDREQ_FLAG_ADD);
        
        // Send request
        if (WinHttpSendRequest(hRequest,
                              WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              (LPVOID)jsonPayload.c_str(), 
                              jsonPayload.length(),
                              jsonPayload.length(), 0) &&
            WinHttpReceiveResponse(hRequest, NULL)) {
            
            DWORD bytesAvailable = 0;
            DWORD bytesRead = 0;
            std::string buffer;
            
            do {
                bytesAvailable = 0;
                WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
                
                if (bytesAvailable > 0) {
                    char* temp = new char[bytesAvailable + 1];
                    WinHttpReadData(hRequest, temp, bytesAvailable, &bytesRead);
                    temp[bytesRead] = 0;
                    response += temp;
                    delete[] temp;
                }
            } while (bytesAvailable > 0);
            
            success = true;
        }
        
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        return success;
    }
#else
    bool httpPostCurl(const std::string& endpoint,
                     const std::string& jsonPayload,
                     std::string& response) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            s_logger.error("Failed to initialize CURL");
            return false;
        }
        
        std::string url = m_baseUrl + endpoint;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string auth = "Authorization: Bearer " + m_apiKey;
        headers = curl_slist_append(headers, auth.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
            +[](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                ((std::string*)userp)->append((char*)contents, size * nmemb);
                return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_timeout / 1000);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            s_logger.error("CURL request failed: {}", curl_easy_strerror(res));
            return false;
        }
        
        return true;
    }
#endif
};

ExternalAPIClient::ExternalAPIClient() : m_impl(new Impl()) {}
ExternalAPIClient::~ExternalAPIClient() { delete m_impl; }

void ExternalAPIClient::setProvider(APIProvider provider) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_provider = provider;
    
    // Set default base URLs
    switch (provider) {
        case APIProvider::OpenAI:
            m_impl->m_baseUrl = "https://api.openai.com";
            break;
        case APIProvider::Anthropic:
            m_impl->m_baseUrl = "https://api.anthropic.com";
            break;
        case APIProvider::Claude:
            m_impl->m_baseUrl = "https://api.claude.ai";
            break;
        case APIProvider::Custom:
            // Will be set via setBaseUrl()
            break;
    }
    
    s_logger.info("API provider set to: {}", static_cast<int>(provider));
}

void ExternalAPIClient::setAPIKey(const std::string& apiKey) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_apiKey = apiKey;
    s_logger.info("API key configured");
}

void ExternalAPIClient::setBaseUrl(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    m_impl->m_baseUrl = url;
    s_logger.info("Base URL set to: {}", url);
}

std::string ExternalAPIClient::chat(const std::vector<ChatMessage>& messages,
                                   const std::string& model) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    if (m_impl->m_apiKey.empty()) {
        s_logger.error("API key not set");
        return "";
    }
    
    // Build JSON payload based on provider
    std::ostringstream json;
    json << "{\"model\":\"" << model << "\",\"messages\":[";
    
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        json << "{\"role\":\"" << msg.role << "\",\"content\":\"" << escapeJson(msg.content) << "\"}";
        if (i + 1 < messages.size()) json << ",";
    }
    
    json << "]}";
    
    std::string response;
    std::string endpoint;
    
    switch (m_impl->m_provider) {
        case APIProvider::OpenAI:
            endpoint = "/v1/chat/completions";
            break;
        case APIProvider::Anthropic:
            endpoint = "/v1/messages";
            break;
        default:
            endpoint = "/v1/chat/completions";
    }
    
    if (!m_impl->httpPost(endpoint, json.str(), response)) {
        s_logger.error("HTTP request failed");
        return "";
    }
    
    // Parse response (simplified - just extract content)
    std::regex contentPattern(R"("content"\s*:\s*"([^"]*)")");
    std::smatch match;
    if (std::regex_search(response, match, contentPattern)) {
        return match[1].str();
    }
    
    s_logger.warn("Failed to parse response");
    return "";
}

bool ExternalAPIClient::isConfigured() const {
    return !m_impl->m_apiKey.empty() && !m_impl->m_baseUrl.empty();
}

std::string ExternalAPIClient::escapeJson(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}
