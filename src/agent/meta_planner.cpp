#include "meta_planner.hpp"


void* MetaPlanner::plan(const std::string& humanWish) {
    // 1. normalise
    std::string wish = humanWish.toLower().trimmed();

    // 2. keyword  template
    if (wish.contains("quant") || wish.contains("quantize"))
        return quantPlan(wish);

    if (wish.contains("kernel") || wish.contains("asm") || wish.contains("neon"))
        return kernelPlan(wish);

    if (wish.contains("ship") || wish.contains("release") || wish.contains("tag"))
        return releasePlan(wish);

    if (wish.contains("fix") || wish.contains("bug") || wish.contains("crash"))
        return fixPlan(wish);

    if (wish.contains("perf") || wish.contains("speed") || wish.contains("fast"))
        return perfPlan(wish);

    if (wish.contains("test") || wish.contains("coverage"))
        return testPlan(wish);

    // 3. fallback - generic dev cycle
    return genericPlan(wish);
}

// ---------- keyword  plan templates ----------

void* MetaPlanner::quantPlan(const std::string& wish) {
    void* plan;
    std::string lastWord = wish.section(' ', -1);
    plan.append(task("add_kernel", "quant_vulkan", void*{{"type", lastWord}}));
    plan.append(task("add_cpp", "quant_vulkan_wrapper", void*{}));
    plan.append(task("bench", "quant_ladder", void*{{"metric", "tokens/sec"}, {"threshold", 0.95}}));
    plan.append(task("self_test", "quant_regression", void*{{"cases", 50}}));
    plan.append(task("release", "patch", void*{{"notes", std::string("Add ") + lastWord + " quantization"}}));
    return plan;
}

void* MetaPlanner::kernelPlan(const std::string& wish) {
    std::string kernel = wish.section(' ', -1); // "AVX2", "NEON", etc.
    void* plan;
    plan.append(task("add_asm", kernel, void*{{"target", kernel}}));
    plan.append(task("bench", "kernel", void*{{"metric", "tokens/sec"}, {"threshold", 1.05}}));
    plan.append(task("self_test", "kernel_regression", void*{{"cases", 100}}));
    plan.append(task("release", "minor", void*{{"notes", std::string("Add ") + kernel + " kernel"}}));
    return plan;
}

void* MetaPlanner::releasePlan(const std::string& wish) {
    std::string part = wish.contains("major") ? "major" :
                   wish.contains("minor") ? "minor" : "patch";
    void* plan;
    plan.append(task("self_test", "all", void*{}));
    plan.append(task("bench", "all", void*{{"metric", "tokens/sec"}}));
    plan.append(task("bump_version", part, void*{}));
    plan.append(task("sign_binary", "RawrXD-QtShell.exe", void*{}));
    plan.append(task("upload_cdn", "RawrXD-QtShell.exe", void*{}));
    plan.append(task("create_release", "v1.x.x", void*{{"changelog", wish}}));
    plan.append(task("tweet", std::string::fromUtf8("\xF0\x9F\x9A\x80 New release: v1.x.x - autonomous IDE"), void*{}));
    return plan;
}

void* MetaPlanner::fixPlan(const std::string& wish) {
    std::string target = wish.section(' ', 1, 1); // guess target from sentence
    void* plan;
    plan.append(task("edit_source", target, void*{{"old", "TODO"}, {"new", "FIX"}}));
    plan.append(task("self_test", "regression", void*{{"cases", 10}}));
    plan.append(task("release", "patch", void*{{"notes", wish}}));
    return plan;
}

void* MetaPlanner::perfPlan(const std::string& wish) {
    std::string metric = wish.contains("speed") ? "tokens/sec" : "latency";
    void* plan;
    plan.append(task("profile", "inference", void*{{"metric", metric}}));
    plan.append(task("auto_tune", "quant", void*{}));
    plan.append(task("bench", "inference", void*{{"metric", metric}, {"threshold", 1.10}}));
    plan.append(task("release", "patch", void*{{"notes", "Performance improvement"}}));
    return plan;
}

void* MetaPlanner::testPlan(const std::string& wish) {
    void* plan;
    plan.append(task("self_test", "all", void*{}));
    plan.append(task("bench", "all", void*{{"metric", "coverage"}}));
    plan.append(task("release", "patch", void*{{"notes", "Test coverage improvement"}}));
    return plan;
}

void* MetaPlanner::genericPlan(const std::string& wish) {
    // fallback: generic dev cycle
    void* plan;
    plan.append(task("edit_source", "main.cpp", void*{{"old", "TODO"}, {"new", wish}}));
    plan.append(task("self_test", "regression", void*{{"cases", 10}}));
    plan.append(task("release", "patch", void*{{"notes", wish}}));
    return plan;
}

// ---------- helper ----------
void* MetaPlanner::task(const std::string& type,
                              const std::string& target,
                              const void*& params) {
    void* t;
    t["type"] = type;
    t["target"] = target;
    t["params"] = params;
    return t;
}

