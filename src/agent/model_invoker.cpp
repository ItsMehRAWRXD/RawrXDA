/**
 * @file model_invoker.cpp
 * @brief LLM invocation layer for wish-to-plan transformation (Qt-free)
 *
 * HTTP transport delegated to StlHttpClient (llm_http_client.hpp)
 * which handles WinHTTP on Windows and curl on POSIX.
 * Supports Ollama, Claude, and OpenAI backends.
 *
 * Post-Qt fix: futures are now retained via StlHttpClient/ChainStep
 * to prevent premature destruction that Qt's event loop hid.
 */
#include "model_invoker.hpp"
#include "llm_http_client.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// SCAFFOLD_289: Model invoker

namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// HTTP helper — thin wrapper over StlHttpClient for backward compat
// ---------------------------------------------------------------------------
namespace {

/// Synchronous HTTP POST returning the response body (empty on error).
/// Delegates to StlHttpClient singleton for proper async/future support.
std::string httpPost(const std::string& url, const std::string& body,
                     const std::string& apiKey = {},
                     const std::string& extraHeaderKey = {},
                     const std::string& extraHeaderVal = {},
                     int timeoutMs = 30000) {
    HttpRequest req;
    req.method         = "POST";
    req.url            = url;
    req.body           = body;
    req.apiKey         = apiKey;
    req.extraHeaderKey = extraHeaderKey;
    req.extraHeaderVal = extraHeaderVal;
    req.timeoutMs      = timeoutMs;

    HttpResponse resp = StlHttpClient::instance().send(req);
    if (resp.success) return resp.body;

    fprintf(stderr, "[WARN] [ModelInvoker] HTTP POST failed: %s (HTTP %d, %dms)\n",
            resp.error.c_str(), resp.statusCode, resp.latencyMs);
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// ModelInvoker
// ---------------------------------------------------------------------------

void ModelInvoker::setLLMBackend(const std::string& backend,
                                  const std::string& endpoint,
                                  const std::string& apiKey) {
    m_backend  = backend;
    m_endpoint = endpoint;
    m_apiKey   = apiKey;

    std::string be = backend;
    std::transform(be.begin(), be.end(), be.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (be == "ollama")       m_model = "mistral";
    else if (be == "claude")  m_model = "claude-3-sonnet-20240229";
    else if (be == "openai")  m_model = "gpt-4o";
    else if (be == "openai-turbo") m_model = "gpt-4-turbo";

    fprintf(stderr, "[INFO] [ModelInvoker] Backend: %s @ %s (model: %s)\n",
            backend.c_str(), endpoint.c_str(), m_model.c_str());
}

void ModelInvoker::setSystemPromptTemplate(const std::string& t) {
    m_customSystemPrompt = t;
}

void ModelInvoker::setCodebaseEmbeddings(const std::map<std::string, float>& e) {
    m_codebaseEmbeddings = e;
}

// ---------------------------------------------------------------------------
LLMResponse ModelInvoker::invoke(const InvocationParams& params) {
    // Cache
    if (m_cachingEnabled) {
        std::string key = getCacheKey(params);
        LLMResponse cached = getCachedResponse(key);
        if (cached.success) {
            fprintf(stderr, "[INFO] [ModelInvoker] Cache hit: %s\n",
                    params.wish.substr(0, 60).c_str());
            return cached;
        }
    }

    fprintf(stderr, "[INFO] [ModelInvoker] Invoking LLM: %s\n",
            params.wish.substr(0, 80).c_str());
    m_isInvoking = true;
    if (onPlanGenerationStarted) onPlanGenerationStarted(params.wish);

    LLMResponse response;

    try {
        std::string userMsg = buildUserMessage(params);
        json llmResp;

        std::string be = m_backend;
        std::transform(be.begin(), be.end(), be.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (be == "ollama")
            llmResp = sendOllamaRequest(m_model, userMsg, params.maxTokens, params.temperature);
        else if (be == "claude")
            llmResp = sendClaudeRequest(userMsg, params.maxTokens, params.temperature);
        else if (be == "openai" || be == "openai-turbo")
            llmResp = sendOpenAIRequest(userMsg, params.maxTokens, params.temperature);
        else {
            response.error = "Unknown backend: " + m_backend;
            m_isInvoking = false;
            return response;
        }

        // --- Fallback chain: if primary backend returned empty, try alternatives ---
        if (llmResp.empty() && !m_apiKey.empty()) {
            fprintf(stderr, "[WARN] [ModelInvoker] Primary backend '%s' failed, attempting fallback chain\n",
                    be.c_str());

            // Fallback 1: OpenAI GPT-4o
            if (be != "openai") {
                std::string savedModel = m_model;
                m_model = "gpt-4o";
                llmResp = sendOpenAIRequest(userMsg, params.maxTokens, params.temperature);
                if (!llmResp.empty()) {
                    be = "openai"; // Update for response parsing below
                    fprintf(stderr, "[INFO] [ModelInvoker] Fallback to GPT-4o succeeded\n");
                } else {
                    m_model = savedModel;
                }
            }

            // Fallback 2: GPT-4-turbo if GPT-4o also failed
            if (llmResp.empty() && m_model != "gpt-4-turbo") {
                std::string savedModel = m_model;
                m_model = "gpt-4-turbo";
                llmResp = sendOpenAIRequest(userMsg, params.maxTokens, params.temperature);
                if (!llmResp.empty()) {
                    be = "openai";
                    fprintf(stderr, "[INFO] [ModelInvoker] Fallback to GPT-4-turbo succeeded\n");
                } else {
                    m_model = savedModel;
                }
            }

            // Fallback 3: Ollama local
            if (llmResp.empty() && be != "ollama" && !m_endpoint.empty()) {
                std::string savedModel = m_model;
                m_model = "mistral";
                llmResp = sendOllamaRequest(m_model, userMsg, params.maxTokens, params.temperature);
                if (!llmResp.empty()) {
                    be = "ollama";
                    fprintf(stderr, "[INFO] [ModelInvoker] Fallback to Ollama/mistral succeeded\n");
                } else {
                    m_model = savedModel;
                }
            }
        }

        if (llmResp.empty()) {
            response.error = "Empty response from LLM";
            m_isInvoking = false;
            if (onInvocationError) onInvocationError(response.error, true);
            return response;
        }

        // Parse backend-specific format
        if (be == "ollama") {
            response.rawOutput  = llmResp.value("response", "");
            response.tokensUsed = llmResp.value("eval_count", 0)
                                + llmResp.value("prompt_eval_count", 0);
        } else if (be == "claude") {
            auto content = llmResp.value("content", json::array());
            if (!content.empty())
                response.rawOutput = content.at(0).value("text", "");
            if (llmResp.contains("usage"))
                response.tokensUsed = llmResp.at("usage").value("output_tokens", 0);
        } else if (be == "openai") {
            auto choices = llmResp.value("choices", json::array());
            if (!choices.empty())
                response.rawOutput = choices.at(0).value("message", json{}).value("content", "");
            if (llmResp.contains("usage"))
                response.tokensUsed = llmResp.at("usage").value("completion_tokens", 0);
        }

        fprintf(stderr, "[INFO] [ModelInvoker] Response (%d tokens): %.200s\n",
                response.tokensUsed, response.rawOutput.c_str());

        response.parsedPlan = parsePlan(response.rawOutput);

        if (!validatePlanSanity(response.parsedPlan)) {
            response.error   = "Plan failed sanity checks";
            response.success = false;
            m_isInvoking = false;
            if (onInvocationError) onInvocationError(response.error, true);
            return response;
        }

        response.success   = true;
        response.reasoning = llmResp.value("reasoning", "");

        if (m_cachingEnabled) cacheResponse(getCacheKey(params), response);

    } catch (const std::exception& e) {
        response.error   = std::string("Exception: ") + e.what();
        response.success = false;
        if (onInvocationError) onInvocationError(response.error, false);
    }

    m_isInvoking = false;
    return response;
}

void ModelInvoker::invokeAsync(const InvocationParams& params) {
    // Post-Qt fix: Use std::async + retain the future as member.
    // Qt's deleteLater() auto-managed reply lifetime; STL requires explicit
    // future retention or the destructor blocks/cancels the async operation.
    m_asyncFuture = std::async(std::launch::async, [this, params]() {
        LLMResponse resp = invoke(params);
        if (onPlanGenerated) onPlanGenerated(resp);
    });
}

void ModelInvoker::cancelPendingRequest() {
    m_isInvoking = false;
    fprintf(stderr, "[INFO] [ModelInvoker] Request cancelled\n");
}

// ---------------------------------------------------------------------------
// Prompt building
// ---------------------------------------------------------------------------
std::string ModelInvoker::buildSystemPrompt(const std::vector<std::string>& tools) {
    std::string p = R"(You are an intelligent IDE agent for the RawrXD code generation framework.

Your role is to transform natural language wishes into structured action plans that can be executed by an automated system.

# Available Tools
)";
    for (const auto& t : tools) p += "- " + t + "\n";

    p += R"(
# Response Format
Respond with a valid JSON array of actions. Each action must have:
- type: string (action type name)
- target: string (file, command, or target)
- params: object (action-specific parameters)
- description: string (human-readable description)

# Constraints
- Do NOT suggest destructive operations without explicit user intent
- Always break complex tasks into manageable steps
- Use existing patterns found in the codebase
)";
    return p;
}

std::string ModelInvoker::buildUserMessage(const InvocationParams& params) {
    std::string msg = "User Wish: " + params.wish + "\n\n";
    if (!params.context.empty())
        msg += "Context: " + params.context + "\n\n";
    if (!params.codebaseContext.empty())
        msg += "Relevant Codebase:\n" + params.codebaseContext + "\n\n";
    msg += "Please generate a structured action plan. Respond with ONLY valid JSON array.";
    return msg;
}

// ---------------------------------------------------------------------------
// Backend-specific HTTP calls
// ---------------------------------------------------------------------------
json ModelInvoker::sendOllamaRequest(const std::string& model,
                                      const std::string& prompt,
                                      int maxTokens, double temperature) {
    json payload = {
        {"model",       model},
        {"prompt",      prompt},
        {"temperature", temperature},
        {"num_predict", maxTokens},
        {"stream",      false}
    };

    std::string url = m_endpoint + "/api/generate";
    fprintf(stderr, "[INFO] [ModelInvoker] POST %s\n", url.c_str());
    std::string resp = httpPost(url, payload.dump(), {}, {}, {}, 30000);

    if (resp.empty()) return {};
    try { return json::parse(resp); }
    catch (const std::exception& e) {
        fprintf(stderr, "[WARN] [ModelInvoker] Ollama JSON parse error: %s\n", e.what());
        return {};
    }
}

json ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                      int maxTokens, double temperature) {
    json payload = {
        {"model",      m_model},
        {"max_tokens", maxTokens},
        {"temperature", temperature},
        {"messages",   json::array({{{"role", "user"}, {"content", prompt}}})}
    };

    std::string resp = httpPost("https://api.anthropic.com/v1/messages",
                                payload.dump(), m_apiKey,
                                "x-api-key", m_apiKey, 30000);
    // Note: Claude uses x-api-key header, not Bearer token; we send both

    if (resp.empty()) return {};
    try { return json::parse(resp); }
    catch (const std::exception& e) {
        fprintf(stderr, "[WARN] [ModelInvoker] Claude JSON parse error: %s\n", e.what());
        return {};
    }
}

json ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                      int maxTokens, double temperature) {
    json payload = {
        {"model",      m_model},
        {"max_tokens", maxTokens},
        {"temperature", temperature},
        {"messages",   json::array({{{"role", "user"}, {"content", prompt}}})}
    };

    std::string resp = httpPost("https://api.openai.com/v1/chat/completions",
                                payload.dump(), m_apiKey, {}, {}, 30000);

    if (resp.empty()) return {};
    try { return json::parse(resp); }
    catch (const std::exception& e) {
        fprintf(stderr, "[WARN] [ModelInvoker] OpenAI JSON parse error: %s\n", e.what());
        return {};
    }
}

