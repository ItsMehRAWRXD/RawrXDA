// agentic_reflection_engine.hpp
// Post-execution validation: verify builds, tests, code quality

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Agentic {

// Forward declarations
struct ExecutionPlan;
struct PlanStep;

// ============================================================================
// Reflection & Verification Results
// ============================================================================

enum class VerificationLevel : uint8_t {
    Syntax = 1,       // Does it compile?
    BuildSuccess = 2, // Successful build?
    UnitTests = 3,    // Unit tests pass?
    Integration = 4,  // Integration tests pass?
    Performance = 5   // Performance within bounds?
};

enum class VerificationStatus : uint8_t {
    NotRun = 0,
    Passed = 1,
    Failed = 2,
    Timeout = 3,
    PartialPass = 4   // Some checks passed, others skipped/warning
};

struct VerificationResult {
    VerificationLevel level;
    VerificationStatus status;
    std::string description;
    std::string error_message;
    
    // Metrics
    int duration_ms{0};
    int tests_passed{0};
    int tests_failed{0};
    int tests_skipped{0};
    float code_change_coverage{0.0f};    // % of changed code covered by tests
    
    // Artifacts
    std::vector<std::string> generated_artifacts;
    std::string log_output;
    
    nlohmann::json toJson() const;
};

struct ReflectionResult {
    std::string step_id;
    std::string time_of_reflection;
    
    // Collection of all verifications
    std::vector<VerificationResult> verifications;
    int total_checks{0};
    int passed_checks{0};
    int failed_checks{0};
    int skipped_checks{0};
    
    // Overall assessment
    bool is_safe_to_auto_approve{false};
    bool has_regressions{false};
    bool meets_quality_gate{false};
    
    // Recommendations
    std::string recommendation;  // "Auto-Approve" / "Manual Review" / "Rollback Recommended"
    std::vector<std::string> issues_found;
    std::vector<std::string> warnings;
    
    float confidence_safe{0.8f};  // 0.0-1.0 confidence that this is safe
    
    nlohmann::json toJson() const;
};

// ============================================================================
// Verification Strategies
// ============================================================================

struct CompilationCheck {
    bool enabled{true};
    int timeout_seconds{300};
    bool treat_warnings_as_errors{false};
    bool run_preprocessor_only{false};
};

struct TestCheck {
    bool enabled{true};
    int timeout_seconds{600};
    bool run_unit_tests{true};
    bool run_integration_tests{false};
    float min_coverage_percent{50.0f};
};

struct CodeQualityCheck {
    bool enabled{false};  // Optional
    std::vector<std::string> linters;  // clang-tidy, cppcheck, etc.
    int timeout_seconds{120};
    bool strict_mode{false};
};

struct PerformanceCheck {
    bool enabled{false};  // Optional
    int timeout_seconds{300};
    float regression_threshold{0.1f};  // 10% slowdown is acceptable
};

struct ReflectionConfig {
    CompilationCheck compilation;
    TestCheck tests;
    CodeQualityCheck quality;
    PerformanceCheck performance;
    
    bool run_all_checks{false};           // Block until all done
    bool run_critical_checks_only{true};  // Only compile, unit tests
    int max_parallel_jobs{4};
    
    nlohmann::json toJson() const;
    static ReflectionConfig Fast();      // Compilation + unit tests only
    static ReflectionConfig Standard();  // + code quality
    static ReflectionConfig Thorough();  // + integration, performance
};

// ============================================================================
// Reflection Engine
// ============================================================================

class ReflectionEngine {
public:
    using CompilationFn = std::function<bool(const std::string& cmd, std::string& output)>;
    using TestExecutorFn = std::function<bool(const std::vector<std::string>& tests, std::string& output)>;
    using LinterFn = std::function<bool(const std::string& linter, std::string& output)>;
    using LogFn = std::function<void(const std::string& entry)>;
    
    ReflectionEngine();
    ~ReflectionEngine();
    
    // Reflection orchestration
    ReflectionResult reflectOnExecution(const ExecutionPlan* plan, int step_index);
    
    // Individual checks (can be called separately)
    VerificationResult verifyCompilation(const PlanStep& step, const std::string& workspace_root);
    VerificationResult verifyTests(const PlanStep& step, const std::string& workspace_root);
    VerificationResult verifyCodeQuality(const PlanStep& step, const std::string& workspace_root);
    VerificationResult verifyPerformance(const PlanStep& step, const std::string& workspace_root);
    
    // Configuration
    void setConfig(const ReflectionConfig& config) { m_config = config; }
    ReflectionConfig getConfig() const { return m_config; }
    
    // Callbacks - wire to actual tool implementations
    void setCompilationFn(CompilationFn fn) { m_compileFn = fn; }
    void setTestExecutorFn(TestExecutorFn fn) { m_testFn = fn; }
    void setLinterFn(LinterFn fn) { m_linterFn = fn; }
    void setLogFn(LogFn fn) { m_logFn = fn; }
    
    // Query result cache
    const ReflectionResult* getLastResult(const std::string& step_id) const;
    
    // Assessment
    bool isResultSafeToAutoApprove(const ReflectionResult& result) const;
    std::string getRecommendation(const ReflectionResult& result) const;
    
private:
    // Helper methods
    void runAsyncVerifications(std::vector<VerificationResult>& results,
                              const PlanStep& step,
                              const std::string& workspace_root);
    
    void assessResult(ReflectionResult& result);
    void detectRegressions(ReflectionResult& result);
    float computeConfidence(const ReflectionResult& result) const;
    
    ReflectionConfig m_config;
    std::map<std::string, ReflectionResult> m_result_cache;
    
    CompilationFn m_compileFn;
    TestExecutorFn m_testFn;
    LinterFn m_linterFn;
    LogFn m_logFn;
};

} // namespace Agentic
