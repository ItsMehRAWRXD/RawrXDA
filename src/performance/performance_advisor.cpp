// ============================================================================
// Performance Advisor — Real-Time Performance Optimization
// Analyzes performance data and suggests/implements optimizations
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

namespace RawrXD::Performance {

enum class OptimizationType {
    ALGORITHM,
    MEMORY,
    CONCURRENCY,
    CACHING,
    NETWORK,
    DATABASE
};

enum class OptimizationPriority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct OptimizationSuggestion {
    OptimizationType type;
    OptimizationPriority priority;
    std::string description;
    std::string currentCode;
    std::string optimizedCode;
    double expectedImprovement;
    std::vector<std::string> risks;
    std::chrono::system_clock::time_point suggestedAt;
};

struct PerformanceData {
    std::string component;
    double cpuUsage;
    double memoryUsage;
    double latencyMs;
    double throughput;
    std::map<std::string, double> customMetrics;
    std::chrono::system_clock::time_point timestamp;
};

struct OptimizationPlan {
    std::string id;
    std::vector<OptimizationSuggestion> suggestions;
    double totalExpectedImprovement;
    std::chrono::system_clock::time_point createdAt;
    OptimizationPriority overallPriority;
};

struct ImpactMetrics {
    double latencyChange;
    double throughputChange;
    double cpuChange;
    double memoryChange;
    std::chrono::system_clock::time_point measuredAt;
};

class PerformanceAdvisor {
public:
    explicit PerformanceAdvisor(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {}

    std::vector<OptimizationSuggestion> AnalyzePerformance(const PerformanceData& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<OptimizationSuggestion> suggestions;
        
        // Analyze CPU usage
        if (data.cpuUsage > 80.0) {
            OptimizationSuggestion suggestion;
            suggestion.type = OptimizationType::ALGORITHM;
            suggestion.priority = OptimizationPriority::HIGH;
            suggestion.description = "High CPU usage detected. Consider algorithmic optimizations.";
            suggestion.expectedImprovement = 30.0;
            suggestion.suggestedAt = std::chrono::system_clock::now();
            suggestions.push_back(suggestion);
        }
        
        // Analyze memory usage
        if (data.memoryUsage > 85.0) {
            OptimizationSuggestion suggestion;
            suggestion.type = OptimizationType::MEMORY;
            suggestion.priority = OptimizationPriority::HIGH;
            suggestion.description = "High memory usage detected. Consider memory optimization.";
            suggestion.expectedImprovement = 25.0;
            suggestion.suggestedAt = std::chrono::system_clock::now();
            suggestions.push_back(suggestion);
        }
        
        // Analyze latency
        if (data.latencyMs > 100.0) {
            OptimizationSuggestion suggestion;
            suggestion.type = OptimizationType::CACHING;
            suggestion.priority = OptimizationPriority::MEDIUM;
            suggestion.description = "High latency detected. Consider caching strategies.";
            suggestion.expectedImprovement = 50.0;
            suggestion.suggestedAt = std::chrono::system_clock::now();
            suggestions.push_back(suggestion);
        }
        
        // AI-powered analysis
        if (m_aiClient && m_aiClient->IsLoaded()) {
            auto aiSuggestions = PerformAIAnalysis(data);
            suggestions.insert(suggestions.end(), aiSuggestions.begin(), aiSuggestions.end());
        }
        
        // Store suggestions
        for (const auto& suggestion : suggestions) {
            m_suggestions[suggestion.description] = suggestion;
        }
        
        return suggestions;
    }

    OptimizationPlan CreateOptimizationPlan(const std::vector<OptimizationSuggestion>& suggestions) {
        OptimizationPlan plan;
        plan.id = GeneratePlanId();
        plan.createdAt = std::chrono::system_clock::now();
        plan.suggestions = suggestions;
        
        // Calculate total expected improvement
        double totalImprovement = 0.0;
        OptimizationPriority highestPriority = OptimizationPriority::LOW;
        
        for (const auto& suggestion : suggestions) {
            totalImprovement += suggestion.expectedImprovement;
            if (static_cast<int>(suggestion.priority) > static_cast<int>(highestPriority)) {
                highestPriority = suggestion.priority;
            }
        }
        
        plan.totalExpectedImprovement = totalImprovement;
        plan.overallPriority = highestPriority;
        
        m_plans[plan.id] = plan;
        return plan;
    }

    void ImplementOptimizations(const OptimizationPlan& plan) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (const auto& suggestion : plan.suggestions) {
            try {
                ImplementOptimization(suggestion);
                m_implementedOptimizations.push_back(suggestion);
            } catch (const std::exception& e) {
                // Log failure
            }
        }
    }

