#include "autonomous_validation_layer.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <iostream>

namespace RawrXD {
namespace Autonomous {

// ─── Default Validator Implementation ────────────────────────────────────────

class DefaultValidator : public IValidator {
private:
    struct BaselineMetrics {
        int file_count = 0;
        int error_count = 0;
        int test_pass_count = 0;
    };
    std::unordered_map<std::string, BaselineMetrics> baselines_;

public:
    bool validateStep(const PlanStep& step, std::vector<std::string>& failed_validations) override {
        failed_validations.clear();
        
        for (const auto& spec : step.validations) {
            bool valid = false;
            
            switch (spec.type) {
                case ValidationSpec::FILE_EXISTS:
                    valid = (std::filesystem::exists(spec.target_path));
                    if (!valid) {
                        failed_validations.push_back("FILE_EXISTS: " + spec.target_path);
                    }
                    break;
                    
                case ValidationSpec::CUSTOM:
                    if (spec.custom_validator) {
                        valid = spec.custom_validator("");
                    }
                    if (!valid) {
                        failed_validations.push_back("CUSTOM: " + spec.description);
                    }
                    break;
                    
                default:
                    // Reject unknown validation types rather than blindly accepting
                    valid = false;
                    failed_validations.push_back("UNKNOWN_TYPE: " + spec.description);
                    break;
            }
            
            if (!valid) {
                return false;
            }
        }
        
        return true;
    }

    void recordBaseline(const std::string& workspace_root) override {
        BaselineMetrics m;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace_root)) {
            if (entry.is_regular_file()) {
                m.file_count++;
            }
        }
        baselines_[workspace_root] = m;
    }

    bool hasRegressed(const std::string& workspace_root) override {
        auto it = baselines_.find(workspace_root);
        if (it == baselines_.end()) return false;
        
        int current_files = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspace_root)) {
            if (entry.is_regular_file()) {
                current_files++;
            }
        }
        
        // Regression if files decreased significantly (>20%)
        double ratio = static_cast<double>(current_files) / it->second.file_count;
        return ratio < 0.8;
    }
};

// ─── Default Failure Classifier Implementation ────────────────────────────────

class DefaultFailureClassifier : public IFailureClassifier {
public:
    FailureType classifyFailure(
        const PlanStep& step,
        const std::string& error_output,
        const std::string& context) override {
        
        // Heuristics for classification
        if (error_output.find("timeout") != std::string::npos ||
            error_output.find("retry") != std::string::npos) {
            return FailureType::TRANSIENT;
        }
        
        if (error_output.find("permission denied") != std::string::npos ||
            error_output.find("out of memory") != std::string::npos) {
            return FailureType::ENVIRONMENT;
        }
        
        if (error_output.find("compilation failed") != std::string::npos ||
            error_output.find("syntax error") != std::string::npos) {
            return FailureType::TOOL_FAILURE;
        }
        
        if (error_output.find("target not found") != std::string::npos) {
            return FailureType::INVALID_PLAN;
        }
        
        return FailureType::LOGIC_ERROR;
    }

    std::string getRetryStrategy(FailureType failure_type) override {
        switch (failure_type) {
            case FailureType::TRANSIENT:
                return "retry_same_step";
            case FailureType::LOGIC_ERROR:
                return "modify_step_parameters";
            case FailureType::TOOL_FAILURE:
                return "try_alternative_tool";
            case FailureType::INVALID_PLAN:
                return "regenerate_entire_plan";
            case FailureType::ENVIRONMENT:
                return "halt_and_escalate";
            default:
                return "retry_same_step";
        }
    }
};

// ─── Default Adaptive Planner Implementation ─────────────────────────────────

class DefaultAdaptivePlanner : public IAdaptivePlanner {
public:
    StrategyPlan planStrategy(
        const std::string& goal,
        const std::vector<AttemptRecord>& history) override {
        
        StrategyPlan plan;
        plan.strategy = Strategy::DEBUG;  // Default to surgical debug approach
        plan.rationale = "Start with surgical fix based on goal: " + goal;
        plan.max_iterations = 5;
        
        // If history shows repeated failures, shift to refactor
        if (history.size() > 2) {
            int logic_errors = 0;
            for (const auto& rec : history) {
                if (rec.failure_type == FailureType::LOGIC_ERROR) logic_errors++;
            }
            if (logic_errors > history.size() / 2) {
                plan.strategy = Strategy::REFACTOR;
                plan.rationale = "Repeated logic errors suggest structural problem";
                plan.max_iterations = 7;
            }
        }
        
        return plan;
    }

    std::vector<PlanStep> planTasks(const StrategyPlan& strategy) override {
        std::vector<PlanStep> steps;

        // Generate concrete steps based on strategy type
        switch (strategy.strategy) {
            case Strategy::DEBUG: {
                PlanStep s1; s1.id = "debug_1"; s1.action = "identify_failure_point";
                s1.expected_outcome = "locate root cause from error logs"; steps.push_back(s1);
                PlanStep s2; s2.id = "debug_2"; s2.action = "apply_surgical_fix";
                s2.expected_outcome = "resolve with minimal code change"; steps.push_back(s2);
                PlanStep s3; s3.id = "debug_3"; s3.action = "validate_fix";
                s3.expected_outcome = "confirm regression eliminated"; steps.push_back(s3);
                break;
            }
            case Strategy::REFACTOR: {
                PlanStep s1; s1.id = "refactor_1"; s1.action = "extract_interfaces";
                s1.expected_outcome = "decouple tightly-coupled modules"; steps.push_back(s1);
                PlanStep s2; s2.id = "refactor_2"; s2.action = "migrate_to_new_abstraction";
                s2.expected_outcome = "new implementation passes unit tests"; steps.push_back(s2);
                break;
            }
            case Strategy::ENUMERATE: {
                PlanStep s1; s1.id = "enum_1"; s1.action = "generate_candidates";
                s1.expected_outcome = "produce N candidate solutions"; steps.push_back(s1);
                PlanStep s2; s2.id = "enum_2"; s2.action = "score_candidates";
                s2.expected_outcome = "rank by confidence and select best"; steps.push_back(s2);
                break;
            }
            default: {
                PlanStep s1; s1.id = "task_0"; s1.action = "analyze_workspace";
                s1.expected_outcome = "understand current state"; steps.push_back(s1);
                break;
            }
        }

        return steps;
    }

