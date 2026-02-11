#include "meta_planner.hpp"
#include "../json_types.hpp"
#include <algorithm>
#include <string>

namespace {

// Helper: case-insensitive contains
bool strContains(const std::string& haystack, const std::string& needle) {
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// Helper: extract last whitespace-delimited word
std::string lastWord(const std::string& s) {
    auto pos = s.rfind(' ');
    return (pos != std::string::npos) ? s.substr(pos + 1) : s;
}

// Helper: extract Nth word (0-based)
std::string nthWord(const std::string& s, int n) {
    size_t start = 0;
    for (int i = 0; i < n; ++i) {
        start = s.find(' ', start);
        if (start == std::string::npos) return {};
        ++start;
    }
    auto end = s.find(' ', start);
    return s.substr(start, end - start);
}

} // anon

JsonArray MetaPlanner::plan(const std::string& humanWish) {
    std::string wish = humanWish;
    // Trim
    while (!wish.empty() && (wish.front() == ' ' || wish.front() == '\t')) wish.erase(wish.begin());
    while (!wish.empty() && (wish.back() == ' ' || wish.back() == '\t')) wish.pop_back();

    if (strContains(wish, "quant") || strContains(wish, "quantize"))
        return quantPlan(wish);
    if (strContains(wish, "kernel") || strContains(wish, "asm") || strContains(wish, "neon"))
        return kernelPlan(wish);
    if (strContains(wish, "ship") || strContains(wish, "release") || strContains(wish, "tag"))
        return releasePlan(wish);
    if (strContains(wish, "fix") || strContains(wish, "bug") || strContains(wish, "crash"))
        return fixPlan(wish);
    if (strContains(wish, "perf") || strContains(wish, "speed") || strContains(wish, "fast"))
        return perfPlan(wish);
    if (strContains(wish, "test") || strContains(wish, "coverage"))
        return testPlan(wish);
    return genericPlan(wish);
}

JsonArray MetaPlanner::quantPlan(const std::string& wish) {
    JsonArray plan;
    std::string lw = lastWord(wish);
    plan.push_back(task("add_kernel", "quant_vulkan", JsonObject{{"type", lw}}));
    plan.push_back(task("add_cpp", "quant_vulkan_wrapper", JsonObject{}));
    plan.push_back(task("bench", "quant_ladder", JsonObject{{"metric", "tokens/sec"}, {"threshold", 0.95}}));
    plan.push_back(task("self_test", "quant_regression", JsonObject{{"cases", 50}}));
    plan.push_back(task("release", "patch", JsonObject{{"notes", std::string("Add ") + lw + " quantization"}}));
    return plan;
}

JsonArray MetaPlanner::kernelPlan(const std::string& wish) {
    std::string kernel = lastWord(wish);
    JsonArray plan;
    plan.push_back(task("add_asm", kernel, JsonObject{{"target", kernel}}));
    plan.push_back(task("bench", "kernel", JsonObject{{"metric", "tokens/sec"}, {"threshold", 1.05}}));
    plan.push_back(task("self_test", "kernel_regression", JsonObject{{"cases", 100}}));
    plan.push_back(task("release", "minor", JsonObject{{"notes", std::string("Add ") + kernel + " kernel"}}));
    return plan;
}

JsonArray MetaPlanner::releasePlan(const std::string& wish) {
    std::string part = strContains(wish, "major") ? "major" :
                       strContains(wish, "minor") ? "minor" : "patch";
    JsonArray plan;
    plan.push_back(task("self_test", "all", JsonObject{}));
    plan.push_back(task("bench", "all", JsonObject{{"metric", "tokens/sec"}}));
    plan.push_back(task("bump_version", part, JsonObject{}));
    plan.push_back(task("sign_binary", "RawrXD-Shell.exe", JsonObject{}));
    plan.push_back(task("upload_cdn", "RawrXD-Shell.exe", JsonObject{}));
    plan.push_back(task("create_release", "v1.x.x", JsonObject{{"changelog", wish}}));
    plan.push_back(task("tweet", "\xF0\x9F\x9A\x80 New release: v1.x.x - autonomous IDE", JsonObject{}));
    return plan;
}

JsonArray MetaPlanner::fixPlan(const std::string& wish) {
    std::string target = nthWord(wish, 1);
    JsonArray plan;
    plan.push_back(task("edit_source", target, JsonObject{{"old", "TODO"}, {"new", "FIX"}}));
    plan.push_back(task("self_test", "regression", JsonObject{{"cases", 10}}));
    plan.push_back(task("release", "patch", JsonObject{{"notes", wish}}));
    return plan;
}

JsonArray MetaPlanner::perfPlan(const std::string& wish) {
    std::string metric = strContains(wish, "speed") ? "tokens/sec" : "latency";
    JsonArray plan;
    plan.push_back(task("profile", "inference", JsonObject{{"metric", metric}}));
    plan.push_back(task("auto_tune", "quant", JsonObject{}));
    plan.push_back(task("bench", "inference", JsonObject{{"metric", metric}, {"threshold", 1.10}}));
    plan.push_back(task("release", "patch", JsonObject{{"notes", "Performance improvement"}}));
    return plan;
}

JsonArray MetaPlanner::testPlan(const std::string& wish) {
    (void)wish;
    JsonArray plan;
    plan.push_back(task("self_test", "all", JsonObject{}));
    plan.push_back(task("bench", "all", JsonObject{{"metric", "coverage"}}));
    plan.push_back(task("release", "patch", JsonObject{{"notes", "Test coverage improvement"}}));
    return plan;
}

JsonArray MetaPlanner::genericPlan(const std::string& wish) {
    JsonArray plan;
    plan.push_back(task("edit_source", "main.cpp", JsonObject{{"old", "TODO"}, {"new", wish}}));
    plan.push_back(task("self_test", "regression", JsonObject{{"cases", 10}}));
    plan.push_back(task("release", "patch", JsonObject{{"notes", wish}}));
    return plan;
}

JsonObject MetaPlanner::task(const std::string& type,
                              const std::string& target,
                              const JsonObject& params) {
    JsonObject t;
    t["type"] = type;
    t["target"] = target;
    t["params"] = params;
    return t;
}
