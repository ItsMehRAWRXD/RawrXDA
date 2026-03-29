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
    // Assuming ActionExecutor has callbacks: onProgress, onComplete, onError
    // Since I refactored ActionExecutor, let's assume I need to set them or it returns a future?
    // The previous code had signals.
    // I'll assume I can set std::function members on m_executor.
    /*
    m_executor->onProgress = [this](int step, int total, const std::string& desc) {
        if (onProgressUpdated) onProgressUpdated(step, total, desc);
    };
    m_executor->onComplete = [this](bool success, const std::string& res) {
        handleExecutionResult(success, res);
    };
    */
    // For now, I'll update m_executor handling when I execute actions.
    
    m_projectRoot = fs::current_path().string();
}

IDEAgentBridge::~IDEAgentBridge() = default;

void IDEAgentBridge::initialize(const std::string& endpoint,
                               const std::string& backend,
                               const std::string& apiKey)
{
    m_invoker->setLLMBackend(backend, endpoint, apiKey);
    std::cout << "[IDEAgentBridge] Initialized with backend: " << backend << std::endl;
}

void IDEAgentBridge::setProjectRoot(const std::string& root)
{
    m_projectRoot = root;
    // Update executor context
    // m_executor->setContext(...) if available
    std::cout << "[IDEAgentBridge] Project root set to: " << root << std::endl;
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

    std::cout << "[IDEAgentBridge] Executing wish: " << wish << std::endl;
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
    
    std::cout << "[IDEAgentBridge] Approving execution..." << std::endl;
    
    // Convert string plan back to json
    json plan = json::parse(m_lastPlanJson);
    
    // Build execution context
    ExecutionContext ctx;
    ctx.projectRoot = m_projectRoot;
    
    m_executor->setContext(ctx);
    
    // Register progress callback for this execution
    m_executor->registerProgressUpdatedCallback(
        [](int current, int total, const char* desc, void* userData) {
            auto* self = static_cast<IDEAgentBridge*>(userData);
            if (self->onProgressUpdated)
                self->onProgressUpdated(current, total, desc ? desc : "");
        }, this);
    
    m_executor->registerPlanCompletedCallback(
        [](bool success, const char* summary, void* userData) {
            auto* self = static_cast<IDEAgentBridge*>(userData);
            self->handleExecutionResult(success, summary ? summary : "Execution completed");
        }, this);
    
    // Convert plan JSON to JsonValue actions array for executor
    JsonValue actions = JsonValue::createArray();
    for (const auto& step : plan) {
        JsonValue action = JsonValue::createObject();
        if (step.contains("action")) action["type"] = step["action"].get<std::string>();
        if (step.contains("description")) action["description"] = step["description"].get<std::string>();
        if (step.contains("params")) action["params"] = step["params"].dump();
        actions.push(action);
    }
    
    // Launch async plan execution via the real executor
    m_executor->executePlan(actions, true /* stopOnError */);
}

void IDEAgentBridge::rejectExecution()
{
    m_isExecuting = false;
    std::cout << "[IDEAgentBridge] Execution rejected." << std::endl;
    m_lastPlanJson.clear();
}

void IDEAgentBridge::cancelExecution()
{
    // m_invoker->cancel();
    // m_executor->cancel();
    m_isExecuting = false;
    std::cout << "[IDEAgentBridge] Cancelled." << std::endl;
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
