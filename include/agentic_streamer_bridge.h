#pragma once

#include "maximus_streamer.h"
#include "agentic/agentic_tool_executor.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Maximus {

// =============================================================================
// AGENTIC STREAMING BRIDGE
// =============================================================================
// Connects Agentic::ToolExecutor to MaximusStreamer for structured output parsing
// and semantic streaming of tool calls, results, and reasoning.
// =============================================================================

struct AgenticToolCall {
    std::string id;
    std::string name;
    std::string arguments;
    float confidence = 1.0f;
};

struct AgenticToolResult {
    std::string callId;
    bool success = false;
    std::string output;
    std::string error;
    int durationMs = 0;
};

struct AgenticReasoningStep {
    std::string thought;
    std::string action;
    std::string observation;
    float confidence = 0.0f;
};

using AgenticToolCallback = std::function<void(const AgenticToolCall&)>;
using AgenticResultCallback = std::function<void(const AgenticToolResult&)>;
using AgenticReasoningCallback = std::function<void(const AgenticReasoningStep&)>;

class AgenticStreamerBridge {
public:
    explicit AgenticStreamerBridge(std::shared_ptr<MaximusStreamer> streamer);
    ~AgenticStreamerBridge() = default;

    // Configuration
    void setToolExecutor(std::shared_ptr<Agentic::ToolExecutor> executor);
    void setToolCallback(AgenticToolCallback cb);
    void setResultCallback(AgenticResultCallback cb);
    void setReasoningCallback(AgenticReasoningCallback cb);

    // Main entry: stream agent output with tool call parsing
    void streamAgentOutput(
        const std::string& prompt,
        std::function<void(const std::string&)> onText,
        std::function<void()> onComplete = nullptr
    );

    // Execute a tool call and stream the result
    void executeToolCall(
        const AgenticToolCall& call,
        std::function<void(const AgenticToolResult&)> onResult
    );

    // Parse tool calls from streamed text (JSON/XML format)
    std::vector<AgenticToolCall> parseToolCalls(const std::string& text);

    // Metrics
    struct Stats {
        uint64_t tokensStreamed = 0;
        uint64_t toolCallsDetected = 0;
        uint64_t toolCallsExecuted = 0;
        uint64_t toolCallsFailed = 0;
        double averageToolLatencyMs = 0.0;
        uint64_t reasoningSteps = 0;
    };
    Stats stats() const;
    void resetStats();

    // Access underlying streamer
    std::shared_ptr<MaximusStreamer> streamer() const { return streamer_; }

private:
    std::shared_ptr<MaximusStreamer> streamer_;
    std::shared_ptr<Agentic::ToolExecutor> toolExecutor_;
    
    AgenticToolCallback toolCallback_;
    AgenticResultCallback resultCallback_;
    AgenticReasoningCallback reasoningCallback_;
    
    Stats stats_;
    mutable std::mutex statsMutex_;
    
    // Internal parsing state
    std::string pendingBuffer_;
    mutable std::mutex bufferMutex_;
    
    void processToken_(const Token& tok);
    void detectToolCalls_(const std::string& text);
    void detectReasoning_(const std::string& text);
    AgenticToolResult executeToolInternal_(const AgenticToolCall& call);
};

// =============================================================================
// CONVENIENCE FACTORY
// =============================================================================

inline std::shared_ptr<AgenticStreamerBridge> makeAgenticBridge(
    std::shared_ptr<MaximusStreamer> streamer
) {
    return std::make_shared<AgenticStreamerBridge>(std::move(streamer));
}

} // namespace Maximus
