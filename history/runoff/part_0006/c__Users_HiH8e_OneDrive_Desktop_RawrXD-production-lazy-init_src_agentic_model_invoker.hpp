#pragma once
#include <string>
#include <functional>
#include <vector>

class ModelInvoker {
public:
    enum class Backend {
        OLLAMA,
        CLAUDE,
        OPENAI,
        LOCAL_GGUF
    };

    struct Config {
        Backend backend;
        std::string endpoint;
        std::string apiKey;
        std::string modelName;
        int maxTokens;
        double temperature;
    };

    struct Plan {
        std::string id;
        std::string description;
        std::vector<std::string> actions;
        std::string reasoning;
        bool requiresApproval;
    };

    ModelInvoker();
    ~ModelInvoker();

    bool initialize(const Config& config);
    Plan invoke(const std::string& wish);
    void invokeAsync(const std::string& wish, std::function<void(Plan)> callback);
    
    void setCacheEnabled(bool enabled);
    void clearCache();

private:
    Config config_;
    bool cacheEnabled_;
    
    Plan parsePlanFromJSON(const std::string& jsonResponse);
    std::string callOllama(const std::string& prompt);
    std::string callClaude(const std::string& prompt);
    std::string callOpenAI(const std::string& prompt);
    std::string callLocalGGUF(const std::string& prompt);
};