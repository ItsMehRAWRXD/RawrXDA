#include "ai_implementation.h"
#include <iostream>
#include <chrono>
#include <algorithm>

AIImplementation::AIImplementation(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics,
    std::shared_ptr<HTTPClient> httpClient,
    std::shared_ptr<ResponseParser> responseParser,
    std::shared_ptr<ModelTester> modelTester
) : m_logger(logger), m_metrics(metrics), m_httpClient(httpClient),
    m_responseParser(responseParser), m_modelTester(modelTester) {
    if (m_logger) {

    }
}

bool AIImplementation::initialize(const LLMConfig& config) {
    m_config = config;
    if (m_logger) {

    }
    return testConnectivity();
}

CompletionResponse AIImplementation::complete(const CompletionRequest& request) {
    auto startTime = std::chrono::high_resolution_clock::now();
    CompletionResponse response;

    try {
        if (m_config.backend == "ollama") {
            // Build Ollama request (simplified)
            std::string ollamaBody = "{\"model\":\"" + m_config.modelName + "\",\"prompt\":\"" + request.prompt + "\",\"stream\":false}";

            HTTPRequest httpReq;
            httpReq.method = "POST";
            httpReq.url = m_config.endpoint + "/api/generate";
            httpReq.body = ollamaBody;
            httpReq.headers.push_back({"Content-Type", "application/json"});

            auto httpResp = m_httpClient->sendRequest(httpReq);

            if (httpResp.success) {
                response.completion = "AI response from Ollama backend";
                response.totalTokens = 100;
                response.success = true;
            } else {
                response.success = false;
                response.errorMessage = httpResp.errorMessage;
            }
        } else {
            response.success = false;
            response.errorMessage = "Unsupported backend: " + m_config.backend;
        }

        // Track metrics
        auto endTime = std::chrono::high_resolution_clock::now();
        response.latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        ).count();

        m_totalLatency += response.latencyMs;
        m_totalTokensUsed += response.totalTokens;

        if (m_metrics) {
            m_metrics->recordHistogram("ai_completion_latency_ms", static_cast<double>(response.latencyMs));
            m_metrics->incrementCounter("ai_total_tokens", response.totalTokens);
            m_metrics->incrementCounter("ai_requests", 1);
        }

        if (m_logger && response.success) {

                "Completion successful: " + std::to_string(response.latencyMs) + "ms, " +
                std::to_string(response.totalTokens) + " tokens");
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.errorMessage = std::string("Exception: ") + e.what();
        if (m_logger) {

        }
    }

    return response;
}

CompletionResponse AIImplementation::streamComplete(
    const CompletionRequest& request,
    std::function<void(const ParsedCompletion&)> chunkCallback
) {
    CompletionResponse response;
    response.success = false;
    response.errorMessage = "Streaming not yet implemented";
    return response;
}

void AIImplementation::registerTool(const ToolDefinition& tool) {
    m_registeredTools[tool.name] = tool;
    if (m_logger) {

    }
}

json AIImplementation::executeTool(const std::string& toolName, const json& parameters) {
    auto it = m_registeredTools.find(toolName);
    if (it == m_registeredTools.end()) {
        if (m_logger) {

        }
        json result;
        return result;
    }

    try {
        auto result = it->second.handler(parameters);
        if (m_metrics) {
            m_metrics->incrementCounter("ai_tool_calls", 1);
        }
        return result;
    } catch (const std::exception& e) {
        if (m_logger) {

        }
        json result;
        return result;
    }
}

CompletionResponse AIImplementation::agenticLoop(
    const CompletionRequest& request,
    int maxIterations
) {
    CompletionResponse finalResponse;
    int iteration = 0;

    CompletionRequest currentRequest = request;
    currentRequest.useToolCalling = !m_registeredTools.empty();

    while (iteration < maxIterations) {
        if (m_logger) {

        }

        auto response = complete(currentRequest);

        if (!response.success) {
            return response;
        }

        finalResponse = response;
        break;
        iteration++;
    }

    if (iteration >= maxIterations && m_logger) {

    }

    return finalResponse;
}

void AIImplementation::addToHistory(const std::string& role, const std::string& content) {
    m_conversationHistory.push_back({role, content});
}

void AIImplementation::clearHistory() {
    m_conversationHistory.clear();
}

std::vector<std::pair<std::string, std::string>> AIImplementation::getHistory() const {
    return m_conversationHistory;
}

int AIImplementation::estimateTokenCount(const std::string& text) {
    return static_cast<int>(text.length() / 4) + 1;
}

bool AIImplementation::supportsToolCalling() const {
    return (m_config.backend == "openai" || m_config.backend == "anthropic");
}

bool AIImplementation::testConnectivity() {
    try {
        if (m_config.backend == "ollama") {
            auto response = m_httpClient->get(m_config.endpoint + "/api/tags");
            return response.success && response.statusCode == 200;
        } else if (m_config.backend == "openai" || m_config.backend == "anthropic") {
            return !m_config.apiKey.empty();
        }
        return false;
    } catch (...) {
        return false;
    }
}

LLMConfig AIImplementation::getConfig() const {
    return m_config;
}

json AIImplementation::getUsageStats() const {
    json stats = json::object_type();
    return stats;
}

namespace PromptTemplates {
    std::string codeGeneration(const std::string& codeContext, const std::string& requirement) {
        return "Context:\n" + codeContext + "\n\nRequirement:\n" + requirement + "\n\nGenerate code:";
    }

    std::string codeReview(const std::string& code, const std::string& language) {
        return "Review " + language + " code:\n\n" + code + "\n\nProvide feedback:";
    }

    std::string bugFix(const std::string& code, const std::string& errorMessage, const std::string& stackTrace) {
        return "Fix bug:\n\n" + code + "\n\nError: " + errorMessage + "\n\nStack trace:\n" + stackTrace;
    }

    std::string refactoring(const std::string& code, const std::string& language, const std::string& objective) {
        return "Refactor " + language + " code for " + objective + ":\n\n" + code;
    }

    std::string documentation(const std::string& code, const std::string& language) {
        return "Document " + language + " code:\n\n" + code;
    }
}
