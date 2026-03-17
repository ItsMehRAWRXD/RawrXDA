/**
 * @file model_invoker.cpp
 * @brief Implementation of LLM invocation layer
 *
 * Provides synchronous and asynchronous wish→plan transformation
 * with support for Ollama (local) and cloud LLMs.
 */

#include "model_invoker.hpp"
#include "json_types.hpp"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>
/**
 * @brief Constructor - initializes network manager and default settings
 */
ModelInvoker::ModelInvoker()
    : m_networkManager(std::make_unique<void/*NetManager*/>(this))
{
    m_backend = "ollama";
    m_endpoint = "http://localhost:11434";

    // Load cached responses from disk if available
    std::string cacheDir = std::filesystem::path::writableLocation(std::filesystem::path::CacheLocation);
    if (!cacheDir.empty()) {
        std::filesystem::path dir(cacheDir);
        if (!dir.exists("agent_cache")) {
            dir.mkdir("agent_cache");
        }
    }
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
    m_backend = backend/* .toLower() - use std::transform */;
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

    /* qInfo removed */ << "[ModelInvoker] Backend set to" << m_backend << "at" << m_endpoint;
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
            fprintf(stderr, "%s\\n", std::string("[ModelInvoker] Cache hit for:" << params.wish;
            return cached;
        }
    }

    fprintf(stderr, "%s\\n", std::string("[ModelInvoker] Invoking LLM with wish:" << params.wish;
    m_isInvoking = true;
    planGenerationStarted(params.wish);

    LLMResponse response;

    try {
        // Build prompts
        std::string systemPrompt = m_customSystemPrompt.empty()
                                   ? buildSystemPrompt(params.availableTools)
                                   : m_customSystemPrompt;
        std::string userMessage = buildUserMessage(params);

        JsonObject llmResponse;

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
            invocationError(response.error, true);
            return response;
        }

        // Parse backend-specific response format
        if (m_backend == "ollama") {
            response.rawOutput = llmResponse.value("response").toString();
            response.tokensUsed = llmResponse.value("eval_count").toInt() + 
                                  llmResponse.value("prompt_eval_count").toInt();
        } else if (m_backend == "claude") {
            auto content = llmResponse.value("content").toArray();
            if (!content.empty()) {
                response.rawOutput = content[0].value("text").toString();
            }
            response.tokensUsed = llmResponse.value("usage").value("output_tokens").toInt();
        } else if (m_backend == "openai") {
            auto choices = llmResponse.value("choices").toArray();
            if (!choices.empty()) {
                response.rawOutput = choices[0].value("message").value("content").toString();
            }
            response.tokensUsed = llmResponse.value("usage").value("completion_tokens").toInt();
        }

        fprintf(stderr, "%s\\n", std::string("[ModelInvoker] LLM response:" << response.rawOutput.left(200);

        // Parse into structured plan
        response.parsedPlan = parsePlan(response.rawOutput);

        // Validate sanity
        if (!validatePlanSanity(response.parsedPlan)) {
            response.error = "Plan failed sanity checks";
            response.success = false;
            m_isInvoking = false;
            invocationError(response.error, true);
            return response;
        }

        response.success = true;
        response.reasoning = llmResponse.value("reasoning").toString();

        // Cache successful response
        if (m_cachingEnabled) {
            cacheResponse(getCacheKey(params), response);
        }

        /* qInfo removed */ << "[ModelInvoker] Generated plan with" << response.parsedPlan.size() << "actions";

    } catch (const std::exception& e) {
        response.error = std::string("Exception: ") + e.what();
        response.success = false;
        invocationError(response.error, false);
    }

    m_isInvoking = false;
    return response;
}

/**
 * @brief Asynchronous invocation (non-blocking)
 */
void ModelInvoker::invokeAsync(const InvocationParams& params)
{
    std::async(std::launch::async, ([this, params]() {
        LLMResponse response = invoke(params);
        planGenerated(response);
    });
}

/**
 * @brief Cancel pending request
 */
void ModelInvoker::cancelPendingRequest()
{
    m_isInvoking = false;
    fprintf(stderr, "%s\\n", std::string("[ModelInvoker] Request cancelled";
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

    for (const std::string& tool : tools) {
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
  },
  {
    "type": "file_edit",
    "target": "src/main.cpp",
    "params": { "action": "append", "content": "// new code" },
    "description": "Add new functionality"
  },
  {
    "type": "build",
    "target": "all",
    "params": { "config": "Release" },
    "description": "Build all targets"
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

/**
 * @brief Send request to Ollama API
 */
JsonObject ModelInvoker::sendOllamaRequest(const std::string& model,
                                            const std::string& prompt,
                                            int maxTokens,
                                            double temperature)
{
    std::string/*url*/ url(m_endpoint + "/api/generate");
    void/*NetRequest*/ request(url);
    request.setHeader(void/*NetRequest*/::ContentTypeHeader, "application/json");

    JsonObject payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["temperature"] = temperature;
    payload["num_predict"] = maxTokens;
    payload["stream"] = false;

    JsonDoc doc(payload);
    std::vector<uint8_t> data = doc.toJson();

    fprintf(stderr, "%s\\n", std::string("[ModelInvoker] Sending request to Ollama:" << url.toString();

    // Synchronous request using event loop
    void/*EventLoop*/ loop;
    void/*NetReply*/* reply = m_networkManager->post(request, data);

    /* FIXME: convert to callback: connect(reply, &void/*NetReply*/::finished, &loop, &void/*EventLoop*/::quit); */

    std::function<void()>/*timer*/::singleShot(30000, &loop, &void/*EventLoop*/::quit); // 30s timeout

    loop.exec();

    if (reply->error() != void/*NetReply*/::NoError) {
        fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Network error:" << reply->errorString();
        reply->deleteLater();
        return JsonObject();
    }

    std::vector<uint8_t> responseData = reply->readAll();
    reply->deleteLater();

    JsonDoc responseDoc = JsonDoc::fromJson(responseData);
    return responseDoc.object();
}

/**
 * @brief Send request to Claude API
 */
JsonObject ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                            int maxTokens,
                                            double temperature)
{
    std::string/*url*/ url("https://api.anthropic.com/v1/messages");
    void/*NetRequest*/ request(url);
    request.setHeader(void/*NetRequest*/::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey/* .c_str() */);
    request.setRawHeader("anthropic-version", "2023-06-01");

    JsonObject payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    JsonArray messages;
    JsonObject message;
    message["role"] = "user";
    message["content"] = prompt;
    messages.append(message);
    payload["messages"] = messages;

    JsonDoc doc(payload);
    std::vector<uint8_t> data = doc.toJson();

    void/*EventLoop*/ loop;
    void/*NetReply*/* reply = m_networkManager->post(request, data);
    /* FIXME: convert to callback: connect(reply, &void/*NetReply*/::finished, &loop, &void/*EventLoop*/::quit); */
    std::function<void()>/*timer*/::singleShot(30000, &loop, &void/*EventLoop*/::quit);
    loop.exec();

    if (reply->error() != void/*NetReply*/::NoError) {
        fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Claude API error:" << reply->errorString();
        reply->deleteLater();
        return JsonObject();
    }

    std::vector<uint8_t> responseData = reply->readAll();
    reply->deleteLater();

    JsonDoc responseDoc = JsonDoc::fromJson(responseData);
    return responseDoc.object();
}

/**
 * @brief Send request to OpenAI API
 */
JsonObject ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                            int maxTokens,
                                            double temperature)
{
    std::string/*url*/ url("https://api.openai.com/v1/chat/completions");
    void/*NetRequest*/ request(url);
    request.setHeader(void/*NetRequest*/::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey)/* .c_str() */);

    JsonObject payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    JsonArray messages;
    JsonObject message;
    message["role"] = "user";
    message["content"] = prompt;
    messages.append(message);
    payload["messages"] = messages;

    JsonDoc doc(payload);
    std::vector<uint8_t> data = doc.toJson();

    void/*EventLoop*/ loop;
    void/*NetReply*/* reply = m_networkManager->post(request, data);
    /* FIXME: convert to callback: connect(reply, &void/*NetReply*/::finished, &loop, &void/*EventLoop*/::quit); */
    std::function<void()>/*timer*/::singleShot(30000, &loop, &void/*EventLoop*/::quit);
    loop.exec();

    if (reply->error() != void/*NetReply*/::NoError) {
        fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] OpenAI API error:" << reply->errorString();
        reply->deleteLater();
        return JsonObject();
    }

    std::vector<uint8_t> responseData = reply->readAll();
    reply->deleteLater();

    JsonDoc responseDoc = JsonDoc::fromJson(responseData);
    return responseDoc.object();
}

/**
 * @brief Parse LLM response into structured plan
 */
JsonArray ModelInvoker::parsePlan(const std::string& llmOutput)
{
    // Strategy 1: Direct JSON extraction (```json ... ```)
    std::regex jsonBlockRegex(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)", 
                                       std::regex::MultilineOption);
    std::smatch match = jsonBlockRegex.match(llmOutput);

    if (match/* .hasMatch() */ size() > 0) {
        std::string jsonStr = match.captured(1);
        JsonDoc doc = JsonDoc::fromJson(jsonStr/* .c_str() */);
        if (doc.isArray()) {
            return doc.array();
        }
    }

    // Strategy 2: Try parsing entire output as JSON
    JsonDoc doc = JsonDoc::fromJson(llmOutput/* .c_str() */);
    if (doc.isArray()) {
        return doc.array();
    }

    // Strategy 3: Fallback - create generic action
    fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Failed to parse plan from LLM output";
    JsonArray fallback;
    JsonObject action;
    action["type"] = "user_input";
    action["description"] = llmOutput.left(500);
    fallback.append(action);
    return fallback;
}

/**
 * @brief Validate plan sanity
 */
bool ModelInvoker::validatePlanSanity(const JsonArray& plan)
{
    if (plan.empty()) {
        fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Empty plan detected";
        return false;
    }

    int actionCount = 0;
    std::vector<std::string> seenTargets;

    for (const JsonValue& val : plan) {
        if (!val.isObject()) {
            fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Non-object in plan";
            return false;
        }

        JsonObject action = val;
        std::string type = action.value("type").toString();

        // Check for dangerous operations
        if (type == "file_delete" || type == "format_drive" || type == "system_reboot") {
            fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Dangerous operation detected:" << type;
            return false;
        }

        // Check for circular dependencies
        std::string target = action.value("target").toString();
        if (seenTargets.contains(target)) {
            fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Circular dependency on target:" << target;
            return false;
        }
        seenTargets.append(target);

        actionCount++;
        if (actionCount > 100) {
            fprintf(stderr, "[WARN] %s\\n", std::string("[ModelInvoker] Plan too large (>100 actions)";
            return false;
        }
    }

    return true;
}

/**
 * @brief Get cache key for request
 */
std::string ModelInvoker::getCacheKey(const InvocationParams& params) const
{
    return params.wish.mid(0, 100);
}

/**
 * @brief Load cached response
 */
LLMResponse ModelInvoker::getCachedResponse(const std::string& key) const
{
    if (m_responseCache.contains(key)) {
        return m_responseCache[key];
    }
    return LLMResponse();
}

/**
 * @brief Store response in cache
 */
void ModelInvoker::cacheResponse(const std::string& key, const LLMResponse& response)
{
    m_responseCache[key] = response;
}
