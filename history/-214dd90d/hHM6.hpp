#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class Planner {
public:
    // Convert natural language wish into structured task list
    nlohmann::json plan(const std::string& humanWish);

private:
    nlohmann::json planQuantKernel(const std::string& wish);
    nlohmann::json planRelease(const std::string& wish);
    nlohmann::json planWebProject(const std::string& wish);
    nlohmann::json planSelfReplication(const std::string& wish);
    nlohmann::json planGeneric(const std::string& wish);
};
