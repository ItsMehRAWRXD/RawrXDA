#include "MultiModalModelRouter.h"
#include <algorithm>
#include <numeric>

// SCAFFOLD_087: Model router and tier hopping


namespace RawrXD {
namespace IDE {

MultiModalModelRouter::MultiModalModelRouter() 
    : m_completionModelLatency(0), m_chatModelLatency(0), m_editModelLatency(0) {
}

ModelSelection MultiModalModelRouter::selectModel(TaskType task) {
    ModelSelection selection;
    
    switch (task) {
        case TaskType::CodeCompletion:
            return getCompletionModel();
        case TaskType::Chat:
            return getChatModel();
        case TaskType::CodeEdit:
            return getEditModel();
        case TaskType::Embedding:
            return getEmbeddingModel();
        case TaskType::Debugging:
            return getDebugModel();
        case TaskType::Optimization:
            return getOptimizationModel();
        case TaskType::Security:
            return getSecurityModel();
        case TaskType::Documentation:
            return getDocumentationModel();
        default:
            return getChatModel();  // Default fallback
    }
}

ModelSelection MultiModalModelRouter::getCompletionModel() {
    // Prefer lightweight, fast models for completions
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "neural-chat:latest",
        "mistral:latest"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getChatModel() {
    // Prefer capable models for conversation
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "dolphin-mixtral",
        "llama2-uncensored"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getEditModel() {
    // Prefer models that understand code modification
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "codegemma",
        "codellama"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getEmbeddingModel() {
    // Specialized embedding models
    std::vector<std::string> preferences = {
        "nomic-embed-text",
        "all-minilm",
        "bge-base"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getDebugModel() {
    // Prefer analytical models for debugging
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "dolphin-mixtral"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getOptimizationModel() {
    // Models good at code analysis
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "codegemma"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getSecurityModel() {
    // Models with security awareness
    std::vector<std::string> preferences = {
        "mistral",
        "dolphin-mixtral",
        "neural-chat"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::getDocumentationModel() {
    // Models good at writing documentation
    std::vector<std::string> preferences = {
        "neural-chat",
        "mistral",
        "dolphin-mixtral"
    };
    
    return selectFromAvailable(preferences);
}

ModelSelection MultiModalModelRouter::selectFromAvailable(
    const std::vector<std::string>& preferences) {
    
    ModelSelection selection;
    selection.modelName = "neural-chat";  // Default
    selection.confidence = 0.8f;
    selection.isAvailable = true;
    selection.estimatedLatencyMs = 150;
    
    // Check preferences against available models
    for (const auto& pref : preferences) {
        auto it = m_availableModels.find(pref);
        if (it != m_availableModels.end()) {
            selection.modelName = pref;
            selection.isAvailable = true;
            selection.estimatedLatencyMs = it->second.estimatedLatencyMs;
            break;
        }
    }
    
    return selection;
}

bool MultiModalModelRouter::registerModel(
    const std::string& modelName, const std::string& description,
    float performanceScore, int estimatedLatencyMs) {
    
    ModelMetadata metadata;
    metadata.name = modelName;
    metadata.description = description;
    metadata.performanceScore = performanceScore;
    metadata.estimatedLatencyMs = estimatedLatencyMs;
    metadata.registeredTime = std::chrono::system_clock::now();
    
    m_availableModels[modelName] = metadata;
    return true;
}

std::vector<std::string> MultiModalModelRouter::getAvailableModels() {
    std::vector<std::string> models;
    for (const auto& entry : m_availableModels) {
        models.push_back(entry.first);
    }
    return models;
}

std::vector<std::string> MultiModalModelRouter::getAvailableModels(TaskType task) {
    std::vector<std::string> compatible;
    
    std::vector<std::string> preferences;
    
    switch (task) {
        case TaskType::CodeCompletion:
            preferences = {"neural-chat", "mistral", "codegemma", "codellama"};
            break;
        case TaskType::Chat:
            preferences = {"neural-chat", "mistral", "dolphin-mixtral", "llama2"};
            break;
        case TaskType::CodeEdit:
            preferences = {"neural-chat", "mistral", "codegemma"};
            break;
        case TaskType::Embedding:
            preferences = {"nomic-embed-text", "all-minilm", "bge-base"};
            break;
        case TaskType::Debugging:
            preferences = {"neural-chat", "mistral", "dolphin-mixtral"};
            break;
        case TaskType::Optimization:
            preferences = {"neural-chat", "mistral", "codegemma"};
            break;
        case TaskType::Security:
            preferences = {"mistral", "dolphin-mixtral", "neural-chat"};
            break;
        case TaskType::Documentation:
            preferences = {"neural-chat", "mistral", "dolphin-mixtral"};
            break;
    }
    
    for (const auto& pref : preferences) {
        if (m_availableModels.find(pref) != m_availableModels.end()) {
            compatible.push_back(pref);
        }
    }
    
    return compatible;
}

float MultiModalModelRouter::getModelPerformanceScore(const std::string& modelName) {
    auto it = m_availableModels.find(modelName);
    if (it != m_availableModels.end()) {
        return it->second.performanceScore;
    }
    return 0.5f;  // Default neutral score
}

int MultiModalModelRouter::getEstimatedLatency(const std::string& modelName) {
    auto it = m_availableModels.find(modelName);
    if (it != m_availableModels.end()) {
        return it->second.estimatedLatencyMs;
    }
    return 500;  // Default conservative estimate
}

std::string MultiModalModelRouter::getBestModel(TaskType task) {
    return selectModel(task).modelName;
}

void MultiModalModelRouter::recordLatency(
    TaskType task, const std::string& modelName, int latencyMs) {
    
    switch (task) {
        case TaskType::CodeCompletion:
            m_completionModelLatency = latencyMs;
            break;
        case TaskType::Chat:
            m_chatModelLatency = latencyMs;
            break;
        case TaskType::CodeEdit:
            m_editModelLatency = latencyMs;
            break;
        default:
            break;
    }
    
    // Update model metadata if tracked
    auto it = m_availableModels.find(modelName);
    if (it != m_availableModels.end()) {
        it->second.estimatedLatencyMs = latencyMs;
    }
}

std::vector<ModelUsageStats> MultiModalModelRouter::getModelUsageStats() {
    std::vector<ModelUsageStats> stats;
    
    for (const auto& entry : m_usageStatistics) {
        stats.push_back(entry.second);
    }
    
    return stats;
}

void MultiModalModelRouter::updateAvailableModels(
    const std::vector<std::string>& modelNames) {
    
    // Register discovered models
    for (const auto& name : modelNames) {
        if (m_availableModels.find(name) == m_availableModels.end()) {
            registerModel(name, "Auto-discovered model", 0.7f, 150);
        }
    }
}

float MultiModalModelRouter::scoreModel(
    const std::string& modelName, TaskType task, float latencyMs) {
    
    float score = 0.0f;
    
    // Base performance score
    float perfScore = getModelPerformanceScore(modelName);
    score += perfScore * 0.4f;
    
    // Latency score (prefer faster models, but with diminishing returns)
    float latencyScore = 1.0f / (1.0f + (latencyMs / 100.0f));
    score += latencyScore * 0.3f;
    
    // Task preference score
    float taskScore = 0.7f;  // Default compatibility
    if (modelName.find("code") != std::string::npos) {
        if (task == TaskType::CodeCompletion || task == TaskType::CodeEdit) {
            taskScore = 1.0f;
        }
    }
    score += taskScore * 0.3f;
    
    return std::min(1.0f, score);
}

void MultiModalModelRouter::recordUsage(
    const std::string& modelName, TaskType task, int durationMs, bool success) {
    
    auto it = m_usageStatistics.find(modelName);
    if (it == m_usageStatistics.end()) {
        ModelUsageStats stats;
        stats.modelName = modelName;
        stats.totalRequests = 0;
        stats.successfulRequests = 0;
        stats.totalDurationMs = 0;
        m_usageStatistics[modelName] = stats;
        it = m_usageStatistics.find(modelName);
    }
    
    it->second.totalRequests++;
    if (success) {
        it->second.successfulRequests++;
    }
    it->second.totalDurationMs += durationMs;
}

float MultiModalModelRouter::getSuccessRate(const std::string& modelName) {
    auto it = m_usageStatistics.find(modelName);
    if (it == m_usageStatistics.end() || it->second.totalRequests == 0) {
        return 0.0f;
    }
    
    return static_cast<float>(it->second.successfulRequests) / 
           static_cast<float>(it->second.totalRequests);
}

float MultiModalModelRouter::getAverageDuration(const std::string& modelName) {
    auto it = m_usageStatistics.find(modelName);
    if (it == m_usageStatistics.end() || it->second.totalRequests == 0) {
        return 0.0f;
    }
    
    return static_cast<float>(it->second.totalDurationMs) / 
           static_cast<float>(it->second.totalRequests);
}

} // namespace IDE
} // namespace RawrXD
