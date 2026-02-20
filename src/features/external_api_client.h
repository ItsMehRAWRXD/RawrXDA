#pragma once

#include <string>
#include <vector>

enum class APIProvider {
    OpenAI,
    Anthropic,
    Claude,
    Custom
};

struct ChatMessage {
    std::string role;
    std::string content;
};

class ExternalAPIClient {
public:
    ExternalAPIClient();
    ~ExternalAPIClient();
    
    void setProvider(APIProvider provider);
    void setAPIKey(const std::string& apiKey);
    void setBaseUrl(const std::string& url);
    
    std::string chat(const std::vector<ChatMessage>& messages,
                    const std::string& model = "gpt-4");
    
    bool isConfigured() const;
    
private:
    class Impl;
    Impl* m_impl;
    
    static std::string escapeJson(const std::string& s);
};
