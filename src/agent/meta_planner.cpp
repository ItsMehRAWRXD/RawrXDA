// ============================================================================
// meta_planner.cpp — Meta-Level Plan Decomposer (Full Implementation)
// ============================================================================
// Architecture: C++20, Win32, no exceptions
// Converts high-level goals into multi-phase task graphs with dependency
// chains, subsystem routing, cost estimation, and validation.
// All return types match meta_planner.hpp: nlohmann::json.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "meta_planner.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <functional>
#include <numeric>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================================
// String helpers (anonymous namespace)
// ============================================================================
namespace {

std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

std::string toUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return r;
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool ci_contains(const std::string& haystack, const std::string& needle) {
    std::string h = toLower(haystack);
    std::string n = toLower(needle);
    return h.find(n) != std::string::npos;
}

std::string textAfter(const std::string& s, const std::string& keyword) {
    auto pos = toLower(s).find(toLower(keyword));
    if (pos == std::string::npos) return {};
    return trim(s.substr(pos + keyword.size()));
}

// Extract the last whitespace-delimited word
std::string lastWord(const std::string& s) {
    auto pos = s.rfind(' ');
    return (pos != std::string::npos) ? trim(s.substr(pos + 1)) : s;
}

// Extract Nth word (0-based)
std::string nthWord(const std::string& s, int n) {
    size_t start = 0;
    for (int i = 0; i < n; ++i) {
        start = s.find(' ', start);
        if (start == std::string::npos) return {};
        ++start;
    }
    auto end = s.find(' ', start);
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

// Split string by delimiter
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string part;
    for (char c : s) {
        if (c == delim) {
            if (!part.empty()) parts.push_back(trim(part));
            part.clear();
        } else {
            part += c;
        }
    }
    if (!part.empty()) parts.push_back(trim(part));
    return parts;
}

// Extract file paths from text (simple heuristic: words containing '/' or '.' with extension)
std::vector<std::string> extractPaths(const std::string& s) {
    std::vector<std::string> paths;
    std::regex pathRe(R"((?:[\w./\\-]+\.(?:cpp|hpp|h|c|asm|comp|py|rs|js|ts|cmake|txt|json|yaml|yml|toml|xml|md)))",
                      std::regex::icase);
    auto begin = std::sregex_iterator(s.begin(), s.end(), pathRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        paths.push_back((*it)[0].str());
    }
    return paths;
}

// Extract quantization type from text
std::string extractQuantType(const std::string& s) {
    std::regex re(R"((Q\d+_[KM](?:_[SML])?|F16|F32|IQ\d+_[A-Z]+))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(s, m, re)) return toUpper(m[1].str());
    return "Q8_K";  // default
}

// ISO 8601 timestamp
std::string isoTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return std::string(buf);
}

// Subsystem name mapping (matches AgenticTaskGraph::TaskNode::Subsystem enum)
const std::unordered_map<std::string, uint8_t> kSubsystemMap = {
    {"completion_engine", 0},
    {"refactor_engine",   1},
    {"agentic_loop",      2},
    {"swarm_engine",      3},
    {"inference_core",    4},
    {"context_analyzer",  5},
    {"hotpatch_engine",   6},
    {"custom",            7}
};

// Task type → default subsystem
const std::unordered_map<std::string, std::string> kTypeToSubsystem = {
    {"add_kernel",     "hotpatch_engine"},
    {"add_asm",        "hotpatch_engine"},
    {"add_comp",       "hotpatch_engine"},
    {"add_cpp",        "refactor_engine"},
    {"edit_source",    "refactor_engine"},
    {"refactor",       "refactor_engine"},
    {"rename",         "refactor_engine"},
    {"extract",        "refactor_engine"},
    {"inline_fn",      "refactor_engine"},
    {"bench",          "inference_core"},
    {"profile",        "inference_core"},
    {"auto_tune",      "inference_core"},
    {"self_test",      "swarm_engine"},
    {"fuzz_test",      "swarm_engine"},
    {"regression_test","swarm_engine"},
    {"integration_test","swarm_engine"},
    {"release",        "agentic_loop"},
    {"bump_version",   "agentic_loop"},
    {"sign_binary",    "agentic_loop"},
    {"upload_cdn",     "agentic_loop"},
    {"create_release", "agentic_loop"},
    {"index_symbols",  "context_analyzer"},
    {"analyze_deps",   "context_analyzer"},
    {"build_embeddings","context_analyzer"},
    {"scan_security",  "context_analyzer"},
    {"hot_reload",     "hotpatch_engine"},
    {"hotpatch",       "hotpatch_engine"},
    {"compile",        "swarm_engine"},
    {"link",           "swarm_engine"},
    {"git_snapshot",   "agentic_loop"},
    {"git_revert",     "agentic_loop"},
    {"migrate",        "agentic_loop"},
    {"deploy",         "agentic_loop"},
    {"notify",         "custom"},
    {"meta_learn",     "inference_core"}
};

// Task type → estimated seconds (defaults)
const std::unordered_map<std::string, uint32_t> kTypeToEstimate = {
    {"add_kernel",     120},
    {"add_asm",        180},
    {"add_comp",       90},
    {"add_cpp",        60},
    {"edit_source",    30},
    {"refactor",       120},
    {"rename",         15},
    {"extract",        45},
    {"inline_fn",      20},
    {"bench",          300},
    {"profile",        240},
    {"auto_tune",      600},
    {"self_test",      180},
    {"fuzz_test",      900},
    {"regression_test",300},
    {"integration_test",600},
    {"release",        30},
    {"bump_version",   5},
    {"sign_binary",    15},
    {"upload_cdn",     60},
    {"create_release", 10},
    {"index_symbols",  120},
    {"analyze_deps",   90},
    {"build_embeddings",300},
    {"scan_security",  180},
    {"hot_reload",     5},
    {"hotpatch",       10},
    {"compile",        120},
    {"link",           60},
    {"git_snapshot",   3},
    {"git_revert",     5},
    {"migrate",        360},
    {"deploy",         180},
    {"notify",         2},
    {"meta_learn",     30}
};

} // anonymous namespace


