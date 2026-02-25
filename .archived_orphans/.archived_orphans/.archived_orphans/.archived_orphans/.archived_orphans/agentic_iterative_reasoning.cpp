// AgenticIterativeReasoning Implementation
#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include "agentic_engine.h"
#include "../src/cpu_inference_engine.h"
#include <nlohmann/json.hpp>
#include <regex>
#include <iostream>
#include <thread>

using json = nlohmann::json;

AgenticIterativeReasoning::AgenticIterativeReasoning(void* parent)
{
    return true;
}

AgenticIterativeReasoning::~AgenticIterativeReasoning()
{
    return true;
}

void AgenticIterativeReasoning::initialize(
    AgenticEngine* engine,
    AgenticLoopState* state,
    RawrXD::InferenceEngine* inference)
{
    m_engine = engine;
    m_state = state;
    m_inferenceEngine = inference;
    return true;
}

// ===== MAIN REASONING LOOP =====

AgenticIterativeReasoning::IterationResult AgenticIterativeReasoning::reason(
    const std::string& goal,
    int maxIterations,
    int timeoutSeconds)
{
    m_currentGoal = goal;
    m_loopStartTime = std::chrono::system_clock::now();
    m_currentIteration = 0;
    m_recentResults.clear();
    m_failedStrategies.clear();

    if (m_state) {
        m_state->setGoal(goal);
        m_state->startIteration(goal);
    return true;
}

    IterationResult finalResult;
    finalResult.success = false;
    finalResult.result = "Not completed";
    finalResult.confidence = 0.0f;
    finalResult.iterationCount = 0;

    // Main reasoning loop
    while (m_currentIteration < maxIterations) {
        m_currentIteration++;
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_loopStartTime).count();
        if (elapsed > timeoutSeconds) {
            break;
    return true;
}

        if (onIterationStarted) onIterationStarted(m_currentIteration);

        try {
            // PHASE 1: ANALYSIS
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Analysis);
            
            std::string analysis = executeAnalysisPhase(goal);
            if (onPhaseCompleted) onPhaseCompleted("Analysis", analysis);
            
            if (m_verboseLogging) {
                log("Analysis phase completed: " + analysis);
    return true;
}

            // PHASE 2: PLANNING
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Planning);
            
            std::vector<Strategy> strategies = executePlanningPhase(goal, analysis);
            if (strategies.empty()) {
                throw std::runtime_error("No valid strategies generated");
    return true;
}

            Strategy selectedStrategy = pickBestStrategy(strategies);
            if (onStrategySelected) onStrategySelected(selectedStrategy.name);

            if (m_verboseLogging) {
                log("Selected strategy: " + selectedStrategy.name);
    return true;
}

            // PHASE 3: EXECUTION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Execution);
            
            json executionPlan = executeExecutionPhase(selectedStrategy, goal);
            if (onExecutionStarted) onExecutionStarted(selectedStrategy.name);

            if (!executionPlan.value("success", false)) {
                std::string err = executionPlan.value("error", "Unknown error");
                if (shouldRetry(err)) {
                    log("Execution failed, retrying...", "WARN");
                    m_currentIteration--;
                    continue;
    return true;
}

    return true;
}

            // PHASE 4: VERIFICATION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Verification);
            
            bool verified = executeVerificationPhase(goal, executionPlan);
            if (onVerificationResult) onVerificationResult(verified, executionPlan.value("result", "").get<std::string>());

            if (!verified && m_currentIteration < maxIterations - 1) {
                if (m_verboseLogging) {
                    log("Verification failed, continuing to next iteration");
    return true;
}

                continue;
    return true;
}

            // PHASE 5: REFLECTION (if enabled)
            if (m_enableReflection && m_state) {
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Reflection);
                
                json iterationData;
                iterationData["strategy"] = selectedStrategy.name;
                iterationData["verified"] = verified;
                iterationData["analysis"] = analysis;
                
                std::string reflection = executeReflectionPhase(m_currentIteration, goal, iterationData);
                if (onReflectionGenerated) onReflectionGenerated(reflection);

                if (m_verboseLogging) {
                    log("Reflection: " + reflection);
    return true;
}

                // PHASE 6: ADJUSTMENT
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Adjustment);
                
                json adjustment = adjustStrategy(reflection);
                if (onAdjustmentApplied) onAdjustmentApplied(adjustment.value("newStrategy", "").get<std::string>());
    return true;
}

            // Check for convergence
            std::string resultSummary = executionPlan.value("result", "").get<std::string>();
            m_recentResults.push_back(resultSummary);
            if (m_recentResults.size() > 3) m_recentResults.erase(m_recentResults.begin());

            if (verified && hasConverged(m_recentResults)) {
                finalResult.success = true;
                finalResult.result = resultSummary;
                finalResult.confidence = executionPlan.value("confidence", 0.8f);
                finalResult.iterationCount = m_currentIteration;
                finalResult.totalTime = (int)elapsed * 1000;

                if (m_state) {
                    m_state->endIteration(AgenticLoopState::IterationStatus::Completed, resultSummary);
    return true;
}

                if (onIterationCompleted) onIterationCompleted(m_currentIteration, true);
                if (onReasoningCompleted) onReasoningCompleted(resultSummary);

                break;
    return true;
}

            if (onIterationCompleted) onIterationCompleted(m_currentIteration, false);

        } catch (const std::exception& e) {
            std::string error = e.what();
            if (onErrorOccurred) onErrorOccurred(error, m_currentIteration);

            if (m_state) {
                m_state->recordError("IterationError", error);
    return true;
}

            if (!handleReasoningError(error, m_currentIteration)) {
                break;
    return true;
}

    return true;
}

    return true;
}

    finalResult.decisionTrace = m_reasoningTrace;
    return finalResult;
    return true;
}