    ImpactMetrics MonitorOptimizationImpact(const std::string& planId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ImpactMetrics metrics;
        metrics.measuredAt = std::chrono::system_clock::now();
        
        auto it = m_plans.find(planId);
        if (it == m_plans.end()) {
            return metrics;
        }
        
        // Compare before/after metrics
        // This would integrate with your performance monitoring system
        
        return metrics;
    }

    std::vector<OptimizationSuggestion> GetPendingSuggestions() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<OptimizationSuggestion> pending;
        for (const auto& [desc, suggestion] : m_suggestions) {
            // Check if not yet implemented
            bool implemented = false;
            for (const auto& impl : m_implementedOptimizations) {
                if (impl.description == desc) {
                    implemented = true;
                    break;
                }
            }
            if (!implemented) {
                pending.push_back(suggestion);
            }
        }
        return pending;
    }

    std::string GeneratePerformanceReport() const {
        std::ostringstream report;
        report << "# Performance Optimization Report\n\n";
        
        report << "## Implemented Optimizations\n";
        for (const auto& opt : m_implementedOptimizations) {
            report << "- " << opt.description << " (" << opt.expectedImprovement << "% improvement)\n";
        }
        
        report << "\n## Pending Suggestions\n";
        auto pending = GetPendingSuggestions();
        for (const auto& suggestion : pending) {
            report << "- [" << PriorityToString(suggestion.priority) << "] " << suggestion.description << "\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, OptimizationSuggestion> m_suggestions;
    std::map<std::string, OptimizationPlan> m_plans;
    std::vector<OptimizationSuggestion> m_implementedOptimizations;

    std::vector<OptimizationSuggestion> PerformAIAnalysis(const PerformanceData& data) {
        std::vector<OptimizationSuggestion> suggestions;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return suggestions;
        }

        std::string prompt = "Analyze this performance data and suggest optimizations:\n" +
                            "Component: " + data.component + "\n" +
                            "CPU: " + std::to_string(data.cpuUsage) + "%\n" +
                            "Memory: " + std::to_string(data.memoryUsage) + "%\n" +
                            "Latency: " + std::to_string(data.latencyMs) + "ms\n";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a performance optimization expert."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            OptimizationSuggestion suggestion;
            suggestion.type = OptimizationType::ALGORITHM;
            suggestion.priority = OptimizationPriority::MEDIUM;
            suggestion.description = "AI Suggestion: " + result.response;
            suggestion.expectedImprovement = 20.0;
            suggestion.suggestedAt = std::chrono::system_clock::now();
            suggestions.push_back(suggestion);
        }
        
        return suggestions;
    }

    void ImplementOptimization(const OptimizationSuggestion& suggestion) {
        // Implement the optimization
        // This would integrate with your code transformation system
    }

    std::string GeneratePlanId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "opt_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string PriorityToString(OptimizationPriority priority) {
        switch (priority) {
            case OptimizationPriority::LOW: return "LOW";
            case OptimizationPriority::MEDIUM: return "MEDIUM";
            case OptimizationPriority::HIGH: return "HIGH";
            case OptimizationPriority::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};

} // namespace RawrXD::Performance