// ============================================================================
// extractIntentSignals — NLP-lite intent extraction
// ============================================================================
MetaPlanner::IntentSignals MetaPlanner::extractIntentSignals(const std::string& wish) {
    IntentSignals sig;
    std::string lower = toLower(wish);

    // Primary domain detection (ordered by specificity)
    if (ci_contains(lower, "security") || ci_contains(lower, "vuln") ||
        ci_contains(lower, "cve") || ci_contains(lower, "audit")) {
        sig.primaryDomain = "security";
    } else if (ci_contains(lower, "migrat") || ci_contains(lower, "upgrade") ||
               ci_contains(lower, "port to")) {
        sig.primaryDomain = "migration";
    } else if (ci_contains(lower, "refactor") || ci_contains(lower, "restructur") ||
               ci_contains(lower, "extract") || ci_contains(lower, "modular")) {
        sig.primaryDomain = "refactor";
    } else if (ci_contains(lower, "quant") || ci_contains(lower, "quantize")) {
        sig.primaryDomain = "quant";
    } else if (ci_contains(lower, "kernel") || ci_contains(lower, "asm") ||
               ci_contains(lower, "neon") || ci_contains(lower, "avx") ||
               ci_contains(lower, "simd")) {
        sig.primaryDomain = "kernel";
    } else if (ci_contains(lower, "ship") || ci_contains(lower, "release") ||
               ci_contains(lower, "tag") || ci_contains(lower, "publish")) {
        sig.primaryDomain = "release";
    } else if (ci_contains(lower, "fix") || ci_contains(lower, "bug") ||
               ci_contains(lower, "crash") || ci_contains(lower, "error") ||
               ci_contains(lower, "broken")) {
        sig.primaryDomain = "fix";
    } else if (ci_contains(lower, "perf") || ci_contains(lower, "speed") ||
               ci_contains(lower, "fast") || ci_contains(lower, "optim") ||
               ci_contains(lower, "latency") || ci_contains(lower, "throughput")) {
        sig.primaryDomain = "perf";
    } else if (ci_contains(lower, "test") || ci_contains(lower, "coverage") ||
               ci_contains(lower, "fuzz") || ci_contains(lower, "regression")) {
        sig.primaryDomain = "test";
    } else {
        sig.primaryDomain = "generic";
    }

    // Target entity extraction
    auto paths = extractPaths(wish);
    if (!paths.empty()) {
        sig.targetEntity = paths[0];
        sig.affectedPaths = paths;
    } else {
        // Try to extract a target from the text
        // Pattern: "fix the <target>" or "optimize <target>"
        std::regex targetRe(R"((?:fix|optimize|refactor|test|bench|migrate|upgrade|patch|update|add|remove|delete)\s+(?:the\s+)?(\w[\w./\\-]*))",
                            std::regex::icase);
        std::smatch m;
        if (std::regex_search(wish, m, targetRe)) {
            sig.targetEntity = m[1].str();
        } else {
            sig.targetEntity = "project";
        }
    }

    // Quant type
    sig.quantType = extractQuantType(wish);

    // Severity detection
    if (ci_contains(lower, "critical") || ci_contains(lower, "urgent") ||
        ci_contains(lower, "asap") || ci_contains(lower, "emergency") ||
        ci_contains(lower, "crash") || ci_contains(lower, "data loss")) {
        sig.severity = "critical";
    } else if (ci_contains(lower, "low priority") || ci_contains(lower, "nice to have") ||
               ci_contains(lower, "when possible") || ci_contains(lower, "eventually")) {
        sig.severity = "low";
    } else {
        sig.severity = "normal";
    }

    // Boolean flags
    sig.requiresTests     = !ci_contains(lower, "skip test") && !ci_contains(lower, "no test");
    sig.requiresBenchmark = ci_contains(lower, "bench") || ci_contains(lower, "perf") ||
                            sig.primaryDomain == "quant" || sig.primaryDomain == "kernel" ||
                            sig.primaryDomain == "perf";
    sig.requiresRollback  = ci_contains(lower, "safe") || ci_contains(lower, "rollback") ||
                            sig.severity == "critical" || sig.primaryDomain == "migration";
    sig.isMultiFile       = ci_contains(lower, "all") || ci_contains(lower, "every") ||
                            ci_contains(lower, "across") || ci_contains(lower, "bulk") ||
                            paths.size() > 1;
    sig.isBreakingChange  = ci_contains(lower, "breaking") || ci_contains(lower, "major") ||
                            sig.primaryDomain == "migration" || sig.primaryDomain == "refactor";

    return sig;
}


// ============================================================================
// task — Single task node builder with full metadata
// ============================================================================
json MetaPlanner::task(const std::string& type,
                       const std::string& target,
                       const json& params,
                       uint32_t priority,
                       uint32_t estimatedSeconds,
                       const std::string& subsystem) {
    uint32_t id = allocTaskId();

    // Look up default estimate if not overridden
    uint32_t est = estimatedSeconds;
    if (est == 60) {
        auto it = kTypeToEstimate.find(type);
        if (it != kTypeToEstimate.end()) est = it->second;
    }

    // Look up default subsystem if not overridden
    std::string sub = subsystem;
    if (sub == "agentic_loop") {
        auto it = kTypeToSubsystem.find(type);
        if (it != kTypeToSubsystem.end()) sub = it->second;
    }

    json t = {
        {"id",                id},
        {"type",              type},
        {"target",            target},
        {"params",            params},
        {"priority",          priority},
        {"estimated_seconds", est},
        {"subsystem",         sub},
        {"depends_on",        json::array()},
        {"rollback_strategy", "snapshot"},
        {"max_retries",       3},
        {"timeout_seconds",   est * 3},
        {"state",             "pending"},
        {"created_at",        isoTimestamp()}
    };
    return t;
}


// ============================================================================
// phase — Phase wrapper with gate condition
// ============================================================================
json MetaPlanner::phase(const std::string& name,
                        uint32_t order,
                        const json& tasks,
                        const std::string& gateCondition) {
    return {
        {"phase_name",      name},
        {"phase_order",     order},
        {"gate_condition",  gateCondition},
        {"task_count",      tasks.size()},
        {"tasks",           tasks}
    };
}


// ============================================================================
// depEdge — Dependency edge descriptor
// ============================================================================
json MetaPlanner::depEdge(uint32_t from, uint32_t to,
                          const std::string& type) {
    return {
        {"from_task_id", from},
        {"to_task_id",   to},
        {"dep_type",     type}
    };
}


// ============================================================================
// plan — Top-level intent router
// ============================================================================
json MetaPlanner::plan(const std::string& humanWish) {
    resetTaskIds();
    std::string wish = trim(humanWish);
    if (wish.empty()) {
        return {
            {"status",  "error"},
            {"message", "Empty wish provided"},
            {"phases",  json::array()}
        };
    }

    IntentSignals sig = extractIntentSignals(wish);

    json rawPlan;
    if      (sig.primaryDomain == "quant")     rawPlan = quantPlan(wish);
    else if (sig.primaryDomain == "kernel")    rawPlan = kernelPlan(wish);
    else if (sig.primaryDomain == "release")   rawPlan = releasePlan(wish);
    else if (sig.primaryDomain == "fix")       rawPlan = fixPlan(wish);
    else if (sig.primaryDomain == "perf")      rawPlan = perfPlan(wish);
    else if (sig.primaryDomain == "test")      rawPlan = testPlan(wish);
    else if (sig.primaryDomain == "refactor")  rawPlan = refactorPlan(wish);
    else if (sig.primaryDomain == "migration") rawPlan = migrationPlan(wish);
    else if (sig.primaryDomain == "security")  rawPlan = securityPlan(wish);
    else                                       rawPlan = genericPlan(wish);

    // Wrap into final plan envelope
    json planEnvelope = {
        {"status",            "ok"},
        {"wish",              wish},
        {"domain",            sig.primaryDomain},
        {"target",            sig.targetEntity},
        {"severity",          sig.severity},
        {"is_breaking",       sig.isBreakingChange},
        {"is_multi_file",     sig.isMultiFile},
        {"requires_tests",    sig.requiresTests},
        {"requires_benchmark",sig.requiresBenchmark},
        {"requires_rollback", sig.requiresRollback},
        {"created_at",        isoTimestamp()},
        {"phases",            rawPlan["phases"]},
        {"dependencies",      rawPlan["dependencies"]},
        {"cost_estimate",     rawPlan["cost_estimate"]}
    };

    return planEnvelope;
}


