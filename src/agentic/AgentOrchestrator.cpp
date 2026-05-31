// =============================================================================
// AgentOrchestrator.cpp — Agentic Loop Orchestrator Implementation
// =============================================================================
#include "AgentOrchestrator.h"
#include "../core/rawrxd_subsystem_api.hpp"
#include "core/thread_lifecycle_registry.h"

// Windows <wingdi.h> defines ERROR as 0 which clashes with LogLevel::ERROR
#ifdef ERROR
#undef ERROR
#endif
#include "agentic_observability.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>

// AgentOrchestrator Task Dispatch Implementation
// Reverse-engineered from IDE integration patterns:
// 1. Task Queue Management
// 2. Priority-based Thread Pooling
// 3. Native-to-Agent Bridging for complex IDE operations (LSP, Build, Debug)

// Shared observability instance for structured logging, metrics, tracing
static AgenticObservability& GetObservability()
{
    static AgenticObservability instance;
    return instance;
}

static const char* kComponent = "AgentOrchestrator";

namespace RawrXD
{
namespace Agent
{

namespace {
struct TaskWorkerState {
    std::atomic<bool> stop{false};
    std::thread thread;
};

std::mutex g_taskWorkerMutex;
std::unordered_map<AgentOrchestrator*, std::unique_ptr<TaskWorkerState>> g_taskWorkers;

TaskWorkerState* FindTaskWorkerState(AgentOrchestrator* orchestrator)
{
    std::lock_guard<std::mutex> lock(g_taskWorkerMutex);
    auto it = g_taskWorkers.find(orchestrator);
    return it != g_taskWorkers.end() ? it->second.get() : nullptr;
}

TaskWorkerState* CreateTaskWorkerState(AgentOrchestrator* orchestrator)
{
    std::lock_guard<std::mutex> lock(g_taskWorkerMutex);
    auto& state = g_taskWorkers[orchestrator];
    if (!state) {
        state = std::make_unique<TaskWorkerState>();
    }
    return state.get();
}

void ShutdownTaskWorkerState(AgentOrchestrator* orchestrator)
{
    std::unique_ptr<TaskWorkerState> state;
    {
        std::lock_guard<std::mutex> lock(g_taskWorkerMutex);
        auto it = g_taskWorkers.find(orchestrator);
        if (it == g_taskWorkers.end()) {
            return;
        }
        state = std::move(it->second);
        g_taskWorkers.erase(it);
    }

    if (!state) {
        return;
    }

    state->stop.store(true);
    if (state->thread.joinable()) {
        state->thread.join();
    }
}

std::vector<std::string> CollectPromptContextFiles(const std::string& rootDir)
{
    namespace fs = std::filesystem;
    std::vector<std::string> files;

    std::error_code ec;
    fs::path base = rootDir.empty() ? fs::path(".") : fs::path(rootDir);
    if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) {
        return files;
    }

    constexpr size_t kMaxFiles = 16;
    for (const auto& entry : fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }

        const fs::path& p = entry.path();
        std::string ext = p.extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".asm" || ext == ".py" || ext == ".md") {
            files.push_back(p.string());
            if (files.size() >= kMaxFiles) {
                break;
            }
        }
    }

    return files;
}

std::string extractToolNameFromPayload(const nlohmann::json& payload)
{
    auto readKey = [&](const char* key) -> std::string {
        return (payload.contains(key) && payload[key].is_string()) ? payload[key].get<std::string>() : std::string{};
    };

    std::string name = readKey("name");
    if (name.empty()) name = readKey("tool_name");
    if (name.empty()) name = readKey("toolName");

    if (name.empty() && payload.contains("tool") && payload["tool"].is_object()) {
        const auto& tool = payload["tool"];
        if (tool.contains("name") && tool["name"].is_string()) {
            name = tool["name"].get<std::string>();
        }
    }

    return name;
}

nlohmann::json extractToolArgsFromPayload(const nlohmann::json& payload)
{
    if (payload.contains("args") && payload["args"].is_object()) return payload["args"];
    if (payload.contains("arguments") && payload["arguments"].is_object()) return payload["arguments"];

    if (payload.contains("tool") && payload["tool"].is_object()) {
        const auto& tool = payload["tool"];
        if (tool.contains("args") && tool["args"].is_object()) return tool["args"];
        if (tool.contains("arguments") && tool["arguments"].is_object()) return tool["arguments"];
    }

    return nlohmann::json::object();
}

