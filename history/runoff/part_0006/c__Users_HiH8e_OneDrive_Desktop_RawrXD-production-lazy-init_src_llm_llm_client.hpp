#pragma once
#include <string>
#include <functional>

class LLMClient {
public:
    enum class Provider {
        OLLAMA,
        CLAUDE,
        OPENAI,
        LOCAL_GGUF
    };

    struct Config {
        Provider provider;
        std::string endpoint;
        std::string apiKey;
        std::string model;
        int maxTokens;
        double temperature;
    };

    struct Response {
        bool success;
        std::string content;
        std::string error;
        int tokensUsed;
    };

    LLMClient();
    ~LLMClient();

    bool initialize(const Config& config);
    Response generate(const std::string& prompt);
    void generateAsync(const std::string& prompt, std::function<void(Response)> callback);
    
    void setStreamCallback(std::function<void(const std::string&)> callback);

private:
    Config config_;
    std::function<void(const std::string&)> streamCallback_;
    
    Response callOllama(const std::string& prompt);
    Response callClaude(const std::string& prompt);
    Response callOpenAI(const std::string& prompt);
    Response callLocalGGUF(const std::string& prompt);
    
    std::string makeHTTPRequest(const std::string& url, const std::string& method, 
                               const std::string& headers, const std::string& body);
};