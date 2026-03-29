#include "llm_router.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <sstream>
#include <limits>

LLMRouter::LLMRouter() = default;

LLMRouter::~LLMRouter() = default;

bool LLMRouter::Initialize(const std::vector<ModelCapabilities>& availableModels) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_modelRegistry = availableModels;
    
    // Initialize performance data for each model
    for (size_t i = 0; i < availableModels.size(); ++i) {
        std::string modelName = generateModelName(static_cast<int>(i));
        PerformanceMetrics metrics;
        metrics.modelName = modelName;
        metrics.lastUpdated = std::chrono::system_clock::now();
        m_performanceData[modelName] = metrics;
    }
    
    m_initialized = true;
    return true;
}

RoutingDecision LLMRouter::RouteRequest(const std::string& prompt, 
                                       const std::string& taskType,
                                       double budgetConstraint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized || m_modelRegistry.empty()) {
        return RoutingDecision{"", 0, "Router not initialized", {}, ModelCapabilities{}, 0.0, 0.0};
    }

    if (m_routingStrategy == "cost") {
        return routeByCost(prompt, budgetConstraint);
    } else if (m_routingStrategy == "latency") {
        return routeByLatency(prompt);
    } else if (m_routingStrategy == "quality") {
        return routeByQuality(prompt, taskType);
    } else {
        return routeBalanced(prompt, taskType, budgetConstraint);
    }
}

void LLMRouter::RecordRequestResult(const std::string& modelName, 
                                   double latencyMs, 
                                   double cost,
                                   bool success) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_performanceData.find(modelName);
    if (it != m_performanceData.end()) {
        auto& metrics = it->second;
        metrics.totalRequests++;
        if (success) {
            metrics.successfulRequests++;
        } else {
            metrics.failedRequests++;
        }
        metrics.avgLatencyMs = (metrics.avgLatencyMs * (metrics.totalRequests - 1) + latencyMs) / metrics.totalRequests;
        metrics.avgCostPerRequest = (metrics.avgCostPerRequest * (metrics.totalRequests - 1) + cost) / metrics.totalRequests;
        metrics.successRate = static_cast<double>(metrics.successfulRequests) / metrics.totalRequests;
        metrics.lastUpdated = std::chrono::system_clock::now();
    }
}

std::vector<PerformanceMetrics> LLMRouter::GetPerformanceMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<PerformanceMetrics> result;
    for (const auto& pair : m_performanceData) {
        result.push_back(pair.second);
    }
    return result;
}

RoutingDecision LLMRouter::routeByCost(const std::string& prompt, double budget) {
    if (m_modelRegistry.empty()) {
        return RoutingDecision{"", 0, "No models available", {}, ModelCapabilities{}, 0.0, 0.0};
    }

    size_t bestIndex = 0;
    double lowestCost = m_modelRegistry[0].costPerToken;
    
    for (size_t i = 1; i < m_modelRegistry.size(); ++i) {
        if (m_modelRegistry[i].costPerToken < lowestCost) {
            lowestCost = m_modelRegistry[i].costPerToken;
            bestIndex = i;
        }
    }

    std::string modelName = generateModelName(static_cast<int>(bestIndex));
    return RoutingDecision{
        modelName, 
        80, 
        "Selected by cost optimization", 
        {}, 
        m_modelRegistry[bestIndex], 
        lowestCost * calculatePromptComplexity(prompt), 
        m_modelRegistry[bestIndex].avgLatencyMs
    };
}

RoutingDecision LLMRouter::routeByLatency(const std::string& prompt) {
    if (m_modelRegistry.empty()) {
        return RoutingDecision{"", 0, "No models available", {}, ModelCapabilities{}, 0.0, 0.0};
    }

    size_t bestIndex = 0;
    double lowestLatency = m_modelRegistry[0].avgLatencyMs;
    
    for (size_t i = 1; i < m_modelRegistry.size(); ++i) {
        if (m_modelRegistry[i].avgLatencyMs < lowestLatency) {
            lowestLatency = m_modelRegistry[i].avgLatencyMs;
            bestIndex = i;
        }
    }

    std::string modelName = generateModelName(static_cast<int>(bestIndex));
    return RoutingDecision{
        modelName, 
        85, 
        "Selected by latency optimization", 
        {}, 
        m_modelRegistry[bestIndex], 
        m_modelRegistry[bestIndex].costPerToken * calculatePromptComplexity(prompt), 
        lowestLatency
    };
}

