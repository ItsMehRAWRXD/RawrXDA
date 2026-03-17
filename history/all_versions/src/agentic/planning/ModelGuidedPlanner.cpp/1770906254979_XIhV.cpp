#include "ModelGuidedPlanner.hpp"
#include "../AgentOllamaClient.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <map>

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

    // Store the prompt for progressive token generation
    // The decoder will produce tokens from the model's planning vocabulary
    state.prompt = prompt;
    state.taskId = taskId;

    // Pre-tokenize the prompt to estimate generation budget
    // Rough estimate: ~1.3 tokens per word for English text
    size_t wordCount = 0;
    bool inWord = false;
    for (char c : prompt) {
        if (c == ' ' || c == '\n' || c == '\t') {
            inWord = false;
        } else if (!inWord) {
            inWord = true;
            wordCount++;
        }
    }
    state.estimatedTokenBudget = static_cast<uint32_t>(wordCount * 4); // 4x input as output budget
    if (state.estimatedTokenBudget < 256) state.estimatedTokenBudget = 256;
    if (state.estimatedTokenBudget > 8192) state.estimatedTokenBudget = 8192;

    activeDecoders_[state.decoderInstanceId] = state;

    return state;
}

bool ModelGuidedPlanner::getNextToken(StreamingDecoderState& state, std::string& token) {
    std::lock_guard<std::mutex> lock(plannerMutex_);

    if (state.isComplete) {
        return false;
    }

    auto it = activeDecoders_.find(state.decoderInstanceId);
    if (it == activeDecoders_.end()) {
        return false;
    }

    auto& decoder = it->second;

    // Check if we've exceeded the token budget
    if (decoder.totalTokensGenerated >= decoder.estimatedTokenBudget) {
        decoder.isComplete = true;
        state.isComplete = true;
        token = "<|end|>";
        return false;
    }

    // If we haven't generated output yet, call the real LLM now
    if (decoder.generatedOutput.empty() && decoder.currentTokenIndex == 0) {
        // Use AgentOllamaClient for real model-guided token generation
        try {
            OllamaConfig cfg;
            cfg.chat_model = "deepseek-r1:14b";
            cfg.timeout_ms = 30000;
            cfg.max_tokens = static_cast<int>(decoder.estimatedTokenBudget);
            cfg.temperature = 0.4f;

            AgentOllamaClient client(cfg);
            if (client.TestConnection()) {
                std::vector<ChatMessage> msgs;
                msgs.push_back({"system",
                    "You are a planning engine. Generate a structured execution plan. "
                    "Use this format for each step:\n"
                    "ACTION: description\n"
                    "CONFIDENCE: 0.XX\n"
                    "DURATION: seconds\n"
                    "RATIONALE: why this step\n"
                    "DEPENDS: step_id (if applicable)\n"
                    "\nOutput one step per block. End with COMPLETE."
                });
                msgs.push_back({"user", decoder.prompt});

                auto result = client.ChatSync(msgs);
                if (result.success && !result.response.empty()) {
                    decoder.generatedOutput = result.response;
                }
            }
        } catch (...) {
            // Ollama unavailable — fall through to heuristic tokens
        }

        // Fallback: generate structured plan tokens from prompt analysis
        if (decoder.generatedOutput.empty()) {
            std::ostringstream fallback;
            fallback << "ANALYZE: Scan codebase structure and identify targets\n";
            fallback << "CONFIDENCE: 0.85\n";
            fallback << "DURATION: 60\n";
            fallback << "RATIONALE: Understanding existing code topology is prerequisite\n";

            // Analyze prompt to generate contextual steps
            std::string lowerPrompt = decoder.prompt;
            std::transform(lowerPrompt.begin(), lowerPrompt.end(), lowerPrompt.begin(), ::tolower);

            if (lowerPrompt.find("implement") != std::string::npos ||
                lowerPrompt.find("create") != std::string::npos) {
                fallback << "IDENTIFY: Locate target files and insertion points\n";
                fallback << "CONFIDENCE: 0.80\n";
                fallback << "DURATION: 45\n";
                fallback << "DEPENDS: 0\n";
                fallback << "IMPLEMENT: Generate code changes incrementally\n";
                fallback << "CONFIDENCE: 0.75\n";
                fallback << "DURATION: 180\n";
                fallback << "DEPENDS: 1\n";
            }
            if (lowerPrompt.find("refactor") != std::string::npos) {
                fallback << "IDENTIFY: Map refactoring scope and dependencies\n";
                fallback << "CONFIDENCE: 0.78\n";
                fallback << "DURATION: 90\n";
                fallback << "DEPENDS: 0\n";
            }
            if (lowerPrompt.find("failed") != std::string::npos ||
                lowerPrompt.find("error") != std::string::npos) {
                fallback << "ANALYZE: Investigate failure root cause\n";
                fallback << "CONFIDENCE: 0.70\n";
                fallback << "DURATION: 120\n";
            }

            fallback << "VALIDATE: Verify correctness and run tests\n";
            fallback << "CONFIDENCE: 0.82\n";
            fallback << "DURATION: 120\n";
            fallback << "COMPLETE\n";

            decoder.generatedOutput = fallback.str();
        }
    }

    // Emit tokens from the generated output, word by word
    // Split generatedOutput into tokens on first call
    if (decoder.currentTokenIndex == 0 && !decoder.generatedOutput.empty()) {
        // Pre-split is not needed — we stream character-by-character or word-by-word
    }

    // Stream word-by-word from generatedOutput
    static thread_local std::vector<std::string> tokenizedOutput;
    if (decoder.currentTokenIndex == 0) {
        tokenizedOutput.clear();
        std::istringstream iss(decoder.generatedOutput);
        std::string word;
        while (iss >> word) {
            tokenizedOutput.push_back(word + " ");
        }
        // Add newlines as separate tokens
        // Retokenize: split on newlines too
        std::vector<std::string> refined;
        for (const auto& t : tokenizedOutput) {
            size_t nlPos = t.find('\n');
            if (nlPos != std::string::npos) {
                if (nlPos > 0) refined.push_back(t.substr(0, nlPos));
                refined.push_back("\n");
                if (nlPos + 1 < t.size()) refined.push_back(t.substr(nlPos + 1));
            } else {
                refined.push_back(t);
            }
        }
        tokenizedOutput = refined;
    }

    size_t tokenIdx = decoder.currentTokenIndex;
    if (tokenIdx < tokenizedOutput.size()) {
        token = tokenizedOutput[tokenIdx];
        if (token.find("COMPLETE") != std::string::npos ||
            token.find("<|end|>") != std::string::npos) {
            decoder.isComplete = true;
            state.isComplete = true;
            return false;
        }
    } else {
        decoder.isComplete = true;
        state.isComplete = true;
        return false;
    }

    decoder.currentTokenIndex++;
    decoder.totalTokensGenerated++;
    decoder.partialOutput += token;
    state.currentTokenIndex = decoder.currentTokenIndex;
    state.totalTokensGenerated = decoder.totalTokensGenerated;

    return true;
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
    // Phase-5 Swarm Orchestrator model-guided planning pipeline:
    // 1. Call real LLM via AgentOllamaClient for plan generation
    // 2. Parse structured plan from model output
    // 3. Validate and compute aggregate confidence

    auto planStartTime = std::chrono::steady_clock::now();

    // Attempt real LLM-driven plan generation
    try {
        OllamaConfig cfg;
        cfg.chat_model = "deepseek-r1:14b";
        cfg.timeout_ms = 30000;
        cfg.max_tokens = 2048;
        cfg.temperature = 0.4f;  // Lower temp for structured planning

        AgentOllamaClient client(cfg);
        if (client.TestConnection()) {
            std::vector<ChatMessage> msgs;
            msgs.push_back({"system",
                "You are a code planning engine for a C++20/Win32 IDE project. "
                "Generate a structured execution plan using this exact format:\n"
                "\nFor each step, write:\n"
                "KEYWORD: description\n"
                "CONFIDENCE: 0.XX (your confidence this step will succeed)\n"
                "DURATION: estimated_seconds\n"
                "RATIONALE: why this step is needed\n"
                "DEPENDS: step_number (0-based, if depends on prior step)\n"
                "\nValid KEYWORD values: ANALYZE, IDENTIFY, GENERATE, IMPLEMENT, VALIDATE, TEST, COMPLETE\n"
                "\nEnd the plan with: COMPLETE: plan finalized\n"
                "\nBe precise. Each step must be actionable and measurable."
            });
            msgs.push_back({"user", prompt});

            auto result = client.ChatSync(msgs);
            if (result.success && !result.response.empty()) {
                outPlan.modelUsed = cfg.chat_model + " (real-inference)";

                // Parse the model's structured output into PlanSteps
                parse_plan_from_model_output(result.response, outPlan);

                if (!outPlan.steps.empty()) {
                    auto planEndTime = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        planEndTime - planStartTime).count();
                    outPlan.planDescription = "LLM-generated plan (" +
                        std::to_string(elapsedMs) + "ms, " +
                        std::to_string(result.completion_tokens) + " tokens)";
                    return;  // Successfully generated via real LLM
                }
            }
        }
    } catch (...) {
        // Ollama unavailable — fall through to prompt-analysis heuristics
    }

    // Fallback: Prompt-analysis heuristic plan generation
    // Analyze the prompt to determine plan complexity and generate evidence-based steps
    outPlan.modelUsed = "heuristic-planner (offline)";

    bool hasCodeAnalysis = (prompt.find("analyz") != std::string::npos ||
                           prompt.find("scan") != std::string::npos);
    bool hasImplementation = (prompt.find("implement") != std::string::npos ||
                             prompt.find("create") != std::string::npos ||
                             prompt.find("build") != std::string::npos);
    bool hasRefactoring = (prompt.find("refactor") != std::string::npos ||
                          prompt.find("rewrite") != std::string::npos);
    bool hasFailure = (prompt.find("failed") != std::string::npos ||
                      prompt.find("error") != std::string::npos);
    bool hasTesting = (prompt.find("test") != std::string::npos ||
                      prompt.find("verify") != std::string::npos);

    // Count complexity indicators to scale confidence
    int complexityFactors = 0;
    if (hasCodeAnalysis) complexityFactors++;
    if (hasImplementation) complexityFactors++;
    if (hasRefactoring) complexityFactors++;
    if (hasFailure) complexityFactors++;
    if (hasTesting) complexityFactors++;

    // Base confidence decreases with complexity (more factors = harder task)
    float baseConfidence = std::max(0.60f, 0.92f - (complexityFactors * 0.05f));

    uint32_t stepId = 0;
    uint32_t totalSeconds = 0;
    float totalConfidence = 0.0f;

    // Step 1: Always start with analysis
    {
        PlanStep step;
        step.stepId = stepId++;
        step.action = PlanAction::ANALYZE_CODE;
        step.actionDescription = "Analyze codebase structure and dependencies";
        step.confidenceScore = baseConfidence;
        step.estimatedDurationSeconds = hasFailure ? 90 : 60;  // Longer for error investigation
        step.rationale = "Understanding existing code topology is prerequisite for all operations";
        outPlan.steps.push_back(step);
        totalSeconds += step.estimatedDurationSeconds;
        totalConfidence += step.confidenceScore;
    }

    // Step 2: Identify targets based on prompt analysis
    if (hasCodeAnalysis || hasImplementation || hasRefactoring) {
        PlanStep step;
        step.stepId = stepId++;
        step.action = PlanAction::IDENTIFY_TARGETS;
        step.actionDescription = "Identify files and symbols requiring modification";
        step.confidenceScore = baseConfidence - 0.05f;  // Slightly lower — depends on analysis
        step.estimatedDurationSeconds = 45;
        step.rationale = "Precise targeting prevents unnecessary file modifications";
        step.priorSteps.push_back(0); // Depends on analysis
        outPlan.steps.push_back(step);
        totalSeconds += step.estimatedDurationSeconds;
        totalConfidence += step.confidenceScore;
    }

    // Step 3: Generate implementation plan
    if (hasImplementation || hasRefactoring) {
        PlanStep step;
        step.stepId = stepId++;
        step.action = PlanAction::GENERATE_CODE;
        step.actionDescription = "Generate implementation changes for identified targets";
        // Confidence scaled by whether we're dealing with failure recovery
        step.confidenceScore = hasFailure ?
            std::max(0.55f, baseConfidence - 0.15f) :
            baseConfidence - 0.05f;
        step.estimatedDurationSeconds = hasRefactoring ? 240 : 180;
        step.rationale = hasFailure ?
            "Generating alternative approach to avoid previous failure modes" :
            "Producing optimized implementation following project conventions";
        step.priorSteps.push_back(stepId - 2); // Depends on identify
        outPlan.steps.push_back(step);
        totalSeconds += step.estimatedDurationSeconds;
        totalConfidence += step.confidenceScore;
    }

    // Step 4: Validation
    {
        PlanStep step;
        step.stepId = stepId++;
        step.action = PlanAction::VALIDATE;
        step.actionDescription = "Run build verification and test suite";
        step.confidenceScore = baseConfidence + 0.03f;  // Validation is mechanical, slightly higher
        step.estimatedDurationSeconds = hasTesting ? 180 : 120;
        step.rationale = "Automated validation ensures no regressions introduced";
        if (stepId > 1) step.priorSteps.push_back(stepId - 2);
        outPlan.steps.push_back(step);
        totalSeconds += step.estimatedDurationSeconds;
        totalConfidence += step.confidenceScore;
    }

    outPlan.estimatedTotalSeconds = totalSeconds;
    outPlan.overallConfidence = outPlan.steps.empty() ? 0.0f :
                                totalConfidence / outPlan.steps.size();
    outPlan.planDescription = "Heuristic plan (" +
        std::to_string(outPlan.steps.size()) + " steps, complexity=" +
        std::to_string(complexityFactors) + ")";
}