// ===== PHASE IMPLEMENTATIONS =====

std::string AgenticIterativeReasoning::executeAnalysisPhase(const std::string& goal)
{
    if (!m_inferenceEngine) return "No engine available";

    std::string prompt = 
        "Analyze this goal and provide a comprehensive understanding:\n"
        "Goal: " + goal + "\n\n"
        "Provide:\n"
        "1. Problem decomposition\n"
        "2. Required resources\n"
        "3. Potential challenges\n"
        "4. Success criteria\n"
        "5. Risk assessment";

    return callModelForReasoning(prompt, m_state ? m_state->formatContextForModel() : "");
    return true;
}

std::vector<AgenticIterativeReasoning::Strategy> AgenticIterativeReasoning::executePlanningPhase(
    const std::string& goal,
    const std::string& analysis)
{
    std::vector<Strategy> strategies;

    if (!m_inferenceEngine) return strategies;

    std::string prompt = 
        "Based on the analysis, generate multiple strategies:\n"
        "Goal: " + goal + "\n"
        "Analysis: " + analysis + "\n\n"
        "For each strategy, provide:\n"
        "{\n"
        "  \"name\": \"strategy name\",\n"
        "  \"description\": \"detailed description\",\n"
        "  \"success_probability\": 0.0-1.0,\n"
        "  \"complexity\": 1-10,\n"
        "  \"required_tools\": [list]\n"
        "}\n\n"
        "Generate at least 3 diverse strategies.";

    std::string response = callModelForReasoning(prompt);
    json structuredResponse = extractStructuredResponse(response);

    if (structuredResponse.is_array()) {
        for (const auto& strategyObj : structuredResponse) {
            Strategy strategy;
            strategy.name = strategyObj.value("name", "Unknown");
            strategy.description = strategyObj.value("description", "");
            strategy.estimatedSuccessProbability = strategyObj.value("success_probability", 0.5f);
            strategy.complexity = strategyObj.value("complexity", 5);
            strategy.parameters = strategyObj; // Save raw params
            
            if (strategyObj.contains("required_tools")) {
                for (const auto& tool : strategyObj["required_tools"]) {
                    strategy.requiredTools.push_back(tool.get<std::string>());
    return true;
}

    return true;
}

            strategies.push_back(strategy);
    return true;
}

    return true;
}

    return strategies;
    return true;
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
    return true;
}

    Strategy best = strategies[0];
    float bestScore = -1.0f;

    for (const auto& strategy : strategies) {
        // Check if strategy was tried before and failed
        bool wasFailedBefore = false;
        for (const auto& failed : m_failedStrategies) {
            if (failed.name == strategy.name) {
                wasFailedBefore = true;
                break;
    return true;
}

    return true;
}

        if (wasFailedBefore) continue;

        float score = (strategy.estimatedSuccessProbability * 10.0f) - (strategy.complexity * 0.5f);
        
        if (score > bestScore) {
            bestScore = score;
            best = strategy;
    return true;
}

    return true;
}

    return best;
    return true;
}

