#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "logging/logger.h"
#include "metrics/metrics.h"

enum class TaskType {
    COMPLETION,
    CHAT,
    EDIT,
    ANALYSIS,
    DOCUMENTATION,
    TESTING,
    EMBEDDING
};

enum class Complexity {
    SIMPLE,
    MEDIUM,
    COMPLEX
};

struct ModelProfile {
    std::string name;
    TaskType primaryTask;
    Complexity maxComplexity;
    int parameterCount;
    float inferenceSpeed;
    float accuracyScore;
    bool supportsStreaming;
    std::vector<std::string> strengths;
};

struct RoutingDecision {
    std::string selectedModel;
    std::string reasoning;
    bool requiresPreload;
    int estimatedLatencyMs;
    float confidenceScore;
};

class MultiModalModelRouterIntegration {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

    std::vector<ModelProfile> m_availableModels;
    std::unordered_map<std::string, float> m_modelPerformance;
    std::unordered_map<std::string, int> m_modelUsageCount;

    int m_maxModelCache = 3;
    bool m_autoSwitchEnabled = true;

public:
    MultiModalModelRouterIntegration(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Core routing interface
    RoutingDecision routeTask(
        TaskType task,
        const std::string& context,
        Complexity complexity = Complexity::MEDIUM
    );

    // Advanced routing features
    std::vector<std::string> getRecommendedModels(TaskType task);
    bool preloadModel(const std::string& modelName);
    bool switchModel(const std::string& modelName);

    // Performance optimization
    void updateModelPerformance(const std::string& model, float performance);
    void analyzeUsagePatterns();

    // Smart routing
    RoutingDecision routeWithFallback(
        TaskType task,
        const std::string& context,
        int maxLatencyMs
    );

    // Model management
    void refreshModelInventory();
    std::vector<ModelProfile> getAvailableModels() const;
    bool isModelAvailable(const std::string& modelName) const;

private:
    void buildTaskModelMap();
    ModelProfile createModelProfile(const std::string& modelName);
    float calculateModelScore(
        const ModelProfile& model,
        TaskType task,
        Complexity complexity,
        const std::string& context
    );
};
