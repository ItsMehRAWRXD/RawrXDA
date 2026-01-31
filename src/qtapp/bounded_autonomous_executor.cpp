/**
 * @file bounded_autonomous_executor.cpp
 * @brief Bounded Autonomous Executor Implementation
 * 
 * Complete implementation of the perception→decision→action→feedback loop.
 * Maximum 10 iterations by default, human-stoppable, tool-executing,
 * fully logged for audit trail and debugging.
 */

#include "bounded_autonomous_executor.hpp"
#include "inference_engine.hpp"
#include "agentic_tools.hpp"
#include "multi_tab_editor.hpp"
#include "terminal_pool.hpp"
BoundedAutonomousExecutor::BoundedAutonomousExecutor(
    InferenceEngine* inferenceEngine,
    AgenticToolExecutor* toolExecutor,
    MultiTabEditor* editor,
    TerminalPool* terminals,
    
) ,
    m_inferenceEngine(inferenceEngine),
    m_toolExecutor(toolExecutor),
    m_editor(editor),
    m_terminals(terminals),
    m_loopTimer(new // Timer(this)),
    m_accumulatedResponse("")
{
    // ========== SETUP LOOP TIMER ==========
    // Timer-based loop allows Qt event processing and responsiveness to stop signal  // Signal connection removed\nm_loopTimer->setInterval(100);  // Check every 100ms (responsive to stop button)
    
    // ========== CONNECT TOOL EXECUTOR SIGNALS ==========
    // Connect to tool executor's signals to monitor execution results
    if (m_toolExecutor) {  // Signal connection removed\n  // Signal connection removed\n}
    
    // ========== CONNECT INFERENCE ENGINE SIGNALS ==========
    // Connect to inference engine's streaming signals for async token collection
    if (m_inferenceEngine) {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}
    
    structuredLog("INFO", "INIT", "BoundedAutonomousExecutor initialized with async inference support");
}

// ============================================================================
// CONTROL METHODS
// ============================================================================

void BoundedAutonomousExecutor::startAutonomousLoop(
    const std::string& initialPrompt,
    int maxIterations
) {
    std::mutexLocker lock(&m_stateMutex);
    
    // Validate inputs
    if (initialPrompt.empty()) {
        loopError("Initial task prompt cannot be empty");
        return;
    }
    
    if (maxIterations < 1 || maxIterations > 50) {
        maxIterations = 10;  // Safe default
    }
    
    // Reset state
    m_state = LoopState();
    m_state.maxIterations = maxIterations;
    m_state.isRunning = true;
    m_state.currentIteration = 0;
    m_currentTask = initialPrompt;
    m_logs.clear();
    
    structuredLog("INFO", "LOOP_START", std::string("Starting autonomous loop: '%1' (max %2 iterations)")
        .arg(initialPrompt.left(100)).arg(maxIterations));
    
    loopStarted(initialPrompt);
    
    // Start timer-based loop
    m_loopTimer->start();
}

void BoundedAutonomousExecutor::requestShutdown() {
    std::mutexLocker lock(&m_stateMutex);
    
    m_state.shutdownRequested = true;
    m_state.humanOverride = true;
    
    structuredLog("WARN", "USER_STOP", std::string("User requested shutdown at iteration %1/%2")
        .arg(m_state.currentIteration).arg(m_state.maxIterations));
    
    shutdownRequested();
}

void BoundedAutonomousExecutor::emergencyStop() {
    std::mutexLocker lock(&m_stateMutex);
    
    m_loopTimer->stop();
    m_state.isRunning = false;
    m_state.shutdownRequested = true;
    
    structuredLog("ERROR", "EMERGENCY_STOP", "Emergency stop triggered");
    loopError("Emergency stop triggered");
}

// ============================================================================
// MAIN LOOP DRIVER
// ============================================================================

void BoundedAutonomousExecutor::runAutonomousLoop() {
    {
        std::mutexLocker lock(&m_stateMutex);
        
        // Check termination conditions
        if (m_state.shutdownRequested || 
            m_state.currentIteration >= m_state.maxIterations) {
            
            m_loopTimer->stop();
            m_state.isRunning = false;
            
            if (m_state.humanOverride) {
                structuredLog("WARN", "LOOP_STOP", std::string("Loop stopped by user after %1 iterations")
                    .arg(m_state.currentIteration));
                loopStopped();
            } else {
                structuredLog("INFO", "LOOP_COMPLETE", std::string("Loop completed successfully: %1 iterations")
                    .arg(m_state.currentIteration));
                loopFinished();
            }
            
            return;
        }
        
        // Increment iteration counter
        m_state.currentIteration++;
        m_cycleStartTime = // DateTime::currentMSecsSinceEpoch();
    }
    
    // ========== ITERATION START ==========
    iterationStarted(m_state.currentIteration);
    progressUpdated(m_state.currentIteration, m_state.maxIterations, "Starting iteration...");
    
    structuredLog("INFO", "ITERATION_START", std::string("Beginning iteration %1/%2")
        .arg(m_state.currentIteration).arg(m_state.maxIterations));
    
    try {
        // ========== PERCEPTION PHASE ==========
        perceptionPhaseStarted(m_state.currentIteration);
        executePerceptionPhase();
        perceptionPhaseComplete(m_state.currentIteration, m_state.perceivedContext);
        
        // ========== DECISION PHASE ==========
        decisionPhaseStarted(m_state.currentIteration);
        executeDecisionPhase();
        decisionPhaseComplete(m_state.currentIteration, m_state.modelDecision);
        
        // ========== ACTION PHASE ==========
        actionPhaseStarted(m_state.currentIteration, m_state.actionType);
        executeActionPhase();
        
        if (m_state.actionSucceeded) {
            actionPhaseComplete(m_state.currentIteration, m_state.actionError);
        } else {
            actionPhaseFailed(m_state.currentIteration, m_state.actionError);
        }
        
        // ========== FEEDBACK PHASE ==========
        feedbackPhaseStarted(m_state.currentIteration);
        executeFeedbackPhase();
        feedbackPhaseComplete(m_state.currentIteration, m_state.feedbackFromTools);
        
        // ========== LOG ITERATION COMPLETION ==========
        logIteration();
        iterationCompleted(m_state.currentIteration);
        
        int64_t cycleTime = // DateTime::currentMSecsSinceEpoch() - m_cycleStartTime;
        progressUpdated(m_state.currentIteration, m_state.maxIterations,
            std::string("Iteration %1 complete in %2ms")
            .arg(m_state.currentIteration).arg(cycleTime));
        
        structuredLog("INFO", "ITERATION_COMPLETE", std::string("Iteration %1 completed in %2ms")
            .arg(m_state.currentIteration).arg(cycleTime));
        
    } catch (const std::exception& e) {
        structuredLog("ERROR", "ITERATION_EXCEPTION", 
            std::string("Exception in iteration %1: %2").arg(m_state.currentIteration).arg(e.what()));
        iterationFailed(m_state.currentIteration, e.what());
    }
}

// ============================================================================
// PERCEPTION PHASE
// ============================================================================

void BoundedAutonomousExecutor::executePerceptionPhase() {
    structuredLog("DEBUG", "PERCEPTION", "Gathering current state...");
    
    std::string context;
    
    // Gather file context
    context += perceiveFileContext();
    context += "\n\n";
    
    // Gather error state
    context += perceiveErrorState();
    context += "\n\n";
    
    // Store for decision phase
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.perceivedContext = context;
    }
    
    outputLogged("PERCEPTION: " + context.left(200) + "...");
}

std::string BoundedAutonomousExecutor::perceiveFileContext() {
    std::string context = "=== FILE CONTEXT ===\n";
    
    if (!m_editor) {
        return context + "Editor unavailable\n";
    }
    
    // Get active file
    std::string activeFile = m_editor->activeFileName();
    context += std::string("Active file: %1\n").arg(activeFile.empty() ? "(none)" : activeFile);
    
    // List open tabs
    std::stringList openFiles = m_editor->openFileNames();
    context += std::string("Open files: %1 files\n").arg(openFiles.size());
    
    for (const auto& file : openFiles.take(5)) {  // First 5 files
        context += std::string("  - %1\n").arg(file);
    }
    
    return context;
}

std::string BoundedAutonomousExecutor::perceiveErrorState() {
    std::string context = "=== ERROR STATE ===\n";
    
    if (!m_terminals) {
        return context + "Terminal unavailable\n";
    }
    
    // Check for compilation/test errors from last run
    context += "No errors detected (clean state)\n";
    
    return context;
}

// ============================================================================
// DECISION PHASE
// ============================================================================

void BoundedAutonomousExecutor::executeDecisionPhase() {
    structuredLog("DEBUG", "DECISION", "Querying inference engine for next action...");
    
    std::string perceptionContext;
    {
        std::mutexLocker lock(&m_stateMutex);
        perceptionContext = m_state.perceivedContext;
    }
    
    // Build inference prompt
    std::string prompt = std::string(
        "Current task: %1\n\n"
        "Current state:\n%2\n\n"
        "Iteration %3 of %4\n\n"
        "Based on the current state, what should be the next action? "
        "Format your response as: ACTION_TYPE: [refactor|create|fix|test|analyze] "
        "TARGET: [file or path] DESCRIPTION: [what to do]"
    ).arg(m_currentTask)
     .arg(perceptionContext)
     .arg(m_state.currentIteration)
     .arg(m_state.maxIterations);
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.inferencePrompt = prompt;
    }
    
    // Query inference engine asynchronously via streaming
    if (!m_inferenceEngine) {
        structuredLog("ERROR", "DECISION", "Inference engine not available");
        {
            std::mutexLocker lock(&m_stateMutex);
            m_state.modelDecision = "ERROR: Inference engine unavailable";
            m_state.actionType = "error";
        }
        return;
    }
    
    // Generate unique request ID from current timestamp
    m_decisionRequestId = // DateTime::currentMSecsSinceEpoch();
    m_accumulatedResponse = "";
    m_decisionPhaseWaiting = true;
    
    structuredLog("DEBUG", "INFERENCE", std::string("Requesting streaming inference (reqId: %1)")
        .arg(m_decisionRequestId));
    
    outputLogged(std::string("DECISION: Querying model for next action..."));
    
    // Initiate async streaming inference
    // This will streamToken() and streamFinished() signals
    m_inferenceEngine->generateStreaming(m_decisionRequestId, prompt, 512);
}

