/**
 * @file model_invoker.cpp
 * @brief Implementation of LLM invocation layer
 *
 * Provides synchronous and asynchronous wish→plan transformation
 * with support for Ollama (local) and cloud LLMs.
 */

#include "model_invoker.hpp"
#include <regex>
#include <future>
#include <algorithm>
#include <iostream>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

/**
 * @brief Constructor
 */
ModelInvoker::ModelInvoker()
{
    m_backend = "ollama";
    m_endpoint = "http://localhost:11434";
}

/**
 * @brief Destructor
 */
ModelInvoker::~ModelInvoker() = default;

/**
 * @brief Set the LLM backend and endpoint
 */
void ModelInvoker::setLLMBackend(const std::string& backend,
                                  const std::string& endpoint,
                                  const std::string& apiKey)
{
    m_backend = backend;
    std::transform(m_backend.begin(), m_backend.end(), m_backend.begin(), ::tolower);
    m_endpoint = endpoint;
    m_apiKey = apiKey;

    // Set default model based on backend
    if (m_backend == "ollama") {
        m_model = "mistral";
    } else if (m_backend == "claude") {
        m_model = "claude-3-sonnet-20240229";
    } else if (m_backend == "openai") {
        m_model = "gpt-4-turbo";
    }
}

/**
 * @brief Set custom system prompt template
 */
void ModelInvoker::setSystemPromptTemplate(const std::string& template_)
{
    m_customSystemPrompt = template_;
}

/**
 * @brief Set codebase embeddings for RAG
 */
void ModelInvoker::setCodebaseEmbeddings(const std::map<std::string, float>& embeddings)
{
    m_codebaseEmbeddings = embeddings;
}

/**
 * @brief Synchronous invocation (blocks caller)
 */
LLMResponse ModelInvoker::invoke(const InvocationParams& params)
{
    // Check cache first
    if (m_cachingEnabled) {
        std::string cacheKey = getCacheKey(params);
        LLMResponse cached = getCachedResponse(cacheKey);
        if (cached.success) {
            return cached;
        }
    }

    m_isInvoking = true;
    if (onPlanGenerationStarted) {
        onPlanGenerationStarted(params.wish);
    }

    LLMResponse response;

    try {
        // Build prompts
        std::string systemPrompt = m_customSystemPrompt.empty()
                                   ? buildSystemPrompt(params.availableTools)
                                   : m_customSystemPrompt;
        std::string userMessage = buildUserMessage(params);

        nlohmann::json llmResponse;

        // Invoke appropriate backend
        if (m_backend == "ollama") {
            llmResponse = sendOllamaRequest(m_model, userMessage, params.maxTokens, params.temperature);
        } else if (m_backend == "claude") {
            llmResponse = sendClaudeRequest(userMessage, params.maxTokens, params.temperature);
        } else if (m_backend == "openai") {
            llmResponse = sendOpenAIRequest(userMessage, params.maxTokens, params.temperature);
        } else {
            response.error = "Unknown backend: " + m_backend;
            m_isInvoking = false;
            return response;
        }

        // Extract response text
        if (llmResponse.empty()) {
            response.error = "Empty response from LLM";
            m_isInvoking = false;
            if (onInvocationError) onInvocationError(response.error, true);
            return response;
        }

        // Parse backend-specific response format
        if (m_backend == "ollama") {
            if (llmResponse.contains("response")) response.rawOutput = llmResponse["response"].get<std::string>();
            if (llmResponse.contains("eval_count") && llmResponse.contains("prompt_eval_count")) {
                response.tokensUsed = llmResponse["eval_count"].get<int>() + 
                                      llmResponse["prompt_eval_count"].get<int>();
            }
        } else if (m_backend == "claude") {
            if (llmResponse.contains("content") && llmResponse["content"].is_array() && !llmResponse["content"].empty()) {
                response.rawOutput = llmResponse["content"][0]["text"].get<std::string>();
            }
            if (llmResponse.contains("usage")) {
                response.tokensUsed = llmResponse["usage"]["output_tokens"].get<int>();
            }
        } else if (m_backend == "openai") {
            if (llmResponse.contains("choices") && llmResponse["choices"].is_array() && !llmResponse["choices"].empty()) {
                response.rawOutput = llmResponse["choices"][0]["message"]["content"].get<std::string>();
            }
            if (llmResponse.contains("usage")) {
                response.tokensUsed = llmResponse["usage"]["completion_tokens"].get<int>();
            }
        }

        // Parse into structured plan
        response.parsedPlan = parsePlan(response.rawOutput);

        // Validate sanity
        if (!validatePlanSanity(response.parsedPlan)) {
            response.error = "Plan failed sanity checks";
            response.success = false;
            m_isInvoking = false;
            if (onInvocationError) onInvocationError(response.error, true);
            return response;
        }

        response.success = true;
        if (llmResponse.contains("reasoning")) response.reasoning = llmResponse["reasoning"].get<std::string>();

        // Cache successful response
        if (m_cachingEnabled) {
            cacheResponse(getCacheKey(params), response);
        }

    } catch (const std::exception& e) {
        response.error = "Exception: " + std::string(e.what());
        response.success = false;
        if (onInvocationError) onInvocationError(response.error, false);
    }

    m_isInvoking = false;
    return response;
}