std::vector<std::string> BuildZeroDependencyPlan(const std::string& request)
{
    const std::string lower = [&]() {
        std::string s = request;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }();

    std::vector<std::string> plan;
    plan.push_back("Analyze request and identify target files/functions.");

    if (lower.find("search") != std::string::npos || lower.find("find") != std::string::npos) {
        plan.push_back("Run search_code to locate relevant symbols and call paths.");
    }
    if (lower.find("fix") != std::string::npos || lower.find("patch") != std::string::npos ||
        lower.find("edit") != std::string::npos || lower.find("refactor") != std::string::npos) {
        plan.push_back("Apply targeted edits with replace_in_file or write_file.");
    }
    if (lower.find("build") != std::string::npos || lower.find("compile") != std::string::npos ||
        lower.find("error") != std::string::npos || lower.find("test") != std::string::npos) {
        plan.push_back("Execute build/diagnostics and iterate on failures.");
    }

    plan.push_back("Summarize concrete changes and verification evidence.");
    return plan;
}

std::string FormatPlanForSystemPrompt(const std::vector<std::string>& plan)
{
    std::ostringstream oss;
    oss << "\n\nLocal planner (zero-dependency)\n";
    for (size_t i = 0; i < plan.size(); ++i) {
        oss << (i + 1) << ". " << plan[i] << "\n";
    }
    return oss.str();
}
}  // namespace

void AgentOrchestrator::DispatchTask(const std::string& task_id, const nlohmann::json& payload)
{
    auto& obs = GetObservability();
    obs.logInfo(kComponent, "Dispatching task", {{"task_id", task_id}, {"payload", payload}});

    TaskWorkerState* taskWorker = FindTaskWorkerState(this);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!taskWorker || taskWorker->stop.load()) {
        obs.logWarn(kComponent, "Dropping task during orchestrator shutdown", {{"task_id", task_id}});
        return;
    }
    m_taskQueue.push({task_id, payload, std::chrono::system_clock::now()});
    m_taskCv.notify_one();
}

void AgentOrchestrator::ProcessTaskQueue()
{
    TaskWorkerState* taskWorker = FindTaskWorkerState(this);
    if (!taskWorker) {
        GetObservability().logWarn(kComponent, "Task worker state missing; queue processor exiting");
        return;
    }

    while (true)
    {
        CHECK_SHUTDOWN_AND_RETURN();
        
        std::unique_lock<std::mutex> lock(m_mutex);
        m_taskCv.wait(lock, [this, taskWorker] { return !m_taskQueue.empty() || taskWorker->stop.load(); });

        if (taskWorker->stop.load())
            break;

        auto task = m_taskQueue.front();
        m_taskQueue.pop();
        lock.unlock();

        // Task execution logic reverse-engineered from AgenticController
        ExecuteTask(task.id, task.payload);
    }
}