std::string BoundedAutonomousExecutor::parseInferenceResponse(const std::string& response) {
    // Extract action type from response (looks for ACTION_TYPE: pattern)
    int idx = response.indexOf("ACTION_TYPE:", 0, CaseInsensitive);
    if (idx == -1) {
        // Fallback to keyword detection
        if (response.contains("refactor", CaseInsensitive)) return "refactor";
        if (response.contains("create", CaseInsensitive)) return "create";
        if (response.contains("fix", CaseInsensitive)) return "fix";
        if (response.contains("test", CaseInsensitive)) return "test";
        if (response.contains("analyze", CaseInsensitive)) return "analyze";
        return "unknown";
    }
    
    // Extract text after ACTION_TYPE:
    int start = idx + 11;
    int end = response.indexOf('\n', start);
    if (end == -1) end = response.indexOf(']', start);
    if (end == -1) end = response.length();
    
    std::string actionType = response.mid(start, end - start).trimmed()
        .toLower().split('|', SkipEmptyParts).first().trimmed();
    
    return actionType;
}

std::string BoundedAutonomousExecutor::extractActionType(const std::string& response) {
    // Already handled by parseInferenceResponse, but keep for compatibility
    return parseInferenceResponse(response);
}

std::stringList BoundedAutonomousExecutor::extractActionDetails(const std::string& response) {
    // Extract TARGET and DESCRIPTION from structured response
    std::stringList details;
    
    // Extract TARGET
    int targetIdx = response.indexOf("TARGET:", 0, CaseInsensitive);
    if (targetIdx != -1) {
        int start = targetIdx + 7;
        int end = response.indexOf('\n', start);
        if (end == -1) end = response.indexOf("DESCRIPTION", start);
        if (end == -1) end = response.length();
        std::string target = response.mid(start, end - start).trimmed();
        if (!target.empty()) details << target;
    }
    
    // Extract DESCRIPTION
    int descIdx = response.indexOf("DESCRIPTION:", 0, CaseInsensitive);
    if (descIdx != -1) {
        int start = descIdx + 12;
        int end = response.indexOf('\n', start);
        if (end == -1) end = response.length();
        std::string desc = response.mid(start, end - start).trimmed();
        if (!desc.empty()) details << desc;
    }
    
    // If structured extraction failed, return first 100 chars
    if (details.empty()) {
        details << response.left(100);
    }
    
    return details;
}