    StrategyPlan adaptStrategy(
        const StrategyPlan& current_strategy,
        const AttemptRecord& attempt) override {
        
        StrategyPlan adapted = current_strategy;
        
        if (!attempt.resolved) {
            adapted.iteration++;
            
            // Shift strategy if current one isn't working
            if (attempt.failure_type == FailureType::INVALID_PLAN) {
                if (current_strategy.strategy == Strategy::DEBUG) {
                    adapted.strategy = Strategy::REFACTOR;
                    adapted.rationale = "Debug approach failed, trying refactor";
                } else if (current_strategy.strategy == Strategy::REFACTOR) {
                    adapted.strategy = Strategy::ENUMERATE;
                    adapted.rationale = "Refactor failed, trying exhaustive search";
                }
            }
        }
        
        return adapted;
    }

    bool isInLoop(const std::vector<AttemptRecord>& history) override {
        if (history.size() < 3) return false;
        
        // Detect if same failure repeats 3+ times
        std::unordered_map<std::string, int> failure_counts;
        for (const auto& rec : history) {
            failure_counts[rec.diagnostic]++;
        }
        
        for (const auto& [diagnostic, count] : failure_counts) {
            if (count >= 3) return true;
        }
        
        return false;
    }
};

// ─── Default Termination Controller Implementation ──────────────────────────

class DefaultTerminationController : public ITerminationController {
public:
    TerminationState shouldTerminate(
        int workspace_issues_now,
        const std::vector<AttemptRecord>& history) override {
        
        TerminationState state;
        state.iteration = static_cast<int>(history.size());
        
        if (history.empty()) {
            state.reason = TerminationReason::SUCCESS;
            return state;
        }
        
        state.issues_before = history.front().workspace_issues_before;
        state.issues_after = workspace_issues_now;
        
        // Success: no issues remaining
        if (workspace_issues_now == 0) {
            state.reason = TerminationReason::SUCCESS;
            return state;
        }
        
        // No progress: issue count unchanged or increased
        if (state.issues_after >= state.issues_before) {
            state.reason = TerminationReason::NO_PROGRESS;
            state.diagnostic = "Issue count did not decrease; plan is ineffective";
            state.should_escalate = true;
            return state;
        }
        
        // Max retries exceeded
        if (history.size() >= 5) {
            state.reason = TerminationReason::MAX_RETRIES;
            state.diagnostic = "Reached maximum retry limit (5)";
            return state;
        }
        
        // Degraded state: more issues than before
        if (state.issues_after > state.issues_before) {
            state.reason = TerminationReason::DEGRADED_STATE;
            state.diagnostic = "Workspace state worse than initial";
            state.should_escalate = true;
            return state;
        }
        
        // Still working, continue
        state.reason = TerminationReason::SUCCESS;
        state.diagnostic = "Iteration " + std::to_string(state.iteration) + 
                           " completed; issues stable at " + std::to_string(state.issues_after);
        return state;
    }

    std::string explainTermination(const TerminationState& state) override {
        switch (state.reason) {
            case TerminationReason::SUCCESS:
                return "Autonomous loop completed successfully; all issues resolved.";
            case TerminationReason::NO_PROGRESS:
                return "No progress made after " + std::to_string(state.iteration) + 
                       " attempts. Issue count remained at " + std::to_string(state.issues_before);
            case TerminationReason::MAX_RETRIES:
                return "Maximum retry limit (" + std::to_string(state.iteration) + ") reached.";
            case TerminationReason::DEGRADED_STATE:
                return "Workspace state degraded from " + std::to_string(state.issues_before) + 
                       " to " + std::to_string(state.issues_after) + " issues.";
            case TerminationReason::HUMAN_REQUIRED:
                return "Human intervention required; autonomous approach exhausted.";
            case TerminationReason::UNRECOVERABLE:
                return "Unrecoverable error encountered.";
            case TerminationReason::STRATEGY_EXHAUSTED:
                return "All available strategies have been attempted without resolution.";
            default:
                return "Unknown termination reason.";
        }
    }
};

// ─── Factory Functions ──────────────────────────────────────────────────────

std::unique_ptr<IValidator> createDefaultValidator() {
    return std::make_unique<DefaultValidator>();
}

std::unique_ptr<IFailureClassifier> createDefaultClassifier() {
    return std::make_unique<DefaultFailureClassifier>();
}

std::unique_ptr<IAdaptivePlanner> createDefaultPlanner() {
    return std::make_unique<DefaultAdaptivePlanner>();
}

std::unique_ptr<ITerminationController> createDefaultTerminationController() {
    return std::make_unique<DefaultTerminationController>();
}

} // namespace Autonomous
} // namespace RawrXD
