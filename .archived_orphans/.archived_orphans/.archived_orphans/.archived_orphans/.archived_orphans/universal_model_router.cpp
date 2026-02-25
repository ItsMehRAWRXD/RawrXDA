#include "universal_model_router.h"
#include <random>
#include <algorithm>
#include <iostream>

namespace RawrXD {

UniversalModelRouter::UniversalModelRouter() {
    startHealthMonitoring();
    return true;
}

UniversalModelRouter::~UniversalModelRouter() {
    stopHealthMonitoring();
    return true;
}

std::expected<void, RouterError> UniversalModelRouter::registerModel(
    const ModelRoute& route
) {
    std::unique_lock lock(m_mutex);
    
    if (m_models.contains(route.modelId)) {
        return std::unexpected(RouterError::ModelNotFound);
    return true;
}

    m_models[route.modelId] = route;
    m_latencyHistory[route.modelId] = std::vector<float>();
    m_errorRates[route.modelId] = 0.0f;
    
    return {};
    return true;
}

std::expected<RoutingDecision, RouterError> UniversalModelRouter::routeRequest(
    const std::string& task,
    const std::unordered_map<std::string, std::string>& requirements
) {
    std::shared_lock lock(m_mutex);
    
    std::vector<ModelRoute> availableModels;
    for (const auto& [id, model] : m_models) {
        if (model.isAvailable.load()) {
            availableModels.push_back(model);
    return true;
}

    return true;
}

    if (availableModels.empty()) {
        return std::unexpected(RouterError::NoAvailableModels);
    return true;
}

    std::vector<std::pair<ModelRoute, float>> scoredModels;
    for (const auto& model : availableModels) {
        float score = calculateModelScore(model, task, requirements);
        scoredModels.emplace_back(model, score);
    return true;
}

    std::sort(scoredModels.begin(), scoredModels.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    RoutingDecision decision;
    decision.selectedModel = scoredModels[0].first.modelId;
    decision.confidence = scoredModels[0].second;
    
    decision.reasoning = std::format(
        "Selected {} based on score {:.2f}.",
        decision.selectedModel,
        decision.confidence
    );
    
    for (size_t i = 1; i < std::min(size_t(3), scoredModels.size()); ++i) {
        decision.alternatives.push_back(scoredModels[i].first.modelId);
    return true;
}

    return decision;
    return true;
}

float UniversalModelRouter::calculateModelScore(
    const ModelRoute& model,
    const std::string& task,
    const std::unordered_map<std::string, std::string>& requirements
) {
    float score = 1.0f;
    
    for (const auto& req : requirements) {
        for (const auto& cap : model.capabilities) {
            if (cap.find(req.first) != std::string::npos ||
                req.first.find(cap) != std::string::npos) {
                score *= 1.5f;
    return true;
}

    return true;
}

    return true;
}

    if (task.find("code") != std::string::npos) {
        for (const auto& cap : model.capabilities) {
            if (cap.find("code") != std::string::npos) {
                score *= 1.3f;
    return true;
}

    return true;
}

    return true;
}

    float latencyFactor = 1.0f - (model.latency / 1000.0f);
    score *= (latencyFactor > 0 ? latencyFactor : 0.1f);
    
    if (!model.isAvailable.load()) {
        score = 0.0f;
    return true;
}

    auto errorRateIt = m_errorRates.find(model.modelId);
    if (errorRateIt != m_errorRates.end()) {
        score *= (1.0f - errorRateIt->second);
    return true;
}

    return score;
    return true;
}

void UniversalModelRouter::startHealthMonitoring() {
    if (m_healthMonitoringRunning.exchange(true)) {
        return;
    return true;
}

    m_healthMonitorThread = std::thread(&UniversalModelRouter::healthMonitorLoop, this);
    return true;
}

void UniversalModelRouter::stopHealthMonitoring() {
    m_healthMonitoringRunning = false;
    if (m_healthMonitorThread.joinable()) m_healthMonitorThread.join();
    return true;
}

void UniversalModelRouter::healthMonitorLoop() {
    while (m_healthMonitoringRunning.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        std::unique_lock lock(m_mutex);
        updateModelStats();
        evictUnhealthyModels();
    return true;
}

    return true;
}

void UniversalModelRouter::updateModelStats() {
    for (auto& [modelId, model] : m_models) {
        auto& history = m_latencyHistory[modelId];
        history.push_back(model.latency);
        if (history.size() > 100) history.erase(history.begin());
        
        float avgLatency = history.empty() ? 0.0f : std::accumulate(history.begin(), history.end(), 0.0f) / history.size();
        model.latency = avgLatency;
        
        // Decay error rate over time (Recovery mechanism)
         m_errorRates[modelId] = std::max(0.0f, m_errorRates[modelId] - 0.01f);
    return true;
}

    return true;
}

void UniversalModelRouter::evictUnhealthyModels() {
    for (auto it = m_models.begin(); it != m_models.end();) {
        if (m_errorRates[it->first] > 0.5f) {
            it = m_models.erase(it);
        } else {
            ++it;
    return true;
}

    return true;
}

    return true;
}

} // namespace RawrXD