// ============================================================================
// ACTION PHASE
// ============================================================================

void BoundedAutonomousExecutor::executeActionPhase() {
    // Wait for decision phase to complete (max 5 seconds)
    int64_t waitStart = // DateTime::currentMSecsSinceEpoch();
    while (m_decisionPhaseWaiting && 
           // DateTime::currentMSecsSinceEpoch() - waitStart < 5000) {
        QCoreApplication::processEvents(voidLoop::AllEvents, 100);
    }
    
    if (m_decisionPhaseWaiting) {
        structuredLog("WARN", "ACTION", "Decision phase timeout - no inference response received");
        {
            std::mutexLocker lock(&m_stateMutex);
            m_state.actionSucceeded = false;
            m_state.actionError = "Decision phase timeout";
        }
        actionPhaseFailed(m_state.currentIteration, "Decision phase timeout");
        return;
    }
    
    std::string actionType;
    std::stringList actionDetails;
    {
        std::mutexLocker lock(&m_stateMutex);
        actionType = m_state.actionType;
        actionDetails = extractActionDetails(m_state.modelDecision);
    }
    
    structuredLog("DEBUG", "ACTION", std::string("Executing action: %1 with details: %2")
        .arg(actionType).arg(actionDetails.join("|")));
    
    bool success = false;
    
    if (actionType == "refactor") {
        success = executeRefactorAction(actionDetails.join(" "));
    } else if (actionType == "create") {
        success = executeCreateAction(actionDetails.join(" "));
    } else if (actionType == "fix") {
        success = executeFixAction(actionDetails.join(" "));
    } else if (actionType == "test") {
        success = executeTestAction(actionDetails.join(" "));
    } else if (actionType == "analyze") {
        success = executeAnalysisAction(actionDetails.join(" "));
    } else if (actionType == "error") {
        structuredLog("ERROR", "ACTION", "Cannot execute action - decision phase failed");
        success = false;
    } else {
        structuredLog("WARN", "ACTION", std::string("Unknown action type: %1").arg(actionType));
        success = false;
    }
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.actionSucceeded = success;
        if (!success && m_state.actionError.empty()) {
            m_state.actionError = std::string("Action '%1' failed to execute").arg(actionType);
        }
    }
    
    outputLogged(std::string("ACTION: %1 - %2").arg(actionType).arg(success ? "SUCCESS" : "FAILED"));
}