RoutingDecision LLMRouter::routeByQuality(const std::string& prompt, const std::string& taskType) {
    if (m_modelRegistry.empty()) {
        return RoutingDecision{"", 0, "No models available", {}, ModelCapabilities{}, 0.0, 0.0};
    }

    size_t bestIndex = 0;
    int highestScore = 0;
    
    for (size_t i = 0; i < m_modelRegistry.size(); ++i) {
        int score = 0;
        if (taskType == "coding") {
            score = m_modelRegistry[i].coding;
        } else if (taskType == "reasoning") {
            score = m_modelRegistry[i].reasoning;
        } else if (taskType == "planning") {
            score = m_modelRegistry[i].planning;
        } else if (taskType == "creative") {
            score = m_modelRegistry[i].creativity;
        } else {
            // General purpose - average of all capabilities
            score = (m_modelRegistry[i].reasoning + m_modelRegistry[i].coding + 
                    m_modelRegistry[i].planning + m_modelRegistry[i].creativity) / 4;
        }
        
        if (score > highestScore) {
            highestScore = score;
            bestIndex = i;
        }
    }

    std::string modelName = generateModelName(static_cast<int>(bestIndex));
    return RoutingDecision{
        modelName, 
        highestScore, 
        "Selected by quality optimization for " + taskType, 
        {}, 
        m_modelRegistry[bestIndex], 
        m_modelRegistry[bestIndex].costPerToken * calculatePromptComplexity(prompt), 
        m_modelRegistry[bestIndex].avgLatencyMs
    };
}

RoutingDecision LLMRouter::routeBalanced(const std::string& prompt, const std::string& taskType, double budget) {
    if (m_modelRegistry.empty()) {
        return RoutingDecision{"", 0, "No models available", {}, ModelCapabilities{}, 0.0, 0.0};
    }

    size_t bestIndex = 0;
    double bestScore = 0.0;
    
    for (size_t i = 0; i < m_modelRegistry.size(); ++i) {
        // Calculate balanced score: quality (40%) + cost efficiency (30%) + speed (30%)
        int qualityScore = 0;
        if (taskType == "coding") {
            qualityScore = m_modelRegistry[i].coding;
        } else if (taskType == "reasoning") {
            qualityScore = m_modelRegistry[i].reasoning;
        } else {
            qualityScore = (m_modelRegistry[i].reasoning + m_modelRegistry[i].coding) / 2;
        }
        
        double costScore = 100.0 - (m_modelRegistry[i].costPerToken * 1000.0); // Normalize cost
        double speedScore = 100.0 - (m_modelRegistry[i].avgLatencyMs / 10.0); // Normalize latency
        
        double score = qualityScore * 0.4 + costScore * 0.3 + speedScore * 0.3;
        
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    std::string modelName = generateModelName(static_cast<int>(bestIndex));
    return RoutingDecision{
        modelName, 
        static_cast<int>(bestScore), 
        "Selected by balanced optimization", 
        {}, 
        m_modelRegistry[bestIndex], 
        m_modelRegistry[bestIndex].costPerToken * calculatePromptComplexity(prompt), 
        m_modelRegistry[bestIndex].avgLatencyMs
    };
}

double LLMRouter::calculatePromptComplexity(const std::string& prompt) const {
    // Simple complexity calculation based on length and content
    double complexity = static_cast<double>(prompt.length()) / 1000.0;
    
    // Add complexity for code-like content
    if (prompt.find("function") != std::string::npos || 
        prompt.find("class") != std::string::npos ||
        prompt.find("import") != std::string::npos) {
        complexity *= 1.5;
    }
    
    return std::max(1.0, complexity);
}

std::string LLMRouter::generateModelName(int index) const {
    return "model_" + std::to_string(index);
}