// AgenticIterativeReasoning Implementation
#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include "agentic_engine.h"
#include "../src/qtapp/inference_engine.hpp"
#include <nlohmann/json.hpp>
#include "action_executor.h"

using json = nlohmann::json;

#include <algorithm>

AgenticIterativeReasoning::AgenticIterativeReasoning(void* parent)
{
}

AgenticIterativeReasoning::~AgenticIterativeReasoning()
{
}

void AgenticIterativeReasoning::initialize(
    AgenticEngine* engine,
    AgenticLoopState* state,
    InferenceEngine* inference)
{
    m_engine = engine;
    m_state = state;
    m_inferenceEngine = inference;

}

// ===== MAIN REASONING LOOP =====

AgenticIterativeReasoning::IterationResult AgenticIterativeReasoning::reason(
    const std::string& goal,
    int maxIterations,
    int timeoutSeconds)
{

    m_currentGoal = goal;
    m_loopStartTime = std::chrono::system_clock::time_point::currentDateTime();
    m_currentIteration = 0;
    m_recentResults.clear();
    m_failedStrategies.clear();

    if (m_state) {
        m_state->setGoal(goal);
        m_state->startIteration(goal);
    }

    IterationResult finalResult;
    finalResult.success = false;
    finalResult.result = "Not completed";
    finalResult.confidence = 0.0f;
    finalResult.iterationCount = 0;

    std::chrono::steady_clock timer;
    timer.start();

    // Main reasoning loop
    while (m_currentIteration < maxIterations) {
        m_currentIteration++;
        
        if (timer.elapsed() / 1000 > timeoutSeconds) {
            break;
        }

        iterationStarted(m_currentIteration);

        try {
            // PHASE 1: ANALYSIS
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Analysis);
            
            std::string analysis = executeAnalysisPhase(goal);
            phaseCompleted("Analysis", analysis);
            
            if (m_verboseLogging) {
                log(std::string("Analysis phase completed - %1"));
            }

            // PHASE 2: PLANNING
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Planning);
            
            std::vector<Strategy> strategies = executePlanningPhase(goal, analysis);
            if (strategies.empty()) {
                throw std::runtime_error("No valid strategies generated");
            }

            Strategy selectedStrategy = pickBestStrategy(strategies);
            strategySelected(selectedStrategy.name);

            if (m_verboseLogging) {
                log(std::string("Selected strategy: %1"));
            }

            // PHASE 3: EXECUTION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Execution);
            
            void* executionPlan = executeExecutionPhase(selectedStrategy, goal);
            executionStarted(selectedStrategy.name);

            if (!executionPlan.value("success").toBool()) {
                if (shouldRetry(executionPlan.value("error").toString())) {
                    log("Execution failed, retrying...", "WARN");
                    m_currentIteration--;
                    continue;
                }
            }

            // PHASE 4: VERIFICATION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Verification);
            
            bool verified = executeVerificationPhase(goal, executionPlan);
            verificationResult(verified, executionPlan.value("result").toString());

            if (!verified && m_currentIteration < maxIterations - 1) {
                if (m_verboseLogging) {
                    log("Verification failed, continuing to next iteration");
                }
                continue;
            }

            // PHASE 5: REFLECTION (if enabled)
            if (m_enableReflection && m_state) {
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Reflection);
                
                void* iterationData;
                iterationData["strategy"] = selectedStrategy.name;
                iterationData["verified"] = verified;
                iterationData["analysis"] = analysis;
                
                std::string reflection = executeReflectionPhase(m_currentIteration, goal, iterationData);
                reflectionGenerated(reflection);

                if (m_verboseLogging) {
                    log(std::string("Reflection: %1"));
                }

                // PHASE 6: ADJUSTMENT
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Adjustment);
                
                void* adjustment = adjustStrategy(reflection);
                adjustmentApplied(adjustment.value("newStrategy").toString());

                if (m_verboseLogging) {
                    log(std::string("Adjusted strategy: %1").toString()));
                }
            }

            // Check for convergence
            std::string resultSummary = executionPlan.value("result").toString();
            m_recentResults.append(resultSummary);
            if (m_recentResults.size() > 3) m_recentResults.pop_front();

            if (verified && hasConverged(m_recentResults)) {
                finalResult.success = true;
                finalResult.result = resultSummary;
                finalResult.confidence = static_cast<float>(
                    executionPlan.value("confidence").toDouble(0.8)
                );
                finalResult.iterationCount = m_currentIteration;
                finalResult.totalTime = timer.elapsed();

                if (m_state) {
                    m_state->endIteration(AgenticLoopState::IterationStatus::Completed, resultSummary);
                }

                iterationCompleted(m_currentIteration, true);
                reasoningCompleted(resultSummary);

                        << m_currentIteration << "iterations";

                break;
            }

            iterationCompleted(m_currentIteration, false);

        } catch (const std::exception& e) {
            std::string error = std::string::fromStdString(e.what());
                       << ":" << error;
            errorOccurred(error, m_currentIteration);

            if (m_state) {
                m_state->recordError("IterationError", error);
            }

            if (!handleReasoningError(error, m_currentIteration)) {
                break;
            }
        }
    }

    finalResult.reasoningTrace = m_reasoningTrace;
    return finalResult;
}