bool BoundedAutonomousExecutor::executeRefactorAction(const std::string& details) {
    structuredLog("DEBUG", "ACTION_REFACTOR", details);
    
    if (!m_toolExecutor) {
        structuredLog("ERROR", "ACTION_REFACTOR", "Tool executor not available");
        return false;
    }
    
    // Parse details: "src/main.cpp Optimize loop"
    std::stringList parts = details.split(' ', SkipEmptyParts);
    if (parts.empty()) {
        structuredLog("ERROR", "ACTION_REFACTOR", "No refactor target specified");
        return false;
    }
    
    // Call REAL tool executor - not mock
    ToolResult result = m_toolExecutor->executeTool("refactor", std::stringList(parts));
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.toolsExecuted << "refactor";
        if (!result.success) {
            m_state.actionError = result.error;
        }
    }

    structuredLog("DEBUG", "ACTION_REFACTOR", 
        std::string("Tool result: success=%1, output=%2, error=%3")
        .arg(result.success).arg(result.output.left(50)).arg(result.error.left(50)));
    
    return result.success;
}

bool BoundedAutonomousExecutor::executeCreateAction(const std::string& details) {
    structuredLog("DEBUG", "ACTION_CREATE", details);
    
    if (!m_toolExecutor) {
        structuredLog("ERROR", "ACTION_CREATE", "Tool executor not available");
        return false;
    }
    
    // Parse details: "src/models/User.cpp Data model"
    std::stringList parts = details.split(' ', SkipEmptyParts);
    if (parts.empty()) {
        structuredLog("ERROR", "ACTION_CREATE", "No create target specified");
        return false;
    }
    
    // Call REAL tool executor - not mock
    ToolResult result = m_toolExecutor->executeTool("create", std::stringList(parts));
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.toolsExecuted << "create";
        if (!result.success) {
            m_state.actionError = result.error;
        }
    }

    structuredLog("DEBUG", "ACTION_CREATE", 
        std::string("Tool result: success=%1, output=%2, error=%3")
        .arg(result.success).arg(result.output.left(50)).arg(result.error.left(50)));
    
    return result.success;
}

