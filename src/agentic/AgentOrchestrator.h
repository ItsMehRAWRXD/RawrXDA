// =============================================================================
// AgentOrchestrator.h — Agentic Loop Orchestrator
// =============================================================================
// Wires together ToolRegistry, AgentOllamaClient, and FIMPromptBuilder into
// a single agentic loop that can:
//   1. Run multi-turn tool-calling conversations (agentic mode)
//   2. Stream FIM completions for ghost text (completion mode)
//   3. Track conversation history and tool call results
//   4. Coordinate with BBCov/DiffCov for coverage-aware edits
//
// The orchestrator implements the core agent loop:
//   user_message -> LLM -> tool_call -> execute -> tool_result -> LLM -> ...
//
// No exceptions. No std::function in hot path. No Qt dependency.
// =============================================================================
#pragma once

#include "ToolRegistry.h"
#include "AgentOllamaClient.h"
#include "FIMPromptBuilder.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// Orchestrator configuration
// ---------------------------------------------------------------------------
struct OrchestratorConfig {
    int max_tool_rounds = 15;           // Max tool-calling iterations per request
    int max_conversation_tokens = 32000; // Conversation history token budget
    bool auto_build_after_edit = true;   // Run build after write/replace tools
    bool auto_diagnostics = true;        // Get diagnostics after build
    bool coverage_aware = false;         // Use BBCov/DiffCov for verification
    bool log_tool_calls = true;          // Log all tool invocations
    std::string working_directory;       // CWD for the agent
};

// ---------------------------------------------------------------------------
// Agent step — one round in the agentic loop
// ---------------------------------------------------------------------------
struct AgentStep {
    enum class Type : uint8_t {
        UserMessage,
        AssistantMessage,
        ToolCall,
        ToolResult,
        Error
    };

    Type type;
    std::string content;
    std::string tool_name;
    json tool_args;
    ToolExecResult tool_result;
    double elapsed_ms;
};

// ---------------------------------------------------------------------------
// Agent session — tracks the full conversation
// ---------------------------------------------------------------------------
struct AgentSession {
    std::string session_id;
    std::vector<AgentStep> steps;
    std::vector<ChatMessage> messages;  // Full chat history
    int tool_calls_made = 0;
    int errors_encountered = 0;
    double total_elapsed_ms = 0.0;
    bool completed = false;
    std::string final_response;
};

// ---------------------------------------------------------------------------
// Orchestrator callbacks
// ---------------------------------------------------------------------------
using StepCallback      = std::function<void(const AgentStep& step)>;
using CompletionCallback = std::function<void(const std::string& text)>;
using StatusCallback    = std::function<void(const std::string& status)>;

// ---------------------------------------------------------------------------
// AgentOrchestrator
// ---------------------------------------------------------------------------
class AgentOrchestrator {
public:
    AgentOrchestrator();
    ~AgentOrchestrator();

    // -- Configuration --
    void SetConfig(const OrchestratorConfig& config);
    void SetOllamaConfig(const OllamaConfig& config);
    const OrchestratorConfig& GetConfig() const { return m_config; }

    // -- Agentic Mode --
    // Run a full agentic loop: user sends message, agent uses tools iteratively
    AgentSession RunAgentLoop(const std::string& user_message,
                              StepCallback on_step = nullptr);

    // Run agentic loop in background thread
    void RunAgentLoopAsync(const std::string& user_message,
                           StepCallback on_step,
                           std::function<void(AgentSession)> on_complete);

    // -- Ghost Text / FIM Mode --
    // Request FIM completion from current editor state (synchronous)
    std::string RequestCompletion(const EditorContext& ctx);

    // Request FIM completion with streaming tokens
    void RequestCompletionStream(const EditorContext& ctx,
                                 TokenCallback on_token,
                                 DoneCallback on_done,
                                 ErrorCallback on_error);

    // -- Session Management --
    const AgentSession& GetCurrentSession() const { return m_currentSession; }
    void ClearSession();

    // -- Cancel --
    void Cancel();
    bool IsRunning() const { return m_running.load(); }

    // -- Stats --
    uint64_t GetTotalSessions() const { return m_totalSessions.load(); }
    uint64_t GetTotalToolCalls() const { return m_totalToolCalls.load(); }

private:
    // Core agentic loop iteration
    bool RunOneRound(AgentSession& session, StepCallback on_step);

    // Process tool calls from LLM response
    void ExecuteToolCalls(const InferenceResult& result,
                          AgentSession& session,
                          StepCallback on_step);

    // Build messages array from session
    std::vector<ChatMessage> BuildMessages(const AgentSession& session) const;

    // Trim conversation history to fit token budget
    void TrimHistory(AgentSession& session);

    // Trigger auto-build after file edits (write_file / replace_in_file)
    void TriggerAutoBuild(AgentSession& session, StepCallback on_step);

    // Generate session ID
    static std::string GenerateSessionId();

    // Task dispatch (Internal)
    void DispatchTask(const std::string& task_id, const nlohmann::json& payload);
    void ProcessTaskQueue();
    void ExecuteTask(const std::string& id, const nlohmann::json& payload);

private:
    struct ManagedTask {
        std::string id;
        nlohmann::json payload;
        std::chrono::system_clock::time_point queuedAt;
    };
    std::queue<ManagedTask> m_taskQueue;
    std::condition_variable m_taskCv;

    OrchestratorConfig m_config;
    OllamaConfig m_ollamaConfig;

    AgentToolRegistry& m_registry;
    std::unique_ptr<AgentOllamaClient> m_client;
    FIMPromptBuilder m_fimBuilder;

    AgentSession m_currentSession;
    std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};

    std::atomic<uint64_t> m_totalSessions{0};
    std::atomic<uint64_t> m_totalToolCalls{0};

    std::thread m_asyncThread;
};

} // namespace Agent
} // namespace RawrXD