// ---------------------------------------------------------------------------
// Plan parsing
// ---------------------------------------------------------------------------
json ModelInvoker::parsePlan(const std::string& llmOutput) {
    // Strategy 1: Extract ```json ... ``` block
    std::regex jsonBlock(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)");
    std::smatch match;
    if (std::regex_search(llmOutput, match, jsonBlock)) {
        try {
            auto doc = json::parse(match[1].str());
            if (doc.is_array()) return doc;
        } catch (...) {}
    }

    // Strategy 2: Parse entire output
    try {
        auto doc = json::parse(llmOutput);
        if (doc.is_array()) return doc;
    } catch (...) {}

    // Strategy 3: Fallback
    fprintf(stderr, "[WARN] [ModelInvoker] Failed to parse plan from LLM output\n");
    return json::array({{
        {"type",        "user_input"},
        {"description", llmOutput.substr(0, 500)}
    }});
}

bool ModelInvoker::validatePlanSanity(const json& plan) {
    if (!plan.is_array() || plan.empty()) {
        fprintf(stderr, "[WARN] [ModelInvoker] Empty plan\n");
        return false;
    }

    int count = 0;
    std::vector<std::string> seen;
    for (size_t i = 0; i < plan.size(); ++i) {
        const auto& action = plan.at(i);
        if (!action.is_object()) return false;
        std::string type = action.value("type", "");

        if (type == "file_delete" || type == "format_drive" || type == "system_reboot") {
            fprintf(stderr, "[WARN] [ModelInvoker] Dangerous op: %s\n", type.c_str());
            return false;
        }

        std::string target = action.value("target", "");
        if (!target.empty() &&
            std::find(seen.begin(), seen.end(), target) != seen.end()) {
            fprintf(stderr, "[WARN] [ModelInvoker] Circular dep on: %s\n", target.c_str());
            return false;
        }
        seen.push_back(target);

        if (++count > 100) {
            fprintf(stderr, "[WARN] [ModelInvoker] Plan too large\n");
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Cache
// ---------------------------------------------------------------------------
std::string ModelInvoker::getCacheKey(const InvocationParams& params) const {
    return params.wish.substr(0, 100);
}

LLMResponse ModelInvoker::getCachedResponse(const std::string& key) const {
    auto it = m_responseCache.find(key);
    if (it != m_responseCache.end()) return it->second;
    return {};
}

void ModelInvoker::cacheResponse(const std::string& key, const LLMResponse& resp) {
    m_responseCache[key] = resp;
}
