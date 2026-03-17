#include "agentic_browser.h"
#include "logger.h"
#include "native_http.h"
#include <regex>
#include <sstream>
#include <algorithm>

AgenticBrowser::AgenticBrowser() {
    m_userAgent = "RawrXD-AgenticBrowser/1.0";
    log_info("AgenticBrowser initialized");
}

AgenticBrowser::~AgenticBrowser() {
    log_info("AgenticBrowser destroyed");
}

void AgenticBrowser::navigate(const std::string& url) {
    if (m_navStartedCb) m_navStartedCb(url);
    
    try {
        HttpResponse response = httpGet(url);
        
        if (response.statusCode == 200) {
            // Update history
            if (!m_currentUrl.empty()) {
                m_backStack.push_back(m_currentUrl);
                m_forwardStack.clear();
            }
            
            m_currentUrl = url;
            m_pageContent = response.body;
            parseHtmlContent(response.body);
            
            if (m_contentUpdatedCb) m_contentUpdatedCb(m_pageContent);
            log_debug("Navigated to: " + url);
            
            if (m_navFinishedCb) m_navFinishedCb(url);
        } else {
            std::string error = "HTTP " + std::to_string(response.statusCode);
            log_error("Navigation failed: " + error);
            if (m_errorCb) m_errorCb(error);
        }
    } catch (const std::exception& e) {
        log_error("Navigation error: " + std::string(e.what()));
        if (m_errorCb) m_errorCb(e.what());
    }
}

void AgenticBrowser::goBack() {
    if (!m_backStack.empty()) {
        m_forwardStack.push_back(m_currentUrl);
        m_currentUrl = m_backStack.back();
        m_backStack.pop_back();
        log_debug("Navigated back to: " + m_currentUrl);
    }
}

void AgenticBrowser::goForward() {
    if (!m_forwardStack.empty()) {
        m_backStack.push_back(m_currentUrl);
        m_currentUrl = m_forwardStack.back();
        m_forwardStack.pop_back();
        log_debug("Navigated forward to: " + m_currentUrl);
    }
}

void AgenticBrowser::stop() {
    log_debug("Navigation stopped");
}

void AgenticBrowser::reload() {
    if (!m_currentUrl.empty()) {
        navigate(m_currentUrl);
    }
}

HttpResponse AgenticBrowser::httpGet(const std::string& url) {
    HttpResponse response;
    
    try {
        // Use native HTTP implementation
        response = native_http_get(url, m_timeout, m_userAgent);
        log_debug("GET request completed: " + url + " (" + std::to_string(response.statusCode) + ")");
    } catch (const std::exception& e) {
        response.statusCode = 0;
        log_error("HTTP GET failed: " + std::string(e.what()));
    }
    
    return response;
}

HttpResponse AgenticBrowser::httpPost(const std::string& url, const std::string& body,
                                       const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    
    try {
        response = native_http_post(url, body, headers, m_timeout, m_userAgent);
        log_debug("POST request completed: " + url + " (" + std::to_string(response.statusCode) + ")");
    } catch (const std::exception& e) {
        response.statusCode = 0;
        log_error("HTTP POST failed: " + std::string(e.what()));
    }
    
    return response;
}

std::string AgenticBrowser::extractMainText() {
    std::string text = m_pageContent;
    
    // Remove HTML tags
    std::regex tag_regex("<[^>]*>");
    text = std::regex_replace(text, tag_regex, " ");
    
    // Remove extra whitespace
    std::regex space_regex("\\s+");
    text = std::regex_replace(text, space_regex, " ");
    
    // Trim leading/trailing whitespace
    text.erase(0, text.find_first_not_of(" \t\n\r"));
    text.erase(text.find_last_not_of(" \t\n\r") + 1);
    
    return text;
}

std::vector<Link> AgenticBrowser::extractLinks(int maxLinks) {
    std::vector<Link> links;
    std::regex link_regex(R"(<a\s+[^>]*href\s*=\s*[""']?([^""'\s>]*)[""']?[^>]*>([^<]*)</a>)", 
                         std::regex::icase);
    
    std::sregex_iterator iter(m_pageContent.begin(), m_pageContent.end(), link_regex);
    std::sregex_iterator end;
    
    int count = 0;
    while (iter != end && count < maxLinks) {
        Link link;
        link.href = (*iter)[1];
        link.text = (*iter)[2];
        links.push_back(link);
        ++iter;
        ++count;
    }
    
    log_debug("Extracted " + std::to_string(links.size()) + " links");
    return links;
}

void AgenticBrowser::parseHtmlContent(const std::string& html) {
    // Extract title
    std::regex title_regex("<title[^>]*>([^<]*)</title>", std::regex::icase);
    std::smatch match;
    if (std::regex_search(html, match, title_regex)) {
        m_pageTitle = match[1];
    }
}
