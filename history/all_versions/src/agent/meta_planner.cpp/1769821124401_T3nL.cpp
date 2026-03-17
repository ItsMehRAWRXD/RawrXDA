#include "meta_planner.hpp"
#include <algorithm>
#include <sstream>

static std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

static std::string trim(const std::string& s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) {
        start++;
    }
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

// Mimic QString::section (simplified)
// section(sep, start, end)
// For now, simple tokenization
static std::string section(const std::string& s, char sep, int index) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, sep)) {
        tokens.push_back(token);
    }
    if (tokens.empty()) return "";
    int sz = (int)tokens.size();
    if (index < 0) index = sz + index;
    if (index >= 0 && index < sz) return tokens[index];
    return "";
}

json MetaPlanner::plan(const std::string& humanWish) {
    // 1. normalise
    std::string wish = trim(toLower(humanWish));

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

json MetaPlanner::task(const std::string& type, const std::string& target, const json& params) {
    json t = params;
    t["type"] = type;
    t["target"] = target;
    return t;
}

// ---------- keyword -> plan templates ----------

json MetaPlanner::quantPlan(const std::string& wish) {
    json plan = json::array();
    std::string lastWord = section(wish, ' ', -1);
    plan.push_back(task("add_kernel", "quant_vulkan", {{"type", lastWord}}));
    plan.push_back(task("add_cpp", "quant_vulkan_wrapper", {}));
    plan.push_back(task("bench", "quant_ladder", {{"metric", "tokens/sec"}, {"threshold", 0.95}}));
    plan.push_back(task("self_test", "quant_regression", {{"cases", 50}}));
    plan.push_back(task("release", "patch", {{"notes", std::string("Add ") + lastWord + " quantization"}}));
    return plan;
}

json MetaPlanner::kernelPlan(const std::string& wish) {
    std::string kernel = section(wish, ' ', -1); // "AVX2", "NEON", etc.
    json plan = json::array();
    plan.push_back(task("add_asm", kernel, {{"target", kernel}}));
    plan.push_back(task("bench", "kernel", {{"metric", "tokens/sec"}, {"threshold", 1.05}}));
    plan.push_back(task("self_test", "kernel_regression", {{"cases", 100}}));
    plan.push_back(task("release", "minor", {{"notes", std::string("Add ") + kernel + " kernel"}}));
    return plan;
}

json MetaPlanner::releasePlan(const std::string& wish) {
    std::string part = (wish.find("major") != std::string::npos) ? "major" :
                   (wish.find("minor") != std::string::npos) ? "minor" : "patch";
    json plan = json::array();
    plan.push_back(task("self_test", "all", {}));
    plan.push_back(task("bench", "all", {{"metric", "tokens/sec"}}));
    plan.push_back(task("bump_version", part, {}));
    plan.push_back(task("sign_binary", "RawrXD-Agent.exe", {}));
    plan.push_back(task("upload_cdn", "RawrXD-Agent.exe", {}));
    plan.push_back(task("create_release", "v1.x.x", {{"changelog", wish}}));
    plan.push_back(task("tweet", "\xF0\x9F\x9A\x80 New release: v1.x.x - autonomous IDE", {}));
    return plan;
}

json MetaPlanner::fixPlan(const std::string& wish) {
    std::string target = section(wish, ' ', 1); // guess target from sentence
    json plan = json::array();
    plan.push_back(task("edit_source", target, {{"old", "TODO"}, {"new", "FIX"}}));
    plan.push_back(task("self_test", "regression", {{"cases", 10}}));
    plan.push_back(task("release", "patch", {{"notes", wish}}));
    return plan;
}

json MetaPlanner::perfPlan(const std::string& wish) {
    std::string metric = (wish.find("speed") != std::string::npos) ? "tokens/sec" : "latency";
    json plan = json::array();
    plan.push_back(task("profile", "inference", {{"metric", metric}}));
    plan.push_back(task("auto_tune", "quant", {}));
    plan.push_back(task("bench", "inference", {{"metric", metric}, {"threshold", 1.10}}));
    plan.push_back(task("release", "patch", {{"notes", "Performance improvement"}}));
    return plan;
}

json MetaPlanner::testPlan(const std::string& wish) {
    json plan = json::array();
    plan.push_back(task("self_test", "all", {}));
    plan.push_back(task("bench", "all", {{"metric", "coverage"}}));
    plan.push_back(task("release", "patch", {{"notes", "Test coverage improvement"}}));
    return plan;
}

json MetaPlanner::genericPlan(const std::string& wish) {
    // fallback: generic dev cycle
    json plan = json::array();
    plan.push_back(task("edit_source", "main.cpp", {{"old", "TODO"}, {"new", wish}}));
    plan.push_back(task("self_test", "regression", {{"cases", 10}}));
    plan.push_back(task("release", "patch", {{"notes", wish}}));
    return plan;
}

