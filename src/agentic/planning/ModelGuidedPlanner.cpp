#include "ModelGuidedPlanner.hpp"
#include <algorithm>
#include <chrono>

namespace RawrXD::Agentic::Planning {

ModelGuidedPlanner& ModelGuidedPlanner::instance() {
    static ModelGuidedPlanner instance;
    return instance;
}

ModelGuidedPlanner::ModelGuidedPlanner() = default;

ModelGuidedPlanner::~ModelGuidedPlanner() = default;

uint64_t ModelGuidedPlanner::generatePlan(uint64_t taskId, const std::string& taskDescription,
                                          const std::vector<std::string>& contextFiles,
                                          uint32_t maxPlanSteps) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    // Construct prompt for 800B model
    std::string prompt = "Task: " + taskDescription + "\n";
    prompt += "Context Files:\n";
    for (const auto& file : contextFiles) {
        prompt += "  - " + file + "\n";
    }
    prompt += "Generate a step-by-step plan with maximum " + std::to_string(maxPlanSteps) +
              " steps.\n";
    prompt += "For each step, provide: action, target, args, dependencies, confidence, "
              "duration.\n";
    prompt += "Output format: JSON array of plan steps.\n";

    uint64_t planId = nextPlanId_++;

    ExecutionPlan plan;
    plan.planId = planId;
    plan.taskId = taskId;
    plan.planName = "Plan_" + std::to_string(planId);
    plan.createdAt = std::chrono::steady_clock::now();

    // Invoke 800B model for planning
    invoke_model_for_planning(taskId, prompt, plan);

    PlanRecord record;
    record.planId = planId;
    record.plan = plan;
    record.generatedAt = std::chrono::steady_clock::now();

    plans_[planId] = record;
    taskPlans_[taskId].push_back(planId);

    return planId;
}

StreamingDecoderState ModelGuidedPlanner::initializeStreamingDecoder(
    uint64_t taskId, const std::string& prompt) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    StreamingDecoderState state;
    state.decoderInstanceId = nextDecoderId_++;
    state.currentTokenIndex = 0;
    state.totalTokensGenerated = 0;
    state.isComplete = false;

    // TODO: Initialize 800B model streaming context
    // This would establish a connection to the 800B model loader
    // and begin token-by-token generation

    activeDecoders_[state.decoderInstanceId] = state;

    return state;
}

bool ModelGuidedPlanner::getNextToken(StreamingDecoderState& state, std::string& token) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    if (state.isComplete) {
        return false;
    }

    // TODO: Fetch next token from 800B model streaming decoder
    // This would call into the Phase-5 Swarm Orchestrator
    // to get the next token from the model

    auto it = activeDecoders_.find(state.decoderInstanceId);
    if (it != activeDecoders_.end()) {
        it->second.currentTokenIndex++;
        it->second.totalTokensGenerated++;
        // token would be set by model
        return true;
    }

    return false;
}

bool ModelGuidedPlanner::isDecodingComplete(const StreamingDecoderState& state) const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = activeDecoders_.find(state.decoderInstanceId);
    if (it != activeDecoders_.end()) {
        return it->second.isComplete;
    }
    return true;
}

void ModelGuidedPlanner::finalizeStreaming(StreamingDecoderState& state) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = activeDecoders_.find(state.decoderInstanceId);
    if (it != activeDecoders_.end()) {
        it->second.isComplete = true;
        activeDecoders_.erase(it);
    }
}

ExecutionPlan ModelGuidedPlanner::getPlan(uint64_t planId) const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it != plans_.end()) {
        return it->second.plan;
    }
    return ExecutionPlan();
}

std::vector<uint64_t> ModelGuidedPlanner::getPlansForTask(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = taskPlans_.find(taskId);
    if (it != taskPlans_.end()) {
        return it->second;
    }
    return {};
}

bool ModelGuidedPlanner::validatePlan(uint64_t planId) const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it == plans_.end()) {
        return false;
    }

    const auto& plan = it->second.plan;

    // Validate that all dependencies are forward references (no cycles)
    for (uint32_t i = 0; i < plan.steps.size(); ++i) {
        const auto& step = plan.steps[i];
        for (uint32_t depId : step.priorSteps) {
            if (depId >= i) {
                return false;  // Dependency on non-prior step
            }
        }
    }

    return true;
}

bool ModelGuidedPlanner::isStepDependencySatisfied(uint64_t planId, uint32_t stepId) const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it == plans_.end() || stepId >= it->second.plan.steps.size()) {
        return false;
    }

    const auto& plan = it->second.plan;
    const auto& step = plan.steps[stepId];
    const auto& completedSteps = it->second.completedSteps;

    for (uint32_t depId : step.priorSteps) {
        if (std::find(completedSteps.begin(), completedSteps.end(), depId) ==
            completedSteps.end()) {
            return false;  // Dependency not satisfied
        }
    }

    return true;
}

