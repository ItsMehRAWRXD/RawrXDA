#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {

class MetaPlanner {
public:
    // natural language -> JSON task list
    nlohmann::json plan(const std::string& humanWish);

    // decompose high-level goal into sub-tasks (simple wrapper for now)
    nlohmann::json decomposeGoal(const std::string& goal) { return plan(goal); }

private:
    nlohmann::json quantPlan(const std::string& wish);
    nlohmann::json kernelPlan(const std::string& wish);
    nlohmann::json releasePlan(const std::string& wish);
    nlohmann::json fixPlan(const std::string& wish);
    nlohmann::json perfPlan(const std::string& wish);
    nlohmann::json testPlan(const std::string& wish);
    nlohmann::json genericPlan(const std::string& wish);

    nlohmann::json task(const std::string& type,
                        const std::string& target,
                        const nlohmann::json& params = nlohmann::json::object());
};

} // namespace RawrXD