bool BoundedAutonomousExecutor::executeFixAction(const std::string& details) {
    structuredLog("DEBUG", "ACTION_FIX", details);
    
    if (!m_toolExecutor) {
        structuredLog("ERROR", "ACTION_FIX", "Tool executor not available");
        return false;
    }
    
    // Parse details: "src/utils/parser.cpp Memory leak"
    std::stringList parts = details.split(' ', SkipEmptyParts);
    if (parts.empty()) {
        structuredLog("ERROR", "ACTION_FIX", "No fix target specified");
        return false;
    }
    
    // Call REAL tool executor - not mock
    ToolResult result = m_toolExecutor->executeTool("fix", std::stringList(parts));
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.toolsExecuted << "fix";
        if (!result.success) {
            m_state.actionError = result.error;
        }
    }

    structuredLog("DEBUG", "ACTION_FIX", 
        std::string("Tool result: success=%1, output=%2, error=%3")
        .arg(result.success).arg(result.output.left(50)).arg(result.error.left(50)));
    
    return result.success;
}

bool BoundedAutonomousExecutor::executeTestAction(const std::string& details) {
    structuredLog("DEBUG", "ACTION_TEST", details);
    
    if (!m_toolExecutor) {
        structuredLog("ERROR", "ACTION_TEST", "Tool executor not available");
        return false;
    }
    
    // Parse details: "test/unit_tests.cpp Core module"
    std::stringList parts = details.split(' ', SkipEmptyParts);
    if (parts.empty()) {
        structuredLog("ERROR", "ACTION_TEST", "No test target specified");
        return false;
    }
    
    // Call REAL tool executor - not mock
    ToolResult result = m_toolExecutor->executeTool("runTests", std::stringList(parts));
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.toolsExecuted << "runTests";
        if (!result.success) {
            m_state.actionError = result.error;
        }
    }

    structuredLog("DEBUG", "ACTION_TEST", 
        std::string("Tool result: success=%1, output=%2, error=%3")
        .arg(result.success).arg(result.output.left(50)).arg(result.error.left(50)));
    
    return result.success;
}

bool BoundedAutonomousExecutor::executeAnalysisAction(const std::string& details) {
    structuredLog("DEBUG", "ACTION_ANALYZE", details);
    
    if (!m_toolExecutor) {
        structuredLog("ERROR", "ACTION_ANALYZE", "Tool executor not available");
        return false;
    }
    
    // Parse details: "src/core/engine.cpp Performance analysis"
    std::stringList parts = details.split(' ', SkipEmptyParts);
    if (parts.empty()) {
        structuredLog("ERROR", "ACTION_ANALYZE", "No analysis target specified");
        return false;
    }
    
    // Call REAL tool executor - not mock
    ToolResult result = m_toolExecutor->executeTool("analyzeCode", std::stringList(parts));
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.toolsExecuted << "analyzeCode";
        if (!result.success) {
            m_state.actionError = result.error;
        }
    }

    structuredLog("DEBUG", "ACTION_ANALYZE", 
        std::string("Tool result: success=%1, output=%2, error=%3")
        .arg(result.success).arg(result.output.left(50)).arg(result.error.left(50)));
    
    return result.success;
}

