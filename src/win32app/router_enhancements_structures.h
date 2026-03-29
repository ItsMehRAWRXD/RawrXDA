// Enhanced structures for LLM Router optimizations (Qt-free implementation)
#include <string>
#include <vector>
#include <chrono>

struct ModelCapabilities {
    int reasoning = 0;           // Logic & analysis (0-100)
    int coding = 0;              // Code generation (0-100)
    int planning = 0;            // Task planning (0-100)
    int creativity = 0;          // Novel solutions (0-100)
    int speed = 0;               // Response latency (0-100, higher = faster)
    int costEfficiency = 0;      // Token cost efficiency (0-100, higher = cheaper)
    int contextWindow = 4096;    // Maximum context length
    double avgLatencyMs = 1000.0; // Average response time
    double costPerToken = 0.0;   // Cost per token in USD
};

struct RoutingDecision {
    std::string selectedModel;
    int confidenceScore = 0;     // 0-100
    std::string routingReason;
    std::vector<std::string> alternativeModels;
    ModelCapabilities capabilities;
    double estimatedCost = 0.0;
    double estimatedLatency = 0.0;
};

struct EnsembleConfig {
    std::vector<std::string> models;
    std::string consensusMethod;     // "voting", "weighted", "unanimous"
    int minModels = 2;
    int maxModels = 5;
    double confidenceThreshold = 0.7;
};

struct ABTestVariant {
    std::string modelName;
    std::string variantId;
    double trafficPercentage = 0.0;
    std::string metricsJson;  // JSON string instead of QJsonObject
    bool isActive = true;
};

struct ABTestExperiment {
    std::string experimentId;
    std::string testName;
    std::string metric;              // "latency", "cost", "accuracy", "user_satisfaction"
    std::vector<ABTestVariant> variants;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    bool isActive = false;
};

struct PerformanceMetrics {
    std::string modelName;
    int totalRequests = 0;
    int successfulRequests = 0;
    int failedRequests = 0;
    double avgLatencyMs = 0.0;
    double avgCostPerRequest = 0.0;
    double successRate = 0.0;
    std::chrono::system_clock::time_point lastUpdated;
};