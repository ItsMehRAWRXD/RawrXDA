// ============================================================================
// File: src/agent/hierarchical_planner.cpp
// Purpose: Hierarchical Task Planner implementation
// Converted from Qt to pure C++17
// ============================================================================
#include "hierarchical_planner.hpp"
#include "../common/logger.hpp"
#include <algorithm>

HierarchicalPlanner::HierarchicalPlanner() : m_idCounter(0) {}
HierarchicalPlanner::~HierarchicalPlanner() {}

std::vector<SubGoal> HierarchicalPlanner::decomposeWish(const std::string& wish, const std::string& context)
{
    (void)context;
    std::vector<SubGoal> subGoals;

    if (StringUtils::containsCI(wish, "add") || StringUtils::containsCI(wish, "implement")) {
        SubGoal analysis; analysis.id = generateUniqueId();
        analysis.description = "Analyze requirements for: " + wish;
        analysis.estimatedTime = 30; analysis.requiredTools = "code_analyzer";
        analysis.completed = false; analysis.status = "pending";
        subGoals.push_back(analysis);

        SubGoal design; design.id = generateUniqueId();
        design.description = "Design implementation approach";
        design.dependencies.push_back(analysis.id);
        design.estimatedTime = 45; design.requiredTools = "design_document";
        design.completed = false; design.status = "pending";
        subGoals.push_back(design);

        SubGoal impl; impl.id = generateUniqueId();
        impl.description = "Implement the feature";
        impl.dependencies.push_back(design.id);
        impl.estimatedTime = 120; impl.requiredTools = "ide,compiler";
        impl.completed = false; impl.status = "pending";
        subGoals.push_back(impl);

        SubGoal testing; testing.id = generateUniqueId();
        testing.description = "Test the implementation";
        testing.dependencies.push_back(impl.id);
        testing.estimatedTime = 60; testing.requiredTools = "test_framework";
        testing.completed = false; testing.status = "pending";
        subGoals.push_back(testing);

        SubGoal docs; docs.id = generateUniqueId();
        docs.description = "Update documentation";
        docs.dependencies.push_back(impl.id);
        docs.estimatedTime = 30; docs.requiredTools = "documentation_tool";
        docs.completed = false; docs.status = "pending";
        subGoals.push_back(docs);
    } else if (StringUtils::containsCI(wish, "optimize") || StringUtils::containsCI(wish, "improve")) {
        SubGoal profiling; profiling.id = generateUniqueId();
        profiling.description = "Profile current performance";
        profiling.estimatedTime = 45; profiling.requiredTools = "profiler";
        profiling.completed = false; profiling.status = "pending";
        subGoals.push_back(profiling);

        SubGoal analysis; analysis.id = generateUniqueId();
        analysis.description = "Analyze bottlenecks";
        analysis.dependencies.push_back(profiling.id);
        analysis.estimatedTime = 30; analysis.requiredTools = "analysis_tool";
        analysis.completed = false; analysis.status = "pending";
        subGoals.push_back(analysis);

        SubGoal impl; impl.id = generateUniqueId();
        impl.description = "Implement optimizations";
        impl.dependencies.push_back(analysis.id);
        impl.estimatedTime = 90; impl.requiredTools = "ide,compiler";
        impl.completed = false; impl.status = "pending";
        subGoals.push_back(impl);

        SubGoal validation; validation.id = generateUniqueId();
        validation.description = "Validate performance improvements";
        validation.dependencies.push_back(impl.id);
        validation.estimatedTime = 45; validation.requiredTools = "benchmark_tool";
        validation.completed = false; validation.status = "pending";
        subGoals.push_back(validation);
    } else {
        SubGoal research; research.id = generateUniqueId();
        research.description = "Research approach for: " + wish;
        research.estimatedTime = 30; research.requiredTools = "web_browser";
        research.completed = false; research.status = "pending";
        subGoals.push_back(research);

        SubGoal planning; planning.id = generateUniqueId();
        planning.description = "Create detailed plan";
        planning.dependencies.push_back(research.id);
        planning.estimatedTime = 45; planning.requiredTools = "planning_tool";
        planning.completed = false; planning.status = "pending";
        subGoals.push_back(planning);

        SubGoal execution; execution.id = generateUniqueId();
        execution.description = "Execute the plan";
        execution.dependencies.push_back(planning.id);
        execution.estimatedTime = 120; execution.requiredTools = "ide";
        execution.completed = false; execution.status = "pending";
        subGoals.push_back(execution);

        SubGoal review; review.id = generateUniqueId();
        review.description = "Review results";
        review.dependencies.push_back(execution.id);
        review.estimatedTime = 30; review.requiredTools = "review_tool";
        review.completed = false; review.status = "pending";
        subGoals.push_back(review);
    }

    auto plan = generateDetailedPlan(wish, subGoals, estimateResources(subGoals), assessRisks(subGoals));
    onPlanGenerated.emit(plan);
    return subGoals;
}