// ============================================================================
// quantPlan — Multi-phase quantization kernel pipeline
// ============================================================================
json MetaPlanner::quantPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string qt = sig.quantType;
    std::string qtLower = toLower(qt);
    json deps = json::array();

    // Phase 1: Analysis & preparation
    json p1tasks = json::array();
    auto t1 = task("index_symbols", "src/core", {{"scope", "quant_related"}}, 3, 120, "context_analyzer");
    auto t2 = task("analyze_deps", qtLower + "_module", {{"depth", 3}}, 3, 90, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_quant_" + qtLower,
        {{"message", "Snapshot before " + qt + " quantization kernel"}}, 2, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);

    // Phase 2: Kernel implementation
    json p2tasks = json::array();
    auto t4 = task("add_comp", "quant_" + qtLower + ".comp",
        {{"quant_type", qt}, {"template", "quant_vulkan.comp"}, {"block_size", 256}}, 8, 120, "hotpatch_engine");
    auto t5 = task("add_asm", "quant_" + qtLower + "_avx512",
        {{"quant_type", qt}, {"instruction_set", "AVX-512"}, {"register_width", 512}}, 8, 180, "hotpatch_engine");
    auto t6 = task("add_cpp", "quant_" + qtLower + "_wrapper",
        {{"quant_type", qt}, {"includes", json::array({"quant_" + qtLower + ".comp", "quant_" + qtLower + "_avx512.asm"})}},
        7, 60, "refactor_engine");
    p2tasks.push_back(t4);
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);

    // t4 and t5 depend on t1 (symbols indexed), t6 depends on t4 and t5
    deps.push_back(depEdge(t1["id"], t4["id"]));
    deps.push_back(depEdge(t1["id"], t5["id"]));
    deps.push_back(depEdge(t4["id"], t6["id"]));
    deps.push_back(depEdge(t5["id"], t6["id"]));
    // t3 (snapshot) must complete before any writes
    deps.push_back(depEdge(t3["id"], t4["id"]));
    deps.push_back(depEdge(t3["id"], t5["id"]));

    // Phase 3: Validation
    json p3tasks = json::array();
    auto t7 = task("self_test", "quant_" + qtLower + "_regression",
        {{"cases", 50}, {"quant_type", qt}, {"tolerance", 0.001}}, 6, 180, "swarm_engine");
    auto t8 = task("bench", "quant_" + qtLower + "_ladder",
        {{"metric", "tokens/sec"}, {"threshold", 0.95}, {"models", json::array({"7B", "13B", "30B"})}},
        6, 300, "inference_core");
    auto t9 = task("fuzz_test", "quant_" + qtLower + "_boundary",
        {{"cases", 1000}, {"targets", json::array({"overflow", "underflow", "denormal", "nan"})}},
        5, 600, "swarm_engine");
    p3tasks.push_back(t7);
    p3tasks.push_back(t8);
    p3tasks.push_back(t9);

    // Validation depends on implementation
    deps.push_back(depEdge(t6["id"], t7["id"]));
    deps.push_back(depEdge(t6["id"], t8["id"]));
    deps.push_back(depEdge(t6["id"], t9["id"]));

    // Phase 4: Integration & telemetry
    json p4tasks = json::array();
    auto t10 = task("hot_reload", "quant_" + qtLower, {{"module", "quant"}}, 7, 5, "hotpatch_engine");
    auto t11 = task("meta_learn", "quant_" + qtLower + "_telemetry",
        {{"quant", qt}, {"kernel", "quant_" + qtLower + "_wrapper"},
         {"gpu", "autodetect"}, {"tps", 0.0}, {"ppl", 0.0}, {"baseline", true}},
        4, 30, "inference_core");
    p4tasks.push_back(t10);
    p4tasks.push_back(t11);

    // Integration depends on all tests passing
    deps.push_back(depEdge(t7["id"], t10["id"]));
    deps.push_back(depEdge(t8["id"], t10["id"]));
    deps.push_back(depEdge(t9["id"], t10["id"]));
    deps.push_back(depEdge(t10["id"], t11["id"]));

    // Cost estimate
    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks, p4tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("analysis_preparation", 1, p1tasks),
            phase("kernel_implementation", 2, p2tasks),
            phase("validation", 3, p3tasks, "all_pass"),
            phase("integration_telemetry", 4, p4tasks)
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds / 2},
            {"phase_count", 4},
            {"task_count", 11}
        }}
    };
}


// ============================================================================
// kernelPlan — MASM/ASM kernel development pipeline
// ============================================================================
json MetaPlanner::kernelPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string kernel = sig.targetEntity.empty() ? lastWord(wish) : sig.targetEntity;
    std::string kernelLower = toLower(kernel);
    json deps = json::array();

    // Phase 1: Research & snapshot
    json p1tasks = json::array();
    auto t1 = task("index_symbols", "src/asm",
        {{"scope", "asm_kernels"}, {"filter", kernelLower}}, 3, 120, "context_analyzer");
    auto t2 = task("analyze_deps", kernelLower + "_deps",
        {{"target", kernel}, {"check_isa", true}}, 3, 90, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_kernel_" + kernelLower,
        {{"message", "Snapshot before " + kernel + " kernel development"}}, 2, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);

    // Phase 2: Implementation
    json p2tasks = json::array();
    auto t4 = task("add_asm", kernelLower + ".asm",
        {{"target", kernel}, {"isa", "x86-64"}, {"calling_convention", "ms_abi"},
         {"registers", json::array({"rax","rbx","rcx","rdx","r8","r9","xmm0","xmm1"})}},
        8, 240, "hotpatch_engine");
    auto t5 = task("add_cpp", kernelLower + "_bridge.cpp",
        {{"target", kernel}, {"bridge_type", "extern_c"},
         {"includes", json::array({kernelLower + ".asm"})}},
        7, 60, "refactor_engine");
    auto t6 = task("add_cpp", kernelLower + "_fallback.cpp",
        {{"target", kernel}, {"fallback", true}, {"guard_macro", "RAWRXD_LINK_" + toUpper(kernel) + "_ASM"}},
        6, 90, "refactor_engine");
    p2tasks.push_back(t4);
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);

    deps.push_back(depEdge(t1["id"], t4["id"]));
    deps.push_back(depEdge(t3["id"], t4["id"]));
    deps.push_back(depEdge(t4["id"], t5["id"]));
    // t6 (fallback) can be parallel with t4/t5
    deps.push_back(depEdge(t2["id"], t6["id"]));
    deps.push_back(depEdge(t3["id"], t6["id"]));

    // Phase 3: Validation
    json p3tasks = json::array();
    auto t7 = task("compile", kernelLower,
        {{"config", "Release"}, {"target", kernelLower}}, 7, 120, "swarm_engine");
    auto t8 = task("self_test", kernelLower + "_regression",
        {{"cases", 100}, {"include_edge_cases", true}}, 6, 180, "swarm_engine");
    auto t9 = task("bench", kernelLower + "_performance",
        {{"metric", "tokens/sec"}, {"threshold", 1.05}, {"iterations", 100}}, 6, 300, "inference_core");
    p3tasks.push_back(t7);
    p3tasks.push_back(t8);
    p3tasks.push_back(t9);

    deps.push_back(depEdge(t5["id"], t7["id"]));
    deps.push_back(depEdge(t6["id"], t7["id"]));
    deps.push_back(depEdge(t7["id"], t8["id"]));
    deps.push_back(depEdge(t7["id"], t9["id"]));

    // Phase 4: Hot-reload integration
    json p4tasks = json::array();
    auto t10 = task("hot_reload", kernelLower,
        {{"module", kernel}, {"verify_after_reload", true}}, 7, 5, "hotpatch_engine");
    auto t11 = task("meta_learn", kernelLower + "_baseline",
        {{"kernel", kernel}, {"gpu", "autodetect"}, {"baseline", true}}, 4, 30, "inference_core");
    p4tasks.push_back(t10);
    p4tasks.push_back(t11);

    deps.push_back(depEdge(t8["id"], t10["id"]));
    deps.push_back(depEdge(t9["id"], t10["id"]));
    deps.push_back(depEdge(t10["id"], t11["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks, p4tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("research_snapshot", 1, p1tasks),
            phase("implementation", 2, p2tasks),
            phase("validation", 3, p3tasks, "all_pass"),
            phase("hot_reload_integration", 4, p4tasks)
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 3 / 5},
            {"phase_count", 4},
            {"task_count", 11}
        }}
    };
}


