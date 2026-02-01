#pragma once

#include <string>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class MetaPlanner {
public:
    // natural language -> JSON task list
    json plan(const std::string& humanWish);

    // decompose high-level goal into sub-tasks (simple wrapper for now)
    json decomposeGoal(const std::string& goal) { return plan(goal); }

private:
    json quantPlan(const std::string& wish);
    json kernelPlan(const std::string& wish);
    json releasePlan(const std::string& wish);
    json fixPlan(const std::string& wish);
    json perfPlan(const std::string& wish);
    json testPlan(const std::string& wish);
    json genericPlan(const std::string& wish);
    json task(const std::string& type, const std::string& target, const json& params);
};

    void* task(const std::string& type,
                     const std::string& target,
                     const void*& params = {});
};

