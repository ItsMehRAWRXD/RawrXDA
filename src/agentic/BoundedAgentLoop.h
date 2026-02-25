// ============================================================================
// BoundedAgentLoop.h — Step-Limited Autonomous Agent Loop
// ============================================================================
// The core agent control loop with:
//   - Bounded step count (MAX_STEPS, default 8)
//   - Deterministic state machine
//   - Full transcript logging (every step recorded)
//   - Structured tool results (ToolCallResult, not bool)
//   - Replayable decision trace
//
// Architecture:
//   while (step < MAX_STEPS) {
//       response = LLM::Chat(messages, ToolRegistry::GetSchemas())
//       if (response has tool_call) {
//           result = ToolExecutor::Execute(tool_call)
//           messages.append(BuildToolResultMessage(result))
//       } else {
//           break  // final answer
//       }
//   }
//
// The loop is the brain. The tools are the hands.
// The transcript is the memory.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

#include "ToolCallResult.h"
#include "AgentTranscript.h"
#include "AgentToolHandlers.h"

namespace RawrXD {
namespace Agent {

// ============================================================================
// Agent loop configuration
// ============================================================================
struct AgentLoopConfig {
    int maxSteps                = 8;        // Bounded step limit
    int maxTokensPerRequest     = 8192;     // Context window budget
    std::string model;                              // Auto-detected from Ollama /api/tags
    std::string ollamaBaseUrl   = "http://localhost:11434";
    std::string workingDirectory;
    std::vector<std::string> openFiles;
    bool dryRun                 = false;    // Log but don't execute tools
    bool autoVerify             = true;     // Run diagnostics after edits
    std::string transcriptPath;             // Where to save transcript JSON
};

// ============================================================================
// Agent loop state
// ============================================================================
enum class AgentLoopState {
    Idle,               // Not running
    WaitingForModel,    // Sent request, waiting for LLM response
    ExecutingTool,      // Running a tool handler
    Verifying,          // Auto-verification step
    Complete,           // Final answer received
    StepLimitReached,   // Hit MAX_STEPS without completing
    Error               // Unrecoverable error
};

// ============================================================================
// Callback for streaming agent progress to UI
// ============================================================================
using AgentProgressCallback = std::function<void(int step, int maxSteps,
                                                  const std::string& status,
                                                  const std::string& detail)>;

using AgentCompleteCallback = std::function<void(const std::string& finalAnswer,
                                                  const AgentTranscript& transcript)>;

// ============================================================================
// LLM Chat interface — abstracted for mock/Ollama/custom backends
// ============================================================================
struct LLMChatRequest {
    std::vector<nlohmann::json> messages;   // OpenAI-format message array
    nlohmann::json tools;                   // Function schemas
    std::string model;
    float temperature         = 0.1f;      // Low temp for tool use
    int maxTokens             = 2048;
};

struct LLMChatResponse {
    bool success              = false;
    std::string content;                    // Text content if final answer
    bool hasToolCall          = false;
    std::string toolCallId;
    std::string toolName;
    nlohmann::json toolArgs;
    std::string reasoning;                  // Extracted thinking block
    int promptTokens          = 0;
    int completionTokens      = 0;
    std::string error;
};

using LLMChatFunction = std::function<LLMChatResponse(const LLMChatRequest&)>;

// ============================================================================
// BoundedAgentLoop — The core autonomous agent
// ============================================================================
class BoundedAgentLoop {
public:
    BoundedAgentLoop();
    ~BoundedAgentLoop();

    // ---- Configuration ----
    void Configure(const AgentLoopConfig& config);
    const AgentLoopConfig& GetConfig() const { return m_config; }
    void SetLLMBackend(LLMChatFunction backend);
    void SetProgressCallback(AgentProgressCallback callback);
    void SetCompleteCallback(AgentCompleteCallback callback);

    // ---- Execution ----
    // Run the bounded agent loop synchronously. Returns final answer.
    std::string Execute(const std::string& userPrompt);

    // Run asynchronously (call from UI thread, fires callbacks)
    void ExecuteAsync(const std::string& userPrompt);

    // Cancel in-flight execution
    void Cancel();

    // ---- State ----
    AgentLoopState GetState() const { return m_state.load(); }
    int GetCurrentStep() const { return m_currentStep.load(); }
    const AgentTranscript& GetTranscript() const { return m_transcript; }
    bool IsRunning() const { return m_state.load() == AgentLoopState::WaitingForModel ||
                                    m_state.load() == AgentLoopState::ExecutingTool; }

private:
    // ---- Internal loop ----
    std::string RunLoop(const std::string& userPrompt);

    // ---- Tool dispatch ----
    ToolCallResult DispatchTool(const std::string& name, const nlohmann::json& args);

    // ---- Message building ----
    nlohmann::json BuildSystemMessage();
    nlohmann::json BuildUserMessage(const std::string& prompt);
    nlohmann::json BuildToolResultMessage(const std::string& callId, const ToolCallResult& result);
    nlohmann::json BuildAssistantToolCallMessage(const LLMChatResponse& response);

    // ---- Default LLM backend (Ollama /api/chat) ----
    static LLMChatResponse OllamaChat(const LLMChatRequest& request, const std::string& baseUrl);

    // ---- State ----
    std::atomic<AgentLoopState> m_state{AgentLoopState::Idle};
    std::atomic<int> m_currentStep{0};
    std::atomic<bool> m_cancelled{false};

    AgentLoopConfig m_config;
    AgentTranscript m_transcript;
    LLMChatFunction m_llmBackend;
    AgentProgressCallback m_progressCallback;
    AgentCompleteCallback m_completeCallback;

    mutable std::mutex m_mutex;
};

} // namespace Agent
} // namespace RawrXD
