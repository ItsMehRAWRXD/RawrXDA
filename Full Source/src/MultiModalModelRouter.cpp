#include "MultiModalModelRouter.h"
#include <algorithm>
#include <numeric>
#include <iostream>

namespace RawrXD {
namespace IDE {

MultiModalModelRouter::MultiModalModelRouter() {
    // Default initialization
}

bool MultiModalModelRouter::initialize(const std::string& ollamaEndpoint) {
    // In a real implementation, we would probe the endpoint
    // For now, we seed with some defaults
    loadModelsFromOllama();
    return true;
}

ModelSelection MultiModalModelRouter::selectModel(TaskType taskType, int contextSize, bool prioritizeSpeed) {
    // Simple selection logic based on task type
    
    std::string bestModelName;
    ModelSelection bestSelection;
    float bestScore = -1.0f;

    for (const auto& pair : m_availableModels) {
        float score = scoreModel(pair.second, taskType, contextSize);
        if (prioritizeSpeed) {
            score += (1000.0f / (pair.second.expectedLatency + 1.0f)) * 0.5f;
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
        }
    }

    if (bestModelName.empty()) {
        // Fallback
        bestSelection.modelName = "neural-chat";
        bestSelection.expectedLatency = 100.0f;
    }

    return bestSelection;
}

ModelSelection MultiModalModelRouter::selectModelForUseCase(const std::string& useCase, bool prioritizeSpeed) {
    // Map string usecase to TaskType if possible, or use generic
    TaskType task = TaskType::CHAT;
    if (useCase.find("code") != std::string::npos) task = TaskType::COMPLETION;
    if (useCase.find("debug") != std::string::npos) task = TaskType::DEBUG;
    if (useCase.find("doc") != std::string::npos) task = TaskType::DOCUMENTATION;
    
    return selectModel(task, 0, prioritizeSpeed);
}

ModelSelection MultiModalModelRouter::getCompletionModel() {
    return selectModel(TaskType::COMPLETION, 0, true);
}

ModelSelection MultiModalModelRouter::getChatModel() {
    return selectModel(TaskType::CHAT);
}

ModelSelection MultiModalModelRouter::getEditModel() {
    return selectModel(TaskType::EDIT);
}

ModelSelection MultiModalModelRouter::getEmbeddingModel() {
    return selectModel(TaskType::EMBEDDING);
}

ModelSelection MultiModalModelRouter::getDebugModel() {
    return selectModel(TaskType::DEBUG);
}

ModelSelection MultiModalModelRouter::getOptimizationModel() {
    return selectModel(TaskType::OPTIMIZATION);
}

ModelSelection MultiModalModelRouter::getSecurityModel() {
    return selectModel(TaskType::SECURITY);
}

ModelSelection MultiModalModelRouter::getDocumentationModel() {
    return selectModel(TaskType::DOCUMENTATION);
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
    
    m_availableModels[modelName] = info;
}

void MultiModalModelRouter::setDefaultModel(TaskType taskType, const std::string& modelName) {
    // TODO: Store default overrides
}

std::vector<ModelSelection> MultiModalModelRouter::getAvailableModels() {
    std::vector<ModelSelection> result;
    for (const auto& pair : m_availableModels) {
        ModelSelection sel;
        sel.modelName = pair.second.name;
        sel.modelUrl = pair.second.url;
        sel.expectedLatency = pair.second.expectedLatency;
        sel.contextWindow = pair.second.contextWindow;
        sel.supportsStreaming = pair.second.supportsStreaming;
        sel.optimalFor = pair.second.optimalFor;
        result.push_back(sel);
    }
    return result;
}

std::vector<ModelSelection> MultiModalModelRouter::getModelsForTask(TaskType taskType) {
    std::vector<ModelSelection> result;
    for (const auto& pair : m_availableModels) {
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
            }
        }
    }
    return result;
}

void MultiModalModelRouter::setSpeedPriority(float speedWeight) {
    // Store pref
}

void MultiModalModelRouter::setQualityPriority(float qualityWeight) {
    // Store pref
}

void MultiModalModelRouter::setMemoryLimit(int limitMB) {
    // Store pref
}

MultiModalModelRouter::RoutingStats MultiModalModelRouter::getStatistics() {
    RoutingStats stats;
    stats.totalRequests = 0;
    for (const auto& pair : m_availableModels) {
        stats.totalRequests += pair.second.usageCount;
        stats.modelUsage[pair.first] = pair.second.usageCount;
        if (pair.second.usageCount > 0)
            stats.avgLatency[pair.first] = pair.second.totalLatency / pair.second.usageCount;
        else
            stats.avgLatency[pair.first] = 0.0f;
    }
    return stats;
}

float MultiModalModelRouter::scoreModel(const ModelInfo& model, TaskType taskType, int contextSize) {
    float score = 0.5f; // Base score
    
    for (auto type : model.optimalFor) {
        if (type == taskType) {
            score += 0.4f;
            break;
        }
    }
    
    // Tiny penalty for very small context if we need it (param ignored for now)
    
    return score;
}

bool MultiModalModelRouter::loadModelsFromOllama() {
    // Register some defaults for now
    registerModel("neural-chat", "http://localhost:11434", {TaskType::CHAT, TaskType::COMPLETION}, 150.0f, 4096, true);
    registerModel("mistral", "http://localhost:11434", {TaskType::CHAT, TaskType::EDIT, TaskType::DOCUMENTATION}, 200.0f, 8192, true);
    registerModel("codegemma", "http://localhost:11434", {TaskType::COMPLETION, TaskType::OPTIMIZATION}, 100.0f, 2048, true);
    registerModel("nomic-embed-text", "http://localhost:11434", {TaskType::EMBEDDING}, 50.0f, 2048, false);
    return true;
}

} // namespace IDE
} // namespace RawrXD
