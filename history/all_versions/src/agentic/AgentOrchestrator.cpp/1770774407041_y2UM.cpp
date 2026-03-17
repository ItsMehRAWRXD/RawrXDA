// =============================================================================
// AgentOrchestrator.cpp — Agentic Loop Orchestrator Implementation
// =============================================================================
#include "AgentOrchestrator.h"
#include "agentic_observability.h"
#include <chrono>
#include <sstream>
#include <random>
#include <iomanip>
#include <cstdio>

// Shared observability instance for structured logging, metrics, tracing
static AgenticObservability& GetObservability() {
    static AgenticObservability instance;
    return instance;
}

static const char* kComponent = "AgentOrchestrator";

using RawrXD::Agent::AgentOrchestrator;
using RawrXD::Agent::AgentSession;
using RawrXD::Agent::AgentStep;

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AgentOrchestrator::AgentOrchestrator()
    : m_registry(AgentToolRegistry::Instance())
{
    m_client = std::make_unique<AgentOllamaClient>(m_ollamaConfig);
    GetObservability().logInfo(kComponent, "AgentOrchestrator initialized", {
        {"max_tool_rounds", m_config.max_tool_rounds},
        {"max_conversation_tokens", m_config.max_conversation_tokens}
    });
}

AgentOrchestrator::~AgentOrchestrator() {
    Cancel();
    if (m_asyncThread.joinable()) {
        m_asyncThread.join();
    }
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void AgentOrchestrator::SetConfig(const OrchestratorConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void AgentOrchestrator::SetOllamaConfig(const OllamaConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ollamaConfig = config;
    m_client = std::make_unique<AgentOllamaClient>(config);
}

// ---------------------------------------------------------------------------
// Agentic Loop — Main Entry Point
// ---------------------------------------------------------------------------

AgentSession AgentOrchestrator::RunAgentLoop(
    const std::string& user_message,
    StepCallback on_step)
{
    m_running.store(true);
    m_cancelRequested.store(false);
    m_totalSessions.fetch_add(1, std::memory_order_relaxed);

    AgentSession session;
    session.session_id = GenerateSessionId();
    session.completed = false;

    auto totalStart = std::chrono::high_resolution_clock::now();

    auto& obs = GetObservability();
    obs.logInfo(kComponent, "Agent loop started", {
        {"session_id", session.session_id},
        {"user_message_len", static_cast<int>(user_message.size())}
    });
    obs.incrementCounter("agent_sessions_total");
    auto timer = obs.measureDuration("agent_loop");

    // System prompt
    ChatMessage sysMsg;
    sysMsg.role = "system";
    sysMsg.content = m_registry.GetSystemPrompt(
        m_config.working_directory.empty() ? "." : m_config.working_directory,
        {} // open files — to be wired from IDE bridge
    );
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
    if (on_step) on_step(userStep);

    // Agentic loop: call LLM, execute tools, feed results back
    for (int round = 0; round < m_config.max_tool_rounds; ++round) {
        if (m_cancelRequested.load()) break;

        bool hasMoreWork = RunOneRound(session, on_step);
        if (!hasMoreWork) break;

        // Trim history if needed
        TrimHistory(session);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    session.total_elapsed_ms = std::chrono::duration<double, std::milli>(
        totalEnd - totalStart).count();
    session.completed = true;

    // Extract final assistant response
    for (auto it = session.steps.rbegin(); it != session.steps.rend(); ++it) {
        if (it->type == AgentStep::Type::AssistantMessage && !it->content.empty()) {
            session.final_response = it->content;
            break;
        }
    }

    m_currentSession = session;
    m_running.store(false);

    obs.logInfo(kComponent, "Agent loop completed", {
        {"session_id", session.session_id},
        {"tool_calls_made", session.tool_calls_made},
        {"errors_encountered", session.errors_encountered},
        {"total_elapsed_ms", session.total_elapsed_ms},
        {"completed", session.completed}
    });
    obs.recordHistogram("agent_loop_latency", static_cast<float>(session.total_elapsed_ms));
    obs.setGauge("agent_tool_calls_last_session", static_cast<float>(session.tool_calls_made));

    return session;
}

// ---------------------------------------------------------------------------
// Async wrapper
// ---------------------------------------------------------------------------

void AgentOrchestrator::RunAgentLoopAsync(
    const std::string& user_message,
    StepCallback on_step,
    std::function<void(AgentSession)> on_complete)
{
    if (m_asyncThread.joinable()) {
        m_asyncThread.join();
    }

    m_asyncThread = std::thread([this, user_message, on_step, on_complete]() {
        AgentSession session = RunAgentLoop(user_message, on_step);
        if (on_complete) on_complete(session);
    });
}

// ---------------------------------------------------------------------------
// One Round of the Agentic Loop
// ---------------------------------------------------------------------------

bool AgentOrchestrator::RunOneRound(AgentSession& session, StepCallback on_step) {
    // Get tool schemas
    json tools = m_registry.GetToolSchemas();

    auto roundStart = std::chrono::high_resolution_clock::now();

    // Call LLM
    InferenceResult result = m_client->ChatSync(session.messages, tools);

    auto roundEnd = std::chrono::high_resolution_clock::now();
    double roundMs = std::chrono::duration<double, std::milli>(roundEnd - roundStart).count();

    if (!result.success) {
        AgentStep errorStep;
        errorStep.type = AgentStep::Type::Error;
        errorStep.content = "LLM inference failed: " + result.error_message;
        errorStep.elapsed_ms = roundMs;
        session.steps.push_back(errorStep);
        session.errors_encountered++;
        if (on_step) on_step(errorStep);

        GetObservability().logError(kComponent, "LLM inference failed", {
            {"error", result.error_message},
            {"round_ms", roundMs}
        });
        GetObservability().incrementCounter("agent_llm_errors");
        return false;
    }
    GetObservability().recordHistogram("agent_llm_latency", static_cast<float>(roundMs));

    // If the LLM wants to call tools
    if (result.has_tool_calls && !result.tool_calls.empty()) {
        // Record assistant message with tool calls
        ChatMessage assistMsg;
        assistMsg.role = "assistant";
        assistMsg.content = result.response;

        // Build tool_calls JSON for the message
        json tcArray = json::array();
        for (size_t i = 0; i < result.tool_calls.size(); ++i) {
            auto& [name, args] = result.tool_calls[i];
            tcArray.push_back({
                {"id", "call_" + std::to_string(i)},
                {"type", "function"},
                {"function", {
                    {"name", name},
                    {"arguments", args}
                }}
            });
        }
        assistMsg.tool_calls = tcArray;
        session.messages.push_back(assistMsg);

        // Execute each tool call
        for (size_t i = 0; i < result.tool_calls.size(); ++i) {
            if (m_cancelRequested.load()) break;

            auto& [name, args] = result.tool_calls[i];

            AgentStep tcStep;
            tcStep.type = AgentStep::Type::ToolCall;
            tcStep.tool_name = name;
            tcStep.tool_args = args;
            if (on_step) on_step(tcStep);

            // Execute
            auto execStart = std::chrono::high_resolution_clock::now();
            ToolExecResult execResult = m_registry.Dispatch(name, args);
            auto execEnd = std::chrono::high_resolution_clock::now();

            tcStep.tool_result = execResult;
            tcStep.elapsed_ms = std::chrono::duration<double, std::milli>(
                execEnd - execStart).count();

            session.steps.push_back(tcStep);
            session.tool_calls_made++;
            m_totalToolCalls.fetch_add(1, std::memory_order_relaxed);

            // Structured logging: tool call with latency
            GetObservability().logInfo(kComponent, "Tool executed", {
                {"tool", name},
                {"success", execResult.success},
                {"elapsed_ms", tcStep.elapsed_ms}
            });
            GetObservability().recordHistogram("agent_tool_latency", static_cast<float>(tcStep.elapsed_ms));
            GetObservability().incrementCounter("agent_tool_calls_total");

            // Log tool result step
            AgentStep resultStep;
            resultStep.type = AgentStep::Type::ToolResult;
            resultStep.tool_name = name;
            resultStep.content = execResult.output;
            resultStep.tool_result = execResult;
            resultStep.elapsed_ms = tcStep.elapsed_ms;
            session.steps.push_back(resultStep);
            if (on_step) on_step(resultStep);

            if (!execResult.success) {
                session.errors_encountered++;
            }

            // Add tool result to conversation
            ChatMessage toolMsg;
            toolMsg.role = "tool";
            toolMsg.tool_call_id = "call_" + std::to_string(i);

            // Truncate very large outputs
            std::string output = execResult.output;
            if (output.size() > 8192) {
                output = output.substr(0, 8192) + "\n... [truncated at 8KB]";
            }
            toolMsg.content = (execResult.success ? "" : "[ERROR] ") + output;
            session.messages.push_back(toolMsg);

            // Auto-build after file edits
            if (m_config.auto_build_after_edit &&
                (name == "write_file" || name == "replace_in_file") &&
                execResult.success) {
                // Queue a build check for the next round
            }
        }

        return true; // More rounds needed (tool results need to go back to LLM)
    }

    // No tool calls — this is the final response
    AgentStep assistStep;
    assistStep.type = AgentStep::Type::AssistantMessage;
    assistStep.content = result.response;
    assistStep.elapsed_ms = roundMs;
    session.steps.push_back(assistStep);
    if (on_step) on_step(assistStep);

    // Add to conversation
    ChatMessage assistMsg;
    assistMsg.role = "assistant";
    assistMsg.content = result.response;
    session.messages.push_back(assistMsg);

    return false; // Done
}

// ---------------------------------------------------------------------------
// Ghost Text / FIM Mode
// ---------------------------------------------------------------------------

std::string AgentOrchestrator::RequestCompletion(const EditorContext& ctx) {
    FIMBuildResult buildResult = m_fimBuilder.Build(ctx);
    if (!buildResult.success) return "";

    InferenceResult result = m_client->FIMSync(
        buildResult.prompt.prefix,
        buildResult.prompt.suffix,
        buildResult.prompt.filename
    );

    if (!result.success) return "";
    return result.response;
}

void AgentOrchestrator::RequestCompletionStream(
    const EditorContext& ctx,
    TokenCallback on_token,
    DoneCallback on_done,
    ErrorCallback on_error)
{
    FIMBuildResult buildResult = m_fimBuilder.Build(ctx);
    if (!buildResult.success) {
        if (on_error) on_error("FIM prompt build failed: " + buildResult.error);
        return;
    }

    m_client->FIMStream(
        buildResult.prompt.prefix,
        buildResult.prompt.suffix,
        buildResult.prompt.filename,
        on_token,
        on_done,
        on_error
    );
}

// ---------------------------------------------------------------------------
// Session Management
// ---------------------------------------------------------------------------

void AgentOrchestrator::ClearSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentSession = AgentSession{};
}

void AgentOrchestrator::Cancel() {
    m_cancelRequested.store(true);
    if (m_client) m_client->CancelStream();
}

// ---------------------------------------------------------------------------
// History Trimming
// ---------------------------------------------------------------------------

void AgentOrchestrator::TrimHistory(AgentSession& session) {
    // Estimate total tokens in conversation
    int totalTokens = 0;
    for (const auto& msg : session.messages) {
        totalTokens += FIMPromptBuilder::EstimateTokens(msg.content);
    }

    // If within budget, nothing to do
    if (totalTokens <= m_config.max_conversation_tokens) return;

    // Strategy: keep system prompt + last N messages
    // Remove oldest messages (skip system prompt at index 0)
    while (totalTokens > m_config.max_conversation_tokens && session.messages.size() > 3) {
        // Remove the second message (first after system prompt)
        int removedTokens = FIMPromptBuilder::EstimateTokens(session.messages[1].content);
        session.messages.erase(session.messages.begin() + 1);
        totalTokens -= removedTokens;
    }
}

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

std::string AgentOrchestrator::GenerateSessionId() {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();

    std::mt19937 rng(static_cast<unsigned>(ms));
    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream oss;
    oss << "session_" << std::hex;
    for (int i = 0; i < 8; ++i) {
        oss << dist(rng);
    }
    return oss.str();
}