// ============================================================================
// releasePlan — Release/ship pipeline with signing
// ============================================================================
json MetaPlanner::releasePlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string part = ci_contains(wish, "major") ? "major" :
                       ci_contains(wish, "minor") ? "minor" : "patch";
    json deps = json::array();

    // Phase 1: Pre-release validation
    json p1tasks = json::array();
    auto t1 = task("self_test", "all",
        {{"scope", "full_suite"}, {"fail_fast", false}}, 9, 300, "swarm_engine");
    auto t2 = task("bench", "release_benchmarks",
        {{"metric", "tokens/sec"}, {"compare_baseline", true}, {"models", json::array({"7B", "13B"})}},
        8, 300, "inference_core");
    auto t3 = task("scan_security", "release_audit",
        {{"scan_deps", true}, {"check_cve", true}}, 7, 180, "context_analyzer");
    auto t4 = task("git_snapshot", "pre_release_" + part,
        {{"message", "Pre-release snapshot (" + part + ")"}}, 2, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);
    p1tasks.push_back(t4);

    // Phase 2: Build & sign
    json p2tasks = json::array();
    auto t5 = task("bump_version", part,
        {{"part", part}}, 8, 5, "agentic_loop");
    auto t6 = task("compile", "RawrXD-Shell",
        {{"config", "Release"}, {"static_crt", true}, {"lto", true}}, 9, 180, "swarm_engine");
    auto t7 = task("sign_binary", "RawrXD-Shell.exe",
        {{"certificate", "code_signing"}, {"timestamp", true}}, 8, 15, "agentic_loop");
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);
    p2tasks.push_back(t7);

    deps.push_back(depEdge(t1["id"], t5["id"]));
    deps.push_back(depEdge(t2["id"], t5["id"]));
    deps.push_back(depEdge(t3["id"], t5["id"]));
    deps.push_back(depEdge(t4["id"], t5["id"]));
    deps.push_back(depEdge(t5["id"], t6["id"]));
    deps.push_back(depEdge(t6["id"], t7["id"]));

    // Phase 3: Distribution
    json p3tasks = json::array();
    auto t8 = task("upload_cdn", "RawrXD-Shell.exe",
        {{"targets", json::array({"github_releases", "cdn_primary"})}, {"checksum", "sha256"}},
        7, 60, "agentic_loop");
    auto t9 = task("create_release", "v_" + part,
        {{"changelog", wish}, {"prerelease", false}, {"draft", false}}, 7, 10, "agentic_loop");
    auto t10 = task("notify", "release_announcement",
        {{"channels", json::array({"discord", "twitter"})},
         {"message", "New " + part + " release shipped"}},
        3, 5, "custom");
    p3tasks.push_back(t8);
    p3tasks.push_back(t9);
    p3tasks.push_back(t10);

    deps.push_back(depEdge(t7["id"], t8["id"]));
    deps.push_back(depEdge(t8["id"], t9["id"]));
    deps.push_back(depEdge(t9["id"], t10["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("pre_release_validation", 1, p1tasks, "all_pass"),
            phase("build_sign", 2, p2tasks),
            phase("distribution", 3, p3tasks)
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", 800},
            {"phase_count", 3},
            {"task_count", 10}
        }}
    };
}


// ============================================================================
// fixPlan — Bug fix pipeline with diagnosis
// ============================================================================
json MetaPlanner::fixPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Diagnosis
    json p1tasks = json::array();
    auto t1 = task("index_symbols", target,
        {{"scope", "crash_related"}, {"include_callgraph", true}}, 8, 60, "context_analyzer");
    auto t2 = task("analyze_deps", target + "_deps",
        {{"target", target}, {"trace_crash_path", true}}, 7, 90, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_fix_" + target,
        {{"message", "Pre-fix snapshot: " + wish}}, 9, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);

    // Phase 2: Fix implementation
    json p2tasks = json::array();
    if (sig.isMultiFile) {
        // Multi-file fix: scan and fix across affected files
        auto t4 = task("edit_source", target,
            {{"strategy", "scan_and_fix"}, {"scope", "all_affected"},
             {"pattern", "error_pattern"}, {"replacement", "fix_pattern"},
             {"max_files", 50}},
            9, 120, "refactor_engine");
        p2tasks.push_back(t4);
        deps.push_back(depEdge(t1["id"], t4["id"]));
        deps.push_back(depEdge(t2["id"], t4["id"]));
        deps.push_back(depEdge(t3["id"], t4["id"]));
    } else {
        auto t4 = task("edit_source", target,
            {{"strategy", "targeted_fix"}, {"context_lines", 10},
             {"verify_syntax", true}},
            9, 30, "refactor_engine");
        p2tasks.push_back(t4);
        deps.push_back(depEdge(t1["id"], t4["id"]));
        deps.push_back(depEdge(t2["id"], t4["id"]));
        deps.push_back(depEdge(t3["id"], t4["id"]));
    }

    // Phase 3: Validation
    json p3tasks = json::array();
    auto t5 = task("compile", target,
        {{"config", "Debug"}, {"warnings_as_errors", true}}, 8, 120, "swarm_engine");
    auto t6 = task("self_test", target + "_regression",
        {{"cases", 30}, {"include_original_repro", true}}, 8, 180, "swarm_engine");
    p3tasks.push_back(t5);
    p3tasks.push_back(t6);

    // All p2 tasks must complete before validation
    for (auto& pt : p2tasks) {
        deps.push_back(depEdge(pt["id"].get<uint32_t>(), t5["id"].get<uint32_t>()));
    }
    deps.push_back(depEdge(t5["id"], t6["id"]));

    // Phase 4: Finalize (only if critical severity)
    json p4tasks = json::array();
    if (sig.severity == "critical") {
        auto t7 = task("hot_reload", target,
            {{"module", target}, {"verify_after_reload", true}}, 9, 5, "hotpatch_engine");
        auto t8 = task("notify", "fix_deployed",
            {{"severity", "critical"}, {"message", "Critical fix deployed: " + wish}},
            3, 2, "custom");
        p4tasks.push_back(t7);
        p4tasks.push_back(t8);
        deps.push_back(depEdge(t6["id"], t7["id"]));
        deps.push_back(depEdge(t7["id"], t8["id"]));
    }

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks, p4tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    json phases = json::array({
        phase("diagnosis", 1, p1tasks),
        phase("fix_implementation", 2, p2tasks),
        phase("validation", 3, p3tasks, "all_pass")
    });
    if (!p4tasks.empty()) {
        phases.push_back(phase("finalize_deploy", 4, p4tasks));
    }

    return {
        {"phases", phases},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 2 / 3},
            {"phase_count", phases.size()},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size() + p4tasks.size()}
        }}
    };
}


// ============================================================================
// perfPlan — Performance optimization pipeline
// ============================================================================
json MetaPlanner::perfPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string metric = ci_contains(wish, "throughput") || ci_contains(wish, "tokens") ? "tokens/sec" :
                         ci_contains(wish, "latency") ? "latency_ms" : "tokens/sec";
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Profile & baseline
    json p1tasks = json::array();
    auto t1 = task("profile", target + "_hotspots",
        {{"metric", metric}, {"profiler", "rdtsc_histogram"},
         {"sample_count", 10000}, {"warm_up", 100}},
        7, 240, "inference_core");
    auto t2 = task("bench", target + "_baseline",
        {{"metric", metric}, {"iterations", 100}, {"record_baseline", true},
         {"models", json::array({"7B", "13B"})}},
        7, 300, "inference_core");
    auto t3 = task("analyze_deps", target + "_bottlenecks",
        {{"target", target}, {"analysis_type", "performance"}, {"include_cache_analysis", true}},
        6, 90, "context_analyzer");
    auto t4 = task("git_snapshot", "pre_perf_" + target,
        {{"message", "Performance baseline snapshot"}}, 2, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);
    p1tasks.push_back(t4);

    // Phase 2: Optimization
    json p2tasks = json::array();
    json searchSpace = json::object();
    searchSpace["batch_size"] = json::array({1, 2, 4, 8, 16});
    searchSpace["thread_count"] = json::array({1, 2, 4, 8});
    searchSpace["prefetch_distance"] = json::array({64, 128, 256, 512});
    auto t5 = task("auto_tune", target + "_quant_params",
        {{"metric", metric}, {"search_space", searchSpace}},
        8, 600, "inference_core");
    auto t6 = task("hotpatch", target + "_critical_path",
        {{"strategy", "inline_hot_loops"}, {"threshold_cycles", 1000}},
        8, 120, "hotpatch_engine");
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);

    deps.push_back(depEdge(t1["id"], t5["id"]));
    deps.push_back(depEdge(t2["id"], t5["id"]));
    deps.push_back(depEdge(t3["id"], t6["id"]));
    deps.push_back(depEdge(t4["id"], t5["id"]));
    deps.push_back(depEdge(t4["id"], t6["id"]));

    // Phase 3: Verify improvement
    json p3tasks = json::array();
    auto t7 = task("bench", target + "_post_optimization",
        {{"metric", metric}, {"threshold", 1.10}, {"compare_baseline", true},
         {"iterations", 200}},
        9, 300, "inference_core");
    auto t8 = task("self_test", target + "_regression",
        {{"cases", 50}, {"verify_correctness", true}, {"tolerance", 0.0001}},
        8, 180, "swarm_engine");
    p3tasks.push_back(t7);
    p3tasks.push_back(t8);

    deps.push_back(depEdge(t5["id"], t7["id"]));
    deps.push_back(depEdge(t6["id"], t7["id"]));
    deps.push_back(depEdge(t5["id"], t8["id"]));
    deps.push_back(depEdge(t6["id"], t8["id"]));

    // Phase 4: Record results
    json p4tasks = json::array();
    auto t9 = task("meta_learn", target + "_perf_record",
        {{"metric", metric}, {"store_improvement_delta", true}},
        4, 30, "inference_core");
    auto t10 = task("hot_reload", target, {{"verify_after_reload", true}}, 6, 5, "hotpatch_engine");
    p4tasks.push_back(t9);
    p4tasks.push_back(t10);

    deps.push_back(depEdge(t7["id"], t9["id"]));
    deps.push_back(depEdge(t8["id"], t10["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks, p4tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("profile_baseline", 1, p1tasks),
            phase("optimization", 2, p2tasks),
            phase("verify_improvement", 3, p3tasks, "all_pass"),
            phase("record_results", 4, p4tasks)
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 3 / 5},
            {"phase_count", 4},
            {"task_count", 10}
        }}
    };
}