void AgentOrchestrator::ExecuteTask(const std::string& id, const nlohmann::json& payload)
{
    const std::string action =
        (payload.contains("action") && payload["action"].is_string()) ? payload["action"].get<std::string>() : std::string{};

    // run_tool: delegate to ToolRegistry for LLM-style tool execution
    if (action == "run_tool")
    {
        std::string name = extractToolNameFromPayload(payload);
        if (name.empty()) {
            GetObservability().logWarn(kComponent, "ExecuteTask run_tool missing tool name",
                                       {{"task_id", id}, {"payload", payload}});
            return;
        }

        json args = extractToolArgsFromPayload(payload);
        ToolExecResult res = m_registry.Dispatch(name, args);
        (void)res;
        GetObservability().logInfo(kComponent, "ExecuteTask run_tool completed",
                                   {{"task_id", id}, {"tool", name}, {"success", res.success}});
        return;
    }
    // prompt/coordinated_task: run a bounded agent loop against the native tool stack.
    if (action == "prompt" || action == "coordinated_task")
    {
        std::string text;
        if (action == "prompt" && payload.contains("text") && payload["text"].is_string()) {
            text = payload["text"].get<std::string>();
        } else if (payload.contains("description") && payload["description"].is_string()) {
            text = payload["description"].get<std::string>();
        }

        std::string specialization;
        if (payload.contains("specialization") && payload["specialization"].is_string()) {
            specialization = payload["specialization"].get<std::string>();
        }

        if (text.empty()) {
            GetObservability().logWarn(kComponent, "ExecuteTask prompt missing text",
                                       {{"task_id", id}, {"action", action}, {"payload", payload}});
            return;
        }

        AgentSession session;
        session.session_id = id;

        const std::string cwd = m_config.working_directory.empty() ? "." : m_config.working_directory;
        const std::vector<std::string> promptFiles = CollectPromptContextFiles(cwd);

        ChatMessage sysMsg;
        sysMsg.role = "system";
        std::vector<std::string> scopedSources;
        sysMsg.content = m_registry.GetSystemPrompt(cwd, promptFiles, &scopedSources);
        session.applied_instruction_sources = scopedSources;
        if (!scopedSources.empty()) {
            GetObservability().logInfo(kComponent,
                                       "Scoped instructions resolved",
                                       {{"task_id", id}, {"source_count", static_cast<int>(scopedSources.size())}, {"sources", scopedSources}});
        }
        if (!specialization.empty()) {
            sysMsg.content += "\n\nSpecialization focus: " + specialization;
        }
        session.messages.push_back(sysMsg);

        ChatMessage userMsg;
        userMsg.role = "user";
        userMsg.content = text;
        session.messages.push_back(userMsg);

        int maxRounds = m_config.max_tool_rounds;
        if (maxRounds < 1) {
            maxRounds = 1;
        }
        if (maxRounds > 4) {
            maxRounds = 4;
        }

        for (int round = 0; round < maxRounds; ++round)
        {
            if (m_cancelRequested.load()) {
                break;
            }

            const bool hasMoreWork = RunOneRound(session, nullptr);
            if (!hasMoreWork) {
                break;
            }
            TrimHistory(session);
        }

        std::string finalResponse;
        for (auto it = session.steps.rbegin(); it != session.steps.rend(); ++it)
        {
            if (it->type == AgentStep::Type::AssistantMessage && !it->content.empty())
            {
                finalResponse = it->content;
                break;
            }
        }

        GetObservability().logInfo(kComponent, "ExecuteTask conversational task completed",
                                   {{"task_id", id},
                                    {"action", action},
                                    {"specialization", specialization},
                                    {"applied_instruction_source_count", static_cast<int>(session.applied_instruction_sources.size())},
                                    {"applied_instruction_sources", session.applied_instruction_sources},
                                    {"tool_calls_made", session.tool_calls_made},
                                    {"errors_encountered", session.errors_encountered},
                                    {"response_len", static_cast<int>(finalResponse.size())}});
        return;
    }

    // mesh_sync: capture coordination state and execute a real synchronized task pass.
    if (action == "mesh_sync")
    {
        nlohmann::json meshState = {
            {"advanced_coordination", static_cast<bool>(m_advancedCoordinator)},
            {"cancel_requested", m_cancelRequested.load()}
        };

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            meshState["pending_local_tasks"] = m_taskQueue.size();
        }

        if (m_advancedCoordinator) {
            const auto metrics = m_advancedCoordinator->getCoordinatorMetrics();
            meshState["coordinator_metrics"] = {
                {"total_agents", metrics.totalAgents},
                {"healthy_agents", metrics.healthyAgents},
                {"pending_tasks", metrics.pendingTasks},
                {"average_load", metrics.averageLoad},
                {"active_recoveries", metrics.activeRecoveries}
            };
        }

        std::string description = "Synchronize coordinator state and resolve queued agent work.";
        if (payload.contains("description") && payload["description"].is_string()) {
            description = payload["description"].get<std::string>();
        } else if (payload.contains("text") && payload["text"].is_string()) {
            description = payload["text"].get<std::string>();
        }

        GetObservability().logInfo(kComponent, "ExecuteTask mesh_sync snapshot", {{"task_id", id}, {"mesh_state", meshState}});
        ExecuteTask(id + "_mesh", {{"action", "coordinated_task"},
                                     {"description", description},
                                     {"specialization", "mesh_sync"},
                                     {"mesh_state", meshState}});
        return;
    }
    GetObservability().logInfo(kComponent, "ExecuteTask unhandled", {{"task_id", id}, {"payload", payload}});
}

}  // namespace Agent
}  // namespace RawrXD

