#include "ModelGuidedPlanner.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include "../../cpu_inference_engine.h"
#include "../../utils/InferenceSettingsManager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD::Agentic::Planning {

// Shared engine instance for the planner to avoid reloading models
static std::unique_ptr<RawrXD::CPUInferenceEngine> g_plannerEngine;
static std::mutex g_plannerEngineMutex;

// Helper to get or initialize the engine
static RawrXD::CPUInferenceEngine* GetPlannerEngine() {
    std::lock_guard<std::mutex> lock(g_plannerEngineMutex);
    if (!g_plannerEngine) {
        g_plannerEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }
    
    // Check if loaded, see note in implementation about optimization
    auto& settings = RawrXD::InferenceSettingsManager::getInstance();
    std::string modelPath = settings.getCurrentModelPath();
    
    if (modelPath.empty()) return nullptr;

    static std::string loadedPath;
    if (loadedPath != modelPath) {
        if (g_plannerEngine->LoadModel(modelPath)) {
            loadedPath = modelPath;
        } else {
            return nullptr;
        }
    }
    
    return g_plannerEngine.get();
}

ModelGuidedPlanner& ModelGuidedPlanner::instance() {
    static ModelGuidedPlanner instance;
    return instance;
}

ModelGuidedPlanner::ModelGuidedPlanner() {
    inferenceEngine_ = std::make_unique<RawrXD::CPUInferenceEngine>();
}

ModelGuidedPlanner::~ModelGuidedPlanner() = default;

void ModelGuidedPlanner::ensureModelLoaded() {
    auto& settings = RawrXD::InferenceSettingsManager::getInstance();
    std::string modelPath = settings.getCurrentModelPath();

    if (modelPath != currentLoadedModelPath_) {
        if (!modelPath.empty()) {
            if (inferenceEngine_->LoadModel(modelPath)) {
                currentLoadedModelPath_ = modelPath;
            } else {
                // Failed to load, clear path so we retry or stay in invalid state
                currentLoadedModelPath_.clear();
            }
        } else {
             currentLoadedModelPath_.clear();
        }
    }
}

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

    // Ensure model is ready
    ensureModelLoaded();
    
    if (currentLoadedModelPath_.empty()) {
        state.isComplete = true;
        state.partialOutput = "Error: No model loaded or model load failed.";
    } else {
        // Model is ready for streaming
        state.isComplete = false;
        // Optionally seed the context with the prompt
        state.partialOutput = prompt; 
    }

    activeDecoders_[state.decoderInstanceId] = state;

    return state;
}

bool ModelGuidedPlanner::getNextToken(StreamingDecoderState& state, std::string& token) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    if (state.isComplete) {
        return false;
    }

    ensureModelLoaded();

    if (currentLoadedModelPath_.empty()) {
        state.isComplete = true;
        return false;
    }

    try {
        // Use the last 500 chars of context to save compute/stay in window
        std::string context = state.partialOutput;
        if (context.length() > 2000) context = context.substr(context.length() - 2000); 
        
        // Use the persistent engine
        // Generate with minimal tokens (1 step)
        std::string generated = inferenceEngine_->infer(context, 1);
        
        if (!generated.empty()) {
            token = generated;
            state.currentTokenIndex++;
            state.partialOutput += token;
            state.totalTokensGenerated++;
            
            // Stop condition
            if (state.totalTokensGenerated > 2048 || generated.find("}]") != std::string::npos) {
                state.isComplete = true;
            }
            return true;
        }
        
        // If empty result from inference, we are done
        state.isComplete = true;
        return false;

    } catch (...) {
        state.isComplete = true;
        return false;
    }
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
    }

    stats.successfulPlans = successCount;
    stats.failedPlans = failureCount;
    stats.averageConfidenceScore = (stats.totalPlansGenerated > 0) ? (totalConfidence / stats.totalPlansGenerated) : 0.0f;
    stats.averagePlanSteps = (stats.totalPlansGenerated > 0) ? (totalSteps / stats.totalPlansGenerated) : 0;
    
    return stats;
}

void ModelGuidedPlanner::invoke_model_for_planning(uint64_t taskId, const std::string& prompt,
                                                   ExecutionPlan& outPlan) {
    auto& settings = RawrXD::InferenceSettingsManager::getInstance();
    std::string modelPath = settings.getCurrentModelPath();
    std::string response;

    outPlan.modelUsed = modelPath.empty() ? "Fallback-Internal" : modelPath;

    if (!modelPath.empty()) {
        try {
            RawrXD::CPUInferenceEngine* engine = GetPlannerEngine();
            if (engine) {
                 response = engine->infer(prompt + "\nResponse format: JSON array of steps.");
            } else {
                 response = "[{\"stepId\":0,\"action\":0,\"actionDescription\":\"Model load failed\",\"confidenceScore\":0.0}]";
            }
        } catch (...) {
             response = "[{\"stepId\":0,\"action\":0,\"actionDescription\":\"Inference exception\",\"confidenceScore\":0.0}]";
        }
    } else {
        response = "[{\"stepId\":0,\"action\":0,\"actionDescription\":\"Analyze codebase structure (Default)\",\"confidenceScore\":1.0}]";
    }

    parse_plan_from_model_output(response, outPlan);
    
    if (!outPlan.steps.empty()) {
        float sum = 0;
        for(auto& s : outPlan.steps) sum += s.confidenceScore;
        outPlan.overallConfidence = sum / outPlan.steps.size();
    }
}

void ModelGuidedPlanner::parse_plan_from_model_output(const std::string& modelOutput,
                                                      ExecutionPlan& outPlan) {
    try {
        std::string jsonStr = modelOutput;
        size_t start = jsonStr.find("[");
        size_t end = jsonStr.rfind("]");
        if (start != std::string::npos && end != std::string::npos) {
            jsonStr = jsonStr.substr(start, end - start + 1);
        }

        auto j = json::parse(jsonStr);
        if (j.is_array()) {
            for (const auto& item : j) {
                PlanStep step;
                step.stepId = item.value("stepId", 0);
                step.action = static_cast<PlanAction>(int(item.value("action", 0)));
                step.actionDescription = item.value("actionDescription", "Unknown Action");
                step.targetResource = item.value("targetResource", "");
                
                if (item.contains("args") && item["args"].is_array()) {
                    for(const auto& arg : item["args"]) {
                        if(arg.is_string()) step.args.push_back(arg.get<std::string>());
                    }
                }
                
                step.confidenceScore = item.value("confidenceScore", 0.5f);
                step.rationale = item.value("rationale", "");
                
                outPlan.steps.push_back(step);
            }
        }
    } catch (const std::exception& e) {
        PlanStep errorStep;
        errorStep.actionDescription = std::string("Parse Error: ") + e.what();
        outPlan.steps.push_back(errorStep);
    }
}

}  // namespace RawrXD::Agentic::Planning