// ============================================================================
// testPlan — Test coverage expansion pipeline
// ============================================================================
json MetaPlanner::testPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    bool doFuzz = ci_contains(wish, "fuzz");
    json deps = json::array();

    // Phase 1: Coverage analysis
    json p1tasks = json::array();
    auto t1 = task("index_symbols", "src",
        {{"scope", "testable_functions"}, {"exclude", json::array({"vendor", "third_party"})}},
        5, 120, "context_analyzer");
    auto t2 = task("analyze_deps", target + "_test_gaps",
        {{"analysis_type", "coverage_gaps"}, {"min_coverage", 0.80}},
        5, 90, "context_analyzer");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);

    // Phase 2: Test generation
    json p2tasks = json::array();
    auto t3 = task("self_test", target + "_unit",
        {{"cases", 200}, {"generate_missing", true}, {"style", "gtest"}},
        7, 300, "swarm_engine");
    auto t4 = task("integration_test", target + "_integration",
        {{"test_interactions", true}, {"mock_externals", true}},
        6, 600, "swarm_engine");
    p2tasks.push_back(t3);
    p2tasks.push_back(t4);

    deps.push_back(depEdge(t1["id"], t3["id"]));
    deps.push_back(depEdge(t2["id"], t3["id"]));
    deps.push_back(depEdge(t1["id"], t4["id"]));
    deps.push_back(depEdge(t2["id"], t4["id"]));

    // Phase 3: Fuzz testing (conditional)
    json p3tasks = json::array();
    if (doFuzz) {
        auto t5 = task("fuzz_test", target + "_fuzz",
            {{"cases", 10000}, {"corpus_dir", ".rawrxd/fuzz_corpus/" + target},
             {"timeout_per_case_ms", 5000},
             {"targets", json::array({"buffer_overflow", "null_deref", "integer_overflow", "format_string"})}},
            6, 900, "swarm_engine");
        p3tasks.push_back(t5);
        deps.push_back(depEdge(t3["id"], t5["id"]));
    }

    // Phase 4: Report
    json p4tasks = json::array();
    auto t6 = task("bench", target + "_coverage_report",
        {{"metric", "coverage"}, {"format", "lcov"}, {"threshold", 0.85}},
        5, 60, "inference_core");
    p4tasks.push_back(t6);

    // Report depends on all prior tests
    deps.push_back(depEdge(t3["id"], t6["id"]));
    deps.push_back(depEdge(t4["id"], t6["id"]));
    if (!p3tasks.empty()) {
        deps.push_back(depEdge(p3tasks[(size_t)0]["id"].get<uint32_t>(), t6["id"].get<uint32_t>()));
    }

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks, p4tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    json phases = json::array({
        phase("coverage_analysis", 1, p1tasks),
        phase("test_generation", 2, p2tasks)
    });
    if (!p3tasks.empty()) phases.push_back(phase("fuzz_testing", 3, p3tasks));
    phases.push_back(phase("report", doFuzz ? 4 : 3, p4tasks));

    return {
        {"phases", phases},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 2 / 3},
            {"phase_count", phases.size()},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size() + p4tasks.size()}
        }}
    };
}


// ============================================================================
// refactorPlan — Safe refactoring pipeline
// ============================================================================
json MetaPlanner::refactorPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Impact analysis
    json p1tasks = json::array();
    auto t1 = task("index_symbols", target,
        {{"scope", "all_references"}, {"include_callgraph", true}, {"include_type_hierarchy", true}},
        7, 120, "context_analyzer");
    auto t2 = task("analyze_deps", target + "_refactor_impact",
        {{"target", target}, {"analysis_type", "impact"},
         {"depth", 5}, {"include_transitive", true}},
        7, 120, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_refactor_" + target,
        {{"message", "Snapshot before refactoring " + target}, {"tag", true}}, 9, 3, "agentic_loop");
    auto t4 = task("self_test", target + "_pre_refactor_baseline",
        {{"cases", 100}, {"record_baseline", true}}, 6, 180, "swarm_engine");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);
    p1tasks.push_back(t4);

    // Phase 2: Refactor execution
    json p2tasks = json::array();
    if (ci_contains(wish, "extract")) {
        auto t5 = task("extract", target,
            {{"extract_type", "function"}, {"preserve_semantics", true}}, 8, 60, "refactor_engine");
        p2tasks.push_back(t5);
    } else if (ci_contains(wish, "rename")) {
        std::string newName = textAfter(wish, "to");
        if (newName.empty()) newName = textAfter(wish, "rename");
        auto t5 = task("rename", target,
            {{"new_name", newName}, {"update_all_references", true}, {"update_comments", true}},
            8, 30, "refactor_engine");
        p2tasks.push_back(t5);
    } else if (ci_contains(wish, "inline")) {
        auto t5 = task("inline_fn", target,
            {{"inline_all_call_sites", true}, {"verify_semantics", true}}, 8, 45, "refactor_engine");
        p2tasks.push_back(t5);
    } else {
        // Generic structural refactor
        auto t5 = task("refactor", target,
            {{"strategy", "restructure"}, {"preserve_api", !sig.isBreakingChange},
             {"update_includes", true}, {"update_cmake", true}},
            8, 180, "refactor_engine");
        p2tasks.push_back(t5);
    }

    for (auto& pt : p2tasks) {
        deps.push_back(depEdge(t1["id"].get<uint32_t>(), pt["id"].get<uint32_t>()));
        deps.push_back(depEdge(t2["id"].get<uint32_t>(), pt["id"].get<uint32_t>()));
        deps.push_back(depEdge(t3["id"].get<uint32_t>(), pt["id"].get<uint32_t>()));
    }

    // Phase 3: Validation
    json p3tasks = json::array();
    auto t6 = task("compile", "all",
        {{"config", "Release"}, {"warnings_as_errors", true}}, 9, 180, "swarm_engine");
    auto t7 = task("self_test", target + "_post_refactor",
        {{"cases", 100}, {"compare_baseline", true}, {"tolerance", 0.0}},
        9, 180, "swarm_engine");
    auto t8 = task("self_test", "full_regression",
        {{"cases", 500}, {"scope", "full_suite"}}, 8, 300, "swarm_engine");
    p3tasks.push_back(t6);
    p3tasks.push_back(t7);
    p3tasks.push_back(t8);

    for (auto& pt : p2tasks) {
        deps.push_back(depEdge(pt["id"].get<uint32_t>(), t6["id"].get<uint32_t>()));
    }
    deps.push_back(depEdge(t6["id"], t7["id"]));
    deps.push_back(depEdge(t6["id"], t8["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("impact_analysis", 1, p1tasks),
            phase("refactor_execution", 2, p2tasks),
            phase("validation", 3, p3tasks, "all_pass")
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 2 / 3},
            {"phase_count", 3},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size()}
        }}
    };
}


