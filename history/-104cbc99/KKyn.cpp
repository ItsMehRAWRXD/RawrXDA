#include "model_router.h"
#include "logger.h"
#include <iostream>
#include <algorithm>
#include <chrono>

ModelRouter* ModelRouter::s_instance = nullptr;

ModelRouter& ModelRouter::getInstance() {
    if (!s_instance) {
        s_instance = new ModelRouter();
    }
    return *s_instance;
}

void ModelRouter::registerModel(const ModelConfig& config) {
    m_models[config.id] = config;
    m_callCounts[config.id] = 0;
    log_info("Model registered: " + config.id + " (" + config.name + ")");
}

void ModelRouter::removeModel(const std::string& modelId) {
    m_models.erase(modelId);
    m_callCounts.erase(modelId);
    log_info("Model removed: " + modelId);
}

std::vector<ModelConfig> ModelRouter::getAvailableModels() const {
    std::vector<ModelConfig> result;
    for (const auto& [id, config] : m_models) {
        result.push_back(config);
    }
    return result;
}

ModelConfig ModelRouter::getModel(const std::string& modelId) const {
    auto it = m_models.find(modelId);
    if (it != m_models.end()) {
        return it->second;
    }
    return {};
}

InferenceResponse ModelRouter::infer(const InferenceRequest& request) {
    InferenceResponse response;
    auto start = std::chrono::high_resolution_clock::now();

    auto it = m_models.find(request.model);
    if (it == m_models.end()) {
        response.error = "Model not found: " + request.model;
        response.success = false;
        log_error(response.error);
        return response;
    }

    log_info("Inference request: " + request.model + " (" + std::to_string(request.prompt.size()) + " chars)");

    // Placeholder for actual inference
    response.text = "Inference response for: " + request.prompt;
    response.tokensUsed = request.prompt.size() / 4;  // Rough estimate
    response.success = true;

    auto end = std::chrono::high_resolution_clock::now();
    response.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();

    m_metrics.totalRequests++;
    m_metrics.totalTokens += response.tokensUsed;

    log_debug("Inference completed in " + std::to_string(response.latencyMs) + "ms");

    return response;
}

InferenceResponse ModelRouter::inferStreaming(const InferenceRequest& request,
                                               std::function<void(const std::string&)> streamCallback) {
    InferenceResponse response;

    if (streamCallback) {
        streamCallback("[streaming response start]\n");
        streamCallback(request.prompt + "\n");
        streamCallback("[streaming response end]\n");
    }

    response.success = true;
    response.text = "[streamed response]";
    response.tokensUsed = 100;

    return response;
}

void ModelRouter::setLoadBalancingStrategy(const std::string& strategy) {
    m_strategy = strategy;
    log_debug("Load balancing strategy set to: " + strategy);
}

std::string ModelRouter::selectBestModel() {
    if (m_models.empty()) {
        return "";
    }

    if (m_strategy == "round-robin") {
        std::string best = m_models.begin()->first;
        int minCalls = m_callCounts[best];

        for (const auto& [id, config] : m_models) {
            if (m_callCounts[id] < minCalls) {
                best = id;
                minCalls = m_callCounts[id];
            }
        }

        m_callCounts[best]++;
        return best;
    }

    return m_models.begin()->first;
}

bool ModelRouter::isModelHealthy(const std::string& modelId) {
    return m_models.find(modelId) != m_models.end();
}

void ModelRouter::checkHealth() {
    log_debug("Health check: " + std::to_string(m_models.size()) + " models available");
    for (const auto& [id, config] : m_models) {
        log_debug("  " + id + " (" + config.backend + "): OK");
    }
}