std::vector<SubGoal> HierarchicalPlanner::resolveDependencies(const std::vector<SubGoal>& subGoals)
{
    std::vector<SubGoal> executionOrder;
    std::map<std::string, int> inDegree;
    std::map<std::string, SubGoal> goalMap;

    for (const auto& goal : subGoals) {
        inDegree[goal.id] = static_cast<int>(goal.dependencies.size());
        goalMap[goal.id] = goal;
    }

    std::vector<std::string> queue;
    for (const auto& goal : subGoals) {
        if (goal.dependencies.empty()) queue.push_back(goal.id);
    }

    while (!queue.empty()) {
        std::string currentId = queue.front();
        queue.erase(queue.begin());
        executionOrder.push_back(goalMap[currentId]);

        for (const auto& goal : subGoals) {
            if (std::find(goal.dependencies.begin(), goal.dependencies.end(), currentId) != goal.dependencies.end()) {
                inDegree[goal.id]--;
                if (inDegree[goal.id] == 0) queue.push_back(goal.id);
            }
        }
    }

    if (executionOrder.size() != subGoals.size()) {
        logWarning() << "Circular dependency detected in plan";
        return subGoals;
    }

    onDependenciesResolved.emit(executionOrder);
    return executionOrder;
}

std::vector<ResourceRequirement> HierarchicalPlanner::estimateResources(const std::vector<SubGoal>& subGoals)
{
    std::vector<ResourceRequirement> resources;
    std::map<std::string, int> toolCount;

    for (const auto& goal : subGoals) {
        if (!goal.requiredTools.empty()) {
            auto tools = StringUtils::split(goal.requiredTools, ',');
            for (const auto& tool : tools) {
                toolCount[StringUtils::trimmed(tool)]++;
            }
        }
    }

    for (const auto& [name, count] : toolCount) {
        ResourceRequirement req;
        req.resourceType = "tool"; req.resourceName = name;
        req.quantity = count; req.unit = "instances";
        resources.push_back(req);
    }

    resources.push_back({"cpu", "processing_cores", 2, "cores"});
    resources.push_back({"memory", "ram", 4, "GB"});
    return resources;
}

std::vector<RiskAssessment> HierarchicalPlanner::assessRisks(const std::vector<SubGoal>& subGoals)
{
    std::vector<RiskAssessment> risks;

    for (const auto& goal : subGoals) {
        if (goal.estimatedTime > 120) {
            RiskAssessment r;
            r.riskId = generateUniqueId();
            r.description = "Sub-goal '" + goal.description + "' has long execution time (" + std::to_string(goal.estimatedTime) + " minutes)";
            r.probability = 0.3; r.impact = 0.7;
            r.mitigation = "Break into smaller sub-tasks or add progress checkpoints";
            r.mitigated = false;
            risks.push_back(r);
        }
        if (goal.dependencies.size() > 3) {
            RiskAssessment r;
            r.riskId = generateUniqueId();
            r.description = "Sub-goal '" + goal.description + "' has many dependencies (" + std::to_string(goal.dependencies.size()) + ")";
            r.probability = 0.4; r.impact = 0.6;
            r.mitigation = "Verify dependency order and add fallback mechanisms";
            r.mitigated = false;
            risks.push_back(r);
        }
        if (StringUtils::containsCI(goal.description, "implement") || StringUtils::containsCI(goal.description, "execute")) {
            RiskAssessment r;
            r.riskId = generateUniqueId();
            r.description = "Sub-goal '" + goal.description + "' is on critical path";
            r.probability = 0.5; r.impact = 0.8;
            r.mitigation = "Monitor progress closely and have backup plan ready";
            r.mitigated = false;
            risks.push_back(r);
        }
    }

    RiskAssessment intRisk;
    intRisk.riskId = generateUniqueId();
    intRisk.description = "Integration issues between sub-goals";
    intRisk.probability = 0.2; intRisk.impact = 0.6;
    intRisk.mitigation = "Implement integration tests and continuous validation";
    intRisk.mitigated = false;
    risks.push_back(intRisk);

    onRisksAssessed.emit(risks);
    return risks;
}

std::vector<SubGoal> HierarchicalPlanner::optimizeExecutionOrder(const std::vector<SubGoal>& subGoals)
{
    return resolveDependencies(subGoals);
}

