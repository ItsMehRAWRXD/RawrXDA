// ============================================================================
// Predictive Resource Scaling — Intelligent Auto-Scaling
// Predicts resource needs and automatically scales infrastructure
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../performance/realtime_profiler.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::Scale {

enum class ScalingDirection {
    SCALE_UP,
    SCALE_DOWN,
    NO_CHANGE
};

struct UsageMetrics {
    double cpuUtilization;
    double memoryUtilization;
    double networkThroughput;
    double requestLatency;
    int activeConnections;
    std::chrono::system_clock::time_point timestamp;
};

struct ScalingPlan {
    ScalingDirection direction;
    int currentInstances;
    int targetInstances;
    std::string reason;
    std::chrono::system_clock::time_point plannedAt;
    std::chrono::system_clock::time_point executeAt;
    std::map<std::string, std::string> parameters;
};

struct ScalingResult {
    std::string planId;
    bool success;
    int instancesBefore;
    int instancesAfter;
    std::chrono::system_clock::time_point executedAt;
    std::string errorMessage;
    std::map<std::string, double> impactMetrics;
};

class PredictiveScaler {
public:
    explicit PredictiveScaler(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient)
        , m_currentInstances(1)
        , m_minInstances(1)
        , m_maxInstances(100) {}

    ScalingPlan CalculateScalingNeeds(const UsageMetrics& metrics) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ScalingPlan plan;
        plan.currentInstances = m_currentInstances;
        plan.plannedAt = std::chrono::system_clock::now();
        
        // Store metrics for trend analysis
        m_metricsHistory.push_back(metrics);
        if (m_metricsHistory.size() > 100) {
            m_metricsHistory.erase(m_metricsHistory.begin());
        }
        
        // Calculate scaling needs based on current metrics
        if (metrics.cpuUtilization > 80.0 || metrics.memoryUtilization > 85.0) {
            plan.direction = ScalingDirection::SCALE_UP;
            plan.targetInstances = std::min(m_currentInstances + CalculateScaleUpAmount(metrics), m_maxInstances);
            plan.reason = "High resource utilization detected";
        } else if (metrics.cpuUtilization < 20.0 && metrics.memoryUtilization < 30.0) {
            plan.direction = ScalingDirection::SCALE_DOWN;
            plan.targetInstances = std::max(m_currentInstances - 1, m_minInstances);
            plan.reason = "Low resource utilization detected";
        } else {
            plan.direction = ScalingDirection::NO_CHANGE;
            plan.targetInstances = m_currentInstances;
            plan.reason = "Resource utilization within normal range";
        }
        
        // Predictive adjustment based on trends
        if (m_metricsHistory.size() >= 10) {
            auto trend = CalculateTrend();
            if (trend > 0.1 && plan.direction == ScalingDirection::NO_CHANGE) {
                // Predictive scale up
                plan.direction = ScalingDirection::SCALE_UP;
                plan.targetInstances = std::min(m_currentInstances + 1, m_maxInstances);
                plan.reason = "Predictive: increasing trend detected";
            }
        }
        
        // Schedule execution
        plan.executeAt = plan.plannedAt + std::chrono::seconds(60); // 1 minute delay
        
        return plan;
    }

    ScalingResult ExecuteScalingOperations(const ScalingPlan& plan) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ScalingResult result;
        result.planId = GeneratePlanId();
        result.instancesBefore = m_currentInstances;
        result.executedAt = std::chrono::system_clock::now();
        
        if (plan.direction == ScalingDirection::NO_CHANGE) {
            result.success = true;
            result.instancesAfter = m_currentInstances;
            return result;
        }
        
        try {
            // Execute scaling
            if (plan.direction == ScalingDirection::SCALE_UP) {
                ScaleUp(plan.targetInstances - m_currentInstances);
            } else {
                ScaleDown(m_currentInstances - plan.targetInstances);
            }
            
            m_currentInstances = plan.targetInstances;
            result.success = true;
            result.instancesAfter = m_currentInstances;
            
            // Calculate impact metrics
            result.impactMetrics = CalculateImpactMetrics(plan);
            
        } catch (const std::exception& e) {
            result.success = false;
            result.errorMessage = e.what();
            result.instancesAfter = m_currentInstances;
        }
        
        m_scalingHistory.push_back(result);
        return result;
    }

    void LearnFromScalingOutcomes(const ScalingResult& result) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Analyze the outcome
        if (result.success) {
            // Update scaling effectiveness model
            auto it = m_scalingEffectiveness.find(result.planId);
            if (it != m_scalingEffectiveness.end()) {
                it->second.successCount++;
            } else {
                m_scalingEffectiveness[result.planId] = {1, 0};
            }
            
            // Adjust thresholds based on impact
            if (result.impactMetrics.find("latency_improvement") != result.impactMetrics.end()) {
                double improvement = result.impactMetrics["latency_improvement"];
                if (improvement < 0.1) {
                    // Scaling had minimal impact, adjust thresholds
                    AdjustThresholds(0.95);
                }
            }
        } else {
            // Learn from failure
            auto it = m_scalingEffectiveness.find(result.planId);
            if (it != m_scalingEffectiveness.end()) {
                it->second.failureCount++;
            } else {
                m_scalingEffectiveness[result.planId] = {0, 1};
            }
        }
    }

    void SetScalingLimits(int minInstances, int maxInstances) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minInstances = minInstances;
        m_maxInstances = maxInstances;
    }

    std::vector<ScalingResult> GetScalingHistory(int limit = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<ScalingResult> history;
        int count = 0;
        for (auto it = m_scalingHistory.rbegin(); 
             it != m_scalingHistory.rend() && count < limit; 
             ++it, ++count) {
            history.push_back(*it);
        }
        return history;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    int m_currentInstances;
    int m_minInstances;
    int m_maxInstances;
    std::vector<UsageMetrics> m_metricsHistory;
    std::vector<ScalingResult> m_scalingHistory;
    
    struct Effectiveness {
        int successCount;
        int failureCount;
    };
    std::map<std::string, Effectiveness> m_scalingEffectiveness;

    int CalculateScaleUpAmount(const UsageMetrics& metrics) {
        // Calculate how many instances to add based on load
        double loadFactor = std::max(metrics.cpuUtilization / 100.0, 
                                     metrics.memoryUtilization / 100.0);
        int additionalInstances = static_cast<int>(std::ceil(loadFactor * 2));
        return std::max(1, additionalInstances);
    }

    double CalculateTrend() {
        if (m_metricsHistory.size() < 10) return 0.0;
        
        // Calculate trend in CPU utilization
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        int n = std::min(10, static_cast<int>(m_metricsHistory.size()));
        
        for (int i = 0; i < n; ++i) {
            sumX += i;
            sumY += m_metricsHistory[m_metricsHistory.size() - n + i].cpuUtilization;
            sumXY += i * m_metricsHistory[m_metricsHistory.size() - n + i].cpuUtilization;
            sumX2 += i * i;
        }
        
        return (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    }

    void ScaleUp(int count) {
        // Implement scale up logic
        // This would integrate with your orchestration system
    }

    void ScaleDown(int count) {
        // Implement scale down logic
        // This would integrate with your orchestration system
    }

    std::map<std::string, double> CalculateImpactMetrics(const ScalingPlan& plan) {
        std::map<std::string, double> metrics;
        
        // Calculate expected improvements
        if (plan.direction == ScalingDirection::SCALE_UP) {
            double loadDistribution = static_cast<double>(plan.currentInstances) / plan.targetInstances;
            metrics["expected_cpu_reduction"] = (1.0 - loadDistribution) * 100.0;
            metrics["expected_latency_improvement"] = loadDistribution * 50.0;
        }
        
        return metrics;
    }

    void AdjustThresholds(double factor) {
        // Adjust scaling thresholds based on learning
    }

    std::string GeneratePlanId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "scale_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }
};

} // namespace RawrXD::Scale
