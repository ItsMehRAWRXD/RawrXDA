#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD {
namespace IDE {

// Task types for model routing
enum class TaskType {
    COMPLETION,      // Quick inline suggestions (fast, small model)
    CHAT,            // Interactive conversation (reasoning, large model)
    EDIT,            // Code transformation (precise, medium model)
    EMBEDDING,       // Semantic search (specialized embedding model)
    DEBUG,           // Debugging assistance (analytical, large model)
    OPTIMIZATION,    // Performance optimization (specialized)
    SECURITY,        // Security analysis (specialized)
    DOCUMENTATION,   // Doc generation (varied)
};

// Model selection strategy
struct ModelSelection {
    std::string modelName;
    std::string modelUrl;
    float expectedLatency;     // ms
    int contextWindow;         // tokens
    bool supportsStreaming;
    std::vector<TaskType> optimalFor;
};

// Multi-modal model router
class MultiModalModelRouter {
public:
    MultiModalModelRouter();
    ~MultiModalModelRouter() = default;

    // Initialize available models
    bool initialize(const std::string& ollamaEndpoint = "http://localhost:11434");

    // Get optimal model for task
    ModelSelection selectModel(
        TaskType taskType,
        int contextSize = 0,
        bool prioritizeSpeed = false
    );

    // Get optimal model for specific use case
    ModelSelection selectModelForUseCase(
        const std::string& useCase,
        bool prioritizeSpeed = false
    );

    // Get completion model (fast, small, optimized for suggestions)
    ModelSelection getCompletionModel();

    // Get chat model (reasoning, large, interactive)
    ModelSelection getChatModel();

    // Get edit model (precise, medium, good at transformations)
    ModelSelection getEditModel();

    // Get embedding model (semantic search)
    ModelSelection getEmbeddingModel();

    // Get debug model (analytical, good at reasoning about bugs)
    ModelSelection getDebugModel();

    // Get optimization model (specialized for performance)
    ModelSelection getOptimizationModel();

    // Get security model (specialized for vulnerability detection)
    ModelSelection getSecurityModel();

    // Get documentation model
    ModelSelection getDocumentationModel();

    // Register custom model
    void registerModel(
        const std::string& modelName,
        const std::string& modelUrl,
        const std::vector<TaskType>& optimalFor,
        float expectedLatency,
        int contextWindow,
        bool supportsStreaming
    );

    // Set default model for task type
    void setDefaultModel(TaskType taskType, const std::string& modelName);

    // Get all available models
    std::vector<ModelSelection> getAvailableModels();

    // Get models for specific task
    std::vector<ModelSelection> getModelsForTask(TaskType taskType);

    // Configure model preferences
    void setSpeedPriority(float speedWeight);      // 0.0-1.0
    void setQualityPriority(float qualityWeight);  // 0.0-1.0
    void setMemoryLimit(int limitMB);

    // Get routing statistics
    struct RoutingStats {
        int totalRequests;
        std::unordered_map<std::string, int> modelUsage;
        std::unordered_map<std::string, float> avgLatency;
    };
    RoutingStats getStatistics();

private:
    struct ModelInfo {
        std::string name;
        std::string url;
        std::vector<TaskType> optimalFor;
        float expectedLatency;
        int contextWindow;
        bool supportsStreaming;
        int usageCount;
        float totalLatency;
    };

    // Model scoring
    float scoreModel(
        const ModelInfo& model,
        TaskType taskType,
        int contextSize
    );

    // Load models from Ollama
    bool loadModelsFromOllama();

    std::string m_ollamaEndpoint;
    std::unordered_map<std::string, ModelInfo> m_models;
    std::unordered_map<int, std::string> m_defaultModels;  // TaskType -> model name
    float m_speedWeight;
    float m_qualityWeight;
    int m_memoryLimit;
};

} // namespace IDE
} // namespace RawrXD
