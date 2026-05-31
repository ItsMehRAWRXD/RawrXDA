#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>
#include <cstdint>

namespace RawrXD {
namespace Autonomous {

// ─── Failure Classification ─────────────────────────────────────────────────

/// @brief Classifies the type of failure for intelligent retry logic
enum class FailureType {
    TRANSIENT,      ///< Network timeout, resource temporarily unavailable → retry same step
    LOGIC_ERROR,    ///< Plan logic is flawed → modify step or strategy
    TOOL_FAILURE,   ///< Tool execution failed (compile, test) → try alternative approach
    INVALID_PLAN,   ///< Entire plan strategy is wrong → regenerate from goal
    ENVIRONMENT,    ///< System constraint (memory, permission) → halt or escalate
    UNKNOWN         ///< Unable to classify → treat as logic error
};

// ─── Validation Contracts ────────────────────────────────────────────────────

struct ValidationSpec {
    /// @brief Type of validation to perform
    enum Type {
        FILE_EXISTS,       ///< File must exist at specified path
        FILE_CONTENT,      ///< File content must match pattern/hash
        COMPILE_SUCCESS,   ///< Code must compile without errors
        TEST_PASS,         ///< Specified test suite must pass
        NO_REGRESSION,     ///< Workspace metrics must not degrade
        METRIC_THRESHOLD,  ///< Metric must be above/below threshold
        CUSTOM             ///< Caller-defined validation function
    };

    Type type = CUSTOM;
    std::string target_path;        ///< File path or metric name
    std::string expected_value;     ///< Expected content, pattern, or threshold
    double tolerance = 0.0;         ///< Tolerance for numeric comparisons
    std::string description;        ///< Human-readable validation description

    /// @brief Custom validation function: returns true if valid
    std::function<bool(const std::string& context)> custom_validator;
};

// ─── Plan Steps with Explicit Success Criteria ──────────────────────────────

struct PlanStep {
    std::string id;                           ///< Unique step identifier
    std::string action;                       ///< What to do (CLI, code edit, etc.)
    std::string expected_outcome;             ///< Human description of success
    std::vector<ValidationSpec> validations;  ///< Success criteria
    int max_retries = 3;                      ///< Max retries for this step
};

// ─── Termination Control ─────────────────────────────────────────────────────

enum class TerminationReason {
    SUCCESS,            ///< Goal achieved, issue count reduced to 0
    NO_PROGRESS,        ///< Issue count unchanged after N iterations
    MAX_RETRIES,        ///< Hit maximum iteration limit
    DEGRADED_STATE,     ///< Workspace state worse than before
    HUMAN_REQUIRED,     ///< Manual intervention needed
    UNRECOVERABLE,      ///< Unrecoverable error occurred
    STRATEGY_EXHAUSTED  ///< All strategies attempted unsuccessfully
};

struct TerminationState {
    TerminationReason reason = TerminationReason::SUCCESS;
    int iteration = 0;
    int issues_before = 0;
    int issues_after = 0;
    std::string diagnostic;
    bool should_escalate = false;
};

// ─── Attempt Record (Decision-Relevant Memory) ───────────────────────────────

struct AttemptRecord {
    int attempt_number = 0;
    PlanStep step;
    std::string strategy_used;              ///< "debug", "refactor", "enumerate"
    bool executed_successfully = false;
    FailureType failure_type = FailureType::UNKNOWN;
    std::string diagnostic;                 ///< Why it failed
    bool resolved = false;
    std::vector<std::string> failed_at_validation;  ///< Which validations failed
    int workspace_issues_before = 0;
    int workspace_issues_after = 0;
    uint64_t elapsed_ms = 0;
};

// ─── Validator Interface ─────────────────────────────────────────────────────

class IValidator {
public:
    virtual ~IValidator() = default;

    /// @brief Validate a plan step against its success criteria
    /// @return true if all validations pass
    virtual bool validateStep(const PlanStep& step, std::vector<std::string>& failed_validations) = 0;

    /// @brief Register workspace baseline for regression detection
    virtual void recordBaseline(const std::string& workspace_root) = 0;

    /// @brief Check if baseline metrics have degraded
    virtual bool hasRegressed(const std::string& workspace_root) = 0;
};

// ─── Failure Classifier Interface ────────────────────────────────────────────

class IFailureClassifier {
public:
    virtual ~IFailureClassifier() = default;

    /// @brief Classify a failure based on output and context
    virtual FailureType classifyFailure(
        const PlanStep& step,
        const std::string& error_output,
        const std::string& context) = 0;

    /// @brief Get retry strategy for a specific failure type
    virtual std::string getRetryStrategy(FailureType failure_type) = 0;
};

// ─── Two-Level Planning (Task + Strategy) ───────────────────────────────────

enum class Strategy {
    DEBUG,       ///< Surgical fix to specific component
    REFACTOR,    ///< Larger structural change
    ENUMERATE,   ///< Exhaustive search through possibilities
    ISOLATE,     ///< Isolate problem to minimal reproducible case
    BYPASS       ///< Work around the issue differently
};

struct StrategyPlan {
    Strategy strategy = Strategy::DEBUG;
    std::string rationale;                       ///< Why this strategy
    std::vector<PlanStep> steps;
    int max_iterations = 5;
    int iteration = 0;

    bool canShift() const {
        return iteration >= std::max(1, max_iterations / 2);
    }
};

// ─── Adaptive Planner Interface ──────────────────────────────────────────────

class IAdaptivePlanner {
public:
    virtual ~IAdaptivePlanner() = default;

    /// @brief Plan at strategy level: choose how to approach the goal
    /// @param goal The high-level goal (e.g., "fix compiler error")
    /// @param history Previous attempts
    /// @return Strategy plan with rationale
    virtual StrategyPlan planStrategy(
        const std::string& goal,
        const std::vector<AttemptRecord>& history) = 0;

    /// @brief Plan at task level: break strategy into concrete steps
    /// @param strategy The strategy to execute
    /// @return List of validated PlanSteps
    virtual std::vector<PlanStep> planTasks(const StrategyPlan& strategy) = 0;

    /// @brief Adapt strategy based on attempt record
    /// @param current_strategy Current strategy plan
    /// @param attempt Last attempt result
    /// @return Modified strategy or new strategy
    virtual StrategyPlan adaptStrategy(
        const StrategyPlan& current_strategy,
        const AttemptRecord& attempt) = 0;

    /// @brief Detect if we're in a loop (repeated failures)
    virtual bool isInLoop(const std::vector<AttemptRecord>& history) = 0;
};

// ─── Termination Controller Interface ────────────────────────────────────────

class ITerminationController {
public:
    virtual ~ITerminationController() = default;

    /// @brief Check if loop should terminate
    /// @param workspace_issues_now Current issue count
    /// @param history Attempt history
    /// @return Termination state with reason
    virtual TerminationState shouldTerminate(
        int workspace_issues_now,
        const std::vector<AttemptRecord>& history) = 0;

    /// @brief Get human-readable termination explanation
    virtual std::string explainTermination(const TerminationState& state) = 0;
};

// ─── Factory Functions ───────────────────────────────────────────────────────

std::unique_ptr<IValidator> createDefaultValidator();
std::unique_ptr<IFailureClassifier> createDefaultClassifier();
std::unique_ptr<IAdaptivePlanner> createDefaultPlanner();
std::unique_ptr<ITerminationController> createDefaultTerminationController();

} // namespace Autonomous
} // namespace RawrXD