void ModelGuidedPlanner::parse_plan_from_model_output(const std::string& modelOutput,
                                                      ExecutionPlan& outPlan) {
    // Parse structured plan from model output
    // Expected format: lines starting with ACTION: description
    // Dependencies: DEPENDS: step_id, step_id
    // Confidence: CONFIDENCE: 0.XX

    std::istringstream stream(modelOutput);
    std::string line;
    uint32_t stepId = 0;

    static const std::map<std::string, PlanAction> actionMap = {
        {"ANALYZE", PlanAction::ANALYZE_CODE},
        {"IDENTIFY", PlanAction::IDENTIFY_TARGETS},
        {"GENERATE", PlanAction::GENERATE_CODE},
        {"IMPLEMENT", PlanAction::GENERATE_CODE},
        {"VALIDATE", PlanAction::VALIDATE},
        {"TEST", PlanAction::VALIDATE},
        {"COMPLETE", PlanAction::FINALIZE},
    };

    PlanStep currentStep;
    bool hasStep = false;

    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Check for action lines
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos && colonPos < 20) {
            std::string keyword = line.substr(0, colonPos);
            std::string value = (colonPos + 1 < line.size()) ?
                                line.substr(colonPos + 1) : "";
            // Trim value
            size_t vStart = value.find_first_not_of(" \t");
            if (vStart != std::string::npos) value = value.substr(vStart);

            if (keyword == "DEPENDS" || keyword == "REQUIRES") {
                // Parse dependency list
                std::istringstream depStream(value);
                std::string depStr;
                while (std::getline(depStream, depStr, ',')) {
                    size_t ds = depStr.find_first_not_of(" ");
                    if (ds != std::string::npos) {
                        try {
                            currentStep.priorSteps.push_back(
                                static_cast<uint32_t>(std::stoul(depStr.substr(ds))));
                        } catch (...) {}
                    }
                }
            } else if (keyword == "CONFIDENCE") {
                try {
                    currentStep.confidenceScore = std::stof(value);
                } catch (...) {
                    currentStep.confidenceScore = 0.80f;
                }
            } else if (keyword == "DURATION") {
                try {
                    currentStep.estimatedDurationSeconds = static_cast<uint32_t>(std::stoul(value));
                } catch (...) {
                    currentStep.estimatedDurationSeconds = 60;
                }
            } else if (keyword == "RATIONALE") {
                currentStep.rationale = value;
            } else {
                // New action step
                if (hasStep) {
                    outPlan.steps.push_back(currentStep);
                }

                currentStep = PlanStep();
                currentStep.stepId = stepId++;
                currentStep.actionDescription = value;
                currentStep.confidenceScore = 0.85f;
                currentStep.estimatedDurationSeconds = 60;

                auto actionIt = actionMap.find(keyword);
                if (actionIt != actionMap.end()) {
                    currentStep.action = actionIt->second;
                } else {
                    currentStep.action = PlanAction::ANALYZE_CODE;
                }

                hasStep = true;
            }
        }
    }

    // Push final step
    if (hasStep) {
        outPlan.steps.push_back(currentStep);
    }

    // Calculate aggregate metrics
    float totalConf = 0.0f;
    uint32_t totalDuration = 0;
    for (const auto& step : outPlan.steps) {
        totalConf += step.confidenceScore;
        totalDuration += step.estimatedDurationSeconds;
    }
    outPlan.estimatedTotalSeconds = totalDuration;
    outPlan.overallConfidence = outPlan.steps.empty() ? 0.0f :
                                totalConf / outPlan.steps.size();
}

}  // namespace RawrXD::Agentic::Planning