// ============================================================================
// migrationPlan — Migration/upgrade pipeline with rollback
// ============================================================================
json MetaPlanner::migrationPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Assessment
    json p1tasks = json::array();
    auto t1 = task("analyze_deps", target + "_migration_scope",
        {{"target", target}, {"analysis_type", "migration"},
         {"detect_breaking_changes", true}, {"depth", 10}},
        8, 180, "context_analyzer");
    auto t2 = task("index_symbols", "src",
        {{"scope", "all_references_to_" + target}, {"include_transitive", true}},
        7, 120, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_migration_" + target,
        {{"message", "Pre-migration snapshot: " + wish}, {"tag", true},
         {"branch", "migration/" + target}},
        9, 5, "agentic_loop");
    auto t4 = task("self_test", "full_baseline",
        {{"cases", 500}, {"record_baseline", true}, {"scope", "full_suite"}},
        7, 300, "swarm_engine");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);
    p1tasks.push_back(t4);

    // Phase 2: Migration execution (sequential, each step verified)
    json p2tasks = json::array();
    auto t5 = task("migrate", target + "_api_surface",
        {{"strategy", "incremental"}, {"step", 1},
         {"update_signatures", true}, {"add_adapters", true}},
        9, 240, "agentic_loop");
    auto t6 = task("migrate", target + "_internals",
        {{"strategy", "incremental"}, {"step", 2},
         {"update_implementations", true}},
        9, 360, "agentic_loop");
    auto t7 = task("migrate", target + "_tests",
        {{"strategy", "incremental"}, {"step", 3},
         {"update_test_expectations", true}},
        8, 180, "agentic_loop");
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);
    p2tasks.push_back(t7);

    deps.push_back(depEdge(t1["id"], t5["id"]));
    deps.push_back(depEdge(t2["id"], t5["id"]));
    deps.push_back(depEdge(t3["id"], t5["id"]));
    deps.push_back(depEdge(t5["id"], t6["id"]));  // Sequential: step 1 before step 2
    deps.push_back(depEdge(t6["id"], t7["id"]));  // Sequential: step 2 before step 3

    // Phase 3: Comprehensive validation
    json p3tasks = json::array();
    auto t8 = task("compile", "all",
        {{"config", "Release"}, {"warnings_as_errors", true}, {"lto", true}}, 9, 180, "swarm_engine");
    auto t9 = task("self_test", "full_post_migration",
        {{"cases", 500}, {"compare_baseline", true}, {"tolerance", 0.0}}, 9, 300, "swarm_engine");
    auto t10 = task("integration_test", "cross_module",
        {{"test_interactions", true}, {"focus", target}}, 8, 600, "swarm_engine");
    auto t11 = task("bench", "post_migration_perf",
        {{"metric", "tokens/sec"}, {"compare_baseline", true}, {"regression_threshold", 0.95}},
        7, 300, "inference_core");
    p3tasks.push_back(t8);
    p3tasks.push_back(t9);
    p3tasks.push_back(t10);
    p3tasks.push_back(t11);

    deps.push_back(depEdge(t7["id"], t8["id"]));
    deps.push_back(depEdge(t8["id"], t9["id"]));
    deps.push_back(depEdge(t8["id"], t10["id"]));
    deps.push_back(depEdge(t8["id"], t11["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("assessment", 1, p1tasks),
            phase("migration_execution", 2, p2tasks, "sequential_pass"),
            phase("comprehensive_validation", 3, p3tasks, "all_pass")
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 4 / 5},
            {"phase_count", 3},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size()}
        }}
    };
}


// ============================================================================
// securityPlan — Security audit & fix pipeline
// ============================================================================
json MetaPlanner::securityPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Security scan
    json p1tasks = json::array();
    auto t1 = task("scan_security", "dependency_audit",
        {{"scan_deps", true}, {"check_cve", true}, {"severity_threshold", "medium"}},
        9, 180, "context_analyzer");
    auto t2 = task("scan_security", "source_audit",
        {{"scan_source", true}, {"checks", json::array({
            "buffer_overflow", "null_deref", "use_after_free", "integer_overflow",
            "format_string", "path_traversal", "injection", "hardcoded_secrets"
         })}},
        9, 240, "context_analyzer");
    auto t3 = task("analyze_deps", "attack_surface",
        {{"target", target}, {"analysis_type", "security"}, {"include_network_endpoints", true}},
        8, 120, "context_analyzer");
    auto t4 = task("git_snapshot", "pre_security_fix",
        {{"message", "Pre-security-audit snapshot"}}, 5, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);
    p1tasks.push_back(t4);

    // Phase 2: Remediation
    json p2tasks = json::array();
    auto t5 = task("edit_source", "security_fixes",
        {{"strategy", "auto_remediate"}, {"severity_min", "medium"},
         {"add_bounds_checks", true}, {"sanitize_inputs", true}},
        9, 180, "refactor_engine");
    auto t6 = task("edit_source", "secret_rotation",
        {{"strategy", "extract_secrets"}, {"target", ".env"},
         {"remove_hardcoded", true}},
        8, 60, "refactor_engine");
    p2tasks.push_back(t5);
    p2tasks.push_back(t6);

    deps.push_back(depEdge(t1["id"], t5["id"]));
    deps.push_back(depEdge(t2["id"], t5["id"]));
    deps.push_back(depEdge(t3["id"], t5["id"]));
    deps.push_back(depEdge(t4["id"], t5["id"]));
    deps.push_back(depEdge(t4["id"], t6["id"]));

    // Phase 3: Verify fixes
    json p3tasks = json::array();
    auto t7 = task("scan_security", "post_fix_rescan",
        {{"scan_deps", true}, {"scan_source", true}, {"verify_fixes", true}},
        9, 180, "context_analyzer");
    auto t8 = task("fuzz_test", "security_fuzz",
        {{"cases", 5000}, {"targets", json::array({"buffer_overflow", "null_deref", "injection"})},
         {"timeout_per_case_ms", 3000}},
        8, 900, "swarm_engine");
    auto t9 = task("self_test", "security_regression",
        {{"cases", 100}, {"focus", "security"}}, 8, 180, "swarm_engine");
    p3tasks.push_back(t7);
    p3tasks.push_back(t8);
    p3tasks.push_back(t9);

    deps.push_back(depEdge(t5["id"], t7["id"]));
    deps.push_back(depEdge(t6["id"], t7["id"]));
    deps.push_back(depEdge(t5["id"], t8["id"]));
    deps.push_back(depEdge(t5["id"], t9["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("security_scan", 1, p1tasks),
            phase("remediation", 2, p2tasks),
            phase("verify_fixes", 3, p3tasks, "all_pass")
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 2 / 3},
            {"phase_count", 3},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size()}
        }}
    };
}


