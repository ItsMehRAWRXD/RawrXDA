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
    if (m_navStartedCb) m_navStartedCb(url);
    m_currentUrl = url;
    m_pageContent = "Content for: " + url;
    if (m_navFinishedCb) m_navFinishedCb(url);
}

void AgenticBrowser::goBack() {}
void AgenticBrowser::goForward() {}
void AgenticBrowser::stop() {}
void AgenticBrowser::reload() { navigate(m_currentUrl); }

HttpResponse AgenticBrowser::httpGet(const std::string& url) {
    HttpResponse response;
    response.statusCode = 200;
    response.body = "GET " + url;
    return response;
}

HttpResponse AgenticBrowser::httpPost(const std::string& url, const std::string& body,
                                       const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    response.statusCode = 200;
    response.body = "POST " + url;
    return response;
}

std::string AgenticBrowser::extractMainText() {
    return m_pageContent;
}

std::vector<Link> AgenticBrowser::extractLinks(int maxLinks) {
    return std::vector<Link>();
}

void AgenticBrowser::parseHtmlContent(const std::string& html) {}