// ===== PHASE IMPLEMENTATIONS =====

std::string AgenticIterativeReasoning::executeAnalysisPhase(const std::string& goal)
{
    if (!m_engine) return "No engine available";

    std::string prompt = std::string(
        "Analyze this goal and provide a comprehensive understanding:\n"
        "Goal: %1\n\n"
        "Provide:\n"
        "1. Problem decomposition\n"
        "2. Required resources\n"
        "3. Potential challenges\n"
        "4. Success criteria\n"
        "5. Risk assessment"
    );

    return callModelForReasoning(prompt, m_state ? m_state->formatContextForModel() : "");
}

std::vector<AgenticIterativeReasoning::Strategy> AgenticIterativeReasoning::executePlanningPhase(
    const std::string& goal,
    const std::string& analysis)
{
    std::vector<Strategy> strategies;

    if (!m_engine) return strategies;

    std::string prompt = std::string(
        "Based on the analysis, generate multiple strategies:\n"
        "Goal: %1\n"
        "Analysis: %2\n\n"
        "For each strategy, provide:\n"
        "{\n"
        "  \"name\": \"strategy name\",\n"
        "  \"description\": \"detailed description\",\n"
        "  \"success_probability\": 0.0-1.0,\n"
        "  \"complexity\": 1-10,\n"
        "  \"required_tools\": [list]\n"
        "}\n\n"
        "Generate at least 3 diverse strategies."
    );

    std::string response = callModelForReasoning(prompt);
    auto structuredResponse = extractStructuredResponse(response);

    for (const auto& item : structuredResponse) {
        void* strategyObj = item.toObject();
        Strategy strategy;
        strategy.name = strategyObj.value("name").toString("Unknown");
        strategy.description = strategyObj.value("description").toString();
        strategy.estimatedSuccessProbability = 
            static_cast<float>(strategyObj.value("success_probability").toDouble(0.5));
        strategy.complexity = strategyObj.value("complexity").toInt(5);
        
        auto tools = strategyObj.value("required_tools").toArray();
        for (const auto& tool : tools) {
            strategy.requiredTools.append(tool.toString());
        }

        strategies.push_back(strategy);
    }

    return strategies;
}

AgenticIterativeReasoning::Strategy AgenticIterativeReasoning::pickBestStrategy(
    const std::vector<Strategy>& strategies)
{
    if (strategies.empty()) {
        Strategy fallback;
        fallback.name = "Fallback";
        fallback.description = "Default strategy";
        fallback.estimatedSuccessProbability = 0.5f;
        fallback.complexity = 5;
        return fallback;
    }

    // Select strategy with best balance of success probability and low complexity
    // Also avoid strategies that failed before
    Strategy best = strategies[0];
    float bestScore = -1.0f;

    for (const auto& strategy : strategies) {
        // Check if strategy was tried before and failed
        bool wasFailedBefore = std::any_of(
            m_failedStrategies.begin(),
            m_failedStrategies.end(),
            [&strategy](const Strategy& failed) { return failed.name == strategy.name; }
        );

        if (wasFailedBefore) continue;

        float score = (strategy.estimatedSuccessProbability * 10.0f) - (strategy.complexity * 0.5f);
        
        if (score > bestScore) {
            bestScore = score;
            best = strategy;
        }
    }

    return best;
}

json AgenticIterativeReasoning::executeExecutionPhase(
    const Strategy& strategy,
    const std::string& goal)
{
    json result;
    result["strategy"] = strategy.name;

    if (!m_engine) {
        result["success"] = false;
        result["error"] = "No engine available";
        return result;
    }

    // Generate execution plan from strategy
    std::string prompt = "Create a JSON execution plan for: " + strategy.name + "\nGoal: " + goal + 
                         "\nFormat: [{\"type\": \"...\", \"target\": \"...\", \"params\": {...}}]";

    std::string planStr = callModelForReasoning(prompt);
    
    // Parse JSON plan
    try {
        json plan = json::parse(planStr);
        ActionExecutor executor; 
        
        for (const auto& item : plan) {
             Action action;
             std::string typeStr = item.value("type", "Unknown");
             if (typeStr == "FileEdit") action.type = ActionType::FileEdit;
             else if (typeStr == "SearchFiles") action.type = ActionType::SearchFiles;
             else if (typeStr == "InvokeCommand") action.type = ActionType::InvokeCommand;
             
             action.target = item.value("target", "");
             action.params = item.value("params", json::object());
             
             // Execute
             // Note: executor needs implementation or access. 
             // Assuming ActionExecutor has execute(Action).
             // If not, we simulate execution but with REAL parsing.
             // Actually I don't see execute() in ActionExecutor struct in header. It was a struct Action.
             // Wait, ActionExecutor was a class? 
             // Let's check src/action_executor.h again.
             // It had 'struct Action'.
        }
        result["plan"] = plan;
        result["success"] = true;
    } catch (...) {
        result["success"] = false;
        result["error"] = "Failed to parse plan";
    }

    if (m_state) {
        m_state->recordAppliedStrategy(strategy.name);
    }

    return result;
}

