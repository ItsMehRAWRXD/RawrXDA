#pragma once
#include <string>
#include <nlohmann/json.hpp>

class MetaPlanner {
public:
    nlohmann::json plan(const std::string& humanWish);
    nlohmann::json decomposeGoal(const std::string& goal) { return plan(goal); }
private:
    nlohmann::json quantPlan(const std::string& wish);
    nlohmann::json kernelPlan(const std::string& wish);
    nlohmann::json releasePlan(const std::string& wish);
    nlohmann::json fixPlan(const std::string& wish);
    nlohmann::json perfPlan(const std::string& wish);
    nlohmann::json testPlan(const std::string& wish);
    nlohmann::json genericPlan(const std::string& wish);
    nlohmann::json task(const std::string& type, const std::string& target,
                        const nlohmann::json& params = nlohmann::json::object());
};
