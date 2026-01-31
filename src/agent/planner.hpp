#pragma once
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Planner {
public:
    // Convert natural language wish into structured task list
    json plan(const std::string& humanWish);

private:
    json planQuantKernel(const std::string& wish);
    json planRelease(const std::string& wish);
    json planWebProject(const std::string& wish);
    json planSelfReplication(const std::string& wish);
    json planGeneric(const std::string& wish);
};
