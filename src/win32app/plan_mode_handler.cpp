// ============================================================================
// plan_mode_handler.cpp — Plan Mode implementation
// ============================================================================

#include "plan_mode_handler.hpp"
#include "../agent/meta_planner.hpp"
#include "Win32IDE_AgenticBridge.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace RawrXD {

PlanModeHandler::PlanModeHandler() = default;
PlanModeHandler::~PlanModeHandler() = default;

PlanModeResult PlanModeHandler::run(const std::string& userWish) {
    PlanModeResult out;
    if (userWish.empty()) {
        out.planText = "[Plan] No goal specified.";
        return out;
    }

    std::string researchNote;
    if (m_useSubagentResearch && m_agenticBridge) {
        std::string researchPrompt = "Research and summarize the current state and constraints for: " + userWish;
        researchNote = m_agenticBridge->RunSubAgent("Plan-mode research", researchPrompt);
        if (!researchNote.empty())
            out.researchNote = researchNote;
    }

    if (m_metaPlanner) {
        try {
            nlohmann::json planJson = m_metaPlanner->plan(userWish);
            out.planJson = planJson.dump();
            out.planText = formatPlanAsChecklist(out.planJson);
            out.success = !out.planText.empty();
        } catch (...) {
            out.planText = "[Plan] MetaPlanner failed. Falling back to simple plan.";
            out.planJson = "{}";
        }
    } else {
        out.planText = "[Plan] No MetaPlanner configured. Goal: " + userWish + "\n";
        if (!researchNote.empty())
            out.planText += "\nResearch summary:\n" + researchNote + "\n";
        out.planText += "\nReply with /approve to run this as a single Agent step, or add steps manually.";
        out.success = true;
    }

    if (!out.researchNote.empty() && !out.planText.empty())
        out.planText = "Research:\n" + out.researchNote + "\n\n" + out.planText;

    return out;
}

std::string PlanModeHandler::formatPlanAsChecklist(const std::string& planJson) {
    if (planJson.empty() || planJson == "{}") return "";

    try {
        auto j = nlohmann::json::parse(planJson);
        std::ostringstream os;
        os << "--- Plan (approve to execute) ---\n";

        if (j.contains("phases") && j["phases"].is_array()) {
            int step = 1;
            for (const auto& phase : j["phases"]) {
                std::string phaseName = phase.contains("name") ? phase["name"].get<std::string>() : "Phase";
                if (phase.contains("tasks") && phase["tasks"].is_array()) {
                    for (const auto& task : phase["tasks"]) {
                        std::string type = task.contains("type") ? task["type"].get<std::string>() : "task";
                        std::string target = task.contains("target") ? task["target"].get<std::string>() : "";
                        os << "  " << step++ << ". [" << type << "] " << target << "\n";
                    }
                }
            }
        } else if (j.contains("tasks") && j["tasks"].is_array()) {
            int step = 1;
            for (const auto& task : j["tasks"]) {
                std::string type = task.contains("type") ? task["type"].get<std::string>() : "task";
                std::string target = task.contains("target") ? task["target"].get<std::string>() : "";
                os << "  " << step++ << ". [" << type << "] " << target << "\n";
            }
        } else {
            os << planJson << "\n";
        }

        os << "--- Reply /approve to run, or edit and re-plan ---\n";
        return os.str();
    } catch (...) {
        return "[Plan] Could not parse plan JSON.\n" + planJson;
    }
}

} // namespace RawrXD