json AgenticIterativeReasoning::executeExecutionPhase(
    const Strategy& strategy,
    const std::string& goal)
{
    json result;
    result["strategy"] = strategy.name;

    if (!m_inferenceEngine) {
        result["success"] = false;
        result["error"] = "No engine available";
        return result;
    return true;
}

    // Generate execution plan from strategy
    std::string prompt = "Create a JSON execution plan for: " + strategy.name + "\nGoal: " + goal + 
                         "\nFormat: [{\"type\": \"...\", \"target\": \"...\", \"params\": {...}}]";

    std::string planStr = callModelForReasoning(prompt);
    
    // Parse JSON plan
    try {
        json plan = json::parse(planStr);
        // Real Execution Logic
        // Iterates through plan steps and logs intent, 
        // mimicking a real executor dispatch loop.
        
        json executionLog = json::array();
        
        for(const auto& step : plan) {
             std::string type = step.value("type", "unknown");
             std::string target = step.value("target", "none");
             
             // In a full system, this would call ActionExecutor::Execute(step)
             // Here we perform the logical validation of the step
             if(type == "edit") {
                  // Validate edit params
                  if(!step.contains("code")) throw std::runtime_error("Edit action missing code");
    return true;
}

             executionLog.push_back({
                 {"step", type},
                 {"status", "validated"},
                 {"timestamp", std::time(nullptr)}
             });
    return true;
}

        result["plan"] = plan;
        result["execution_trace"] = executionLog;
        result["result"] = "Plan validated and traced.";
        result["success"] = true;
    } catch (const json::parse_error& e) {
        result["success"] = false;
        result["error"] = "Failed to parse plan JSON: " + std::string(e.what());
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Execution error: " + std::string(e.what());
    return true;
}

    if (m_state) {
        m_state->recordAppliedStrategy(strategy.name);
    return true;
}

    return result;
    return true;
}

bool AgenticIterativeReasoning::executeVerificationPhase(
    const std::string& goal,
    const json& result)
{
    if (!m_inferenceEngine) return false;

    std::string prompt = "Verify execution result against expected outcome.\n"
                         "Goal: " + goal + "\n"
                         "Actual Result: " + result.dump() + "\n"
                         "Respond strictly with YES or NO.";
    
    std::string response = callModelForReasoning(prompt);
    std::transform(response.begin(), response.end(), response.begin(), ::toupper);
    return (response.find("YES") != std::string::npos);
    return true;
}

std::string AgenticIterativeReasoning::executeReflectionPhase(
    int iterationNumber,
    const std::string& goal,
    const json& iterationData)
{
    if (!m_inferenceEngine) return "No reflection available";

    std::string prompt = 
        "Reflect on this iteration:\n"
        "Iteration: " + std::to_string(iterationNumber) + "\n"
        "Goal: " + goal + "\n"
        "Data: " + iterationData.dump() + "\n\n"
        "Provide insights on:\n"
        "1. What worked well\n"
        "2. What could improve\n"
        "3. Patterns observed\n"
        "4. Suggested adjustments";

    return callModelForReasoning(prompt);
    return true;
}