/**
 * @brief Asynchronous invocation (non-blocking)
 */
void ModelInvoker::invokeAsync(const InvocationParams& params)
{
    std::thread([this, params]() {
        LLMResponse response = invoke(params);
        if (onPlanGenerated) {
            onPlanGenerated(response);
        }
    }).detach();
}

/**
 * @brief Cancel pending request
 */
void ModelInvoker::cancelPendingRequest()
{
    m_isInvoking = false;
}

/**
 * @brief Build system prompt with tool descriptions
 */
std::string ModelInvoker::buildSystemPrompt(const std::vector<std::string>& tools)
{
    std::string prompt = R"(You are an intelligent IDE agent for the RawrXD code generation framework.

Your role is to transform natural language wishes into structured action plans that can be executed by an automated system.

# Available Tools
You can use the following tools:
)";

    for (const auto& tool : tools) {
        prompt += "- " + tool + "\n";
    }

    prompt += R"(
# Response Format
You MUST respond with a valid JSON array of actions. Each action must have:
- type: string (action type name)
- target: string (file, command, or target)
- params: object (action-specific parameters)
- description: string (human-readable description)

Example:
```json
[
  {
    "type": "search_files",
    "target": "src/",
    "params": { "pattern": "*.cpp", "query": "TODO" },
    "description": "Find all TODO comments in C++ files"
  }
]
```

# Constraints
- Do NOT suggest destructive operations without explicit user intent
- Do NOT modify system files or configuration files without user approval
- Do NOT create infinite loops or recursive procedures
- Always break complex tasks into manageable steps
- Use existing patterns found in the codebase

# Context
The system is RawrXD: A production-grade IDE for GGUF quantization and model serving.
Current capabilities include: file search, text editing, project builds, test execution, and code generation.
)";

    return prompt;
}

/**
 * @brief Build user message with wish and context
 */
std::string ModelInvoker::buildUserMessage(const InvocationParams& params)
{
    std::string message = "User Wish: " + params.wish + "\n\n";

    if (!params.context.empty()) {
        message += "Context: " + params.context + "\n\n";
    }

    if (!params.codebaseContext.empty()) {
        message += "Relevant Codebase:\n" + params.codebaseContext + "\n\n";
    }

    message += "Please generate a structured action plan to fulfill this wish. "
               "Respond with ONLY valid JSON array, no additional text.";

    return message;
}

nlohmann::json ModelInvoker::sendOllamaRequest(const std::string& model,
                                                const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["temperature"] = temperature;
    payload["num_predict"] = maxTokens;
    payload["stream"] = false;

    std::string url = m_endpoint + "/api/generate";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {{"Content-Type", "application/json"}});

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});

    std::string url = "https://api.anthropic.com/v1/messages";
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"x-api-key", m_apiKey},
        {"anthropic-version", "2023-06-01"}
    };

    std::string responseData = performHttpRequest(url, "POST", payload.dump(), headers);

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});

    std::string url = "https://api.openai.com/v1/chat/completions";
    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + m_apiKey}
    };

    std::string responseData = performHttpRequest(url, "POST", payload.dump(), headers);

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::parsePlan(const std::string& llmOutput)
{
    std::regex jsonBlockRegex(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)");
    std::smatch match;

    if (std::regex_search(llmOutput, match, jsonBlockRegex)) {
        try {
            return nlohmann::json::parse(match.str(1));
        } catch (...) {}
    }

    try {
        return nlohmann::json::parse(llmOutput);
    } catch (...) {}

    nlohmann::json fallback = nlohmann::json::array();
    fallback.push_back({{"type", "user_input"}, {"description", llmOutput.substr(0, 500)}});
    return fallback;
}

bool ModelInvoker::validatePlanSanity(const nlohmann::json& plan)
{
    if (!plan.is_array() || plan.empty()) return false;

    int actionCount = 0;
    std::vector<std::string> seenTargets;

    for (const auto& action : plan) {
        if (!action.is_object()) return false;

        std::string type = action.value("type", "");
        if (type == "file_delete" || type == "format_drive" || type == "system_reboot") return false;

        std::string target = action.value("target", "");
        if (!target.empty()) {
            if (std::find(seenTargets.begin(), seenTargets.end(), target) != seenTargets.end()) return false;
            seenTargets.push_back(target);
        }

        if (++actionCount > 100) return false;
    }

    return true;
}

std::string ModelInvoker::getCacheKey(const InvocationParams& params) const
{
    return params.wish.substr(0, 100);
}

LLMResponse ModelInvoker::getCachedResponse(const std::string& key) const
{
    auto it = m_responseCache.find(key);
    if (it != m_responseCache.end()) return it->second;
    return LLMResponse();
}

void ModelInvoker::cacheResponse(const std::string& key, const LLMResponse& response)
{
    m_responseCache[key] = response;
}

std::string ModelInvoker::performHttpRequest(const std::string& url, 
                                             const std::string& method, 
                                             const std::string& body, 
                                             const std::map<std::string, std::string>& headers)
{
    // Headless implementation placeholder
    // In a real headless C++ app, this would use libcurl, CPR, or WinHTTP.
    // For now, we return empty or stubbed data if needed.
    return "{}"; 
}
