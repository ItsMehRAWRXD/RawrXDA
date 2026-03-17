#include "ide_agent_bridge.hpp"
#include "model_invoker.hpp"
#include "action_executor.hpp"
#include <chrono>

namespace RawrXD {

IDEAgentBridge::IDEAgentBridge()
    : m_modelInvoker(std::make_unique<ModelInvoker>()),
      m_actionExecutor(std::make_unique<ActionExecutor>()),
      m_projectRoot("."), m_dryRunMode(false), m_requireApproval(true),
      m_executionInProgress(false) {}

IDEAgentBridge::~IDEAgentBridge() {}

void IDEAgentBridge::initialize(const std::string& endpoint, LLMBackend backend) {
    m_modelInvoker->setEndpoint(endpoint);
    m_modelInvoker->setBackend(backend);
    // Connect callbacks to internal handlers
    m_modelInvoker->setPlanGenerationStartedCallback([this](const std::string& w){
        if (m_agentThinkingStartedCallback) m_agentThinkingStartedCallback(w);
    });
    m_modelInvoker->setPlanGeneratedCallback([this](const LLMResponse& resp){
        if (m_agentGeneratedPlanCallback) m_agentGeneratedPlanCallback(resp.plan);
        // Store current plan for later execution
        m_currentPlan = resp.plan;
        // If approval not required, start execution immediately
        if (!m_requireApproval) {
            onPlanGenerated(resp);
        } else {
            if (m_planApprovalNeededCallback) m_planApprovalNeededCallback(resp.plan);
        }
    });
    m_modelInvoker->setInvocationErrorCallback([this](const std::string& err, bool rec){
        if (m_agentErrorCallback) m_agentErrorCallback(err, rec);
    });
    // Action executor callbacks
    m_actionExecutor->setPlanStartedCallback([this](int total){
        if (m_agentExecutionStartedCallback) m_agentExecutionStartedCallback(total);
    });
    m_actionExecutor->setActionStartedCallback([this](int idx, const std::string& desc){
        if (m_agentExecutionProgressCallback) m_agentExecutionProgressCallback(idx, desc, false);
    });
    m_actionExecutor->setActionCompletedCallback([this](int idx, bool success, const ActionResult& res){
        if (m_agentExecutionProgressCallback) m_agentExecutionProgressCallback(idx, res.output, success);
    });
    m_actionExecutor->setPlanCompletedCallback([this](bool success, const PlanResult& res){
        if (m_agentCompletedCallback) {
            std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch());
            m_agentCompletedCallback(res.planId, elapsed);
        }
        addExecutionHistory(m_currentPlan.wish, success, getElapsedTime(), "Plan completed");
    });
    m_actionExecutor->setActionFailedCallback([this](int idx, const std::string& err, bool rec){
        if (m_agentErrorCallback) m_agentErrorCallback(err, rec);
    });
}

void IDEAgentBridge::setProjectRoot(const std::string& root) { m_projectRoot = root; m_actionExecutor->setProjectRoot(root); }
void IDEAgentBridge::setDryRunMode(bool enabled) { m_dryRunMode = enabled; m_actionExecutor->setDryRunMode(enabled); }
void IDEAgentBridge::setRequireApproval(bool enabled) { m_requireApproval = enabled; }

void IDEAgentBridge::executeWish(const std::string& wish, bool requireApproval) {
    m_requireApproval = requireApproval;
    // Trigger model invocation (async)
    m_modelInvoker->invokeAsync(wish, [this](const LLMResponse& resp){
        // Callback already wired via setPlanGeneratedCallback
    });
}

void IDEAgentBridge::planWish(const std::string& wish) {
    // Preview only – do not start execution
    m_modelInvoker->invokeAsync(wish, [this](const LLMResponse& resp){
        if (m_agentGeneratedPlanCallback) m_agentGeneratedPlanCallback(resp.plan);
    });
}

