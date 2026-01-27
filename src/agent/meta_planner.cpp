#include "meta_planner.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

QJsonArray MetaPlanner::plan(const QString& humanWish) {
    // 1. normalise
    QString wish = humanWish.toLower().trimmed();

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

QJsonArray MetaPlanner::quantPlan(const QString& wish) {
    QJsonArray plan;
    QString lastWord = wish.section(' ', -1);
    plan.append(task("add_kernel", "quant_vulkan", QJsonObject{{"type", lastWord}}));
    plan.append(task("add_cpp", "quant_vulkan_wrapper", QJsonObject{}));
    plan.append(task("bench", "quant_ladder", QJsonObject{{"metric", "tokens/sec"}, {"threshold", 0.95}}));
    plan.append(task("self_test", "quant_regression", QJsonObject{{"cases", 50}}));
    plan.append(task("release", "patch", QJsonObject{{"notes", QString("Add ") + lastWord + " quantization"}}));
    return plan;
}

QJsonArray MetaPlanner::kernelPlan(const QString& wish) {
    QString kernel = wish.section(' ', -1); // "AVX2", "NEON", etc.
    QJsonArray plan;
    plan.append(task("add_asm", kernel, QJsonObject{{"target", kernel}}));
    plan.append(task("bench", "kernel", QJsonObject{{"metric", "tokens/sec"}, {"threshold", 1.05}}));
    plan.append(task("self_test", "kernel_regression", QJsonObject{{"cases", 100}}));
    plan.append(task("release", "minor", QJsonObject{{"notes", QString("Add ") + kernel + " kernel"}}));
    return plan;
}

QJsonArray MetaPlanner::releasePlan(const QString& wish) {
    QString part = wish.contains("major") ? "major" :
                   wish.contains("minor") ? "minor" : "patch";
    QJsonArray plan;
    plan.append(task("self_test", "all", QJsonObject{}));
    plan.append(task("bench", "all", QJsonObject{{"metric", "tokens/sec"}}));
    plan.append(task("bump_version", part, QJsonObject{}));
    plan.append(task("sign_binary", "RawrXD-QtShell.exe", QJsonObject{}));
    plan.append(task("upload_cdn", "RawrXD-QtShell.exe", QJsonObject{}));
    plan.append(task("create_release", "v1.x.x", QJsonObject{{"changelog", wish}}));
    plan.append(task("tweet", QString::fromUtf8("\xF0\x9F\x9A\x80 New release: v1.x.x - autonomous IDE"), QJsonObject{}));
    return plan;
}

QJsonArray MetaPlanner::fixPlan(const QString& wish) {
    QString target = wish.section(' ', 1, 1); // guess target from sentence
    QJsonArray plan;
    plan.append(task("edit_source", target, QJsonObject{{"old", "TODO"}, {"new", "FIX"}}));
    plan.append(task("self_test", "regression", QJsonObject{{"cases", 10}}));
    plan.append(task("release", "patch", QJsonObject{{"notes", wish}}));
    return plan;
}

QJsonArray MetaPlanner::perfPlan(const QString& wish) {
    QString metric = wish.contains("speed") ? "tokens/sec" : "latency";
    QJsonArray plan;
    plan.append(task("profile", "inference", QJsonObject{{"metric", metric}}));
    plan.append(task("auto_tune", "quant", QJsonObject{}));
    plan.append(task("bench", "inference", QJsonObject{{"metric", metric}, {"threshold", 1.10}}));
    plan.append(task("release", "patch", QJsonObject{{"notes", "Performance improvement"}}));
    return plan;
}

QJsonArray MetaPlanner::testPlan(const QString& wish) {
    QJsonArray plan;
    plan.append(task("self_test", "all", QJsonObject{}));
    plan.append(task("bench", "all", QJsonObject{{"metric", "coverage"}}));
    plan.append(task("release", "patch", QJsonObject{{"notes", "Test coverage improvement"}}));
    return plan;
}

QJsonArray MetaPlanner::genericPlan(const QString& wish) {
    // fallback: generic dev cycle
    QJsonArray plan;
    plan.append(task("edit_source", "main.cpp", QJsonObject{{"old", "TODO"}, {"new", wish}}));
    plan.append(task("self_test", "regression", QJsonObject{{"cases", 10}}));
    plan.append(task("release", "patch", QJsonObject{{"notes", wish}}));
    return plan;
}

// ---------- helper ----------
QJsonObject MetaPlanner::task(const QString& type,
                              const QString& target,
                              const QJsonObject& params) {
    QJsonObject t;
    t["type"] = type;
    t["target"] = target;
    t["params"] = params;
    return t;
}
