// ============================================================================
// StreamingResultChannel - Phase 1 Implementation
// Real-time injection of tool results into response stream
// ============================================================================

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Agentic
{

// Forward declarations
struct ToolExecutionEvent;
struct StreamingContext;

/// Tool execution event emitted during streaming
struct ToolExecutionEvent
{
    enum class Type
    {
        TOOL_STARTED,      // Tool execution begins
        TOOL_RESULT,       // Tool completed with result
        TOOL_ERROR,        // Tool execution failed
        TOOL_TIMEOUT,      // Tool exceeded timeout
        TOOL_PARTIAL,      // Partial result available (for long-running tools)
        STREAM_COMPLETE    // All tools complete, response ready
    };

    Type eventType;
    std::string toolName;
    std::string content;           // Tool result, error message, or partial output
    uint64_t executionTimeMs = 0;  // How long tool took
    bool success = true;           // Whether tool succeeded
    int toolIndex = 0;             // Which tool in sequence (0-based)
    int totalTools = 0;            // Total number of tools in this batch
};

/// Callback signature for tool events during streaming
using ToolEventCallback = std::function<void(const ToolExecutionEvent&)>;

/// Callback signature for final aggregated result
using ResultCompleteCallback = std::function<void(const std::string&)>;

/// Streaming context for tool execution
/// Manages event emission and result aggregation during tool runs
class StreamingResultChannel
{
  public:
    StreamingResultChannel();
    ~StreamingResultChannel();

    // Non-copyable
    StreamingResultChannel(const StreamingResultChannel&) = delete;
    StreamingResultChannel& operator=(const StreamingResultChannel&) = delete;

    // ---- Configuration ----

    /// Register callback for tool events (called on tool start/result/error/complete)
    void SetToolEventCallback(ToolEventCallback cb)
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_toolEventCallback = std::move(cb);
    }

    /// Register callback for final aggregated result
    void SetResultCompleteCallback(ResultCompleteCallback cb)
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_resultCompleteCallback = std::move(cb);
    }

    /// Enable/disable streaming (if disabled, events still emitted but no callbacks)
    void SetStreamingEnabled(bool enabled)
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_streamingEnabled = enabled;
    }

    /// Set maximum time to wait for all tools to complete (default: 60s)
    void SetTimeoutMs(uint64_t timeoutMs)
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_timeoutMs = timeoutMs;
    }

    // ---- Emission API (called by tool executor) ----

    /// Emit when tool execution starts
    /// @param toolName Name of the tool being executed
    /// @param toolIndex Which tool in the sequence (0-based)
    /// @param totalTools Total number of tools in this batch
    void EmitToolStarted(const std::string& toolName, int toolIndex, int totalTools);

    /// Emit when tool execution completes with result
    /// @param toolName Name of the tool
    /// @param result The tool result/output
    /// @param executionTimeMs How long the tool took
    /// @param toolIndex Position in sequence
    /// @param totalTools Total tools
    void EmitToolResult(const std::string& toolName, const std::string& result, uint64_t executionTimeMs,
                        int toolIndex, int totalTools);

    /// Emit when tool execution fails
    /// @param toolName Name of the tool
    /// @param errorMessage Error description
    /// @param executionTimeMs How long before failure
    /// @param toolIndex Position in sequence
    /// @param totalTools Total tools
    void EmitToolError(const std::string& toolName, const std::string& errorMessage, uint64_t executionTimeMs,
                       int toolIndex, int totalTools);

    /// Emit when tool execution exceeds timeout
    /// @param toolName Name of the tool
    /// @param timeoutMs Configured timeout
    /// @param toolIndex Position in sequence
    /// @param totalTools Total tools
    void EmitToolTimeout(const std::string& toolName, uint64_t timeoutMs, int toolIndex, int totalTools);

    /// Emit partial results for long-running tools (optional)
    /// @param toolName Name of the tool
    /// @param partialContent Partial output so far
    /// @param toolIndex Position in sequence
    /// @param totalTools Total tools
    void EmitToolPartial(const std::string& toolName, const std::string& partialContent, int toolIndex,
                         int totalTools);

    /// Signal completion of all tools (call after last tool done or timeout)
    /// @param finalResponse The complete final response to emit
    void EmitStreamComplete(const std::string& finalResponse);

    // ---- Query API ----

    /// Get all events emitted so far (for logging/replay)
    std::vector<ToolExecutionEvent> GetEvents() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_events;
    }

    /// Get current state (running / complete)
    bool IsRunning() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_isRunning;
    }

    /// Get count of tools that have started
    int GetToolsStarted() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_toolsStarted;
    }

    /// Get count of tools that have completed
    int GetToolsCompleted() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_toolsCompleted;
    }

    /// Get count of tools that failed
    int GetToolsFailed() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_toolsFailed;
    }

    /// Get aggregated result (available after EmitStreamComplete)
    std::string GetFinalResult() const
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        return m_finalResult;
    }

    /// Reset channel for new streaming session
    void Reset();

    // ---- Utility ----

    /// Format a tool event as human-readable string (for logging/display)
    static std::string FormatEvent(const ToolExecutionEvent& event);

    /// Convert event type to string
    static std::string EventTypeToString(ToolExecutionEvent::Type type);

  private:
    // Event emission implementation
    void emitEvent(const ToolExecutionEvent& event);

    // State tracking
    bool m_streamingEnabled = true;
    bool m_isRunning = false;
    uint64_t m_timeoutMs = 60000;

    // Statistics
    int m_toolsStarted = 0;
    int m_toolsCompleted = 0;
    int m_toolsFailed = 0;

    // Event history and final result
    std::vector<ToolExecutionEvent> m_events;
    std::string m_finalResult;

    // Callbacks
    ToolEventCallback m_toolEventCallback;
    ResultCompleteCallback m_resultCompleteCallback;
    mutable std::mutex m_emitMutex;
};

}  // namespace RawrXD::Agentic
