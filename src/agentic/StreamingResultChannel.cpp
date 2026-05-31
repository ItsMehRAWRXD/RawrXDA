// ============================================================================
// StreamingResultChannel Implementation - Phase 1
// Real-time injection of tool results into response stream
// ============================================================================

#include "StreamingResultChannel.h"
#include "../logging/Logger.h"
#include "observability/Logger.hpp"
#include <chrono>
#include <sstream>

namespace RawrXD::Agentic
{

StreamingResultChannel::StreamingResultChannel()
    : m_streamingEnabled(true), m_isRunning(false), m_timeoutMs(60000), m_toolsStarted(0),
      m_toolsCompleted(0), m_toolsFailed(0)
{
}

StreamingResultChannel::~StreamingResultChannel() = default;

void StreamingResultChannel::EmitToolStarted(const std::string& toolName, int toolIndex, int totalTools)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::TOOL_STARTED;
    event.toolName = toolName;
    event.toolIndex = toolIndex;
    event.totalTools = totalTools;
    event.success = true;

    ToolEventCallback toolCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_toolsStarted++;
        m_isRunning = true;
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
    }

    LOG_DEBUG("StreamingResultChannel",
              "Tool started - " + toolName + " (" + std::to_string(toolIndex + 1) + "/" +
                  std::to_string(totalTools) + ")");
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
}

void StreamingResultChannel::EmitToolResult(const std::string& toolName, const std::string& result,
                                            uint64_t executionTimeMs, int toolIndex, int totalTools)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::TOOL_RESULT;
    event.toolName = toolName;
    event.content = result;
    event.executionTimeMs = executionTimeMs;
    event.toolIndex = toolIndex;
    event.totalTools = totalTools;
    event.success = true;

    ToolEventCallback toolCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_toolsCompleted++;
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
    }

    LOG_DEBUG("StreamingResultChannel",
              "Tool completed - " + toolName + " in " + std::to_string(executionTimeMs) + "ms");
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
}

void StreamingResultChannel::EmitToolError(const std::string& toolName, const std::string& errorMessage,
                                           uint64_t executionTimeMs, int toolIndex, int totalTools)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::TOOL_ERROR;
    event.toolName = toolName;
    event.content = errorMessage;
    event.executionTimeMs = executionTimeMs;
    event.toolIndex = toolIndex;
    event.totalTools = totalTools;
    event.success = false;

    ToolEventCallback toolCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_toolsFailed++;
        m_toolsCompleted++;
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
    }

    LOG_WARNING("StreamingResultChannel", "Tool error - " + toolName + ": " + errorMessage);
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
}

void StreamingResultChannel::EmitToolTimeout(const std::string& toolName, uint64_t timeoutMs, int toolIndex,
                                             int totalTools)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::TOOL_TIMEOUT;
    event.toolName = toolName;
    event.content = "Tool execution exceeded timeout (" + std::to_string(timeoutMs) + "ms)";
    event.executionTimeMs = timeoutMs;
    event.toolIndex = toolIndex;
    event.totalTools = totalTools;
    event.success = false;

    ToolEventCallback toolCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_toolsFailed++;
        m_toolsCompleted++;
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
    }

    LOG_WARNING("StreamingResultChannel", "Tool timeout - " + toolName);
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
}

void StreamingResultChannel::EmitToolPartial(const std::string& toolName, const std::string& partialContent,
                                             int toolIndex, int totalTools)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::TOOL_PARTIAL;
    event.toolName = toolName;
    event.content = partialContent;
    event.toolIndex = toolIndex;
    event.totalTools = totalTools;
    event.success = true;

    ToolEventCallback toolCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
    }

    LOG_DEBUG("StreamingResultChannel",
              "Tool partial - " + toolName + " (" + std::to_string(partialContent.size()) + " bytes)");
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
}

void StreamingResultChannel::EmitStreamComplete(const std::string& finalResponse)
{
    ToolExecutionEvent event;
    event.eventType = ToolExecutionEvent::Type::STREAM_COMPLETE;
    event.content = finalResponse;

    ToolEventCallback toolCallback;
    ResultCompleteCallback resultCallback;
    bool streamingEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_emitMutex);
        m_finalResult = finalResponse;
        m_isRunning = false;
        m_events.push_back(event);
        streamingEnabled = m_streamingEnabled;
        toolCallback = m_toolEventCallback;
        resultCallback = m_resultCompleteCallback;
    }

    LOG_INFO("StreamingResultChannel",
             "Stream complete - " + std::to_string(m_toolsCompleted) + " tools completed, " +
                 std::to_string(m_toolsFailed) + " failed");
    if (streamingEnabled && toolCallback)
    {
        toolCallback(event);
    }
    if (streamingEnabled && resultCallback)
    {
        resultCallback(event.content);
    }
}

void StreamingResultChannel::Reset()
{
    std::lock_guard<std::mutex> lock(m_emitMutex);
    m_isRunning = false;
    m_toolsStarted = 0;
    m_toolsCompleted = 0;
    m_toolsFailed = 0;
    m_finalResult.clear();
    m_events.clear();
}

std::string StreamingResultChannel::EventTypeToString(ToolExecutionEvent::Type type)
{
    switch (type)
    {
    case ToolExecutionEvent::Type::TOOL_STARTED:
        return "TOOL_STARTED";
    case ToolExecutionEvent::Type::TOOL_RESULT:
        return "TOOL_RESULT";
    case ToolExecutionEvent::Type::TOOL_ERROR:
        return "TOOL_ERROR";
    case ToolExecutionEvent::Type::TOOL_TIMEOUT:
        return "TOOL_TIMEOUT";
    case ToolExecutionEvent::Type::TOOL_PARTIAL:
        return "TOOL_PARTIAL";
    case ToolExecutionEvent::Type::STREAM_COMPLETE:
        return "STREAM_COMPLETE";
    default:
        return "UNKNOWN";
    }
}

std::string StreamingResultChannel::FormatEvent(const ToolExecutionEvent& event)
{
    std::ostringstream oss;
    oss << "[" << EventTypeToString(event.eventType) << "] ";

    if (!event.toolName.empty())
    {
        oss << "Tool: " << event.toolName;
        if (event.toolIndex >= 0 && event.totalTools > 0)
        {
            oss << " (" << (event.toolIndex + 1) << "/" << event.totalTools << ")";
        }
        oss << " | ";
    }

    if (event.executionTimeMs > 0)
    {
        oss << "Time: " << event.executionTimeMs << "ms | ";
    }

    if (!event.content.empty())
    {
        const size_t maxLen = 100;
        if (event.content.size() > maxLen)
        {
            oss << "Content: " << event.content.substr(0, maxLen) << "... ("
                << (event.content.size() - maxLen) << " more)";
        }
        else
        {
            oss << "Content: " << event.content;
        }
    }

    oss << " | Success: " << (event.success ? "yes" : "no");

    return oss.str();
}

void StreamingResultChannel::emitEvent(const ToolExecutionEvent& event)
{
    // Always record event in history
    m_events.push_back(event);

    // Emit via callbacks if streaming is enabled
    if (m_streamingEnabled)
    {
        if (m_toolEventCallback)
        {
            m_toolEventCallback(event);
        }

        // For STREAM_COMPLETE events, also call result complete callback
        if (event.eventType == ToolExecutionEvent::Type::STREAM_COMPLETE && m_resultCompleteCallback)
        {
            m_resultCompleteCallback(event.content);
        }
    }
}

}  // namespace RawrXD::Agentic
