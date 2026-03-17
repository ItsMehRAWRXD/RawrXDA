#pragma once
#ifndef MODEL_INVOKER_H
#define MODEL_INVOKER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "nlohmann/json.hpp"

namespace RawrXD {

// LLM backend types
enum class LLMBackend {
    OLLAMA,
    CLAUDE,
    OPENAI,
    LOCAL_GGUF
};

// Plan execution status
enum class PlanStatus {
    PENDING,
    GENERATING,
    READY,
    ERROR,
    CANCELLED
};

// Action types for the execution plan
enum class ActionType {
    FILE_EDIT,
    SEARCH_FILES,
    RUN_BUILD,
    EXECUTE_TESTS,
    COMMIT_GIT,
    INVOKE_COMMAND,
    RECURSIVE_AGENT,
    QUERY_USER
};

// Individual action in a plan
struct Action {
    ActionType type;
    std::string description;
    nlohmann::json parameters;
    std::string id;
    std::vector<std::string> dependencies;
    
    Action() : type(ActionType::FILE_EDIT) {}
    Action(ActionType t, const std::string& desc, const nlohmann::json& params = {})
        : type(t), description(desc), parameters(params) {
        id = generateId();
    }
    
private:
    std::string generateId() {
        static int counter = 0;
        return "action_" + std::to_string(++counter);
    }
};

// Complete execution plan
struct ExecutionPlan {
    std::string id;
    std::string wish;
    std::vector<Action> actions;
    PlanStatus status;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    nlohmann::json metadata;
    
    ExecutionPlan() : status(PlanStatus::PENDING) {
        id = generateId();
        created = std::chrono::system_clock::now();
        modified = created;
    }
    
    ExecutionPlan(const std::string& w) : wish(w), status(PlanStatus::PENDING) {
        id = generateId();
        created = std::chrono::system_clock::now();
        modified = created;
    }
    
private:
    std::string generateId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return "plan_" + std::to_string(timestamp);
    }
};

// LLM response with plan
struct LLMResponse {
    bool success;
    std::string rawResponse;
    ExecutionPlan plan;
    std::string errorMessage;
    std::chrono::milliseconds latency;
    
    LLMResponse() : success(false), latency(0) {}
};

// ModelInvoker - transforms wishes to action plans via LLM
class ModelInvoker {
public:
    using PlanCallback = std::function<void(const LLMResponse&)>;
    using ErrorCallback = std::function<void(const std::string&, bool)>;
    
    ModelInvoker();
    ~ModelInvoker();
    
    // Configuration
    void setBackend(LLMBackend backend);
    void setEndpoint(const std::string& endpoint);
    void setApiKey(const std::string& apiKey);
    void setModel(const std::string& model);
    void setTemperature(float temperature);
    void setMaxTokens(int maxTokens);
    void setTimeout(std::chrono::milliseconds timeout);
    
    // Synchronous invocation
    LLMResponse invoke(const std::string& wish);
    
    // Asynchronous invocation
    void invokeAsync(const std::string& wish, PlanCallback callback);
    
    // Plan validation
    bool validatePlan(const ExecutionPlan& plan);
    
    // Cache management
    void clearCache();
    void setCacheEnabled(bool enabled);
    
    // Signal equivalents (callbacks)
    void setPlanGenerationStartedCallback(std::function<void(const std::string&)> callback);
    void setPlanGeneratedCallback(std::function<void(const LLMResponse&)> callback);
    void setInvocationErrorCallback(std::function<void(const std::string&, bool)> callback);
    
private:
    LLMBackend m_backend;
    std::string m_endpoint;
    std::string m_apiKey;
    std::string m_model;
    float m_temperature;
    int m_maxTokens;
    std::chrono::milliseconds m_timeout;
    bool m_cacheEnabled;
    
    std::unordered_map<std::string, LLMResponse> m_cache;
    
    std::function<void(const std::string&)> m_planGenerationStartedCallback;
    std::function<void(const LLMResponse&)> m_planGeneratedCallback;
    std::function<void(const std::string&, bool)> m_invocationErrorCallback;
    
    // Backend-specific implementations
    LLMResponse invokeOllama(const std::string& wish);
    LLMResponse invokeClaude(const std::string& wish);
    LLMResponse invokeOpenAI(const std::string& wish);
    LLMResponse invokeLocalGGUF(const std::string& wish);
    
    // Prompt building
    std::string buildPrompt(const std::string& wish);
    
    // Response parsing
    ExecutionPlan parseResponse(const std::string& response);
    
    // HTTP client (simplified)
    std::string httpPost(const std::string& url, const std::string& data, 
                        const std::string& contentType = "application/json");
    
    // Cache key generation
    std::string generateCacheKey(const std::string& wish);
};

} // namespace RawrXD

#endif // MODEL_INVOKER_H