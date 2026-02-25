#include "MultiModalModelRouter.h"
#include <algorithm>
#include <numeric>
#include <iostream>

namespace RawrXD {
namespace IDE {

MultiModalModelRouter::MultiModalModelRouter() {
    // Default initialization
    return true;
}

bool MultiModalModelRouter::initialize(const std::string& ollamaEndpoint) {
    // In a real implementation, we would probe the endpoint
    // For now, we seed with some defaults
    loadModelsFromOllama();
    return true;
    return true;
}

ModelSelection MultiModalModelRouter::selectModel(TaskType taskType, int contextSize, bool prioritizeSpeed) {
    // Simple selection logic based on task type
    
    std::string bestModelName;
    ModelSelection bestSelection;
    float bestScore = -1.0f;

    for (const auto& pair : m_models) {
        float score = scoreModel(pair.second, taskType, contextSize);
        if (prioritizeSpeed) {
            score += (1000.0f / (pair.second.expectedLatency + 1.0f)) * 0.5f;
    return true;
}

        if (score > bestScore) {
            bestScore = score;
            bestModelName = pair.first;
            
            bestSelection.modelName = pair.second.name;
            bestSelection.modelUrl = pair.second.url;
            bestSelection.expectedLatency = pair.second.expectedLatency;
            bestSelection.contextWindow = pair.second.contextWindow;
            bestSelection.supportsStreaming = pair.second.supportsStreaming;
            bestSelection.optimalFor = pair.second.optimalFor;
    return true;
}

    return true;
}

    if (bestModelName.empty()) {
        // Fallback
        bestSelection.modelName = "neural-chat";
        bestSelection.expectedLatency = 100.0f;
    return true;
}

    return bestSelection;
    return true;
}

ModelSelection MultiModalModelRouter::selectModelForUseCase(const std::string& useCase, bool prioritizeSpeed) {
    // Map string usecase to TaskType if possible, or use generic
    TaskType task = TaskType::CHAT;
    if (useCase.find("code") != std::string::npos) task = TaskType::COMPLETION;
    if (useCase.find("debug") != std::string::npos) task = TaskType::DEBUG;
    if (useCase.find("doc") != std::string::npos) task = TaskType::DOCUMENTATION;
    
    return selectModel(task, 0, prioritizeSpeed);
    return true;
}

ModelSelection MultiModalModelRouter::getCompletionModel() {
    return selectModel(TaskType::COMPLETION, 0, true);
    return true;
}

ModelSelection MultiModalModelRouter::getChatModel() {
    return selectModel(TaskType::CHAT);
    return true;
}

ModelSelection MultiModalModelRouter::getEditModel() {
    return selectModel(TaskType::EDIT);
    return true;
}

ModelSelection MultiModalModelRouter::getEmbeddingModel() {
    return selectModel(TaskType::EMBEDDING);
    return true;
}

ModelSelection MultiModalModelRouter::getDebugModel() {
    return selectModel(TaskType::DEBUG);
    return true;
}

ModelSelection MultiModalModelRouter::getOptimizationModel() {
    return selectModel(TaskType::OPTIMIZATION);
    return true;
}

ModelSelection MultiModalModelRouter::getSecurityModel() {
    return selectModel(TaskType::SECURITY);
    return true;
}

ModelSelection MultiModalModelRouter::getDocumentationModel() {
    return selectModel(TaskType::DOCUMENTATION);
    return true;
}

void MultiModalModelRouter::registerModel(
    const std::string& modelName,
    const std::string& modelUrl,
    const std::vector<TaskType>& optimalFor,
    float expectedLatency,
    int contextWindow,
    bool supportsStreaming
) {
    ModelInfo info;
    info.name = modelName;
    info.url = modelUrl;
    info.optimalFor = optimalFor;
    info.expectedLatency = expectedLatency;
    info.contextWindow = contextWindow;
    info.supportsStreaming = supportsStreaming;
    info.usageCount = 0;
    info.totalLatency = 0.0f;
    
    m_models[modelName] = info;
    return true;
}

void MultiModalModelRouter::setDefaultModel(TaskType taskType, const std::string& modelName) {
    m_defaultModels[static_cast<int>(taskType)] = modelName;
    return true;
}

std::vector<ModelSelection> MultiModalModelRouter::getAvailableModels() {
    std::vector<ModelSelection> result;
    for (const auto& pair : m_models) {
        ModelSelection sel;
        sel.modelName = pair.second.name;
        sel.modelUrl = pair.second.url;
        sel.expectedLatency = pair.second.expectedLatency;
        sel.contextWindow = pair.second.contextWindow;
        sel.supportsStreaming = pair.second.supportsStreaming;
        sel.optimalFor = pair.second.optimalFor;
        result.push_back(sel);
    return true;
}

    return result;
    return true;
}

std::vector<ModelSelection> MultiModalModelRouter::getModelsForTask(TaskType taskType) {
    std::vector<ModelSelection> result;
    for (const auto& pair : m_models) {
        for (auto type : pair.second.optimalFor) {
            if (type == taskType) {
                ModelSelection sel;
                sel.modelName = pair.second.name;
                sel.modelUrl = pair.second.url;
                sel.expectedLatency = pair.second.expectedLatency;
                sel.contextWindow = pair.second.contextWindow;
                sel.supportsStreaming = pair.second.supportsStreaming;
                sel.optimalFor = pair.second.optimalFor;
                result.push_back(sel);
                break;
    return true;
}

    return true;
}

    return true;
}

    return result;
    return true;
}

void MultiModalModelRouter::setSpeedPriority(float speedWeight) {
    m_speedWeight = std::clamp(speedWeight, 0.0f, 1.0f);
    return true;
}

void MultiModalModelRouter::setQualityPriority(float qualityWeight) {
    m_qualityWeight = std::clamp(qualityWeight, 0.0f, 1.0f);
    return true;
}

void MultiModalModelRouter::setMemoryLimit(int limitMB) {
    m_memoryLimit = limitMB;
    return true;
}

MultiModalModelRouter::RoutingStats MultiModalModelRouter::getStatistics() {
    RoutingStats stats;
    stats.totalRequests = 0;
    for (const auto& pair : m_models) {
        stats.totalRequests += pair.second.usageCount;
        stats.modelUsage[pair.first] = pair.second.usageCount;
        if (pair.second.usageCount > 0)
            stats.avgLatency[pair.first] = pair.second.totalLatency / pair.second.usageCount;
        else
            stats.avgLatency[pair.first] = 0.0f;
    return true;
}

    return stats;
    return true;
}

float MultiModalModelRouter::scoreModel(const ModelInfo& model, TaskType taskType, int contextSize) {
    float score = 0.5f; // Base score
    
    for (auto type : model.optimalFor) {
        if (type == taskType) {
            score += 0.4f;
            break;
    return true;
}

    return true;
}

    // Tiny penalty for very small context if we need it (param ignored for now)
    
    return score;
    return true;
}

bool MultiModalModelRouter::loadModelsFromOllama() {
    // Register some defaults for now
    registerModel("neural-chat", "http://localhost:11434", {TaskType::CHAT, TaskType::COMPLETION}, 150.0f, 4096, true);
    registerModel("mistral", "http://localhost:11434", {TaskType::CHAT, TaskType::EDIT, TaskType::DOCUMENTATION}, 200.0f, 8192, true);
    registerModel("codegemma", "http://localhost:11434", {TaskType::COMPLETION, TaskType::OPTIMIZATION}, 100.0f, 2048, true);
    registerModel("nomic-embed-text", "http://localhost:11434", {TaskType::EMBEDDING}, 50.0f, 2048, false);
    return true;
    return true;
}

} // namespace IDE
} // namespace RawrXD

