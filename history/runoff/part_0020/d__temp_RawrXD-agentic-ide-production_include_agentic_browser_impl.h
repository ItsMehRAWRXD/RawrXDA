#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

// ============================================================================
// AGENTIC BROWSER - SANDBOXED WEB NAVIGATION
// ============================================================================

struct HttpResponse {
    int statusCode = 0;
    std::string contentType;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct Link {
    std::string text;
    std::string href;
    std::string title;
};

class AgenticBrowser {
public:
    AgenticBrowser();
    ~AgenticBrowser();

    // Navigation
    void navigate(const std::string& url);
    void goBack();
    void goForward();
    void stop();
    void reload();

    // HTTP operations
    HttpResponse httpGet(const std::string& url);
    HttpResponse httpPost(const std::string& url, const std::string& body, 
                          const std::map<std::string, std::string>& headers = {});

    // Content extraction
    std::string extractMainText();
    std::vector<Link> extractLinks(int maxLinks = 200);
    std::string getPageTitle() const { return m_pageTitle; }
    std::string getCurrentUrl() const { return m_currentUrl; }

    // History
    std::vector<std::string> getBackHistory() const { return m_backStack; }
    std::vector<std::string> getForwardHistory() const { return m_forwardStack; }

    // Settings
    void setTimeout(int seconds) { m_timeout = seconds; }
    void setUserAgent(const std::string& ua) { m_userAgent = ua; }
    void setCookieEnabled(bool enabled) { m_cookiesEnabled = enabled; }

    // Callbacks
    using NavigationCallback = std::function<void(const std::string&)>;
    using ContentCallback = std::function<void(const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    void setNavigationStartedCallback(NavigationCallback cb) { m_navStartedCb = cb; }
    void setNavigationFinishedCallback(NavigationCallback cb) { m_navFinishedCb = cb; }
    void setContentUpdatedCallback(ContentCallback cb) { m_contentUpdatedCb = cb; }
    void setErrorCallback(ErrorCallback cb) { m_errorCb = cb; }

private:
    std::string m_currentUrl;
    std::string m_pageTitle;
    std::string m_pageContent;
    std::vector<std::string> m_backStack;
    std::vector<std::string> m_forwardStack;
    int m_timeout = 15;  // seconds
    std::string m_userAgent = "RawrXD-AgenticBrowser/1.0";
    bool m_cookiesEnabled = false;

    NavigationCallback m_navStartedCb;
    NavigationCallback m_navFinishedCb;
    ContentCallback m_contentUpdatedCb;
    ErrorCallback m_errorCb;

    void parseHtmlContent(const std::string& html);
};

#endif
