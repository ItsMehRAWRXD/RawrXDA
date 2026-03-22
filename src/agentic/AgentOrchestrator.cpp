// =============================================================================
// AgentOrchestrator.cpp — Agentic Loop Orchestrator Implementation
// =============================================================================
#include "AgentOrchestrator.h"
#include "../core/rawrxd_subsystem_api.hpp"

// Windows <wingdi.h> defines ERROR as 0 which clashes with LogLevel::ERROR
#ifdef ERROR
#undef ERROR
#endif
#include "agentic_observability.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

// SCAFFOLD_061: AgentOrchestrator task dispatch implementation
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
}  // namespace

void AgentOrchestrator::DispatchTask(const std::string& task_id, const nlohmann::json& payload)
{
    auto& obs = GetObservability();
    obs.logInfo(kComponent, "Dispatching task", {{"task_id", task_id}, {"payload", payload}});

    std::lock_guard<std::mutex> lock(m_mutex);
    m_taskQueue.push({task_id, payload, std::chrono::system_clock::now()});
    m_taskCv.notify_one();
}

void AgentOrchestrator::ProcessTaskQueue()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_taskCv.wait(lock, [this] { return !m_taskQueue.empty() || m_cancelRequested.load(); });

        if (m_cancelRequested.load())
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
    // run_tool: delegate to ToolRegistry for LLM-style tool execution
    if (payload.contains("action") && payload["action"] == "run_tool")
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
    // prompt: one-shot user message (log; extend with m_client->chat for real one-shot reply if needed)
    if (payload.contains("action") && payload["action"] == "prompt" && payload.contains("text"))
    {
        std::string text = payload["text"].get<std::string>();
        GetObservability().logInfo(kComponent, "ExecuteTask prompt",
                                   {{"task_id", id}, {"text_len", static_cast<int>(text.size())}});
        return;
    }

    // mesh_sync: handoff to Titan Sovereign Link (MASM64) when implemented
    if (payload.contains("action") && payload["action"] == "mesh_sync")
    {
        GetObservability().logInfo(kComponent, "ExecuteTask mesh_sync (no-op)", {{"task_id", id}});
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
    m_client = std::make_unique<AgentOllamaClient>(m_ollamaConfig);

    // Batch 2: Initialize Advanced Agent Coordinator
    m_advancedCoordinator = std::make_unique<Agentic::AdvancedAgentCoordinator>();

    GetObservability().logInfo(kComponent, "AgentOrchestrator initialized",
                               nlohmann::json::object({{"max_tool_rounds", m_config.max_tool_rounds},
                                                       {"max_conversation_tokens", m_config.max_conversation_tokens},
                                                       {"advanced_coordination", true}}));
}

AgentOrchestrator::~AgentOrchestrator()
{
    // Batch 2: Shutdown Advanced Coordinator first
    if (m_advancedCoordinator) {
        m_advancedCoordinator->shutdown();
    }

    Cancel();
    if (m_asyncThread.joinable())
    {
        m_asyncThread.join();
    }
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void AgentOrchestrator::SetConfig(const OrchestratorConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void AgentOrchestrator::SetNativeConfig(const OllamaConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ollamaConfig = config;
    m_client = std::make_unique<AgentOllamaClient>(config);
}

void AgentOrchestrator::SetOllamaConfig(const OllamaConfig& config)
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
    sysMsg.content = m_registry.GetSystemPrompt(cwd, promptFiles);
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
    if (!m_advancedCoordinator) {
        GetObservability().logWarn(kComponent, "Advanced coordination not enabled, using basic dispatch");
        // Fallback to basic task dispatch
        nlohmann::json payload = {
            {"action", "coordinated_task"},
            {"description", taskDescription},
            {"specialization", specialization}
        };
        DispatchTask("coordinated_" + std::to_string(rand()), payload);
        return;
    }

    // Create coordinated task
    auto task = std::make_shared<RawrXD::Agentic::AgentTask>();
    task->id = "coord_" + GenerateSessionId();
    task->description = taskDescription;
    task->specialization = specialization;
    task->parameters = nlohmann::json{
        {"description", taskDescription},
        {"specialization", specialization},
        {"coordinated", true}
    };

    // Submit to advanced coordinator
    m_advancedCoordinator->submitTask(task, priority);

    GetObservability().logInfo(kComponent, "Coordinated task submitted",
                               nlohmann::json::object({
                                   {"task_id", task->id},
                                   {"specialization", specialization},
                                   {"priority", static_cast<int>(priority)}
                               }));
}
