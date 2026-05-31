// ============================================================================
// agent_controller_minimal.h
// Minimal Agent Controller with Tool Registry
// ============================================================================
// Provides:
// - Tool definitions and registrations
// - Agent loop orchestration (LLM → Tool → LLM)
// - State management across turns
// - Integration hook for existing inference pipeline
// ============================================================================

#pragma once

#include "../context/workspace_context.h"
#include "../orchestration/session_state.h"
#include "execution_contracts.h"
#include "execution_plan_ir.h"


#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>


// Forward declarations for JSON support
namespace nlohmann
{
class json;
}

// Forward declaration for inference engine
namespace RawrXD
{
class InferenceEngine;
}

namespace rawrxd
{

// Minimal tool definition
struct MinimalTool
{
    std::string name;
    std::string description;
    std::unordered_map<std::string, std::string> parameters;  // Simple string-based schema
    std::function<std::string(const std::string&)> handler;   // JSON args → JSON result (as strings)
};

// Agent request/response
struct MinimalAgenticRequest
{
    std::string message;
    std::string session_id;
    std::string model_path;  // GGUF model path
    bool enable_tools = true;
    int max_iterations = 10;  // Safety limit
    /// When set (IDE/CLI), syncs `WorkspaceContext` + tool sandbox (`allowedRoots`) before running.
    std::string workspace_root;
};

/// One executed tool step (for Copilot-style disk + UI replay parity).
struct MinimalAgenticToolStep
{
    std::string tool_name;
    std::string arguments_json;
    std::string result_text;
};

struct ChatMessage
{
    std::string role;
    std::string content;
};

struct MinimalAgenticResponse
{
    std::string final_message;
    bool success = false;
    std::string error;
    int tool_calls_made = 0;
    /// Ordered tool invocations (arguments + engine result) for this response.
    std::vector<MinimalAgenticToolStep> tool_steps;
    /// Messages appended during this request only (starts with the user turn), for IDE/CLI replay + disk parity.
    std::vector<ChatMessage> transcript_delta;
};

struct SessionContext
{
    std::vector<ChatMessage> history;
    size_t total_tokens = 0;
    int retry_count = 0;
    bool fallback_active = false;
};

struct EditorContext
{
    std::string workspaceRoot;
    std::string filePath;
    std::string language;
    std::string selection;
    std::string modelPath;
    int cursorLine = 0;
    int cursorColumn = 0;
};

struct CompletionResult
{
    std::string suggestion;
    bool success = false;
    TrustScore trust = TrustScore::ModelInvalid;
    std::string rationale;
    std::string planId;
    std::string sessionId;
};

struct RewriteResult
{
    std::string rewrittenText;
    bool success = false;
    bool usedModel = false;
    TrustScore trust = TrustScore::DeterministicOnly;
    std::string rationale;
    std::string error;
};

struct DiffPlan
{
    ExecutionPlan plan;
    std::string proposedChange;
    std::string preview;
    bool requiresApproval = true;
};

enum class IdeRequestKind
{
    InlineCompletion,
    SelectionRewrite,
    CodeEdit,
    AgenticTask,
};

struct IdeRequest
{
    IdeRequestKind kind = IdeRequestKind::AgenticTask;
    std::string sessionId;
    std::string workspaceSnapshotRef;
    std::string primaryText;
    std::string secondaryText;
    std::string payload;
    std::string filePath;
    EditorContext editorContext;
    MinimalAgenticRequest agentRequest;
};

struct AgentResponse
{
    bool success = false;
    std::string message;
    std::string planIrPreview;
    std::string planIrId;
    bool hasExecutablePlan = false;
    CompletionResult completion;
    RewriteResult rewrite;
    DiffPlan diff;
    MinimalAgenticResponse agentic{"", false, "", 0, {}, {}};
};

// Minimal Agent Controller with built-in tools
class MinimalAgentController
{
  public:
    static MinimalAgentController& instance();

    // Initialize with available tools
    void initialize();

    // Register built-in tools
    void registerDefaultTools();

    // Main agentic processing loop
    MinimalAgenticResponse process(const MinimalAgenticRequest& request);

    void setWorkspaceRoot(const std::string& workspaceRoot);
    CompletionResult OnInlineCompletionRequest(const std::string& prefix, const EditorContext& ctx);
    std::string DescribeSessionPlanGraph(const std::string& sessionId, size_t maxNodes = 8) const;
    void RecordInlineCompletionFeedback(const std::string& sessionId, const std::string& planId, bool accepted,
                                        const std::string& detail);
    RewriteResult OnSelectionRewrite(const std::string& selectedText, const std::string& instruction,
                                     const EditorContext& ctx);
    DiffPlan OnCodeEditRequest(const std::string& filePath, const std::string& proposedChange,
                               const EditorContext& ctx);
    AgentResponse HandleIDERequest(const IdeRequest& req);

    // Check if agentic mode is available
    bool isAvailable() const { return initialized_; }

    // Optional: Set a reference to the inference engine for LLM calls
    void setInferenceEngine(RawrXD::InferenceEngine* engine) { inference_engine_ = engine; }

  private:
    MinimalAgentController() = default;

    bool initialized_ = false;
    std::vector<MinimalTool> tools_;
    RawrXD::InferenceEngine* inference_engine_ = nullptr;
    WorkspaceContext workspace_context_;
    SessionState session_state_;
    std::unordered_map<std::string, SessionContext> sessions_;
    mutable std::mutex sessions_mutex_;

    // Build system prompt with tool descriptions
    std::string buildSystemPrompt() const;

    // Parse tool calls from LLM response
    std::vector<std::pair<std::string, std::string>> parseToolCalls(const std::string& response);

    // Execute a tool
    std::string executeTool(const std::string& name, const std::string& args_json);

    // Call LLM with messages
    std::string callLLM(const std::string& system_prompt, const std::string& user_message,
                        const std::string& model_path);

    // Helper for JSON parsing
    std::string getJsonValue(const std::string& json_str, const std::string& key);

    TaskIntent classifyIntent(const std::string& message) const;
    ExecutionLane selectLane(TaskIntent intent) const;
    ExecutionPlan buildExecutionPlan(const std::string& message) const;
    TrustScore evaluateTrust(const std::string& response, const std::string& prompt) const;
    AgentResponse EvaluateIDEAction(const IdeRequest& req);
    ExecutionPlanIR EmitPlanIR(const IdeRequest& req) const;
    ExecutionPlanIR ConvertExecutionPlanToIR(const ExecutionPlan& plan, const std::string& userMessage) const;
    std::string DescribePlanIR(const ExecutionPlanIR& plan) const;
    SessionContext loadSessionContext(const std::string& sessionId) const;
    void storeSessionContext(const std::string& sessionId, const SessionContext& session);
    MinimalAgenticResponse executeSyntheticFallback(const MinimalAgenticRequest& request, const ExecutionPlan& plan,
                                                    SessionContext& session, int toolCallsTotal,
                                                    size_t transcriptStartIndex);
};

}  // namespace rawrxd
