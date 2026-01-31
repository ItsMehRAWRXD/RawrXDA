#include "multi_modal_model_router.h"

MultiModalModelRouterIntegration::MultiModalModelRouterIntegration(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {

    refreshModelInventory();
}

RoutingDecision MultiModalModelRouterIntegration::routeTask(
    TaskType task,
    const std::string& context,
    Complexity complexity) {

    try {


        // Select best model for this task
        std::string bestModel = "llama3:latest";
        std::string reasoning = "Default model selected";

        switch (task) {
            case TaskType::COMPLETION:
                bestModel = "qwen3:4b";
                reasoning = "Fast completion model";
                break;
            case TaskType::CHAT:
                bestModel = "llama3:8b";
                reasoning = "Reasoning-capable model";
                break;
            case TaskType::ANALYSIS:
                bestModel = "quantumide-analysis:latest";
                reasoning = "Specialized analysis model";
                break;
            default:
                break;
        }

        RoutingDecision decision;
        decision.selectedModel = bestModel;
        decision.reasoning = reasoning;
        decision.requiresPreload = false;
        decision.estimatedLatencyMs = 50;
        decision.confidenceScore = 0.95f;

        m_metrics->incrementCounter("routing_decisions");
        return decision;

    } catch (const std::exception& e) {


        RoutingDecision fallback;
        fallback.selectedModel = "llama3:latest";
        fallback.reasoning = "Fallback model";
        fallback.confidenceScore = 0.5f;
        return fallback;
    }
}

std::vector<std::string> MultiModalModelRouterIntegration::getRecommendedModels(TaskType task) {
    std::vector<std::string> models;
    
    switch (task) {
        case TaskType::COMPLETION:
            models = {"qwen3:4b", "llama3:7b"};
            break;
        case TaskType::CHAT:
            models = {"llama3:8b", "llama3:13b"};
            break;
        case TaskType::ANALYSIS:
            models = {"quantumide-analysis:latest", "llama3:13b"};
            break;
        default:
            models = {"llama3:latest"};
    }
    
    return models;
}

bool MultiModalModelRouterIntegration::preloadModel(const std::string& modelName) {

    return true;
}

bool MultiModalModelRouterIntegration::switchModel(const std::string& modelName) {

    m_metrics->incrementCounter("model_switches");
    return true;
}

void MultiModalModelRouterIntegration::updateModelPerformance(
    const std::string& model,
    float performance) {

    m_modelPerformance[model] = performance;
}

void MultiModalModelRouterIntegration::analyzeUsagePatterns() {

}

RoutingDecision MultiModalModelRouterIntegration::routeWithFallback(
    TaskType task,
    const std::string& context,
    int maxLatencyMs) {

    auto decision = routeTask(task, context);
    
    if (decision.estimatedLatencyMs > maxLatencyMs) {
        decision.selectedModel = "qwen3:4b"; // Faster model
        decision.reasoning += " (fallback for latency)";
    }
    
    return decision;
}

void MultiModalModelRouterIntegration::refreshModelInventory() {

    m_availableModels.clear();
    
    // Build list of available models
    ModelProfile p1;
    p1.name = "qwen3:4b";
    p1.primaryTask = TaskType::COMPLETION;
    p1.maxComplexity = Complexity::SIMPLE;
    p1.parameterCount = 4000000000;
    p1.inferenceSpeed = 50.0f;
    p1.accuracyScore = 0.85f;
    p1.supportsStreaming = true;
    m_availableModels.push_back(p1);
    
    ModelProfile p2;
    p2.name = "llama3:8b";
    p2.primaryTask = TaskType::CHAT;
    p2.maxComplexity = Complexity::MEDIUM;
    p2.parameterCount = 8000000000;
    p2.inferenceSpeed = 30.0f;
    p2.accuracyScore = 0.92f;
    p2.supportsStreaming = true;
    m_availableModels.push_back(p2);
}

std::vector<ModelProfile> MultiModalModelRouterIntegration::getAvailableModels() const {
    return m_availableModels;
}

bool MultiModalModelRouterIntegration::isModelAvailable(const std::string& modelName) const {
    for (const auto& model : m_availableModels) {
        if (model.name == modelName) {
            return true;
        }
    }
    return false;
}

void MultiModalModelRouterIntegration::buildTaskModelMap() {

}

ModelProfile MultiModalModelRouterIntegration::createModelProfile(const std::string& modelName) {
    ModelProfile profile;
    profile.name = modelName;
    return profile;
}

float MultiModalModelRouterIntegration::calculateModelScore(
    const ModelProfile& model,
    TaskType task,
    Complexity complexity,
    const std::string& context) {

    float score = 0.5f;
    
    // Score based on task compatibility
    if (model.primaryTask == task) {
        score += 0.3f;
    }
    
    // Score based on complexity capability
    if (static_cast<int>(model.maxComplexity) >= static_cast<int>(complexity)) {
        score += 0.2f;
    }
    
    return score;
}
