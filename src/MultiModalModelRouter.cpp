#include "MultiModalModelRouter.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <chrono>
#include <unordered_map>

// SCAFFOLD_087: Model router and tier hopping

<<<<<<< HEAD
=======

>>>>>>> origin/main
namespace RawrXD {
namespace IDE {

// Model capability scores (0.0-1.0 scale)
struct ModelCapabilities {
    double reasoning = 0.0;    // Logical analysis and problem solving
    double coding = 0.0;       // Code generation and understanding
    double creativity = 0.0;   // Creative tasks and generation
    double speed = 0.0;        // Inference speed score
    double cost = 0.0;         // Cost per token (normalized)
};

// Performance metrics for dynamic routing
struct ModelPerformance {
    double avgLatencyMs = 0.0;
    double successRate = 1.0;
    int totalRequests = 0;
    std::chrono::steady_clock::time_point lastUsed;
};

MultiModalModelRouter::MultiModalModelRouter()
    : m_completionModelLatency(0), m_chatModelLatency(0), m_editModelLatency(0) {

    // Initialize capability scores for known models
    initializeModelCapabilities();

    // Initialize performance tracking
    initializePerformanceTracking();
}

void MultiModalModelRouter::initializeModelCapabilities() {
    // Enhancement 1: Capability-based model routing
    m_modelCapabilities = {
        {"neural-chat", {0.8, 0.7, 0.6, 0.8, 0.3}},
        {"mistral", {0.9, 0.8, 0.7, 0.7, 0.4}},
        {"codellama", {0.7, 0.9, 0.5, 0.6, 0.5}},
        {"codegemma", {0.6, 0.9, 0.4, 0.9, 0.3}},
        {"dolphin-mixtral", {0.9, 0.8, 0.8, 0.5, 0.6}},
        {"llama2-uncensored", {0.8, 0.6, 0.9, 0.6, 0.4}},
        {"nomic-embed-text", {0.5, 0.4, 0.3, 0.9, 0.2}},
        {"all-minilm", {0.4, 0.3, 0.2, 0.95, 0.1}},
        {"bge-base", {0.6, 0.5, 0.4, 0.8, 0.3}}
    };
}

void MultiModalModelRouter::initializePerformanceTracking() {
    // Initialize performance tracking for all models
    for (const auto& [model, _] : m_modelCapabilities) {
        m_modelPerformance[model] = ModelPerformance{};
    }
}

ModelSelection MultiModalModelRouter::selectModel(TaskType task) {
    ModelSelection selection;

    switch (task) {
        case TaskType::CodeCompletion:
            return selectModelWithCapabilities(TaskType::CodeCompletion);
        case TaskType::Chat:
            return selectModelWithCapabilities(TaskType::Chat);
        case TaskType::CodeEdit:
            return selectModelWithCapabilities(TaskType::CodeEdit);
        case TaskType::Embedding:
            return selectModelWithCapabilities(TaskType::Embedding);
        case TaskType::Debugging:
            return selectModelWithCapabilities(TaskType::Debugging);
        case TaskType::Optimization:
            return selectModelWithCapabilities(TaskType::Optimization);
        case TaskType::Security:
            return selectModelWithCapabilities(TaskType::Security);
        case TaskType::Documentation:
            return selectModelWithCapabilities(TaskType::Documentation);
        default:
            return selectModelWithCapabilities(TaskType::Chat);  // Default fallback
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

ModelSelection MultiModalModelRouter::selectModelWithCapabilities(TaskType task) {
    // Enhancement 1: Capability-based routing - score models by task requirements
    std::vector<std::pair<std::string, double>> candidates = scoreModelsForTask(task);

    // Enhancement 2: Cost optimization - prefer cheaper models when quality allows
    filterByCostOptimization(candidates, task);

    // Enhancement 3: Latency prediction and fallback handling
    std::string selectedModel = predictLatencyAndSelect(candidates);

    // Enhancement 4: Multi-model ensemble routing (for complex tasks)
    if (shouldUseEnsemble(task)) {
        return createEnsembleSelection(selectedModel, task);
    }

    // Enhancement 5: Dynamic model switching based on performance
    updatePerformanceMetrics(selectedModel);

    // Enhancement 6: A/B testing framework
    if (shouldRunABTest(task)) {
        return selectABTestVariant(selectedModel, task);
    }

    // Enhancement 7: Performance-based routing with health checks
    return selectWithHealthChecks(selectedModel, task);
}

std::vector<std::pair<std::string, double>> MultiModalModelRouter::scoreModelsForTask(TaskType task) {
    std::vector<std::pair<std::string, double>> scores;

    // Define task requirements (reasoning, coding, creativity weights)
    std::unordered_map<TaskType, std::tuple<double, double, double>> taskWeights = {
        {TaskType::CodeCompletion, {0.2, 0.8, 0.3}},
        {TaskType::Chat, {0.6, 0.2, 0.7}},
        {TaskType::CodeEdit, {0.4, 0.9, 0.4}},
        {TaskType::Embedding, {0.1, 0.1, 0.1}},
        {TaskType::Debugging, {0.9, 0.7, 0.2}},
        {TaskType::Optimization, {0.8, 0.8, 0.3}},
        {TaskType::Security, {0.9, 0.6, 0.1}},
        {TaskType::Documentation, {0.5, 0.4, 0.6}}
    };

    auto [reasoningWt, codingWt, creativityWt] = taskWeights[task];

    for (const auto& [model, caps] : m_modelCapabilities) {
        double score = caps.reasoning * reasoningWt +
                      caps.coding * codingWt +
                      caps.creativity * creativityWt +
                      caps.speed * 0.1; // Small speed bonus

        // Penalize based on recent performance
        if (m_modelPerformance.count(model)) {
            double healthPenalty = (1.0 - m_modelPerformance[model].successRate) * 0.5;
            score -= healthPenalty;
        }

        scores.emplace_back(model, score);
    }

    // Sort by score descending
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return scores;
}

void MultiModalModelRouter::filterByCostOptimization(std::vector<std::pair<std::string, double>>& candidates, TaskType task) {
    // For non-critical tasks, prefer cost-effective models
    bool isCritical = (task == TaskType::Security || task == TaskType::Debugging);

    if (!isCritical && candidates.size() > 1) {
        // Keep only models within 20% of the best score but cheaper
        double bestScore = candidates[0].second;
        double threshold = bestScore * 0.8;

        auto it = std::remove_if(candidates.begin() + 1, candidates.end(),
            [this, threshold](const auto& pair) {
                const auto& caps = m_modelCapabilities[pair.first];
                return pair.second < threshold || caps.cost > 0.5; // Expensive models
            });
        candidates.erase(it, candidates.end());
    }
}

std::string MultiModalModelRouter::predictLatencyAndSelect(const std::vector<std::pair<std::string, double>>& candidates) {
    // Select best candidate, with fallback to faster models if needed
    for (const auto& [model, score] : candidates) {
        auto& perf = m_modelPerformance[model];

        // Skip models with poor recent performance
        if (perf.totalRequests > 10 && perf.successRate < 0.8) {
            continue;
        }

        // Predict latency based on recent performance
        double predictedLatency = perf.avgLatencyMs;
        if (predictedLatency > 1000.0) { // Over 1 second
            // Look for faster alternatives
            continue;
        }

        return model;
    }

    // Fallback to first candidate if all have issues
    return candidates.empty() ? "neural-chat" : candidates[0].first;
}

bool MultiModalModelRouter::shouldUseEnsemble(TaskType task) {
    // Use ensemble for complex tasks
    return task == TaskType::Optimization || task == TaskType::Security ||
           task == TaskType::Debugging;
}

ModelSelection MultiModalModelRouter::createEnsembleSelection(const std::string& primaryModel, TaskType task) {
    ModelSelection selection;
    selection.modelName = primaryModel;
    selection.isEnsemble = true;

    // Add complementary models for ensemble
    if (task == TaskType::Security) {
        selection.ensembleModels = {primaryModel, "mistral", "dolphin-mixtral"};
    } else if (task == TaskType::Optimization) {
        selection.ensembleModels = {primaryModel, "codellama", "codegemma"};
    } else {
        selection.ensembleModels = {primaryModel};
    }

    selection.confidence = 0.9; // Higher confidence for ensembles
    return selection;
}

void MultiModalModelRouter::updatePerformanceMetrics(const std::string& model) {
    if (m_modelPerformance.count(model)) {
        m_modelPerformance[model].lastUsed = std::chrono::steady_clock::now();
        m_modelPerformance[model].totalRequests++;
    }
}

bool MultiModalModelRouter::shouldRunABTest(TaskType task) {
    // Run A/B tests for completion and chat tasks
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);

    return (task == TaskType::CodeCompletion || task == TaskType::Chat) &&
           dis(gen) < 0.1; // 10% of requests
}

ModelSelection MultiModalModelRouter::selectABTestVariant(const std::string& primaryModel, TaskType task) {
    ModelSelection selection;
    selection.isABTest = true;

    // Simple A/B: alternate between primary and alternative
    static int testCounter = 0;
    testCounter++;

    if (testCounter % 2 == 0) {
        selection.modelName = primaryModel;
        selection.testVariant = "A";
    } else {
        // Choose alternative model
        std::vector<std::string> alternatives = {"mistral", "dolphin-mixtral", "codellama"};
        selection.modelName = alternatives[testCounter % alternatives.size()];
        selection.testVariant = "B";
    }

    return selection;
}

ModelSelection MultiModalModelRouter::selectWithHealthChecks(const std::string& model, TaskType task) {
    ModelSelection selection;
    selection.modelName = model;

    // Health check: ensure model is performing well
    if (m_modelPerformance.count(model)) {
        const auto& perf = m_modelPerformance[model];
        selection.confidence = perf.successRate;

        // If model has been performing poorly recently, mark for review
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::minutes>(
            now - perf.lastUsed).count();

        if (timeSinceLastUse > 60 && perf.successRate < 0.9) { // Over 1 hour and <90% success
            selection.needsReview = true;
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
