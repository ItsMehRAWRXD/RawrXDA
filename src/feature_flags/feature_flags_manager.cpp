// ============================================================================
// Feature Flags Manager — Dynamic Feature Management
// Runtime feature toggling with A/B testing support
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <set>

namespace RawrXD::FeatureFlags {

enum class FeatureState {
    OFF,
    ON,
    PARTIAL
};

enum class RolloutStrategy {
    ALL_USERS,
    PERCENTAGE,
    USER_SEGMENT,
    GRADUAL
};

struct FeatureFlag {
    std::string key;
    std::string name;
    std::string description;
    FeatureState state;
    RolloutStrategy strategy;
    int rolloutPercentage;
    std::set<std::string> targetUsers;
    std::set<std::string> excludedUsers;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    std::string modifiedBy;
    std::map<std::string, std::string> metadata;
};

struct FeatureMetrics {
    std::string featureKey;
    int totalEvaluations;
    int enabledCount;
    int disabledCount;
    std::map<std::string, int> userSegmentCounts;
    std::chrono::system_clock::time_point measuredAt;
};

struct ABTest {
    std::string id;
    std::string featureKey;
    std::string variantA;
    std::string variantB;
    int trafficSplit;
    std::map<std::string, double> metrics;
    bool isActive;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point endedAt;
};

class FeatureFlagsManager {
public:
    explicit FeatureFlagsManager(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    void RegisterFeature(const FeatureFlag& feature) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_features[feature.key] = feature;
    }

    bool IsEnabled(const std::string& featureKey, const std::string& userId = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it == m_features.end()) {
            return false;
        }
        
        const auto& feature = it->second;
        
        // Check if explicitly off
        if (feature.state == FeatureState::OFF) {
            return false;
        }
        
        // Check if explicitly on
        if (feature.state == FeatureState::ON) {
            return true;
        }
        
        // Check excluded users
        if (feature.excludedUsers.find(userId) != feature.excludedUsers.end()) {
            return false;
        }
        
        // Check target users
        if (!feature.targetUsers.empty() && 
            feature.targetUsers.find(userId) == feature.targetUsers.end()) {
            return false;
        }
        
        // Check rollout percentage
        if (feature.strategy == RolloutStrategy::PERCENTAGE ||
            feature.strategy == RolloutStrategy::GRADUAL) {
            int userHash = std::hash<std::string>{}(userId + featureKey) % 100;
            return userHash < feature.rolloutPercentage;
        }
        
        return true;
    }

