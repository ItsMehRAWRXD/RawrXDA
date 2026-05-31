#pragma once

#include "autonomous_validation_layer.h"
#include <memory>
#include <vector>
#include <string>

namespace RawrXD {
namespace Autonomous {

/**
 * Orchestrator Integration Layer
 * 
 * This header defines the integration points for the validation layer
 * into the existing autonomous_intelligence_orchestrator.cpp loop.
 * 
 * Usage Pattern:
 * 
 * 1. Initialization (in orchestratorLoop):
 *    auto validator = createDefaultValidator();
 *    auto classifier = createDefaultClassifier();
 *    auto planner = createDefaultPlanner();
 *    auto controller = createDefaultTerminationController();
 * 
 * 2. Per-Goal Loop:
 *    - planStrategy() to get high-level approach
 *    - planTasks() to get concrete steps
 *    - For each step:
 *        - executeStep()
 *        - validateStep() → check PlanStep validations
 *        - If FAIL: classifyFailure() → determine retry strategy
 *        - Record in attemptHistory
 *    - canContinue() → check termination condition
 */

class OrchestratorIntegrationAdapter {
private:
    std::unique_ptr<IValidator> validator_;
    std::unique_ptr<IFailureClassifier> classifier_;
    std::unique_ptr<IAdaptivePlanner> planner_;
    std::unique_ptr<ITerminationController> terminator_;
    
    std::vector<AttemptRecord> attempt_history_;
    std::string current_workspace_root_;
    
public:
    OrchestratorIntegrationAdapter() 
        : validator_(createDefaultValidator()),
          classifier_(createDefaultClassifier()),
          planner_(createDefaultPlanner()),
          terminator_(createDefaultTerminationController()) {
    }

    // Phase 1: Plan strategy and tasks
    void initializeGoal(const std::string& goal, const std::string& workspace_root) {
        current_workspace_root_ = workspace_root;
        attempt_history_.clear();
        validator_->recordBaseline(workspace_root);
    }

    StrategyPlan getInitialStrategy(const std::string& goal) {
        return planner_->planStrategy(goal, attempt_history_);
    }

    std::vector<PlanStep> getTasksForStrategy(const StrategyPlan& strategy) {
        return planner_->planTasks(strategy);
    }

    // Phase 2: Execute and validate each step
    bool validateStepOutcome(
        const PlanStep& step,
        std::vector<std::string>& failures) {
        return validator_->validateStep(step, failures);
    }

    // Phase 3: Classify failure and decide retry strategy
    FailureType classifyStepFailure(
        const PlanStep& step,
        const std::string& error_output) {
        return classifier_->classifyFailure(step, error_output, "");
    }

    std::string getRetryStrategy(FailureType failure_type) {
        return classifier_->getRetryStrategy(failure_type);
    }

    // Phase 4: Record attempt with decision context
    void recordAttempt(
        const PlanStep& step,
        FailureType failure_type,
        const std::string& diagnostic,
        bool resolved,
        int workspace_issues_before,
        int workspace_issues_after) {
        
        AttemptRecord rec;
        rec.attempt_number = static_cast<int>(attempt_history_.size()) + 1;
        rec.step = step;
        rec.failure_type = failure_type;
        rec.diagnostic = diagnostic;
        rec.resolved = resolved;
        rec.workspace_issues_before = workspace_issues_before;
        rec.workspace_issues_after = workspace_issues_after;
        rec.elapsed_ms = 0;  // Would be filled in by orchestrator
        
        attempt_history_.push_back(rec);
    }

    // Phase 5: Adapt strategy if needed
    StrategyPlan adaptCurrentStrategy(const StrategyPlan& current) {
        if (attempt_history_.empty()) return current;
        return planner_->adaptStrategy(current, attempt_history_.back());
    }

    // Phase 6: Check termination condition
    bool shouldContinue(int current_workspace_issues) {
        auto term_state = terminator_->shouldTerminate(current_workspace_issues, attempt_history_);
        
        switch (term_state.reason) {
            case TerminationReason::SUCCESS:
                // If success but still issues, continue
                return current_workspace_issues > 0 && attempt_history_.size() < 5;
            case TerminationReason::NO_PROGRESS:
            case TerminationReason::DEGRADED_STATE:
            case TerminationReason::UNRECOVERABLE:
                return false;
            default:
                return attempt_history_.size() < 5;
        }
    }

    bool isInLoop() {
        return planner_->isInLoop(attempt_history_);
    }

    std::string getTerminationExplanation(int current_workspace_issues) {
        auto term_state = terminator_->shouldTerminate(current_workspace_issues, attempt_history_);
        return terminator_->explainTermination(term_state);
    }

    // Diagnostics
    const std::vector<AttemptRecord>& getAttemptHistory() const {
        return attempt_history_;
    }

    int getAttemptCount() const {
        return static_cast<int>(attempt_history_.size());
    }
};

} // namespace Autonomous
} // namespace RawrXD