// ============================================================================
// genericPlan — Fallback for unrecognized intents
// ============================================================================
json MetaPlanner::genericPlan(const std::string& wish) {
    IntentSignals sig = extractIntentSignals(wish);
    std::string target = sig.targetEntity;
    json deps = json::array();

    // Phase 1: Understand scope
    json p1tasks = json::array();
    auto t1 = task("index_symbols", target,
        {{"scope", "relevant_symbols"}, {"include_callgraph", true}}, 5, 120, "context_analyzer");
    auto t2 = task("analyze_deps", target + "_context",
        {{"target", target}, {"depth", 3}}, 5, 90, "context_analyzer");
    auto t3 = task("git_snapshot", "pre_generic_" + target,
        {{"message", "Snapshot before: " + wish}}, 3, 3, "agentic_loop");
    p1tasks.push_back(t1);
    p1tasks.push_back(t2);
    p1tasks.push_back(t3);

    // Phase 2: Execute primary action
    json p2tasks = json::array();
    auto t4 = task("edit_source", target,
        {{"wish", wish}, {"strategy", "llm_guided"}, {"context_lines", 20},
         {"verify_syntax", true}},
        7, 120, "agentic_loop");
    p2tasks.push_back(t4);

    deps.push_back(depEdge(t1["id"], t4["id"]));
    deps.push_back(depEdge(t2["id"], t4["id"]));
    deps.push_back(depEdge(t3["id"], t4["id"]));

    // Phase 3: Validate
    json p3tasks = json::array();
    auto t5 = task("compile", target,
        {{"config", "Release"}, {"warnings_as_errors", false}}, 7, 120, "swarm_engine");
    auto t6 = task("self_test", target + "_regression",
        {{"cases", 20}}, 6, 120, "swarm_engine");
    p3tasks.push_back(t5);
    p3tasks.push_back(t6);

    deps.push_back(depEdge(t4["id"], t5["id"]));
    deps.push_back(depEdge(t5["id"], t6["id"]));

    uint32_t totalSeconds = 0;
    for (auto& arr : {p1tasks, p2tasks, p3tasks})
        for (auto& t : arr) totalSeconds += t["estimated_seconds"].get<uint32_t>();

    return {
        {"phases", json::array({
            phase("understand_scope", 1, p1tasks),
            phase("execute_action", 2, p2tasks),
            phase("validate", 3, p3tasks, "all_pass")
        })},
        {"dependencies", deps},
        {"cost_estimate", {
            {"total_estimated_seconds", totalSeconds},
            {"parallel_critical_path_seconds", totalSeconds * 2 / 3},
            {"phase_count", 3},
            {"task_count", p1tasks.size() + p2tasks.size() + p3tasks.size()}
        }}
    };
}


// ============================================================================
// decomposeMultiPhase — Decompose an arbitrary goal into phases
// ============================================================================
json MetaPlanner::decomposeMultiPhase(const std::string& goal) {
    // Use plan() to generate phases, then flatten all tasks for the caller
    json result = plan(goal);
    if (result.value("status", "") == "error") return result;

    // Flatten all tasks across phases into a single array for graph insertion
    json allTasks = json::array();
    for (auto& ph : result["phases"]) {
        for (auto& t : ph["tasks"]) {
            json flatTask = t;
            flatTask["phase"] = ph["phase_name"];
            flatTask["phase_order"] = ph["phase_order"];
            allTasks.push_back(flatTask);
        }
    }

    result["flat_tasks"] = allTasks;
    result["total_tasks"] = allTasks.size();
    return result;
}


// ============================================================================
// inferDependencies — Given flat tasks, infer dependency edges
// ============================================================================
json MetaPlanner::inferDependencies(const json& flatTasks) {
    json deps = json::array();
    if (!flatTasks.is_array()) return deps;

    // Build a map of task id → task for reference
    std::unordered_map<uint32_t, const json*> taskMap;
    for (auto& t : flatTasks) {
        uint32_t id = t.value("id", 0u);
        if (id > 0) taskMap[id] = &t;
    }

    // Infer dependencies based on ordering rules:
    // 1. git_snapshot must precede all edit/add/refactor/migrate tasks
    // 2. compile must follow all edit/add tasks
    // 3. self_test must follow compile
    // 4. bench must follow compile
    // 5. hot_reload must follow all tests passing
    // 6. release/deploy tasks must come last
    // 7. index_symbols/analyze_deps are prerequisites for edit tasks

    std::vector<uint32_t> snapshotIds, editIds, compileIds, testIds, benchIds,
                          reloadIds, releaseIds, analyzeIds;

    for (auto& t : flatTasks) {
        uint32_t id = t.value("id", 0u);
        std::string type = t.value("type", "");

        if (type == "git_snapshot")                          snapshotIds.push_back(id);
        else if (type == "edit_source" || type == "add_cpp" ||
                 type == "add_asm" || type == "add_comp" ||
                 type == "add_kernel" || type == "refactor" ||
                 type == "rename" || type == "extract" ||
                 type == "inline_fn" || type == "migrate" ||
                 type == "hotpatch")                         editIds.push_back(id);
        else if (type == "compile" || type == "link")        compileIds.push_back(id);
        else if (type == "self_test" || type == "fuzz_test" ||
                 type == "regression_test" || type == "integration_test")
                                                             testIds.push_back(id);
        else if (type == "bench" || type == "profile")       benchIds.push_back(id);
        else if (type == "hot_reload")                       reloadIds.push_back(id);
        else if (type == "release" || type == "deploy" ||
                 type == "upload_cdn" || type == "create_release" ||
                 type == "sign_binary" || type == "notify")  releaseIds.push_back(id);
        else if (type == "index_symbols" || type == "analyze_deps" ||
                 type == "scan_security" || type == "build_embeddings")
                                                             analyzeIds.push_back(id);
    }

    // Snapshots before edits
    for (uint32_t sid : snapshotIds)
        for (uint32_t eid : editIds)
            deps.push_back(depEdge(sid, eid, "prerequisite"));

    // Analysis before edits
    for (uint32_t aid : analyzeIds)
        for (uint32_t eid : editIds)
            deps.push_back(depEdge(aid, eid, "data_flow"));

    // Edits before compile
    for (uint32_t eid : editIds)
        for (uint32_t cid : compileIds)
            deps.push_back(depEdge(eid, cid, "build_order"));

    // Compile before tests and benchmarks
    for (uint32_t cid : compileIds) {
        for (uint32_t tid : testIds)
            deps.push_back(depEdge(cid, tid, "build_order"));
        for (uint32_t bid : benchIds)
            deps.push_back(depEdge(cid, bid, "build_order"));
    }

    // Tests and benchmarks before hot_reload
    for (uint32_t tid : testIds)
        for (uint32_t rid : reloadIds)
            deps.push_back(depEdge(tid, rid, "gate"));
    for (uint32_t bid : benchIds)
        for (uint32_t rid : reloadIds)
            deps.push_back(depEdge(bid, rid, "gate"));

    // Everything before release
    for (uint32_t tid : testIds)
        for (uint32_t rid : releaseIds)
            deps.push_back(depEdge(tid, rid, "gate"));
    for (uint32_t bid : benchIds)
        for (uint32_t rid : releaseIds)
            deps.push_back(depEdge(bid, rid, "gate"));
    for (uint32_t hid : reloadIds)
        for (uint32_t rid : releaseIds)
            deps.push_back(depEdge(hid, rid, "gate"));

    return deps;
}