void IDEAgentBridge::approvePlan() {
    if (m_currentPlan.actions.empty()) return;
    // Start execution of stored plan
    m_actionExecutor->executePlanAsync(m_currentPlan, [this](bool success, const PlanResult& result){
        // Execution callbacks already wired in initialize(); optional no-op here
    });
}

void IDEAgentBridge::rejectPlan() {
    // Discard current plan
    m_currentPlan = ExecutionPlan();
    // Notify UI if needed
    if (m_agentErrorCallback) m_agentErrorCallback("Plan rejected by user", false);
}

void IDEAgentBridge::cancelExecution() { m_actionExecutor->cancelExecution(); }
void IDEAgentBridge::pauseExecution() { m_actionExecutor->pauseExecution(); }
void IDEAgentBridge::resumeExecution() { m_actionExecutor->resumeExecution(); }

std::vector<ExecutionHistoryEntry> IDEAgentBridge::getExecutionHistory(int limit) {
    if (limit <= 0 || limit > static_cast<int>(m_executionHistory.size()))
        return m_executionHistory;
    return std::vector<ExecutionHistoryEntry>(m_executionHistory.end() - limit, m_executionHistory.end());
}

void IDEAgentBridge::clearExecutionHistory() { m_executionHistory.clear(); }

void IDEAgentBridge::setModel(const std::string& model) { m_modelInvoker->setModel(model); }
void IDEAgentBridge::setTemperature(float temperature) { m_modelInvoker->setTemperature(temperature); }
void IDEAgentBridge::setMaxTokens(int maxTokens) { m_modelInvoker->setMaxTokens(maxTokens); }

// Callback setters – simply store the user‑provided functions.
void IDEAgentBridge::setAgentThinkingStartedCallback(std::function<void(const std::string&)> cb) { m_agentThinkingStartedCallback = std::move(cb); }
void IDEAgentBridge::setAgentGeneratedPlanCallback(std::function<void(const ExecutionPlan&)> cb) { m_agentGeneratedPlanCallback = std::move(cb); }
void IDEAgentBridge::setPlanApprovalNeededCallback(std::function<void(const ExecutionPlan&)> cb) { m_planApprovalNeededCallback = std::move(cb); }
void IDEAgentBridge::setAgentExecutionStartedCallback(std::function<void(int)> cb) { m_agentExecutionStartedCallback = std::move(cb); }
void IDEAgentBridge::setAgentExecutionProgressCallback(std::function<void(int, const std::string&, bool)> cb) { m_agentExecutionProgressCallback = std::move(cb); }
void IDEAgentBridge::setAgentProgressUpdatedCallback(std::function<void(int, int, std::chrono::milliseconds)> cb) { m_agentProgressUpdatedCallback = std::move(cb); }
void IDEAgentBridge::setAgentCompletedCallback(std::function<void(const std::string&, std::chrono::milliseconds)> cb) { m_agentCompletedCallback = std::move(cb); }
void IDEAgentBridge::setAgentErrorCallback(std::function<void(const std::string&, bool)> cb) { m_agentErrorCallback = std::move(cb); }
void IDEAgentBridge::setUserInputRequestedCallback(std::function<void(const std::string&, const std::vector<std::string>&)> cb) { m_userInputRequestedCallback = std::move(cb); }

// Internal helpers
void IDEAgentBridge::onPlanGenerated(const LLMResponse& response) {
    // Store plan and optionally start execution if auto‑approve
    m_currentPlan = response.plan;
    if (!m_requireApproval) {
        approvePlan();
    } else if (m_planApprovalNeededCallback) {
        m_planApprovalNeededCallback(m_currentPlan);
    }
}

void IDEAgentBridge::addExecutionHistory(const std::string& wish, bool success,
                                        std::chrono::milliseconds duration,
                                        const std::string& summary) {
    ExecutionHistoryEntry entry(wish);
    entry.success = success;
    entry.duration = duration;
    entry.summary = summary;
    m_executionHistory.push_back(entry);
}

std::chrono::milliseconds IDEAgentBridge::getElapsedTime() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

} // namespace RawrXD
