/**
 * @file llm_http_bridge.hpp
 * @brief Bridges StlHttpClient to ChainOfThoughtEngine and ModelInvoker
 *
 * Provides factory functions that create properly configured
 * CoTInferenceCallback instances backed by StlHttpClient (WinHTTP/curl).
 *
 * Compatible with both CLI (RawrEngine) and GUI (RawrXD-Win32IDE).
 * No Qt dependencies. All structured results, no exceptions.
 */
#pragma once

#include "llm_http_client.hpp"
#include "../include/chain_of_thought_engine.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <string>

namespace llm_bridge {

using json = nlohmann::json;

/**
 * @brief Create a CoTInferenceCallback that calls Ollama via StlHttpClient
 * @param endpoint Base URL, e.g. "http://localhost:11434"
 * @param defaultModel Default model name, e.g. "bigdaddyg-fast:latest"
 */
inline CoTInferenceCallback makeOllamaCallback(
    const std::string& endpoint = "http://localhost:11434",
    const std::string& defaultModel = "bigdaddyg-fast:latest")
{
    return [endpoint, defaultModel](
        const std::string& systemPrompt,
        const std::string& userMessage,
        const std::string& model) -> std::string
    {
        std::string useModel = model.empty() ? defaultModel : model;

        json payload = {
            {"model",       useModel},
            {"prompt",      userMessage},
            {"system",      systemPrompt},
            {"temperature", 0.7},
            {"num_predict", 2000},
            {"stream",      false}
        };

        HttpResponse resp = StlHttpClient::instance().postJson(
            endpoint + "/api/generate", payload.dump(), 60000);

        if (!resp.success) {
            fprintf(stderr, "[WARN] [LLM Bridge] Ollama POST failed: %s\n",
                    resp.error.c_str());
            return {};
        }

        try {
            auto doc = json::parse(resp.body);
            return doc.value("response", "");
        } catch (const json::exception& e) {
            fprintf(stderr, "[WARN] [LLM Bridge] JSON parse error: %s\n", e.what());
            return {};
        }
    };
}

/**
 * @brief Create a CoTInferenceCallback that calls OpenAI-compatible APIs
 * @param endpoint API endpoint URL
 * @param apiKey Bearer token
 * @param defaultModel Default model name
 */
inline CoTInferenceCallback makeOpenAICallback(
    const std::string& endpoint = "https://api.openai.com/v1/chat/completions",
    const std::string& apiKey = "",
    const std::string& defaultModel = "gpt-4-turbo")
{
    return [endpoint, apiKey, defaultModel](
        const std::string& systemPrompt,
        const std::string& userMessage,
        const std::string& model) -> std::string
    {
        std::string useModel = model.empty() ? defaultModel : model;

        json payload = {
            {"model", useModel},
            {"max_tokens", 2000},
            {"temperature", 0.7},
            {"messages", json::array({
                {{"role", "system"}, {"content", systemPrompt}},
                {{"role", "user"},   {"content", userMessage}}
            })}
        };

        HttpRequest req;
        req.method    = "POST";
        req.url       = endpoint;
        req.body      = payload.dump();
        req.apiKey    = apiKey;
        req.timeoutMs = 60000;

        HttpResponse resp = StlHttpClient::instance().send(req);
        if (!resp.success) {
            fprintf(stderr, "[WARN] [LLM Bridge] OpenAI POST failed: %s\n",
                    resp.error.c_str());
            return {};
        }

        try {
            auto doc = json::parse(resp.body);
            auto choices = doc.value("choices", json::array());
            if (!choices.empty()) {
                return choices[0].value("message", json{}).value("content", "");
            }
            return {};
        } catch (const json::exception& e) {
            fprintf(stderr, "[WARN] [LLM Bridge] JSON parse error: %s\n", e.what());
            return {};
        }
    };
}

/**
 * @brief Create a CoTInferenceCallback that calls Claude API
 * @param apiKey Anthropic API key (sent as both x-api-key and Bearer)
 * @param defaultModel Default model name
 */
inline CoTInferenceCallback makeClaudeCallback(
    const std::string& apiKey = "",
    const std::string& defaultModel = "claude-3-sonnet-20240229")
{
    return [apiKey, defaultModel](
        const std::string& systemPrompt,
        const std::string& userMessage,
        const std::string& model) -> std::string
    {
        std::string useModel = model.empty() ? defaultModel : model;

        json payload = {
            {"model", useModel},
            {"max_tokens", 2000},
            {"temperature", 0.7},
            {"system", systemPrompt},
            {"messages", json::array({
                {{"role", "user"}, {"content", userMessage}}
            })}
        };

        HttpRequest req;
        req.method         = "POST";
        req.url            = "https://api.anthropic.com/v1/messages";
        req.body           = payload.dump();
        req.apiKey         = apiKey;
        req.extraHeaderKey = "x-api-key";
        req.extraHeaderVal = apiKey;
        req.timeoutMs      = 60000;

        HttpResponse resp = StlHttpClient::instance().send(req);
        if (!resp.success) {
            fprintf(stderr, "[WARN] [LLM Bridge] Claude POST failed: %s\n",
                    resp.error.c_str());
            return {};
        }

        try {
            auto doc = json::parse(resp.body);
            auto content = doc.value("content", json::array());
            if (!content.empty()) {
                return content[0].value("text", "");
            }
            return {};
        } catch (const json::exception& e) {
            fprintf(stderr, "[WARN] [LLM Bridge] JSON parse error: %s\n", e.what());
            return {};
        }
    };
}

/**
 * @brief Wire a ChainOfThoughtEngine to use Ollama via StlHttpClient.
 * Call once at startup to configure the CoT engine for Ollama inference.
 * Works identically in CLI and Win32 GUI contexts.
 */
inline void wireCoTToOllama(
    ChainOfThoughtEngine& engine,
    const std::string& endpoint = "http://localhost:11434",
    const std::string& model = "bigdaddyg-fast:latest")
{
    engine.setDefaultModel(model);
    engine.setInferenceCallback(makeOllamaCallback(endpoint, model));
    fprintf(stderr, "[INFO] [LLM Bridge] CoT wired to Ollama at %s model=%s\n",
            endpoint.c_str(), model.c_str());
}

/**
 * @brief Quick connectivity test — hit Ollama /api/tags endpoint.
 * Returns true if the server is reachable and responds.
 */
inline bool testOllamaConnection(const std::string& endpoint = "http://localhost:11434") {
    HttpResponse resp = StlHttpClient::instance().get(endpoint + "/api/tags", 5000);
    if (resp.success) {
        fprintf(stderr, "[INFO] [LLM Bridge] Ollama alive at %s (%dms)\n",
                endpoint.c_str(), resp.latencyMs);
        return true;
    }
    fprintf(stderr, "[WARN] [LLM Bridge] Ollama unreachable at %s: %s\n",
            endpoint.c_str(), resp.error.c_str());
    return false;
}

/**
 * @brief List available models from Ollama.
 * Returns a JSON array of model names, or empty array on failure.
 */
inline std::vector<std::string> listOllamaModels(
    const std::string& endpoint = "http://localhost:11434")
{
    std::vector<std::string> models;
    HttpResponse resp = StlHttpClient::instance().get(endpoint + "/api/tags", 5000);
    if (!resp.success) return models;

    try {
        auto doc = json::parse(resp.body);
        auto arr = doc.value("models", json::array());
        for (const auto& m : arr) {
            models.push_back(m.value("name", ""));
        }
    } catch (...) {}
    return models;
}

} // namespace llm_bridge
