#include "meta_planner.hpp"
#include <regex>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace RawrXD {

nlohmann::json MetaPlanner::plan(const std::string& humanWish) {
    // 1. normalise
    std::string wish = humanWish;
    std::transform(wish.begin(), wish.end(), wish.begin(), ::tolower);
    wish.erase(0, wish.find_first_not_of(" \t\n\r"));
    wish.erase(wish.find_last_not_of(" \t\n\r") + 1);

    // 2. keyword -> template
    if (wish.find("quant") != std::string::npos || wish.find("quantize") != std::string::npos)
        return quantPlan(wish);

    if (wish.find("kernel") != std::string::npos || wish.find("asm") != std::string::npos || wish.find("neon") != std::string::npos)
        return kernelPlan(wish);

    if (wish.find("ship") != std::string::npos || wish.find("release") != std::string::npos || wish.find("tag") != std::string::npos)
        return releasePlan(wish);

    if (wish.find("fix") != std::string::npos || wish.find("bug") != std::string::npos || wish.find("crash") != std::string::npos)
        return fixPlan(wish);

    if (wish.find("perf") != std::string::npos || wish.find("speed") != std::string::npos || wish.find("fast") != std::string::npos)
        return perfPlan(wish);

    if (wish.find("test") != std::string::npos || wish.find("coverage") != std::string::npos)
        return testPlan(wish);

    // 3. fallback - generic dev cycle
    return genericPlan(wish);
}

// ---------- keyword -> plan templates ----------

nlohmann::json MetaPlanner::quantPlan(const std::string& wish) {
    nlohmann::json plan = nlohmann::json::array();
    std::string lastWord = wish.substr(wish.find_last_of(' ') + 1);
    plan.push_back(task("add_kernel", "quant_vulkan", {{"type", lastWord}}));
    plan.push_back(task("add_cpp", "quant_vulkan_wrapper", {}));
    plan.push_back(task("bench", "quant_ladder", {{"metric", "tokens/sec"}, {"threshold", 0.95}}));
    plan.push_back(task("self_test", "quant_regression", {{"cases", 50}}));
    plan.push_back(task("release", "patch", {{"notes", "Add " + lastWord + " quantization"}}));
    return plan;
}

nlohmann::json MetaPlanner::kernelPlan(const std::string& wish) {
    std::string kernel = wish.substr(wish.find_last_of(' ') + 1);
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("add_asm", kernel, {{"target", kernel}}));
    plan.push_back(task("bench", "kernel", {{"metric", "tokens/sec"}, {"threshold", 1.05}}));
    plan.push_back(task("self_test", "kernel_regression", {{"cases", 100}}));
    plan.push_back(task("release", "minor", {{"notes", "Add " + kernel + " kernel"}}));
    return plan;
}

nlohmann::json MetaPlanner::releasePlan(const std::string& wish) {
    std::string part = (wish.find("major") != std::string::npos) ? "major" :
                       (wish.find("minor") != std::string::npos) ? "minor" : "patch";
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("self_test", "all", {}));
    plan.push_back(task("bench", "all", {{"metric", "tokens/sec"}}));
    plan.push_back(task("bump_version", part, {}));
    plan.push_back(task("sign_binary", "RawrXD-SovereignIDE.exe", {}));
    plan.push_back(task("upload_cdn", "RawrXD-SovereignIDE.exe", {}));
    plan.push_back(task("create_release", "v1.x.x", {{"changelog", wish}}));
    plan.push_back(task("tweet", "\xF0\x9F\x9A\x80 New release: v1.x.x - autonomous IDE", {}));
    return plan;
}

nlohmann::json MetaPlanner::fixPlan(const std::string& wish) {
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("edit_source", "guess", {{"old", "TODO"}, {"new", "FIX"}}));
    plan.push_back(task("self_test", "regression", {{"cases", 10}}));
    plan.push_back(task("release", "patch", {{"notes", wish}}));
    return plan;
}

nlohmann::json MetaPlanner::perfPlan(const std::string& wish) {
    std::string metric = (wish.find("speed") != std::string::npos) ? "tokens/sec" : "latency";
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("profile", "inference", {{"metric", metric}}));
    plan.push_back(task("auto_tune", "quant", {}));
    plan.push_back(task("bench", "inference", {{"metric", metric}, {"threshold", 1.10}}));
    plan.push_back(task("release", "patch", {{"notes", "Performance improvement"}}));
    return plan;
}

nlohmann::json MetaPlanner::testPlan(const std::string& wish) {
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("self_test", "all", {}));
    plan.push_back(task("bench", "all", {{"metric", "coverage"}}));
    plan.push_back(task("release", "patch", {{"notes", "Test coverage improvement"}}));
    return plan;
}

nlohmann::json MetaPlanner::genericPlan(const std::string& wish) {
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back(task("edit_source", "main.cpp", {{"old", "TODO"}, {"new", wish}}));
    return plan;
}

nlohmann::json MetaPlanner::task(const std::string& type,
                                 const std::string& target,
                                 const nlohmann::json& params) {
    nlohmann::json t;
    t["type"] = type;
    t["target"] = target;
    t["params"] = params;
    return t;
}

} // namespace RawrXD