using RawrXD::Agent::AgentOrchestrator;
using RawrXD::Agent::AgentSession;
using RawrXD::Agent::AgentStep;
using RawrXD::Agent::ChatMessage;
using RawrXD::Agent::InferenceResult;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AgentOrchestrator::AgentOrchestrator() : m_registry(AgentToolRegistry::Instance())
{
    m_client = std::make_unique<NativeInferenceClient>(m_nativeConfig);
    if (TaskWorkerState* taskWorker = CreateTaskWorkerState(this)) {
        taskWorker->thread = std::thread([this]() {
            REGISTER_THREAD("AgentOrchestrator", "task queue processor");
            ProcessTaskQueue();
            RawrXD::Core::ThreadLifecycleRegistry::Instance().MarkExited(std::this_thread::get_id());
        });
    }

    // Batch 2: Initialize Advanced Agent Coordinator
    m_advancedCoordinator = std::make_unique<Agentic::AdvancedAgentCoordinator>();

    GetObservability().logInfo(kComponent, "AgentOrchestrator initialized",
                               nlohmann::json::object({{"max_tool_rounds", m_config.max_tool_rounds},
                                                       {"max_conversation_tokens", m_config.max_conversation_tokens},
                                                       {"advanced_coordination", true}}));
}

AgentOrchestrator::~AgentOrchestrator()
{
    if (TaskWorkerState* taskWorker = FindTaskWorkerState(this)) {
        taskWorker->stop.store(true);
    }
    m_taskCv.notify_all();

    // Batch 2: Shutdown Advanced Coordinator first
    if (m_advancedCoordinator) {
        m_advancedCoordinator->shutdown();
    }

    Cancel();
    if (m_asyncThread.joinable())
    {
        m_asyncThread.join();
    }
    ShutdownTaskWorkerState(this);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void AgentOrchestrator::SetConfig(const OrchestratorConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void AgentOrchestrator::SetNativeConfig(const NativeInferenceConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nativeConfig = config;
    m_client = std::make_unique<NativeInferenceClient>(config);
}

void AgentOrchestrator::SetOllamaConfig(const NativeInferenceConfig& config)
{
    SetNativeConfig(config);
}

// ---------------------------------------------------------------------------
// Agentic Loop — Main Entry Point
// ---------------------------------------------------------------------------

AgentSession AgentOrchestrator::RunAgentLoop(const std::string& user_message, StepCallback on_step)
{
    m_running.store(true);
    m_cancelRequested.store(false);
    m_totalSessions.fetch_add(1, std::memory_order_relaxed);

    AgentSession session;
    session.session_id = GenerateSessionId();
    session.completed = false;

    auto totalStart = std::chrono::high_resolution_clock::now();

    auto& obs = GetObservability();
    obs.logInfo(kComponent, "Agent loop started",
                nlohmann::json::object(
                    {{"session_id", session.session_id}, {"user_message_len", static_cast<int>(user_message.size())}}));
    obs.incrementCounter("agent_sessions_total");
    auto timer = obs.measureDuration("agent_loop");

    // System prompt
    ChatMessage sysMsg;
    sysMsg.role = "system";
    const std::string cwd = m_config.working_directory.empty() ? "." : m_config.working_directory;
    const std::vector<std::string> promptFiles = CollectPromptContextFiles(cwd);
    std::vector<std::string> scopedSources;
    sysMsg.content = m_registry.GetSystemPrompt(cwd, promptFiles, &scopedSources);
    session.applied_instruction_sources = scopedSources;
    if (!scopedSources.empty()) {
        obs.logInfo(kComponent,
                    "Scoped instructions resolved",
                    nlohmann::json::object(
                        {{"session_id", session.session_id},
                         {"source_count", static_cast<int>(scopedSources.size())},
                         {"sources", scopedSources}}));
    }

    // Zero-dependency planner loop: deterministic local planning before tool rounds.
    const auto localPlan = BuildZeroDependencyPlan(user_message);
    sysMsg.content += FormatPlanForSystemPrompt(localPlan);
    session.messages.push_back(sysMsg);

    // User message
    ChatMessage userMsg;
    userMsg.role = "user";
    userMsg.content = user_message;
    session.messages.push_back(userMsg);

    AgentStep userStep;
    userStep.type = AgentStep::Type::UserMessage;
    userStep.content = user_message;
    userStep.elapsed_ms = 0;
    session.steps.push_back(userStep);
    if (on_step)
        on_step(userStep);

    // Emit planner trace step to keep runtime introspectable.
    AgentStep plannerStep;
    plannerStep.type = AgentStep::Type::AssistantMessage;
    plannerStep.content = "[planner]\n" + FormatPlanForSystemPrompt(localPlan);
    plannerStep.elapsed_ms = 0.0;
    session.steps.push_back(plannerStep);
    if (on_step)
        on_step(plannerStep);

    // Agentic loop: call LLM, execute tools, feed results back
    for (int round = 0; round < m_config.max_tool_rounds; ++round)
    {
        if (m_cancelRequested.load())
            break;

        bool hasMoreWork = RunOneRound(session, on_step);
        if (!hasMoreWork)
            break;

        // Trim history if needed
        TrimHistory(session);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    session.total_elapsed_ms = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();
    session.completed = true;

    // Extract final assistant response
    for (auto it = session.steps.rbegin(); it != session.steps.rend(); ++it)
    {
        if (it->type == AgentStep::Type::AssistantMessage && !it->content.empty())
        {
            session.final_response = it->content;
            break;
        }
    }

    m_currentSession = session;
    m_running.store(false);

    obs.logInfo(kComponent, "Agent loop completed",
                nlohmann::json::object({{"session_id", session.session_id},
                                        {"applied_instruction_source_count", static_cast<int>(session.applied_instruction_sources.size())},
                                        {"applied_instruction_sources", session.applied_instruction_sources},
                                        {"tool_calls_made", session.tool_calls_made},
                                        {"errors_encountered", session.errors_encountered},
                                        {"total_elapsed_ms", session.total_elapsed_ms},
                                        {"completed", session.completed}}));
    obs.recordHistogram("agent_loop_latency", static_cast<float>(session.total_elapsed_ms));
    obs.setGauge("agent_tool_calls_last_session", static_cast<float>(session.tool_calls_made));

    return session;
}

// ---------------------------------------------------------------------------
// Async wrapper
// ---------------------------------------------------------------------------

void AgentOrchestrator::RunAgentLoopAsync(const std::string& user_message, StepCallback on_step,
                                          std::function<void(AgentSession)> on_complete)
{
    if (m_asyncThread.joinable())
    {
        m_asyncThread.join();
    }

    m_asyncThread = std::thread(
        [this, user_message, on_step, on_complete]()
        {
            AgentSession session = RunAgentLoop(user_message, on_step);
            if (on_complete)
                on_complete(session);
        });
}

// ---------------------------------------------------------------------------
// One Round of the Agentic Loop
// ---------------------------------------------------------------------------

bool AgentOrchestrator::RunOneRound(AgentSession& session, StepCallback on_step)
{
    // Get tool schemas
    json tools = m_registry.GetToolSchemas();

    auto roundStart = std::chrono::high_resolution_clock::now();

    // Call LLM
    InferenceResult result = m_client->ChatSync(session.messages, tools);

    auto roundEnd = std::chrono::high_resolution_clock::now();
    double roundMs = std::chrono::duration<double, std::milli>(roundEnd - roundStart).count();

    if (!result.success)
    {
        AgentStep errorStep;
        errorStep.type = AgentStep::Type::Error;
        errorStep.content = "LLM inference failed: " + result.error_message;
        errorStep.elapsed_ms = roundMs;
        session.steps.push_back(errorStep);
        session.errors_encountered++;
        if (on_step)
            on_step(errorStep);

        GetObservability().logError(kComponent, "LLM inference failed",
                                    nlohmann::json::object({{"error", result.error_message}, {"round_ms", roundMs}}));
        GetObservability().incrementCounter("agent_llm_errors");
        return false;
    }
    GetObservability().recordHistogram("agent_llm_latency", static_cast<float>(roundMs));

    // If the LLM wants to call tools — delegate to ExecuteToolCalls
    if (result.has_tool_calls && !result.tool_calls.empty())
    {
        ExecuteToolCalls(result, session, on_step);
        return true;  // More rounds needed (tool results need to go back to LLM)
    }

    // No tool calls — this is the final response
    AgentStep assistStep;
    assistStep.type = AgentStep::Type::AssistantMessage;
    assistStep.content = result.response;
    assistStep.elapsed_ms = roundMs;
    session.steps.push_back(assistStep);
    if (on_step)
        on_step(assistStep);

    // Add to conversation
    ChatMessage assistMsg;
    assistMsg.role = "assistant";
    assistMsg.content = result.response;
    session.messages.push_back(assistMsg);

    return false;  // Done
}

// ---------------------------------------------------------------------------
// ExecuteToolCalls — Delegated from RunOneRound
// ---------------------------------------------------------------------------

void AgentOrchestrator::ExecuteToolCalls(const InferenceResult& result, AgentSession& session, StepCallback on_step)
{
    auto& obs = GetObservability();

    // Record assistant message with tool calls
    ChatMessage assistMsg;
    assistMsg.role = "assistant";
    assistMsg.content = result.response;

    json tcArray = json::array();
    for (size_t i = 0; i < result.tool_calls.size(); ++i)
    {
        auto& [name, args] = result.tool_calls[i];
        tcArray.push_back(
            nlohmann::json::object({{"id", "call_" + std::to_string(i)},
                                    {"type", "function"},
                                    {"function", nlohmann::json::object({{"name", name}, {"arguments", args}})}}));
    }
    assistMsg.tool_calls = tcArray;
    session.messages.push_back(assistMsg);

    // Execute each tool call
    for (size_t i = 0; i < result.tool_calls.size(); ++i)
    {
        if (m_cancelRequested.load())
            break;

        auto& [name, args] = result.tool_calls[i];

        AgentStep tcStep;
        tcStep.type = AgentStep::Type::ToolCall;
        tcStep.tool_name = name;
        tcStep.tool_args = args;
        if (on_step)
            on_step(tcStep);

        // Execute
        auto execStart = std::chrono::high_resolution_clock::now();
        ToolExecResult execResult = m_registry.Dispatch(name, args);
        auto execEnd = std::chrono::high_resolution_clock::now();

        tcStep.tool_result = execResult;
        tcStep.elapsed_ms = std::chrono::duration<double, std::milli>(execEnd - execStart).count();

        session.steps.push_back(tcStep);
        session.tool_calls_made++;
        m_totalToolCalls.fetch_add(1, std::memory_order_relaxed);

        // Structured logging: tool call with latency
        obs.logInfo(kComponent, "Tool executed",
                    nlohmann::json::object(
                        {{"tool", name}, {"success", execResult.success}, {"elapsed_ms", tcStep.elapsed_ms}}));
        obs.recordHistogram("agent_tool_latency", static_cast<float>(tcStep.elapsed_ms));
        obs.incrementCounter("agent_tool_calls_total");

        // Log tool result step
        AgentStep resultStep;
        resultStep.type = AgentStep::Type::ToolResult;
        resultStep.tool_name = name;
        resultStep.content = execResult.output;
        resultStep.tool_result = execResult;
        resultStep.elapsed_ms = tcStep.elapsed_ms;
        session.steps.push_back(resultStep);
        if (on_step)
            on_step(resultStep);

        if (!execResult.success)
        {
            session.errors_encountered++;
        }

        // Add tool result to conversation
        ChatMessage toolMsg;
        toolMsg.role = "tool";
        toolMsg.tool_call_id = "call_" + std::to_string(i);

        // Truncate very large outputs
        std::string output = execResult.output;
        if (output.size() > 8192)
        {
            output = output.substr(0, 8192) + "\n... [truncated at 8KB]";
        }
        toolMsg.content = (execResult.success ? "" : "[ERROR] ") + output;
        session.messages.push_back(toolMsg);

        // Auto-build after file edits for immediate feedback
        if (m_config.auto_build_after_edit && (name == "write_file" || name == "replace_in_file") && execResult.success)
        {
            TriggerAutoBuild(session, on_step);
        }
    }
}

// ---------------------------------------------------------------------------
// BuildMessages — Reconstruct messages from session state
// ---------------------------------------------------------------------------

std::vector<ChatMessage> AgentOrchestrator::BuildMessages(const AgentSession& session) const
{
    std::vector<ChatMessage> messages;
    messages.reserve(session.messages.size());

    // Deep copy — needed when passing to LLM client which may mutate
    for (const auto& msg : session.messages)
    {
        ChatMessage copy;
        copy.role = msg.role;
        copy.content = msg.content;
        copy.tool_call_id = msg.tool_call_id;
        copy.tool_calls = msg.tool_calls;
        messages.push_back(std::move(copy));
    }

    return messages;
}

// ---------------------------------------------------------------------------
// Auto-Build Trigger — Queue a build check after file write/replace
// ---------------------------------------------------------------------------

void AgentOrchestrator::TriggerAutoBuild(AgentSession& session, StepCallback on_step)
{
    auto& obs = GetObservability();

    // Run build via the registered run_build tool
    json buildArgs;
    if (!m_config.working_directory.empty())
    {
        buildArgs["directory"] = m_config.working_directory;
    }
    buildArgs["target"] = "all";
    buildArgs["config"] = "Debug";

    auto buildStart = std::chrono::high_resolution_clock::now();
    ToolExecResult buildResult = m_registry.Dispatch("run_build", buildArgs);
    auto buildEnd = std::chrono::high_resolution_clock::now();
    double buildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

    AgentStep buildStep;
    buildStep.type = AgentStep::Type::ToolCall;
    buildStep.tool_name = "run_build";
    buildStep.tool_args = buildArgs;
    buildStep.tool_result = buildResult;
    buildStep.elapsed_ms = buildMs;
    session.steps.push_back(buildStep);
    if (on_step)
        on_step(buildStep);

    obs.logInfo(kComponent, "Auto-build triggered",
                nlohmann::json::object({{"success", buildResult.success}, {"elapsed_ms", buildMs}}));

    // If auto_diagnostics is enabled and build failed, get diagnostics
    if (m_config.auto_diagnostics && !buildResult.success)
    {
        json diagArgs;
        ToolExecResult diagResult = m_registry.Dispatch("get_diagnostics", diagArgs);

        AgentStep diagStep;
        diagStep.type = AgentStep::Type::ToolResult;
        diagStep.tool_name = "get_diagnostics";
        diagStep.content = diagResult.output;
        diagStep.tool_result = diagResult;
        session.steps.push_back(diagStep);
        if (on_step)
            on_step(diagStep);

        // Feed diagnostics back to conversation so LLM can self-correct
        ChatMessage diagMsg;
        diagMsg.role = "tool";
        diagMsg.tool_call_id = "auto_diag";
        diagMsg.content = "[AUTO-BUILD FAILED]\n" + buildResult.output + "\n[DIAGNOSTICS]\n" + diagResult.output;
        session.messages.push_back(diagMsg);
    }

    // Differential Coverage — run DiffCov after successful builds to verify
    // that code changes don't regress coverage (Mode 18: DiffCov)
    if (m_config.coverage_aware && buildResult.success)
    {
        SubsystemParams covParams{};
        covParams.id = SubsystemId::DiffCov;
        SubsystemResult covResult = SubsystemRegistry::instance().invoke(covParams);

        AgentStep covStep;
        covStep.type = AgentStep::Type::ToolResult;
        covStep.tool_name = "get_coverage";
        covStep.content = covResult.detail ? covResult.detail : "";
        session.steps.push_back(covStep);
        if (on_step)
            on_step(covStep);

        if (covResult.success && covResult.artifactPath)
        {
            ChatMessage covMsg;
            covMsg.role = "tool";
            covMsg.tool_call_id = "auto_diffcov";
            covMsg.content =
                "[AUTO-DIFFCOV] Differential coverage analysis complete — see " + std::string(covResult.artifactPath);
            session.messages.push_back(covMsg);
        }
        obs.logInfo(kComponent, "Auto-DiffCov triggered",
                    nlohmann::json::object({{"success", covResult.success},
                                            {"artifact", covResult.artifactPath ? covResult.artifactPath : "none"}}));
    }
}

// ---------------------------------------------------------------------------
// Ghost Text / FIM Mode
// ---------------------------------------------------------------------------

std::string AgentOrchestrator::RequestCompletion(const EditorContext& ctx)
{
    FIMBuildResult buildResult = m_fimBuilder.Build(ctx);
    if (!buildResult.success)
        return "";

    InferenceResult result =
        m_client->FIMSync(buildResult.prompt.prefix, buildResult.prompt.suffix, buildResult.prompt.filename);

    if (!result.success)
        return "";
    return result.response;
}

void AgentOrchestrator::RequestCompletionStream(const EditorContext& ctx, TokenCallback on_token, DoneCallback on_done,
                                                ErrorCallback on_error)
{
    FIMBuildResult buildResult = m_fimBuilder.Build(ctx);
    if (!buildResult.success)
    {
        if (on_error)
            on_error("FIM prompt build failed: " + buildResult.error);
        return;
    }

    m_client->FIMStream(buildResult.prompt.prefix, buildResult.prompt.suffix, buildResult.prompt.filename, on_token,
                        on_done, on_error);
}

// ---------------------------------------------------------------------------
// Session Management
// ---------------------------------------------------------------------------

void AgentOrchestrator::ClearSession()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSession = AgentSession{};
}

void AgentOrchestrator::Cancel()
{
    m_cancelRequested.store(true);
    if (m_client)
        m_client->CancelStream();
}

// ---------------------------------------------------------------------------
// History Trimming
// ---------------------------------------------------------------------------

void AgentOrchestrator::TrimHistory(AgentSession& session)
{
    // Estimate total tokens in conversation
    int totalTokens = 0;
    for (const auto& msg : session.messages)
    {
        totalTokens += FIMPromptBuilder::EstimateTokens(msg.content);
    }

    // If within budget, nothing to do
    if (totalTokens <= m_config.max_conversation_tokens)
        return;

    // Strategy: keep system prompt + last N messages
    // Remove oldest messages (skip system prompt at index 0)
    while (totalTokens > m_config.max_conversation_tokens && session.messages.size() > 3)
    {
        // Remove the second message (first after system prompt)
        int removedTokens = FIMPromptBuilder::EstimateTokens(session.messages[1].content);
        session.messages.erase(session.messages.begin() + 1);
        totalTokens -= removedTokens;
    }
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

std::string AgentOrchestrator::GenerateSessionId()
{
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    std::mt19937 rng(static_cast<unsigned>(ms));
    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream oss;
    oss << "session_" << std::hex;
    for (int i = 0; i < 8; ++i)
    {
        oss << dist(rng);
    }
    return oss.str();
}

// =============================================================================
// Batch 2: Advanced Agent Coordination Implementation
// =============================================================================

void AgentOrchestrator::EnableAdvancedCoordination(const Agentic::ScalingPolicy& scaling,
                                                   const Agentic::RedundancyConfig& redundancy)
{
    if (m_advancedCoordinator) {
        bool success = m_advancedCoordinator->initialize(scaling, redundancy);
        if (success) {
            GetObservability().logInfo(kComponent, "Advanced Agent Coordination enabled",
                                       nlohmann::json::object({
                                           {"min_agents", scaling.minAgents},
                                           {"max_agents", scaling.maxAgents},
                                           {"replication_factor", redundancy.replicationFactor}
                                       }));
        } else {
            GetObservability().logError(kComponent, "Failed to initialize Advanced Agent Coordination");
        }
    }
}

RawrXD::Agentic::AgentMetrics AgentOrchestrator::GetCoordinatorMetrics() const
{
    if (m_advancedCoordinator) {
        return m_advancedCoordinator->getCoordinatorMetrics();
    }
    return RawrXD::Agentic::AgentMetrics{};
}

void AgentOrchestrator::SubmitCoordinatedTask(const std::string& taskDescription,
                                             const std::string& specialization,
                                             RawrXD::Agentic::TaskPriority priority)
{
    const std::string taskId = "coord_" + GenerateSessionId();
    nlohmann::json payload = {
        {"action", "coordinated_task"},
        {"description", taskDescription},
        {"specialization", specialization},
        {"priority", static_cast<int>(priority)}
    };

    if (!m_advancedCoordinator) {
        GetObservability().logWarn(kComponent, "Advanced coordination not enabled, using basic dispatch");
        DispatchTask(taskId, payload);
        return;
    }

    // Create coordinated task
    auto task = std::make_shared<RawrXD::Agentic::AgentTask>();
    task->id = taskId;
    task->description = taskDescription;
    task->specialization = specialization;
    task->parameters = nlohmann::json{
        {"description", taskDescription},
        {"specialization", specialization},
        {"coordinated", true}
    };

    // Submit to advanced coordinator
    m_advancedCoordinator->submitTask(task, priority);
    DispatchTask(task->id, payload);

    GetObservability().logInfo(kComponent, "Coordinated task submitted",
                               nlohmann::json::object({
                                   {"task_id", task->id},
                                   {"specialization", specialization},
                                   {"priority", static_cast<int>(priority)}
                               }));
}