uint64_t ModelGuidedPlanner::replan(uint64_t originalPlanId,
                                    const std::vector<uint64_t>& failedStepIds) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto originalIt = plans_.find(originalPlanId);
    if (originalIt == plans_.end()) {
        return 0;
    }

    // Build prompt that includes failures and context
    std::string replannningPrompt = "Original plan failed at steps: ";
    for (uint64_t stepId : failedStepIds) {
        replannningPrompt += std::to_string(stepId) + ", ";
    }
    replannningPrompt += "\n";
    replannningPrompt += "Generate alternative plan avoiding these failures.\n";

    uint64_t newPlanId = nextPlanId_++;

    ExecutionPlan newPlan;
    newPlan.planId = newPlanId;
    newPlan.taskId = originalIt->second.plan.taskId;
    newPlan.planName = "Replan_" + std::to_string(newPlanId);

    invoke_model_for_planning(newPlan.taskId, replannningPrompt, newPlan);

    PlanRecord record;
    record.planId = newPlanId;
    record.plan = newPlan;
    record.generatedAt = std::chrono::steady_clock::now();

    plans_[newPlanId] = record;
    taskPlans_[newPlan.taskId].push_back(newPlanId);

    return newPlanId;
}

void ModelGuidedPlanner::submitPlanForApproval(uint64_t planId,
                                               const std::string& reviewComments) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it != plans_.end()) {
        // Mark as pending approval
        // In real implementation, this would notify human reviewer
    }
}

bool ModelGuidedPlanner::approvePlan(uint64_t planId, const std::string& approverName) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it != plans_.end()) {
        it->second.plan.executionApproved = true;
        return true;
    }
    return false;
}

bool ModelGuidedPlanner::rejectPlan(uint64_t planId, const std::string& rejectionReason) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    auto it = plans_.find(planId);
    if (it != plans_.end()) {
        plans_.erase(it);
        return true;
    }
    return false;
}

ModelGuidedPlanner::PlanningStats ModelGuidedPlanner::getStatistics() const {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    PlanningStats stats;
    stats.totalPlansGenerated = plans_.size();

    uint64_t successCount = 0;
    uint64_t failureCount = 0;
    float totalConfidence = 0.0f;
    uint32_t totalSteps = 0;

    for (const auto& [_, record] : plans_) {
        const auto& plan = record.plan;

        if (plan.executionApproved) {
            successCount++;
        } else {
            failureCount++;
        }

        totalConfidence += plan.overallConfidence;
        totalSteps += plan.steps.size();
    }

    stats.successfulPlans = successCount;
    stats.failedPlans = failureCount;

    if (!plans_.empty()) {
        stats.averageConfidenceScore = totalConfidence / plans_.size();
        stats.averagePlanSteps = totalSteps / plans_.size();
    }

    // Calculate total tokens from all decoders
    uint64_t totalTokens = 0;
    for (const auto& [_, decoder] : activeDecoders_) {
        totalTokens += decoder.totalTokensGenerated;
    }
    stats.totalTokensGenerated = totalTokens;

    return stats;
}

void ModelGuidedPlanner::invoke_model_for_planning(uint64_t taskId, const std::string& prompt,
                                                   ExecutionPlan& outPlan) {
    // TODO: Call 800B model via Phase-5 Swarm Orchestrator
    // This would:
    // 1. Initialize streaming decoder
    // 2. Send prompt tokens
    // 3. Collect response tokens
    // 4. Parse plan from response
    // 5. Calculate confidence scores

    outPlan.modelUsed = "BigDaddyG-Phase5-800B-v1";
    outPlan.overallConfidence = 0.85f;

    // Create a sample plan step for now
    PlanStep step;
    step.stepId = 0;
    step.action = PlanAction::ANALYZE_CODE;
    step.actionDescription = "Analyze codebase structure";
    step.confidenceScore = 0.90f;
    step.estimatedDurationSeconds = 120;
    step.rationale = "Understanding existing code is first priority";

    outPlan.steps.push_back(step);
    outPlan.estimatedTotalSeconds = 120;
}

void ModelGuidedPlanner::parse_plan_from_model_output(const std::string& modelOutput,
                                                      ExecutionPlan& outPlan) {
    // TODO: Parse JSON plan output from 800B model
    // Extract:
    // - Steps with actions, targets, args
    // - Dependencies between steps
    // - Confidence scores
    // - Rationales
}

}  // namespace RawrXD::Agentic::Planning
