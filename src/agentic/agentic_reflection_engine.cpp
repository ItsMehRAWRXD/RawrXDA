// agentic_reflection_engine.cpp
// Post-execution validation: run builds, tests, code quality checks

#include "agentic_reflection_engine.hpp"
#include "agentic_planning_orchestrator.hpp"
#include "agentic_tool_executor.hpp"
#include "observability/Logger.hpp"
#include <sstream>
#include <ctime>

namespace Agentic {

// ============================================================================
// VerificationResult Implementation
// ============================================================================

nlohmann::json VerificationResult::toJson() const {
    nlohmann::json j;
    j["level"] = static_cast<int>(level);
    j["status"] = static_cast<int>(status);
    j["description"] = description;
    j["duration_ms"] = duration_ms;
    j["tests_passed"] = tests_passed;
    j["tests_failed"] = tests_failed;
    j["tests_skipped"] = tests_skipped;
    j["coverage"] = code_change_coverage;
    return j;
}

// ============================================================================
// ReflectionResult Implementation
// ============================================================================

nlohmann::json ReflectionResult::toJson() const {
    nlohmann::json j;
    j["step_id"] = step_id;
    j["time"] = time_of_reflection;
    j["total_checks"] = total_checks;
    j["passed"] = passed_checks;
    j["failed"] = failed_checks;
    j["safe_to_approve"] = is_safe_to_auto_approve;
    j["has_regressions"] = has_regressions;
    j["meets_quality_gate"] = meets_quality_gate;
    j["confidence"] = confidence_safe;
    j["recommendation"] = recommendation;
    
    nlohmann::json issues_arr = nlohmann::json::array();
    for (const auto& issue : issues_found) {
        issues_arr.push_back(issue);
    }
    j["issues"] = issues_arr;
    
    return j;
}

// ============================================================================
// ReflectionConfig Implementation
// ============================================================================

nlohmann::json ReflectionConfig::toJson() const {
    nlohmann::json j;
    j["run_critical_only"] = run_critical_checks_only;
    j["compilation_enabled"] = compilation.enabled;
    j["tests_enabled"] = tests.enabled;
    j["quality_enabled"] = quality.enabled;
    j["performance_enabled"] = performance.enabled;
    return j;
}

ReflectionConfig ReflectionConfig::Fast() {
    ReflectionConfig cfg;
    cfg.compilation.enabled = true;
    cfg.compilation.timeout_seconds = 180;
    cfg.tests.enabled = true;
    cfg.tests.run_unit_tests = true;
    cfg.tests.run_integration_tests = false;
    cfg.quality.enabled = false;
    cfg.performance.enabled = false;
    cfg.run_critical_checks_only = true;
    return cfg;
}

ReflectionConfig ReflectionConfig::Standard() {
    ReflectionConfig cfg = Fast();
    cfg.quality.enabled = true;
    cfg.quality.strict_mode = false;
    return cfg;
}

ReflectionConfig ReflectionConfig::Thorough() {
    ReflectionConfig cfg = Standard();
    cfg.tests.run_integration_tests = true;
    cfg.performance.enabled = true;
    cfg.run_all_checks = true;
    return cfg;
}

// ============================================================================
// ReflectionEngine Implementation
// ============================================================================

ReflectionEngine::ReflectionEngine()
    : m_config(ReflectionConfig::Standard()) {
}

ReflectionEngine::~ReflectionEngine() {
}

ReflectionResult ReflectionEngine::reflectOnExecution(const ExecutionPlan* plan, int step_index) {
    ReflectionResult result;
    
    if (!plan || step_index < 0 || step_index >= static_cast<int>(plan->steps.size())) {
        result.step_id = "invalid";
        result.recommendation = "Invalid step reference";
        return result;
    }
    
    const auto& step = plan->steps[step_index];
    result.step_id = step.id;
    
    auto now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    result.time_of_reflection = time_buf;
    
    if (m_logFn) {
        m_logFn("[Reflection] Starting reflection on step " + step.id);
    }
    
    // Run verifications
    runAsyncVerifications(result.verifications, step, plan->workspace_root);
    
    // Count results
    for (const auto& v : result.verifications) {
        result.total_checks++;
        if (v.status == VerificationStatus::Passed) {
            result.passed_checks++;
        } else if (v.status == VerificationStatus::Failed) {
            result.failed_checks++;
            result.issues_found.push_back(v.description + ": " + v.error_message);
        } else if (v.status == VerificationStatus::PartialPass) {
            result.skipped_checks++;
            result.warnings.push_back(v.description);
        }
    }
    
    // Assess overall result
    assessResult(result);
    detectRegressions(result);
    result.confidence_safe = computeConfidence(result);
    result.recommendation = getRecommendation(result);
    
    // Cache result
    m_result_cache[step.id] = result;
    
    if (m_logFn) {
        m_logFn("[Reflection] Result for " + step.id + ": " + result.recommendation +
               " (confidence: " + std::to_string(static_cast<int>(result.confidence_safe * 100)) + "%)");
    }
    
    return result;
}

VerificationResult ReflectionEngine::verifyCompilation(const PlanStep& step, const std::string& workspace_root) {
    VerificationResult result;
    result.level = VerificationLevel::BuildSuccess;
    result.status = VerificationStatus::NotRun;
    
    if (!m_config.compilation.enabled) {
        result.status = VerificationStatus::PartialPass;
        result.description = "Compilation check disabled";
        return result;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string output;
    bool success = false;

    if (m_compileFn) {
        success = m_compileFn("cmake --build . --config Release", output);
    } else {
        ToolExecutor executor;
        if (m_logFn) m_logFn("[ReflectionEngine] Running compilation via ToolExecutor");
        ExecutionRequest req;
        req.tool_name = "cmake";
        req.args = {"--build", ".", "--config", "Release"};
        req.working_dir = workspace_root;
        auto execRes = executor.execute(req);
        output = execRes.stdout_text;
        if (!execRes.stderr_text.empty()) {
            if (!output.empty()) output += "\n";
            output += execRes.stderr_text;
        }
        success = execRes.success;
    }

    if (success) {
        result.status = VerificationStatus::Passed;
        result.description = "Compilation successful";
        result.log_output = output;
    } else {
        result.status = VerificationStatus::Failed;
        result.description = "Compilation failed";
        result.error_message = output;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

VerificationResult ReflectionEngine::verifyTests(const PlanStep& step, const std::string& workspace_root) {
    VerificationResult result;
    result.level = VerificationLevel::UnitTests;
    result.status = VerificationStatus::NotRun;
    
    if (!m_config.tests.enabled) {
        result.status = VerificationStatus::PartialPass;
        result.description = "Test check disabled";
        return result;
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Collect test targets from affected files
    std::vector<std::string> test_targets;
    for (const auto& file : step.affected_files) {
        if (file.find("test") != std::string::npos) {
            test_targets.push_back(file);
        }
    }
    
    std::string output;
    bool success = false;

    if (m_testFn) {
        success = m_testFn(test_targets, output);
    } else {
        ToolExecutor executor;
        if (m_logFn) m_logFn("[ReflectionEngine] Running tests via ToolExecutor");
        ExecutionRequest req;
        req.tool_name = "ctest";
        req.args = {"--output-on-failure"};
        req.working_dir = workspace_root;
        auto execRes = executor.execute(req);
        output = execRes.stdout_text;
        if (!execRes.stderr_text.empty()) {
            if (!output.empty()) output += "\n";
            output += execRes.stderr_text;
        }
        success = execRes.success;
    }

    if (success) {
        result.status = VerificationStatus::Passed;
        result.description = "All tests passed";
        result.log_output = output;
        result.tests_passed = static_cast<int>(test_targets.size());
    } else {
        result.status = VerificationStatus::Failed;
        result.description = "Some tests failed";
        result.error_message = output;
        result.tests_failed = static_cast<int>(test_targets.empty() ? 1 : test_targets.size());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    return result;
}

VerificationResult ReflectionEngine::verifyCodeQuality(const PlanStep& step, const std::string& workspace_root) {
    VerificationResult result;
    result.level = VerificationLevel::Syntax;
    result.status = VerificationStatus::NotRun;
    
    if (!m_config.quality.enabled) {
        result.status = VerificationStatus::PartialPass;
        result.description = "Code quality check disabled";
        return result;
    }
    
    // Run configured linters or fallback to default linter command
    for (const auto& linter : m_config.quality.linters) {
        std::string output;
        bool success = false;
        if (m_linterFn) {
            success = m_linterFn(linter, output);
        } else {
            ToolExecutor executor;
            if (m_logFn) m_logFn("[ReflectionEngine] Running linter " + linter);
            ExecutionRequest req;
            req.tool_name = linter;
            req.working_dir = workspace_root;
            auto execRes = executor.execute(req);
            output = execRes.stdout_text;
            if (!execRes.stderr_text.empty()) {
                if (!output.empty()) output += "\n";
                output += execRes.stderr_text;
            }
            success = execRes.success;
        }

        if (!success) {
            result.status = VerificationStatus::Failed;
            result.description = "Linter " + linter + " failed";
            result.error_message = output;
            return result;
        }
    }

    result.status = VerificationStatus::Passed;
    result.description = "Code quality checks passed";
    result.log_output = "Linters executed successfully";

    return result;
}

VerificationResult ReflectionEngine::verifyPerformance(const PlanStep& step, const std::string& workspace_root) {
    VerificationResult result;
    result.level = VerificationLevel::Performance;
    result.status = VerificationStatus::NotRun;
    
    if (!m_config.performance.enabled) {
        return result;
    }

    std::string output;
    bool success = false;
    ToolExecutor executor;
    if (m_logFn) m_logFn("[ReflectionEngine] Running performance checks via ToolExecutor");
    ExecutionRequest req;
    req.tool_name = "ctest";
    req.args = {"-L", "performance", "--output-on-failure"};
    req.working_dir = workspace_root;
    auto execRes = executor.execute(req);
    output = execRes.stdout_text;
    if (!execRes.stderr_text.empty()) {
        if (!output.empty()) output += "\n";
        output += execRes.stderr_text;
    }
    success = execRes.success;

    if (success) {
        result.status = VerificationStatus::Passed;
        result.description = "Performance benchmarks passed";
        result.log_output = output;
    } else {
        result.status = VerificationStatus::Failed;
        result.description = "Performance benchmarks failed";
        result.error_message = output;
    }

    return result;
}

void ReflectionEngine::runAsyncVerifications(std::vector<VerificationResult>& results,
                                             const PlanStep& step,
                                             const std::string& workspace_root) {
    // Run verifications (in production could be parallel)
    results.push_back(verifyCompilation(step, workspace_root));
    
    if (m_config.tests.enabled) {
        results.push_back(verifyTests(step, workspace_root));
    }
    
    if (m_config.quality.enabled) {
        results.push_back(verifyCodeQuality(step, workspace_root));
    }
    
    if (m_config.performance.enabled) {
        results.push_back(verifyPerformance(step, workspace_root));
    }
}

void ReflectionEngine::assessResult(ReflectionResult& result) {
    // All critical checks must pass
    if (result.failed_checks > 0) {
        result.is_safe_to_auto_approve = false;
        result.confidence_safe = 0.0f;
        return;
    }
    
    // If all passed
    if (result.passed_checks == result.total_checks) {
        result.meets_quality_gate = true;
        result.is_safe_to_auto_approve = true;
        result.confidence_safe = 0.95f;
    } else if (result.passed_checks == result.total_checks - result.skipped_checks) {
        // All required checks passed (some skipped)
        result.meets_quality_gate = true;
        result.is_safe_to_auto_approve = true;
        result.confidence_safe = 0.85f;
    } else {
        result.is_safe_to_auto_approve = false;
        result.confidence_safe = 0.5f;
    }
}

void ReflectionEngine::detectRegressions(ReflectionResult& result) {
    // Check for indicators of regressions
    for (const auto& issue : result.issues_found) {
        if (issue.find("test") != std::string::npos ||
            issue.find("regression") != std::string::npos ||
            issue.find("failed") != std::string::npos) {
            result.has_regressions = true;
            result.confidence_safe *= 0.5f;  // Reduce confidence
        }
    }
}

float ReflectionEngine::computeConfidence(const ReflectionResult& result) const {
    if (result.total_checks <= 0) {
        return 0.0f;
    }

    // Weighted confidence based on pass/fail/skip proportions.
    const float total = static_cast<float>(result.total_checks);
    const float pass_ratio = static_cast<float>(result.passed_checks) / total;
    const float fail_ratio = static_cast<float>(result.failed_checks) / total;
    const float skip_ratio = static_cast<float>(result.skipped_checks) / total;

    float confidence = 0.2f + (0.8f * pass_ratio) - (0.6f * fail_ratio) - (0.2f * skip_ratio);

    if (result.has_regressions) {
        confidence *= 0.5f;
    }

    if (!result.meets_quality_gate) {
        confidence *= 0.75f;
    }

    if (confidence < 0.0f) {
        confidence = 0.0f;
    } else if (confidence > 1.0f) {
        confidence = 1.0f;
    }

    return confidence;
}

bool ReflectionEngine::isResultSafeToAutoApprove(const ReflectionResult& result) const {
    return result.is_safe_to_auto_approve && result.confidence_safe > 0.8f;
}

std::string ReflectionEngine::getRecommendation(const ReflectionResult& result) const {
    if (isResultSafeToAutoApprove(result)) {
        return "Auto-Approve - All checks passed with high confidence";
    }
    
    if (result.has_regressions) {
        return "Manual Review - Possible regressions detected";
    }
    
    if (result.failed_checks > 0) {
        return "Rollback Recommended - Quality checks failed";
    }
    
    if (result.confidence_safe < 0.5f) {
        return "Manual Review - Low confidence in safety";
    }
    
    return "Manual Review - Some checks skipped";
}

const ReflectionResult* ReflectionEngine::getLastResult(const std::string& step_id) const {
    auto it = m_result_cache.find(step_id);
    if (it != m_result_cache.end()) {
        return &it->second;
    }
    return nullptr;
}

} // namespace Agentic
