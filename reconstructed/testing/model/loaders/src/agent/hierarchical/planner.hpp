// ============================================================================
// File: src/agent/hierarchical_planner.hpp
// Purpose: Hierarchical Task Planner for Complex Wish Decomposition
// Converted from Qt to pure C++17
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

struct SubGoal {
    std::string id;
    std::string description;
    std::vector<std::string> dependencies;
    int estimatedTime;
    std::string requiredTools;
    bool completed;
    std::string status;
};

struct ResourceRequirement {
    std::string resourceType;
    std::string resourceName;
    int quantity;
    std::string unit;
};

struct RiskAssessment {
    std::string riskId;
    std::string description;
    double probability;
    double impact;
    std::string mitigation;
    bool mitigated;
};

class HierarchicalPlanner {
public:
    HierarchicalPlanner();
    ~HierarchicalPlanner();

    std::vector<SubGoal> decomposeWish(const std::string& wish, const std::string& context);
    std::vector<SubGoal> resolveDependencies(const std::vector<SubGoal>& subGoals);
    std::vector<ResourceRequirement> estimateResources(const std::vector<SubGoal>& subGoals);
    std::vector<RiskAssessment> assessRisks(const std::vector<SubGoal>& subGoals);
    std::vector<SubGoal> optimizeExecutionOrder(const std::vector<SubGoal>& subGoals);
    JsonObject generateDetailedPlan(const std::string& wish, const std::vector<SubGoal>& subGoals,
                                   const std::vector<ResourceRequirement>& resources,
                                   const std::vector<RiskAssessment>& risks);
    std::vector<SubGoal> replanOnFailure(const SubGoal& failedSubGoal,
                                        const std::vector<SubGoal>& currentPlan,
                                        const std::string& errorMessage);
    std::vector<SubGoal> getCriticalPath(const std::vector<SubGoal>& subGoals);

    CallbackList<const JsonObject&> onPlanGenerated;
    CallbackList<const std::vector<SubGoal>&> onDependenciesResolved;
    CallbackList<const std::vector<RiskAssessment>&> onRisksAssessed;
    CallbackList<const std::vector<SubGoal>&> onReplanOccurred;

private:
    std::string generateUniqueId();
    std::vector<SubGoal> calculateCriticalPath(const std::vector<SubGoal>& subGoals);
    bool validateDependencies(const std::vector<SubGoal>& subGoals);
    std::vector<SubGoal> generateAlternatives(const SubGoal& failedSubGoal, const std::string& errorMessage);

    int m_idCounter;
};
