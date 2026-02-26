/**
 * @file model_invoker.hpp
 * @brief LLM invocation layer for wish-to-plan transformation (Qt-free)
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <future>
#include <nlohmann/json.hpp>

struct InvocationParams {
    std::string wish;
    std::string context;
    std::vector<std::string> availableTools;
    std::string codebaseContext;
    int maxTokens = 2000;
    double temperature = 0.7;
    int timeoutMs = 30000;
};

struct LLMResponse {
    bool success = false;
    std::string rawOutput;
    nlohmann::json parsedPlan;
    std::string reasoning;
    int tokensUsed = 0;
    std::string error;
};

class ModelInvoker {
public:
    ModelInvoker() = default;
    ~ModelInvoker() = default;

    void setLLMBackend(const std::string& backend, const std::string& endpoint, const std::string& apiKey = "");
    std::string getLLMBackend() const { return m_backend; }
    LLMResponse invoke(const InvocationParams& params);
    void invokeAsync(const InvocationParams& params);
    void cancelPendingRequest();
    bool isInvoking() const { return m_isInvoking; }
    void setSystemPromptTemplate(const std::string& template_);
    void setCodebaseEmbeddings(const std::map<std::string, float>& embeddings);
    void setCachingEnabled(bool enabled) { m_cachingEnabled = enabled; }

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&)> onPlanGenerationStarted;
    std::function<void(const LLMResponse&)> onPlanGenerated;
    std::function<void(const std::string&, bool)> onInvocationError;
    std::function<void(const std::string&)> onStatusUpdated;

private:
    std::string buildSystemPrompt(const std::vector<std::string>& tools);
    std::string buildUserMessage(const InvocationParams& params);
    nlohmann::json sendOllamaRequest(const std::string& model, const std::string& prompt, int maxTokens, double temperature);
    nlohmann::json sendClaudeRequest(const std::string& prompt, int maxTokens, double temperature);
    nlohmann::json sendOpenAIRequest(const std::string& prompt, int maxTokens, double temperature);
    nlohmann::json parsePlan(const std::string& llmOutput);
    bool validatePlanSanity(const nlohmann::json& plan);
    std::string getCacheKey(const InvocationParams& params) const;
    LLMResponse getCachedResponse(const std::string& key) const;
    void cacheResponse(const std::string& key, const LLMResponse& response);

    std::string m_backend;
    std::string m_endpoint;
    std::string m_apiKey;
    std::string m_model = "mistral";
    bool m_isInvoking = false;
    bool m_cachingEnabled = true;
    std::map<std::string, LLMResponse> m_responseCache;
    std::string m_customSystemPrompt;
    std::map<std::string, float> m_codebaseEmbeddings;
    std::future<void> m_asyncFuture;  // Post-Qt: retained to prevent premature destruction
};
