#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Planner {
public:
    nlohmann::json plan(const std::string& humanWish);
private:
    nlohmann::json planQuantKernel(const std::string& wish);
    nlohmann::json planRelease(const std::string& wish);
    nlohmann::json planWebProject(const std::string& wish);
    nlohmann::json planSelfReplication(const std::string& wish);
    nlohmann::json planBulkFix(const std::string& wish);
    nlohmann::json planGeneric(const std::string& wish);
};
