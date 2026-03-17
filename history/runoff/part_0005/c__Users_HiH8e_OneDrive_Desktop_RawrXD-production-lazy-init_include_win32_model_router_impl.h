#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

// ============================================================================
// MODEL ROUTER - Multi-Backend LLM Routing
// ============================================================================

struct ModelConfig {
    std::string id;
    std::string name;
    std::string type;  // "local", "cloud", "quantized"
    std::string backend;  // "ollama", "openai", "anthropic", etc.
    std::string endpoint;
    int maxTokens = 2048;
    double temperature = 0.7;
};

struct InferenceRequest {
    std::string model;
    std::string prompt;
    int maxTokens = 2048;
    double temperature = 0.7;
};

struct InferenceResponse {
    bool success = false;
    std::string text;
    int tokensUsed = 0;
    double latencyMs = 0.0;
    std::string error;
};

class ModelRouter {
public:
    static ModelRouter& getInstance();

    // Model management
    void registerModel(const ModelConfig& config);
    void removeModel(const std::string& modelId);
    std::vector<ModelConfig> getAvailableModels() const;
    ModelConfig getModel(const std::string& modelId) const;

    // Inference
    InferenceResponse infer(const InferenceRequest& request);
    InferenceResponse inferStreaming(const InferenceRequest& request, 
                                      std::function<void(const std::string&)> streamCallback);

    // Load balancing
    void setLoadBalancingStrategy(const std::string& strategy);
    std::string selectBestModel();

    // Health checks
    bool isModelHealthy(const std::string& modelId);
    void checkHealth();

    // Metrics
    struct Metrics {
        int totalRequests = 0;
        int totalTokens = 0;
        double averageLatencyMs = 0.0;
    };
    Metrics getMetrics() const { return m_metrics; }

private:
    ModelRouter() = default;
    ~ModelRouter() = default;

    std::map<std::string, ModelConfig> m_models;
    std::map<std::string, int> m_callCounts;  // For round-robin
    Metrics m_metrics;
    std::string m_strategy = "round-robin";

    static ModelRouter* s_instance;
};

