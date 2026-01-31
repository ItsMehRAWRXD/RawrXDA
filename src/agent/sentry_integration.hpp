#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

class SentryIntegration
{
public:
    static SentryIntegration* instance();
    
    bool initialize();
    
    void captureException(const std::string& exception, const nlohmann::json& context = nlohmann::json::object());
    void captureMessage(const std::string& message, const std::string& level = "info");
    
    void addBreadcrumb(const std::string& message, const std::string& category = "default", const std::string& level = "info");
    
private:
    SentryIntegration();
    ~SentryIntegration();

    // No copy
    SentryIntegration(const SentryIntegration&) = delete;
    SentryIntegration& operator=(const SentryIntegration&) = delete;

    void sendEvent(const nlohmann::json& event);

    static SentryIntegration* s_instance;
    bool m_initialized = false;
    std::string m_dsn;
    std::string m_projectId;
    std::string m_publicKey;
    std::string m_sentryEndpoint;
    
    mutable std::mutex m_mutex;
    std::vector<nlohmann::json> m_breadcrumbs;
};