    void EnableFeature(const std::string& featureKey, const std::string& modifiedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it != m_features.end()) {
            it->second.state = FeatureState::ON;
            it->second.modifiedAt = std::chrono::system_clock::now();
            it->second.modifiedBy = modifiedBy;
        }
    }

    void DisableFeature(const std::string& featureKey, const std::string& modifiedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it != m_features.end()) {
            it->second.state = FeatureState::OFF;
            it->second.modifiedAt = std::chrono::system_clock::now();
            it->second.modifiedBy = modifiedBy;
        }
    }

    void SetRolloutPercentage(const std::string& featureKey, int percentage,
                             const std::string& modifiedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it != m_features.end()) {
            it->second.rolloutPercentage = std::max(0, std::min(100, percentage));
            it->second.strategy = RolloutStrategy::PERCENTAGE;
            it->second.modifiedAt = std::chrono::system_clock::now();
            it->second.modifiedBy = modifiedBy;
        }
    }

    void AddTargetUser(const std::string& featureKey, const std::string& userId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it != m_features.end()) {
            it->second.targetUsers.insert(userId);
        }
    }

    void RemoveTargetUser(const std::string& featureKey, const std::string& userId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_features.find(featureKey);
        if (it != m_features.end()) {
            it->second.targetUsers.erase(userId);
        }
    }

    ABTest StartABTest(const std::string& featureKey, const std::string& variantA,
                      const std::string& variantB, int trafficSplit) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ABTest test;
        test.id = GenerateTestId();
        test.featureKey = featureKey;
        test.variantA = variantA;
        test.variantB = variantB;
        test.trafficSplit = trafficSplit;
        test.isActive = true;
        test.startedAt = std::chrono::system_clock::now();
        
        m_abTests[test.id] = test;
        return test;
    }

    void EndABTest(const std::string& testId, const std::string& winningVariant) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_abTests.find(testId);
        if (it != m_abTests.end()) {
            it->second.isActive = false;
            it->second.endedAt = std::chrono::system_clock::now();
            
            // Apply winning variant
            auto featureIt = m_features.find(it->second.featureKey);
            if (featureIt != m_features.end()) {
                featureIt->second.metadata["winning_variant"] = winningVariant;
            }
        }
    }

    std::string GetABTestVariant(const std::string& testId, const std::string& userId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_abTests.find(testId);
        if (it == m_abTests.end() || !it->second.isActive) {
            return "";
        }
        
        int userHash = std::hash<std::string>{}(userId + testId) % 100;
        return userHash < it->second.trafficSplit ? it->second.variantA : it->second.variantB;
    }

    FeatureMetrics GetMetrics(const std::string& featureKey) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_metrics.find(featureKey);
        if (it != m_metrics.end()) {
            return it->second;
        }
        return FeatureMetrics{};
    }

    void RecordEvaluation(const std::string& featureKey, bool enabled, 
                         const std::string& userId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& metrics = m_metrics[featureKey];
        metrics.featureKey = featureKey;
        metrics.totalEvaluations++;
        
        if (enabled) {
            metrics.enabledCount++;
        } else {
            metrics.disabledCount++;
        }
        
        metrics.measuredAt = std::chrono::system_clock::now();
    }

    std::vector<FeatureFlag> GetAllFeatures() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        std::vector<FeatureFlag> features;
        for (const auto& [key, feature] : m_features) {
            features.push_back(feature);
        }
        return features;
    }

    std::string GenerateFeatureReport() {
        std::ostringstream report;
        report << "# Feature Flags Report\n\n";
        
        report << "## Active Features\n";
        for (const auto& [key, feature] : m_features) {
            report << "### " << feature.name << "\n";
            report << "- **Key:** " << feature.key << "\n";
            report << "- **State:** " << StateToString(feature.state) << "\n";
            report << "- **Strategy:** " << StrategyToString(feature.strategy) << "\n";
            report << "- **Rollout:** " << feature.rolloutPercentage << "%\n\n";
            
            auto metrics = GetMetrics(key);
            if (metrics.totalEvaluations > 0) {
                report << "**Metrics:**\n";
                report << "- Evaluations: " << metrics.totalEvaluations << "\n";
                report << "- Enabled: " << metrics.enabledCount << "\n";
                report << "- Disabled: " << metrics.disabledCount << "\n\n";
            }
        }
        
        report << "## A/B Tests\n";
        for (const auto& [id, test] : m_abTests) {
            report << "### Test " << id << "\n";
            report << "- **Feature:** " << test.featureKey << "\n";
            report << "- **Status:** " << (test.isActive ? "Active" : "Ended") << "\n";
            report << "- **Split:** " << test.trafficSplit << "% / " << (100 - test.trafficSplit) << "%\n\n";
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, FeatureFlag> m_features;
    std::map<std::string, FeatureMetrics> m_metrics;
    std::map<std::string, ABTest> m_abTests;

    std::string GenerateTestId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "abtest_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string StateToString(FeatureState state) {
        switch (state) {
            case FeatureState::OFF: return "Off";
            case FeatureState::ON: return "On";
            case FeatureState::PARTIAL: return "Partial";
            default: return "Unknown";
        }
    }

    std::string StrategyToString(RolloutStrategy strategy) {
        switch (strategy) {
            case RolloutStrategy::ALL_USERS: return "All Users";
            case RolloutStrategy::PERCENTAGE: return "Percentage";
            case RolloutStrategy::USER_SEGMENT: return "User Segment";
            case RolloutStrategy::GRADUAL: return "Gradual";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::FeatureFlags
