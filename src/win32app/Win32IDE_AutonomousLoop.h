// ============================================================================
// Win32IDE_AutonomousLoop.h
// Real autonomous multi-step agent loop for RawrXD Win32IDE.
//
// Fills the gap documented in IDE_NOT_CAPABLE_AUDIT.md §2:
//   - Agent loop routes through BackendSwitcher (Ollama/cloud/local)
//   - Workspace root + file list injected into every prompt
//   - Real tool execution: read_file, write_file, run_shell,
//     search_workspace, list_dir, apply_diff
//   - Risk-tiered approval gates: AUTO / CONFIRM / BLOCK
//   - Streams tokens to IDE output panel via PostMessage
//   - Human-in-the-loop: mutating tools pause and wait for UI approval
// ============================================================================
#pragma once

#include <windows.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class Win32IDE;

namespace RawrXD {

// ── Risk tier ────────────────────────────────────────────────────────────────
enum class ToolRisk {
    Auto,     // read-only; execute immediately
    Confirm,  // mutating; pause and ask user
    Block     // destructive / shell; blocked unless policy allows
};

// ── A single tool the agent may call ─────────────────────────────────────────
struct AgentTool {
    std::string name;
    std::string description;
    ToolRisk    risk;
    // Returns tool output or error string prefixed with "ERROR:"
    std::function<std::string(const std::string& argsJson)> execute;
};

// ── One step in the running plan ─────────────────────────────────────────────
struct LoopStep {
    int         index      = 0;
    std::string thought;       // model's <think> block
    std::string toolName;      // empty = final answer
    std::string toolArgs;      // raw JSON args string
    std::string toolResult;
    std::string answer;        // set when toolName is empty
    bool        approved   = false;
    bool        blocked    = false;
};

// ── Approval request posted to UI thread ─────────────────────────────────────
struct ApprovalRequest {
    int         stepIndex;
    std::string toolName;
    std::string toolArgs;
    std::string summary;       // human-readable one-liner
    HANDLE      hEvent;        // loop waits on this; UI sets it after decision
    bool        approved = false;
};

// ── Policy controlling what the loop may do ──────────────────────────────────
struct LoopPolicy {
    bool  allowShell        = false;  // run_shell / apply_diff
    bool  allowWrite        = true;   // write_file
    bool  allowRead         = true;   // read_file, list_dir, search_workspace
    int   maxSteps          = 20;
    int   maxOutputBytes    = 256 * 1024;
    bool  autoApproveWrites = false;  // skip confirm for write_file
};

// ── Result returned when the loop finishes ───────────────────────────────────
struct LoopResult {
    bool        success    = false;
    std::string finalAnswer;
    std::string errorMsg;
    int         stepsRun   = 0;
    std::vector<LoopStep> steps;
};

// ── The loop itself ───────────────────────────────────────────────────────────
class AutonomousLoop {
public:
    explicit AutonomousLoop(Win32IDE* ide);
    ~AutonomousLoop();

    // Configure before calling run()
    void setPolicy(const LoopPolicy& p)   { m_policy = p; }
    void setWorkspaceRoot(const std::string& r) { m_workspaceRoot = r; }

    // Approval callback: called on the loop thread when a Confirm-tier tool
    // is about to execute.  Implementation posts to UI thread and blocks.
    using ApprovalFn = std::function<bool(ApprovalRequest&)>;
    void setApprovalCallback(ApprovalFn fn) { m_approvalFn = std::move(fn); }

    // Token streaming callback (called on loop thread, safe to PostMessage)
    using TokenFn = std::function<void(const std::string& token)>;
    void setTokenCallback(TokenFn fn) { m_tokenFn = std::move(fn); }

    // Run synchronously on a background thread (call from detached thread).
    // goal: natural-language task description.
    LoopResult run(const std::string& goal);

    // Signal the running loop to stop after the current step.
    void requestStop() { m_stopRequested.store(true); }
    bool isRunning()   const { return m_running.load(); }

private:
    // ── LLM call ─────────────────────────────────────────────────────────────
    // Routes through Win32IDE::routeInferenceRequest (BackendSwitcher).
    std::string callLLM(const std::string& prompt);

    // ── Prompt construction ───────────────────────────────────────────────────
    std::string buildSystemPrompt() const;
    std::string buildUserPrompt(const std::string& goal,
                                const std::vector<LoopStep>& history) const;

    // ── Response parsing ──────────────────────────────────────────────────────
    // Returns true if a tool call was found; populates step.toolName/toolArgs.
    // Returns false if this is a final answer; populates step.answer.
    bool parseModelOutput(const std::string& raw, LoopStep& step) const;

    // ── Tool registry ─────────────────────────────────────────────────────────
    void registerTools();
    const AgentTool* findTool(const std::string& name) const;

    // ── Tool implementations ──────────────────────────────────────────────────
    std::string toolReadFile(const std::string& argsJson);
    std::string toolWriteFile(const std::string& argsJson);
    std::string toolListDir(const std::string& argsJson);
    std::string toolSearchWorkspace(const std::string& argsJson);
    std::string toolRunShell(const std::string& argsJson);
    std::string toolApplyDiff(const std::string& argsJson);

    // ── Approval gate ─────────────────────────────────────────────────────────
    bool requestApproval(LoopStep& step, const AgentTool& tool);

    // ── Helpers ───────────────────────────────────────────────────────────────
    void emit(const std::string& text);   // stream to output panel
    std::string workspacePath(const std::string& rel) const;
    bool isPathSafe(const std::string& abs) const;

    // ── JSON helpers (no external deps) ──────────────────────────────────────
    static std::string jsonGetString(const std::string& json,
                                     const std::string& key);

    Win32IDE*              m_ide;
    LoopPolicy             m_policy;
    std::string            m_workspaceRoot;
    ApprovalFn             m_approvalFn;
    TokenFn                m_tokenFn;
    std::vector<AgentTool> m_tools;
    std::atomic<bool>      m_stopRequested{false};
    std::atomic<bool>      m_running{false};
    mutable std::mutex     m_mutex;
};

} // namespace RawrXD
