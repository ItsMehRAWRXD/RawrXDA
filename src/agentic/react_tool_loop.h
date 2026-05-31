// ============================================================================
// react_tool_loop.h — ReAct (Reason + Act) Dynamic Tool Loop
// ============================================================================
// Implements a Reason → Act → Observe → Re-plan loop on top of the
// existing AgenticPlanningOrchestrator.
//
// Architecture:
//   1. Reason:  LLM generates a thought + next action from context
//   2. Act:     Execute the chosen tool via AgentToolRegistry::Dispatch
//   3. Observe: Append tool output to the scratchpad
//   4. Re-plan: If goal not met, loop back to Reason
//
// Anti-loops:
//   - Max iterations (hard cap)
//   - Observation similarity detector (consecutive identical obs → bail)
//   - Tool-call budget per step type
//   - Confidence threshold gate on the thought step
//
// Integrations:
//   - AgenticPlanningOrchestrator (src/agentic/agentic_planning_orchestrator.hpp)
//   - AgentToolRegistry (src/agentic/ToolRegistry.h)
//   - ChainOfThought (src/agentic/chain_of_thought.h)
//   - AutonomousInferenceEngine (src/inference/ultra_fast_inference.h)
//
// Threading: runs on a single orchestration thread; tool calls may spawn
//            their own threads internally.
// ============================================================================
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD {
namespace Agent {

// ---------------------------------------------------------------------------
// Trace entry — one iteration of the ReAct loop
// ---------------------------------------------------------------------------
struct ReactTrace {
    uint32_t    iteration;
    std::string thought;            // LLM's reasoning
    std::string action;             // chosen tool name
    std::string actionArgs;         // JSON args string
    std::string observation;        // tool output
    float       confidence;         // LLM's self-assessed confidence
    double      durationMs;         // wall time for this iteration
    bool        goalReached;        // LLM decided the goal is satisfied
};

// ---------------------------------------------------------------------------
// ReactConfig
// ---------------------------------------------------------------------------
struct ReactConfig {
    uint32_t maxIterations          = 15;     // hard cap on loop count
    uint32_t maxToolCallsPerStep    = 3;      // tool calls per reasoning step
    float    confidenceThreshold    = 0.3f;   // bail if confidence drops below
    float    duplicationThreshold   = 0.95f;  // cosine similarity of consecutive obs
    uint32_t totalTimeoutMs         = 120000; // 2 minutes total
    bool     enableScratchpadPrune  = true;   // drop old obs when scratchpad > N
    uint32_t scratchpadMaxEntries   = 30;     // keep last N entries
};

// ---------------------------------------------------------------------------
// ReactResult — final outcome
// ---------------------------------------------------------------------------
struct ReactResult {
    bool                    goalReached;
    std::string             finalAnswer;
    std::vector<ReactTrace> trace;
    uint32_t                totalIterations;
    double                  totalMs;
    std::string             failReason;   // empty if goal reached
};

// ---------------------------------------------------------------------------
// Callbacks — pluggable LLM + tool execution
// ---------------------------------------------------------------------------

// Given the scratchpad (serialized context), produce a thought + next action.
// Returns: {thought, toolName, toolArgsJson, confidence, goalReached}
struct ReactThought {
    std::string thought;
    std::string toolName;
    std::string toolArgsJson;
    float       confidence;
    bool        goalReached;
    std::string finalAnswer; // only valid when goalReached == true
};

using ThinkFn = std::function<ReactThought(
    const std::string& goal,
    const std::vector<ReactTrace>& scratchpad)>;

// Execute a tool by name.  Returns the observation string.
using ActFn = std::function<std::string(
    const std::string& toolName,
    const std::string& argsJson)>;

// ---------------------------------------------------------------------------
// ReactToolLoop
// ---------------------------------------------------------------------------
class ReactToolLoop {
public:
    explicit ReactToolLoop(const ReactConfig& cfg = {});
    ~ReactToolLoop();

    // Set the reasoning function (wraps LLM inference)
    void setThinkFn(ThinkFn fn)   { m_thinkFn = std::move(fn); }

    // Set the action executor (wraps ToolRegistry::Dispatch)
    void setActFn(ActFn fn)       { m_actFn = std::move(fn); }

    // Run the ReAct loop until the goal is reached or limits hit.
    ReactResult run(const std::string& goal);

    // Abort a running loop (from another thread).
    void abort();

    // Is a loop currently running?
    bool isRunning() const;

private:
    // Simple "is the latest observation nearly identical to the previous?"
    static bool isDuplicate(const std::string& a, const std::string& b,
                            float threshold);

    ReactConfig          m_cfg;
    ThinkFn              m_thinkFn;
    ActFn                m_actFn;

    std::atomic<bool>    m_running{false};
    std::atomic<bool>    m_abortFlag{false};
    mutable std::mutex   m_mu;
};

} // namespace Agent
} // namespace RawrXD
