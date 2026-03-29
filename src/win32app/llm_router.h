#pragma once

#include "router_enhancements_structures.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

// LLM Router for intelligent model selection and routing
class LLMRouter {
public:
    LLMRouter();
    ~LLMRouter();

    bool Initialize(const std::vector<ModelCapabilities>& availableModels);
    RoutingDecision RouteRequest(const std::string& prompt, 
                               const std::string& taskType,
                               double budgetConstraint = 0.0);
    
    void RecordRequestResult(const std::string& modelName, 
                           double latencyMs, 
                           double cost,
                           bool success);
    
    void SetRoutingStrategy(const std::string& strategy) { m_routingStrategy = strategy; }
    std::string GetRoutingStrategy() const { return m_routingStrategy; }
    
    std::vector<PerformanceMetrics> GetPerformanceMetrics() const;

private:
    bool RegisterModel(const ModelCapabilities& capabilities);
    
    RoutingDecision routeByCost(const std::string& prompt, double budget);
    RoutingDecision routeByLatency(const std::string& prompt);
    RoutingDecision routeByQuality(const std::string& prompt, const std::string& taskType);
    RoutingDecision routeBalanced(const std::string& prompt, const std::string& taskType, double budget);
    
    double calculatePromptComplexity(const std::string& prompt) const;
    std::string generateModelName(int index) const;
    
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    std::string m_routingStrategy = "balanced";
    std::vector<ModelCapabilities> m_modelRegistry;
    std::unordered_map<std::string, PerformanceMetrics> m_performanceData;
};