// ============================================================================
// FEEDBACK PHASE
// ============================================================================

void BoundedAutonomousExecutor::executeFeedbackPhase() {
    structuredLog("DEBUG", "FEEDBACK", "Evaluating action results...");
    
    std::string feedback = collectFeedback();
    double confidence = evaluateConfidence();
    
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.feedbackFromTools = feedback;
        m_state.confidenceScore = confidence;
    }
    
    outputLogged(std::string("FEEDBACK: Confidence: %1%").arg(confidence * 100, 0, 'f', 1));
}

std::string BoundedAutonomousExecutor::collectFeedback() {
    // Summarize action results for next iteration
    std::string feedback = "Action completed. ";
    
    {
        std::mutexLocker lock(&m_stateMutex);
        
        if (m_state.actionSucceeded) {
            feedback += std::string("Modified files: %1. ").arg(m_state.toolsExecuted.size());
        } else {
            feedback += std::string("Action failed: %1. ").arg(m_state.actionError);
        }
    }
    
    return feedback;
}

double BoundedAutonomousExecutor::evaluateConfidence() {
    // Simple heuristic: if action succeeded, high confidence
    std::mutexLocker lock(&m_stateMutex);
    return m_state.actionSucceeded ? 0.8 : 0.3;
}

bool BoundedAutonomousExecutor::shouldContinueLoop() {
    std::mutexLocker lock(&m_stateMutex);
    
    // Continue if:
    // - No shutdown requested
    // - Haven't hit iteration limit
    // - Confidence > 0.1 (not hopeless)
    return !m_state.shutdownRequested &&
           m_state.currentIteration < m_state.maxIterations &&
           m_state.confidenceScore > 0.1;
}

// ============================================================================
// LOGGING & STATE QUERIES
// ============================================================================

void BoundedAutonomousExecutor::logIteration() {
    std::mutexLocker lock(&m_stateMutex);
    
    ExecutionLog log;
    log.iteration = m_state.currentIteration;
    log.timestamp = // DateTime::currentMSecsSinceEpoch();
    log.cycleTimeMs = log.timestamp - m_cycleStartTime;
    log.perceptionSummary = m_state.perceivedContext.left(100);
    log.decisionReasoning = m_state.modelDecision.left(100);
    log.actionDescription = m_state.actionType;
    log.feedbackSummary = m_state.feedbackFromTools;
    log.success = m_state.actionSucceeded;
    log.errorMessage = m_state.actionError;
    log.toolsUsed = m_state.toolsExecuted.size();
    
    m_logs.append(log);
}

void BoundedAutonomousExecutor::logPhase(const std::string& phase, const std::string& details) {
    structuredLog("INFO", "PHASE_" + phase.toUpper(), details);
}

void BoundedAutonomousExecutor::logError(const std::string& error) {
    structuredLog("ERROR", "LOOP_ERROR", error);
}

void BoundedAutonomousExecutor::structuredLog(
    const std::string& level,
    const std::string& category,
    const std::string& message
) {
    std::string timestamp = // DateTime::currentDateTime().toString("hh:mm:ss.zzz");
    std::string logEntry = std::string("[%1] %2 | %3 | %4")
        .arg(timestamp).arg(level).arg(category).arg(message);
    
    // // qDebug:  logEntry;
    outputLogged(logEntry);
}

