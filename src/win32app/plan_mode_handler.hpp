#pragma once
// ============================================================================
// plan_mode_handler.hpp — Plan Mode: research → checklist → approval
// ============================================================================

#include <string>
#include <functional>
#include <memory>

class MetaPlanner;
class AgenticBridge;

namespace RawrXD {

struct PlanModeResult {
    std::string planText;       // Formatted checklist for user
    std::string planJson;       // Raw JSON from MetaPlanner (if used)
    std::string researchNote;   // Optional subagent research summary
    bool success = false;
};

// Runs Plan mode: optional MetaPlanner decomposition + optional subagent
// research, then returns a formatted plan. Does not execute the plan;
// approval/execution is a separate step (e.g. user sends "/approve" or
// clicks Execute in UI).
class PlanModeHandler {
public:
    PlanModeHandler();
    ~PlanModeHandler();

    void setMetaPlanner(MetaPlanner* planner) { m_metaPlanner = planner; }
    void setAgenticBridge(AgenticBridge* bridge) { m_agenticBridge = bridge; }

    // Run research subagent first (optional). If bridge is set and
    // useSubagentResearch is true, spawns subagent with prompt like
    // "Research and summarize: <user goal>" and merges into plan context.
    void setUseSubagentResearch(bool use) { m_useSubagentResearch = use; }
    bool getUseSubagentResearch() const { return m_useSubagentResearch; }

    // Synchronous: plan from user wish. Returns formatted checklist + JSON.
    PlanModeResult run(const std::string& userWish);

    // Format a JSON plan (from MetaPlanner) as human-readable checklist.
    static std::string formatPlanAsChecklist(const std::string& planJson);

private:
    MetaPlanner* m_metaPlanner = nullptr;
    AgenticBridge* m_agenticBridge = nullptr;
    bool m_useSubagentResearch = true;
};

} // namespace RawrXD