JsonObject HierarchicalPlanner::generateDetailedPlan(const std::string& wish,
                                                    const std::vector<SubGoal>& subGoals,
                                                    const std::vector<ResourceRequirement>& resources,
                                                    const std::vector<RiskAssessment>& risks)
{
    JsonObject plan;
    plan["wish"] = JsonValue(wish);
    plan["generatedAt"] = JsonValue(TimeUtils::toISOString(TimeUtils::now()));
    plan["version"] = JsonValue("1.0");

    JsonArray goalsArray;
    for (const auto& goal : subGoals) {
        JsonObject goalObj;
        goalObj["id"] = JsonValue(goal.id);
        goalObj["description"] = JsonValue(goal.description);
        goalObj["estimatedTime"] = JsonValue(goal.estimatedTime);
        goalObj["requiredTools"] = JsonValue(goal.requiredTools);
        goalObj["completed"] = JsonValue(goal.completed);
        goalObj["status"] = JsonValue(goal.status);
        if (!goal.dependencies.empty()) {
            JsonArray depsArray;
            for (const auto& dep : goal.dependencies) depsArray.push_back(JsonValue(dep));
            goalObj["dependencies"] = JsonValue(depsArray);
        }
        goalsArray.push_back(JsonValue(goalObj));
    }
    plan["subGoals"] = JsonValue(goalsArray);

    JsonArray resArray;
    for (const auto& req : resources) {
        JsonObject obj;
        obj["resourceType"] = JsonValue(req.resourceType);
        obj["resourceName"] = JsonValue(req.resourceName);
        obj["quantity"] = JsonValue(req.quantity);
        obj["unit"] = JsonValue(req.unit);
        resArray.push_back(JsonValue(obj));
    }
    plan["resources"] = JsonValue(resArray);

    JsonArray risksArray;
    for (const auto& risk : risks) {
        JsonObject obj;
        obj["riskId"] = JsonValue(risk.riskId);
        obj["description"] = JsonValue(risk.description);
        obj["probability"] = JsonValue(risk.probability);
        obj["impact"] = JsonValue(risk.impact);
        obj["mitigation"] = JsonValue(risk.mitigation);
        obj["mitigated"] = JsonValue(risk.mitigated);
        risksArray.push_back(JsonValue(obj));
    }
    plan["risks"] = JsonValue(risksArray);
    return plan;
}

std::vector<SubGoal> HierarchicalPlanner::replanOnFailure(const SubGoal& failedSubGoal,
                                                         const std::vector<SubGoal>& currentPlan,
                                                         const std::string& errorMessage)
{
    auto updatedPlan = currentPlan;
    int failedIndex = -1;
    for (int i = 0; i < static_cast<int>(updatedPlan.size()); ++i) {
        if (updatedPlan[i].id == failedSubGoal.id) { failedIndex = i; break; }
    }

    if (failedIndex != -1) {
        updatedPlan[failedIndex].status = "failed";
        updatedPlan[failedIndex].completed = false;
        auto alternatives = generateAlternatives(failedSubGoal, errorMessage);
        for (int i = 0; i < static_cast<int>(alternatives.size()); ++i) {
            updatedPlan.insert(updatedPlan.begin() + failedIndex + 1 + i, alternatives[i]);
        }
    }
    onReplanOccurred.emit(updatedPlan);
    return updatedPlan;
}

std::vector<SubGoal> HierarchicalPlanner::getCriticalPath(const std::vector<SubGoal>& subGoals)
{
    return calculateCriticalPath(subGoals);
}

std::string HierarchicalPlanner::generateUniqueId() { return std::to_string(m_idCounter++); }

std::vector<SubGoal> HierarchicalPlanner::calculateCriticalPath(const std::vector<SubGoal>& subGoals)
{
    std::vector<SubGoal> criticalPath;
    for (const auto& goal : subGoals) {
        bool isCritical = false;
        bool hasDependents = false;
        for (const auto& other : subGoals) {
            if (std::find(other.dependencies.begin(), other.dependencies.end(), goal.id) != other.dependencies.end()) {
                hasDependents = true; break;
            }
        }
        if (!hasDependents) isCritical = true;
        if (goal.estimatedTime > 90) isCritical = true;
        if (StringUtils::containsCI(goal.description, "implement") ||
            StringUtils::containsCI(goal.description, "execute") ||
            StringUtils::containsCI(goal.description, "deploy")) isCritical = true;
        if (isCritical) criticalPath.push_back(goal);
    }
    return criticalPath;
}

bool HierarchicalPlanner::validateDependencies(const std::vector<SubGoal>& subGoals)
{
    for (const auto& goal : subGoals) {
        for (const auto& depId : goal.dependencies) {
            bool exists = false;
            for (const auto& other : subGoals) {
                if (other.id == depId) { exists = true; break; }
            }
            if (!exists) {
                logWarning() << "Sub-goal" << goal.id << "depends on non-existent goal" << depId;
                return false;
            }
        }
    }
    return true;
}

std::vector<SubGoal> HierarchicalPlanner::generateAlternatives(const SubGoal& failedSubGoal,
                                                               const std::string& errorMessage)
{
    (void)errorMessage;
    std::vector<SubGoal> alternatives;

    SubGoal alt1;
    alt1.id = generateUniqueId();
    alt1.description = "Alternative approach for: " + failedSubGoal.description;
    alt1.dependencies = failedSubGoal.dependencies;
    alt1.estimatedTime = static_cast<int>(failedSubGoal.estimatedTime * 1.2);
    alt1.requiredTools = failedSubGoal.requiredTools;
    alt1.completed = false; alt1.status = "pending";
    alternatives.push_back(alt1);

    SubGoal alt2;
    alt2.id = generateUniqueId();
    alt2.description = "Conservative approach for: " + failedSubGoal.description;
    alt2.dependencies = failedSubGoal.dependencies;
    alt2.estimatedTime = static_cast<int>(failedSubGoal.estimatedTime * 1.5);
    alt2.requiredTools = failedSubGoal.requiredTools;
    alt2.completed = false; alt2.status = "pending";
    alternatives.push_back(alt2);

    return alternatives;
}