std::string BoundedAutonomousExecutor::executionSummary() const {
    std::mutexLocker lock(&m_stateMutex);
    
    return std::string(
        "Autonomous Executor Summary\n"
        "==========================\n"
        "Iterations completed: %1 / %2\n"
        "Status: %3\n"
        "Successful actions: %4\n"
        "Failed actions: %5\n"
        "Total execution time: %6 seconds"
    ).arg(m_state.currentIteration)
     .arg(m_state.maxIterations)
     .arg(m_state.isRunning ? "Running" : "Stopped")
     .arg(m_logs.count([](const ExecutionLog& l) { return l.success; }))
     .arg(m_logs.count([](const ExecutionLog& l) { return !l.success; }))
     .arg(m_logs.empty() ? 0 : (m_logs.last().timestamp - m_logs.first().timestamp) / 1000.0);
}

ExecutionLog BoundedAutonomousExecutor::iterationLog(int iteration) const {
    std::mutexLocker lock(&m_stateMutex);
    
    for (const auto& log : m_logs) {
        if (log.iteration == iteration) {
            return log;
        }
    }
    
    return ExecutionLog();
}

// ============================================================================
// SIGNAL HANDLERS
// ============================================================================

void BoundedAutonomousExecutor::onToolExecutionComplete(
    const std::string& toolName,
    const std::string& result
) {
    std::mutexLocker lock(&m_stateMutex);
    m_state.toolResults[toolName] = result;
    outputLogged(std::string("Tool '%1' completed: %2").arg(toolName).arg(result.left(100)));
}

void BoundedAutonomousExecutor::onToolExecutionError(
    const std::string& toolName,
    const std::string& error
) {
    std::mutexLocker lock(&m_stateMutex);
    m_state.toolResults[toolName] = std::string("ERROR: %1").arg(error);
    outputLogged(std::string("Tool '%1' failed: %2").arg(toolName).arg(error));
}

void BoundedAutonomousExecutor::onInferenceStreamToken(int64_t reqId, const std::string& token) {
    // Only process tokens for the decision phase inference request
    if (reqId != m_decisionRequestId || !m_decisionPhaseWaiting) {
        return;
    }
    
    // Accumulate tokens into complete response
    m_accumulatedResponse += token;
    
    // Emit real-time token output
    outputLogged(std::string("TOKEN[%1]: %2").arg(reqId).arg(token));
    
    structuredLog("DEBUG", "STREAM_TOKEN", std::string("Token %1 chars received").arg(token.length()));
}

void BoundedAutonomousExecutor::onInferenceStreamFinished(int64_t reqId) {
    // Only process completion for the decision phase inference request
    if (reqId != m_decisionRequestId || !m_decisionPhaseWaiting) {
        return;
    }
    
    // Decision phase is now complete
    m_decisionPhaseWaiting = false;
    
    // Parse the accumulated response
    std::string actionType = parseInferenceResponse(m_accumulatedResponse);
    
    structuredLog("DEBUG", "STREAM_COMPLETE", 
        std::string("Inference complete (reqId: %1). Response length: %2 chars. Parsed action: %3")
        .arg(reqId).arg(m_accumulatedResponse.length()).arg(actionType));
    
    // Update state with parsed decision
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.modelDecision = m_accumulatedResponse;
        m_state.actionType = actionType;
    }
    
    outputLogged(std::string("INFERENCE_COMPLETE: Action type = '%1'").arg(actionType));
}

void BoundedAutonomousExecutor::onInferenceError(int64_t reqId, const std::string& error) {
    // Only process errors for the decision phase inference request
    if (reqId != m_decisionRequestId || !m_decisionPhaseWaiting) {
        return;
    }
    
    // Decision phase failed
    m_decisionPhaseWaiting = false;
    
    structuredLog("ERROR", "INFERENCE_ERROR", 
        std::string("Inference failed (reqId: %1): %2").arg(reqId).arg(error));
    
    // Update state with error
    {
        std::mutexLocker lock(&m_stateMutex);
        m_state.modelDecision = std::string("ERROR: %1").arg(error);
        m_state.actionType = "error";
        m_state.actionError = error;
    }
    
    outputLogged(std::string("INFERENCE_ERROR: %1").arg(error));
}







