#include "agentic_browser.h"
#include "logger.h"
#include <iostream>

AgenticBrowser::AgenticBrowser() {
    log_info("AgenticBrowser initialized");
}

AgenticBrowser::~AgenticBrowser() {
    log_info("AgenticBrowser destroyed");
}

void AgenticBrowser::navigate(const std::string& url) {
    if (m_navStarted) m_navStarted(url);
    log_debug("Navigating to: " + url);
    if (m_navFinished) m_navFinished(url, true, 200, 0);
}

void AgenticBrowser::navigate(const std::string& url, const NavigationOptions& opts) {
    navigate(url);
}

void AgenticBrowser::goBack() {
    log_debug("Go back");
}

void AgenticBrowser::goForward() {
    log_debug("Go forward");
}

void AgenticBrowser::stop() {
    log_debug("Stop navigation");
}

void AgenticBrowser::httpGet(const std::string& url) {
    navigate(url);
}

void AgenticBrowser::httpGet(const std::string& url, const NavigationOptions& opts) {
    navigate(url, opts);
}

void AgenticBrowser::httpPost(const std::string& url, const std::string& body) {
    navigate(url);
}

void AgenticBrowser::httpPost(const std::string& url, const std::string& body, 
                              const std::vector<std::pair<std::string,std::string>>& headers) {
    navigate(url);
}

void AgenticBrowser::httpPost(const std::string& url, const std::string& body,
                             const std::vector<std::pair<std::string,std::string>>& headers,
                             const NavigationOptions& opts) {
    navigate(url, opts);
}

std::string AgenticBrowser::extractMainText() const {
    return "Extracted main text";
}

std::vector<std::string> AgenticBrowser::extractLinks(int maxLinks) const {
    return std::vector<std::string>();
}

void AgenticBrowser::setMetrics(EnterpriseMetricsCollector* metrics) {
    log_debug("Metrics collector set");
}