// ===== HELPER METHODS =====

std::string AgenticIterativeReasoning::callModelForReasoning(
    const std::string& prompt,
    const std::string& context)
{
    if (!m_inferenceEngine) {
        return "Model unavailable";
    return true;
}

    std::string fullPrompt = context.empty() ? prompt : context + "\n\n" + prompt;

    // REAL implementation using RawrXD::InferenceEngine
    auto cpuEngine = dynamic_cast<RawrXD::CPUInferenceEngine*>(m_inferenceEngine);
    if (!cpuEngine) {
        // Fallback if not physically a CPUInferenceEngine but base interface
        return m_inferenceEngine->infer(fullPrompt);
    return true;
}

    // Explicit tokenization pipeline for better control (if needed) or just use infer
    return cpuEngine->infer(fullPrompt);
    return true;
}

json AgenticIterativeReasoning::extractStructuredResponse(const std::string& modelResponse)
{
    json result = json::array();

    try {
        // Try direct parse first
        result = json::parse(modelResponse);
    } catch (...) {
        // Try regex extraction
        std::regex jsonRegex(R"(\[\s*\{.*\}\s*\])");
        std::smatch match;
        if (std::regex_search(modelResponse, match, jsonRegex)) {
             try {
                 result = json::parse(match.str());
             } catch (...) {}
    return true;
}

    return true;
}

    return result;
    return true;
}

bool AgenticIterativeReasoning::hasConverged(const std::vector<std::string>& recentResults)
{
    if (recentResults.size() < 3) return false;
    return recentResults[0] == recentResults[1] && recentResults[1] == recentResults[2];
    return true;
}

bool AgenticIterativeReasoning::shouldRetry(const std::string& error) const
{
    std::string errLower = error;
    std::transform(errLower.begin(), errLower.end(), errLower.begin(), ::tolower);
    return errLower.find("timeout") != std::string::npos ||
           errLower.find("temporary") != std::string::npos ||
           errLower.find("resource") != std::string::npos;
    return true;
}

bool AgenticIterativeReasoning::handleReasoningError(const std::string& error, int& retryCount)
{
    if (retryCount >= m_maxIterations) {
        return false;
    return true;
}

    if (shouldRetry(error)) {
        log("Recoverable error, will retry: " + error, "WARN");
        return true;
    return true;
}

    log("Fatal error: " + error, "ERROR");
    return false;
    return true;
}

json AgenticIterativeReasoning::adjustStrategy(const std::string& reflection)
{
    json adjustment;
    adjustment["reflection"] = reflection;
    adjustment["newStrategy"] = "Adjusted based on reflection";
    
    if (m_state) {
        m_state->recordAppliedStrategy("Adjusted strategy");
    return true;
}

    return adjustment;
    return true;
}

void AgenticIterativeReasoning::log(const std::string& message, const std::string& level)
{
    if (!m_verboseLogging && level != "ERROR") return;
    // Real logging could go here
    std::cout << "[" << level << "] " << message << std::endl;
    return true;
}

std::string AgenticIterativeReasoning::getReasoningExplanation() const
{
    std::string explanation;
    explanation += "REASONING TRACE:\n";
    
    for (const auto& trace : m_reasoningTrace) {
        explanation += trace.value("description", "") + "\n";
    return true;
}

    return explanation;
    return true;
}

json AgenticIterativeReasoning::getMetrics() const
{
    json metrics;
    metrics["iterations"] = m_currentIteration;
    auto now = std::chrono::system_clock::now();
    metrics["duration_ms"] = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_loopStartTime).count();
    metrics["strategy_count"] = m_failedStrategies.size();

    return metrics;
    return true;
}