// ============================================================================
// estimateCost — Estimate total time/resource cost for a plan
// ============================================================================
json MetaPlanner::estimateCost(const json& planJson) {
    uint32_t totalTasks = 0;
    uint32_t totalSeconds = 0;
    uint32_t criticalPathSeconds = 0;
    uint32_t maxPhaseSeconds = 0;
    std::map<std::string, uint32_t> subsystemLoad;

    if (planJson.contains("phases")) {
        for (auto& ph : planJson["phases"]) {
            uint32_t phaseSeconds = 0;
            uint32_t phaseMaxTask = 0;
            for (auto& t : ph["tasks"]) {
                uint32_t est = t.value("estimated_seconds", 60u);
                totalSeconds += est;
                phaseSeconds += est;
                if (est > phaseMaxTask) phaseMaxTask = est;
                totalTasks++;

                std::string sub = t.value("subsystem", "agentic_loop");
                subsystemLoad[sub] += est;
            }
            // Critical path estimate: max task in each phase (parallel within phase)
            criticalPathSeconds += phaseMaxTask;
            if (phaseSeconds > maxPhaseSeconds) maxPhaseSeconds = phaseSeconds;
        }
    } else if (planJson.contains("flat_tasks")) {
        for (auto& t : planJson["flat_tasks"]) {
            uint32_t est = t.value("estimated_seconds", 60u);
            totalSeconds += est;
            totalTasks++;
            std::string sub = t.value("subsystem", "agentic_loop");
            subsystemLoad[sub] += est;
        }
        criticalPathSeconds = totalSeconds / 2;  // rough estimate
    }

    json load = json::object();
    for (auto& [k, v] : subsystemLoad) {
        load[k] = v;
    }

    return {
        {"total_tasks",                    totalTasks},
        {"total_estimated_seconds",        totalSeconds},
        {"parallel_critical_path_seconds", criticalPathSeconds},
        {"heaviest_phase_seconds",         maxPhaseSeconds},
        {"subsystem_load_seconds",         load},
        {"estimated_wall_clock_minutes",   std::ceil(criticalPathSeconds / 60.0)}
    };
}


// ============================================================================
// routeToSubsystems — Assign subsystem IDs to each task
// ============================================================================
json MetaPlanner::routeToSubsystems(const json& planJson) {
    json result = planJson;  // deep copy

    auto routeTask = [](json& t) {
        std::string type = t.value("type", "");
        auto it = kTypeToSubsystem.find(type);
        if (it != kTypeToSubsystem.end()) {
            t["subsystem"] = it->second;
        }
        // Map subsystem name to numeric ID for AgenticTaskGraph
        std::string sub = t.value("subsystem", "agentic_loop");
        auto idIt = kSubsystemMap.find(sub);
        t["subsystem_id"] = (idIt != kSubsystemMap.end()) ? idIt->second : 2;
    };

    if (result.contains("phases")) {
        for (auto& ph : result["phases"]) {
            for (auto& t : ph["tasks"]) {
                routeTask(t);
            }
        }
    }
    if (result.contains("flat_tasks")) {
        for (auto& t : result["flat_tasks"]) {
            routeTask(t);
        }
    }

    return result;
}


// ============================================================================
// mergePlans — Combine multiple plans with cross-plan dependencies
// ============================================================================
json MetaPlanner::mergePlans(const std::vector<json>& plans) {
    json mergedPhases = json::array();
    json mergedDeps = json::array();
    uint32_t totalTasks = 0;
    uint32_t totalSeconds = 0;
    uint32_t phaseOffset = 0;

    for (size_t planIdx = 0; planIdx < plans.size(); ++planIdx) {
        const json& p = plans[planIdx];
        if (!p.contains("phases")) continue;

        for (auto& ph : p["phases"]) {
            json mergedPhase = ph;
            uint32_t newOrder = ph.value("phase_order", 0u) + phaseOffset;
            mergedPhase["phase_order"] = newOrder;
            mergedPhase["source_plan"] = planIdx;
            mergedPhases.push_back(mergedPhase);

            for (auto& t : ph["tasks"]) {
                totalTasks++;
                totalSeconds += t.value("estimated_seconds", 60u);
            }
        }

        if (p.contains("dependencies")) {
            for (auto& dep : p["dependencies"]) {
                mergedDeps.push_back(dep);
            }
        }

        phaseOffset += static_cast<uint32_t>(p["phases"].size());
    }

    return {
        {"status",       "ok"},
        {"merged",       true},
        {"plan_count",   plans.size()},
        {"phases",       mergedPhases},
        {"dependencies", mergedDeps},
        {"cost_estimate", {
            {"total_tasks",             totalTasks},
            {"total_estimated_seconds", totalSeconds},
            {"phase_count",             mergedPhases.size()}
        }}
    };
}


// ============================================================================
// validatePlan — Check plan for cycles, missing deps, unreachable tasks
// ============================================================================
json MetaPlanner::validatePlan(const json& planJson) {
    json issues = json::array();
    bool valid = true;

    // Collect all task IDs
    std::unordered_set<uint32_t> allIds;
    std::unordered_map<uint32_t, std::string> idToName;

    auto collectTasks = [&](const json& tasks) {
        for (auto& t : tasks) {
            uint32_t id = t.value("id", 0u);
            if (id == 0) {
                issues.push_back({{"severity", "error"}, {"message", "Task with id=0 found"}});
                valid = false;
                continue;
            }
            if (allIds.count(id)) {
                issues.push_back({{"severity", "error"},
                    {"message", "Duplicate task id: " + std::to_string(id)}});
                valid = false;
            }
            allIds.insert(id);
            idToName[id] = t.value("target", "unknown");
        }
    };

    if (planJson.contains("phases")) {
        for (auto& ph : planJson["phases"]) {
            if (ph.contains("tasks")) collectTasks(ph["tasks"]);
        }
    }
    if (planJson.contains("flat_tasks")) {
        collectTasks(planJson["flat_tasks"]);
    }

    // Validate dependency edges
    std::unordered_map<uint32_t, std::vector<uint32_t>> adjList;
    if (planJson.contains("dependencies")) {
        for (auto& dep : planJson["dependencies"]) {
            uint32_t from = dep.value("from_task_id", 0u);
            uint32_t to   = dep.value("to_task_id", 0u);

            if (!allIds.count(from)) {
                issues.push_back({{"severity", "warning"},
                    {"message", "Dependency references unknown from_task_id: " + std::to_string(from)}});
            }
            if (!allIds.count(to)) {
                issues.push_back({{"severity", "warning"},
                    {"message", "Dependency references unknown to_task_id: " + std::to_string(to)}});
            }
            if (from == to) {
                issues.push_back({{"severity", "error"},
                    {"message", "Self-dependency on task " + std::to_string(from)}});
                valid = false;
            }
            adjList[from].push_back(to);
        }
    }

    // Cycle detection via DFS coloring
    enum Color : uint8_t { WHITE = 0, GRAY = 1, BLACK = 2 };
    std::unordered_map<uint32_t, Color> color;
    for (uint32_t id : allIds) color[id] = WHITE;

    bool hasCycle = false;
    std::function<void(uint32_t)> dfs = [&](uint32_t u) {
        if (hasCycle) return;
        color[u] = GRAY;
        if (adjList.count(u)) {
            for (uint32_t v : adjList[u]) {
                if (color.count(v) && color[v] == GRAY) {
                    hasCycle = true;
                    issues.push_back({{"severity", "error"},
                        {"message", "Cycle detected involving task " + std::to_string(u) +
                                    " → " + std::to_string(v)}});
                    valid = false;
                    return;
                }
                if (color.count(v) && color[v] == WHITE) {
                    dfs(v);
                }
            }
        }
        color[u] = BLACK;
    };

    for (uint32_t id : allIds) {
        if (color[id] == WHITE) dfs(id);
        if (hasCycle) break;
    }

    // Check for orphaned tasks (no incoming or outgoing edges and not a root/leaf)
    if (allIds.size() > 1) {
        std::unordered_set<uint32_t> hasIncoming, hasOutgoing;
        for (auto& [from, tos] : adjList) {
            hasOutgoing.insert(from);
            for (uint32_t to : tos) hasIncoming.insert(to);
        }
        for (uint32_t id : allIds) {
            if (!hasIncoming.count(id) && !hasOutgoing.count(id)) {
                issues.push_back({{"severity", "info"},
                    {"message", "Isolated task (no dependencies): " + std::to_string(id) +
                                " (" + idToName[id] + ")"}});
            }
        }
    }

    return {
        {"valid",       valid},
        {"task_count",  allIds.size()},
        {"dep_count",   planJson.contains("dependencies") ? planJson["dependencies"].size() : 0},
        {"issue_count", issues.size()},
        {"issues",      issues}
    };
}