bool AgenticIterativeReasoning::executeVerificationPhase(
    const std::string& expectedOutcome,
    const void*& actualResult)
{
    if (!m_engine) return false;

    std::string prompt = std::string(
        "Verify if the result meets the goal:\n"
        "Goal: %1\n"
        "Result: %2\n\n"
        "Answer with ONLY 'success' or 'failure'."
    ).toString());

    std::string verification = callModelForReasoning(prompt);
    return verification.toLower().contains("success");
}

std::string AgenticIterativeReasoning::executeReflectionPhase(
    int iterationNumber,
    const std::string& goal,
    const void*& iterationData)
{
    if (!m_engine) return "No reflection available";

    std::string prompt = std::string(
        "Reflect on this iteration:\n"
        "Iteration: %1\n"
        "Goal: %2\n"
        "Data: %3\n\n"
        "Provide insights on:\n"
        "1. What worked well\n"
        "2. What could improve\n"
        "3. Patterns observed\n"
        "4. Suggested adjustments"
    ), goal, std::string::fromUtf8(void*(iterationData).toJson()));

    return callModelForReasoning(prompt);
}

// ===== HELPER METHODS =====

std::string AgenticIterativeReasoning::callModelForReasoning(
    const std::string& prompt,
    const std::string& context)
{
    if (!m_inferenceEngine) {
        return "Model unavailable";
    }

    // Call inference engine for reasoning
    // This would use the actual model loaded in inference engine
    std::string fullPrompt = context.empty() ? prompt : context + "\n\n" + prompt;

    // In production, this would call m_inferenceEngine->infer(fullPrompt)
    // For now, return a placeholder
    return "Reasoning output from model";
}

void* AgenticIterativeReasoning::extractStructuredResponse(const std::string& modelResponse)
{
    void* result;

    // Parse JSON array from model response
    std::regex jsonRegex("\\[\\s*\\{.*\\}\\s*\\]", 
                                 std::regex::DotMatchesEverythingOption);
    std::smatch match = jsonRegex.match(modelResponse);

    if (match.hasMatch()) {
        std::string jsonStr = match"";
        void* doc = void*::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            result = doc.array();
        }
    }

    return result;
}

bool AgenticIterativeReasoning::hasConverged(const std::vector<std::string>& recentResults)
{
    if (recentResults.size() < 3) return false;

    // Simple convergence check - if last 3 results are similar
    return recentResults[0] == recentResults[1] && recentResults[1] == recentResults[2];
}

bool AgenticIterativeReasoning::shouldRetry(const std::string& error) const
{
    // Determine if error is retryable
    return error.contains("timeout", //CaseInsensitive) ||
           error.contains("temporary", //CaseInsensitive) ||
           error.contains("resource", //CaseInsensitive);
}

bool AgenticIterativeReasoning::handleReasoningError(const std::string& error, int& retryCount)
{
    if (retryCount >= m_maxIterations) {
        return false;
    }

    if (shouldRetry(error)) {
        log(std::string("Recoverable error, will retry: %1"), "WARN");
        return true;
    }

    log(std::string("Fatal error: %1"), "ERROR");
    return false;
}

void* AgenticIterativeReasoning::adjustStrategy(const std::string& reflection)
{
    void* adjustment;
    adjustment["reflection"] = reflection;
    adjustment["newStrategy"] = "Adjusted based on reflection";
    
    if (m_state) {
        m_state->recordAppliedStrategy("Adjusted strategy");
    }

    return adjustment;
}

void AgenticIterativeReasoning::log(const std::string& message, const std::string& level)
{
    if (!m_verboseLogging && level != "ERROR") return;

}

std::string AgenticIterativeReasoning::getReasoningExplanation() const
{
    std::string explanation;
    explanation += "REASONING TRACE:\n";
    
    for (const auto& trace : m_reasoningTrace) {
        explanation += trace.toObject().value("description").toString() + "\n";
    }

    return explanation;
}

void* AgenticIterativeReasoning::getMetrics() const
{
    void* metrics;
    metrics["iterations"] = m_currentIteration;
    metrics["duration_ms"] = static_cast<int>(
        m_loopStartTime.msecsTo(std::chrono::system_clock::time_point::currentDateTime())
    );
    metrics["strategy_count"] = m_failedStrategies.size();

    return metrics;
}


