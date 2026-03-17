#include "ide_agent_bridge.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

IDEAgentBridge::IDEAgentBridge() {
    m_invoker = std::make_unique<ModelInvoker>();
    m_executor = std::make_unique<ActionExecutor>();

    // Wire up ModelInvoker callbacks
    m_invoker->onThinkingStarted = [this]() {
        if (onAgentThinkingStarted) onAgentThinkingStarted("Agent is thinking...");
    };

    m_invoker->onPlanGenerated = [this](const LLMResponse& resp) {
        handlePlanGenerated(resp);
    };

    m_invoker->onInvocationError = [this](const std::string& err, bool retry) {
        m_isExecuting = false;
        if (onAgentError) onAgentError(err, retry);
    };

    // Wire up ActionExecutor callbacks
    m_executor->onProgressUpdated = [this](int step, int total) {
         if (onProgressUpdated) onProgressUpdated(step, total, "Action in progress...");
    };
    
    m_executor->onActionStarted = [this](int step, const std::string& desc) {
         // Update progress with description
         // We might need to access total actions from executor context if not passed, 
         // but onProgressUpdated usually fires right after? 
         // Let's assume onProgressUpdated handles the count, we just want to ensure description matches.
         // Actually onProgressUpdated(step, total) is called by executor. 
         // onActionStarted is called separately.
         // Let's try to pass the description in a way onProgressUpdated logic can use?
         // Simplest: Just fire onProgressUpdated again with desc.
         int total = m_executor->context().totalActions;
         if (onProgressUpdated) onProgressUpdated(step, total, desc);
    };

    m_executor->onPlanCompleted = [this](bool success, const nlohmann::json& result) {
         handleExecutionResult(success, result.dump());
    };
    
    m_projectRoot = fs::current_path().string();
}

IDEAgentBridge::~IDEAgentBridge() = default;

void IDEAgentBridge::initialize(const std::string& endpoint,
                               const std::string& backend,
                               const std::string& apiKey)
{
    m_invoker->setLLMBackend(backend, endpoint, apiKey);
    
}

void IDEAgentBridge::setProjectRoot(const std::string& root)
{
    m_projectRoot = root;
    // Update executor context
    // m_executor->setContext(...) if available
    
}

void IDEAgentBridge::executeWish(const std::string& wish, bool requireApproval)
{
    if (m_isExecuting) {
        if (onAgentError) onAgentError("Execution already in progress", false);
        return;
    }

    if (wish.empty()) {
        if (onAgentError) onAgentError("Wish cannot be empty", false);
        return;
    }

    m_isExecuting = true;
    m_requireApproval = requireApproval;

    InvocationParams params;
    params.wish = wish;
    params.context = buildExecutionContext().dump();
    params.availableTools = {"search_files", "file_edit", "run_build",
                             "execute_tests", "commit_git", "invoke_command"};


    m_invoker->invokeAsync(params);
}

void IDEAgentBridge::planWish(const std::string& wish)
{
    executeWish(wish, true); // Force approval -> preview mode
}

void IDEAgentBridge::handlePlanGenerated(const LLMResponse& response)
{
    if (!response.success) {
        m_isExecuting = false;
        if (onAgentError) onAgentError(response.error, false);
        return;
    }

    m_lastPlanJson = response.parsedPlan.dump(); // Store plan

    if (onPlanGenerated) onPlanGenerated(m_lastPlanJson);

    if (m_requireApproval) {
        if (onApprovalRequested) onApprovalRequested(m_lastPlanJson);
        // Wait for user approval
    } else {
        approveExecution();
    }
}

void IDEAgentBridge::approveExecution()
{
    if (m_lastPlanJson.empty()) return;

    try {
        json plan = json::parse(m_lastPlanJson);
        
        ExecutionContext ctx;
        ctx.projectRoot = m_projectRoot;
        ctx.dryRun = m_dryRun;
        m_executor->setContext(ctx);

        // Execute async
        m_executor->executePlan(plan);
    } catch (const std::exception& e) {
        if (onAgentError) onAgentError(std::string("Execution failed to start: ") + e.what(), false);
        m_isExecuting = false;
    }
}

void IDEAgentBridge::rejectExecution()
{
    m_isExecuting = false;
    
    m_lastPlanJson.clear();
}

void IDEAgentBridge::cancelExecution()
{
    // m_invoker->cancel();
    // m_executor->cancel();
    m_isExecuting = false;
    
}

void IDEAgentBridge::setDryRun(bool dryRun)
{
    m_dryRun = dryRun;
}

json IDEAgentBridge::buildExecutionContext() {
    json ctx;
    ctx["project_root"] = m_projectRoot;
    ctx["os"] = "windows";
    return ctx;
}

void IDEAgentBridge::handleExecutionResult(bool success, const std::string& result)
{
    m_isExecuting = false;
    if (onExecutionComplete) onExecutionComplete(result);
}
