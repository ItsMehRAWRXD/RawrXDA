#pragma once
// ============================================================================
// meta_planner.hpp — Meta-Level Plan Decomposer
// ============================================================================
// Architecture: C++20, Win32, no exceptions
// Decomposes high-level goals into multi-phase task graphs with dependency
// chains, subsystem routing, cost estimation, and AgenticTaskGraph integration.
// Returns nlohmann::json throughout (no JsonArray/JsonObject).
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MetaPlanner {
public:
    // -----------------------------------------------------------------------
    // Primary entry point: natural language → structured task graph JSON
    // -----------------------------------------------------------------------
    json plan(const std::string& humanWish);

    // Alias for plan() — used by autonomous_workflow_engine
    json decomposeGoal(const std::string& goal) { return plan(goal); }

    // -----------------------------------------------------------------------
    // Multi-phase decomposition: breaks a goal into ordered phases,
    // each phase containing parallel-safe tasks with dependencies
    // -----------------------------------------------------------------------
    json decomposeMultiPhase(const std::string& goal);

    // -----------------------------------------------------------------------
    // Dependency chain generator: given a flat task list, infers
    // dependency edges based on task types and targets
    // -----------------------------------------------------------------------
    json inferDependencies(const json& flatTasks);

    // -----------------------------------------------------------------------
    // Cost estimation: estimates time/resource cost for a plan
    // -----------------------------------------------------------------------
    json estimateCost(const json& plan);

    // -----------------------------------------------------------------------
    // Subsystem router: assigns each task to the correct AI subsystem
    // (matches AgenticTaskGraph::TaskNode::Subsystem enum)
    // -----------------------------------------------------------------------
    json routeToSubsystems(const json& plan);

    // -----------------------------------------------------------------------
    // Plan merging: combine multiple plans with cross-plan dependencies
    // -----------------------------------------------------------------------
    json mergePlans(const std::vector<json>& plans);

    // -----------------------------------------------------------------------
    // Plan validation: check for cycles, missing deps, unreachable tasks
    // -----------------------------------------------------------------------
    json validatePlan(const json& plan);

private:
    // Domain-specific plan generators (multi-phase, with real decomposition)
    json quantPlan(const std::string& wish);
    json kernelPlan(const std::string& wish);
    json releasePlan(const std::string& wish);
    json fixPlan(const std::string& wish);
    json perfPlan(const std::string& wish);
    json testPlan(const std::string& wish);
    json refactorPlan(const std::string& wish);
    json migrationPlan(const std::string& wish);
    json securityPlan(const std::string& wish);
    json genericPlan(const std::string& wish);

    // Task builder: creates a single task node with full metadata
    json task(const std::string& type,
              const std::string& target,
              const json& params = json::object(),
              uint32_t priority = 5,
              uint32_t estimatedSeconds = 60,
              const std::string& subsystem = "agentic_loop");

    // Phase builder: wraps tasks into a named phase with ordering
    json phase(const std::string& name,
               uint32_t order,
               const json& tasks,
               const std::string& gateCondition = "all_pass");

    // Dependency edge builder
    json depEdge(uint32_t from, uint32_t to,
                 const std::string& type = "completion");

    // Internal: extract intent signals from natural language
    struct IntentSignals {
        std::string primaryDomain;    // quant, kernel, release, fix, perf, test, etc.
        std::string targetEntity;     // file, module, or system being acted on
        std::string quantType;        // Q8_K, Q6_K, etc. (if quant-related)
        std::string severity;         // critical, normal, low
        bool requiresTests;
        bool requiresBenchmark;
        bool requiresRollback;
        bool isMultiFile;
        bool isBreakingChange;
        std::vector<std::string> affectedPaths;
    };
    IntentSignals extractIntentSignals(const std::string& wish);

    // Internal: next task ID generator
    uint32_t nextTaskId_ = 1;
    uint32_t allocTaskId() { return nextTaskId_++; }
    void resetTaskIds() { nextTaskId_ = 1; }
};
