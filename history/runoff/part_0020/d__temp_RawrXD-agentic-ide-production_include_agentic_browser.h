#pragma once

#include <string>
#include <deque>
#include <functional>
#include <optional>

class EnterpriseMetricsCollector;

class AgenticBrowser {
public:
    struct NavigationOptions {
        int timeoutMs = 15000;
        std::string userAgent = "RawrXD-AgenticBrowser/1.0";
        bool followRedirects = true;
        bool allowCookies = false;
    };

    using NavigationCallback = std::function<void(const std::string& url)>;
    using NavigationFinishedCallback = std::function<void(const std::string& url, bool success, int httpStatus, long long bytes)>;
    using NavigationErrorCallback = std::function<void(const std::string& url, const std::string& error)>;
    using ContentUpdatedCallback = std::function<void(const std::string& url, const std::string& title)>;

    explicit AgenticBrowser();
    ~AgenticBrowser();

    void navigate(const std::string& url);
    void navigate(const std::string& url, const NavigationOptions& opts);
    void goBack();
    void goForward();
    void stop();

    void httpGet(const std::string& url);
    void httpGet(const std::string& url, const NavigationOptions& opts);
    void httpPost(const std::string& url, const std::string& body);
    void httpPost(const std::string& url, const std::string& body, const std::vector<std::pair<std::string,std::string>>& headers);
    void httpPost(const std::string& url, const std::string& body,
                  const std::vector<std::pair<std::string,std::string>>& headers,
                  const NavigationOptions& opts);

    std::string extractMainText() const;
    std::vector<std::string> extractLinks(int maxLinks = 200) const;

    void setMetrics(EnterpriseMetricsCollector* metrics);

    // callbacks
    void setNavigationStartedCallback(NavigationCallback cb) { m_navStarted = std::move(cb); }
    void setNavigationFinishedCallback(NavigationFinishedCallback cb) { m_navFinished = std::move(cb); }
    void setNavigationErrorCallback(NavigationErrorCallback cb) { m_navError = std::move(cb); }
    void setContentUpdatedCallback(ContentUpdatedCallback cb) { m_contentUpdated = std::move(cb); }

private:
    struct HistoryEntry { std::string url; std::string title; };

    NavigationOptions m_lastOpts;
    std::deque<HistoryEntry> m_backStack;
    std::deque<HistoryEntry> m_fwdStack;
    std::string m_currentUrl;
    EnterpriseMetricsCollector* m_metrics = nullptr;

    // Callbacks
    NavigationCallback m_navStarted;
    NavigationFinishedCallback m_navFinished;
    NavigationErrorCallback m_navError;
    ContentUpdatedCallback m_contentUpdated;

    // Helpers
    void setLoading(bool loading);
    void pushHistory(const std::string& url, const std::string& title);
    std::string sanitizeHtml(const std::string& html) const;
    std::string guessTitle(const std::string& html) const;
    void recordMetrics(const std::string& event,
                       double latencyMs,
                       int status,
                       long long bytes) const;
    void logStructured(const char* level, const std::string& msg,
                       const std::string& url = {},
                       int status = 0,
                       long long bytes = 0,
                       double latencyMs = 0.0) const;
};
