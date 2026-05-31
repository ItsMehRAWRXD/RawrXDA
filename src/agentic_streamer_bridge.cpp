#include "agentic_streamer_bridge.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <mutex>
#include <regex>
#include <sstream>

namespace Maximus {

// =============================================================================
// CONSTRUCTOR
// =============================================================================

AgenticStreamerBridge::AgenticStreamerBridge(std::shared_ptr<MaximusStreamer> streamer)
    : streamer_(std::move(streamer))
{
}

// =============================================================================
// CONFIGURATION
// =============================================================================

void AgenticStreamerBridge::setToolExecutor(std::shared_ptr<Agentic::ToolExecutor> executor) {
    toolExecutor_ = std::move(executor);
}

void AgenticStreamerBridge::setToolCallback(AgenticToolCallback cb) {
    toolCallback_ = std::move(cb);
}

void AgenticStreamerBridge::setResultCallback(AgenticResultCallback cb) {
    resultCallback_ = std::move(cb);
}

void AgenticStreamerBridge::setReasoningCallback(AgenticReasoningCallback cb) {
    reasoningCallback_ = std::move(cb);
}

// =============================================================================
// MAIN STREAMING ENTRY
// =============================================================================

void AgenticStreamerBridge::streamAgentOutput(
    const std::string& prompt,
    std::function<void(const std::string&)> onText,
    std::function<void()> onComplete
) {
    if (!streamer_) {
        if (onComplete) onComplete();
        return;
    }

    // Reset state
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        pendingBuffer_.clear();
    }

    // Stream with tool call detection
    streamer_->stream(
        prompt,
        [this, onText](const Token& tok) {
            // Forward text to consumer
            if (onText && !tok.text.empty()) {
                onText(std::string(tok.text));
            }

            // Process token for tool calls and reasoning
            processToken_(tok);

            // Update stats
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.tokensStreamed++;
        },
        [this, onComplete]() {
            // Flush remaining buffer
            {
                std::lock_guard<std::mutex> lock(bufferMutex_);
                if (!pendingBuffer_.empty()) {
                    detectToolCalls_(pendingBuffer_);
                    detectReasoning_(pendingBuffer_);
                    pendingBuffer_.clear();
                }
            }

            if (onComplete) onComplete();
        }
    );
}

// =============================================================================
// TOOL CALL EXECUTION
// =============================================================================

void AgenticStreamerBridge::executeToolCall(
    const AgenticToolCall& call,
    std::function<void(const AgenticToolResult&)> onResult
) {
    auto result = executeToolInternal_(call);

    if (onResult) {
        onResult(result);
    }

    if (resultCallback_) {
        resultCallback_(result);
    }
}

// =============================================================================
// PARSING
// =============================================================================

std::vector<AgenticToolCall> AgenticStreamerBridge::parseToolCalls(const std::string& text) {
    std::vector<AgenticToolCall> calls;

    // Pattern 1: JSON tool calls
    // Match: {"name": "tool_name", "arguments": {...}}
    static const std::regex jsonPattern(
        "\\{\\s*\"name\"\\s*:\\s*\"([^\"]+)\"\\s*,\\s*\"arguments\"\\s*:\\s*(\\{[^}]*\\})\\s*\\}",
        std::regex::ECMAScript
    );

    std::sregex_iterator jsonIt(text.begin(), text.end(), jsonPattern);
    std::sregex_iterator jsonEnd;
    for (; jsonIt != jsonEnd; ++jsonIt) {
        AgenticToolCall call;
        call.name = (*jsonIt)[1].str();
        call.arguments = (*jsonIt)[2].str();
        call.id = "call_" + std::to_string(calls.size());
        calls.push_back(std::move(call));
    }

    // Pattern 2: XML-style tool calls
    // Match: <tool name="tool_name">...</tool>
    static const std::regex xmlPattern(
        "<tool\\s+name=\"([^\"]+)\"\\s*>([\\s\\S]*?)<\\/tool>",
        std::regex::ECMAScript
    );

    std::sregex_iterator xmlIt(text.begin(), text.end(), xmlPattern);
    std::sregex_iterator xmlEnd;
    for (; xmlIt != xmlEnd; ++xmlIt) {
        AgenticToolCall call;
        call.name = (*xmlIt)[1].str();
        call.arguments = (*xmlIt)[2].str();
        call.id = "call_" + std::to_string(calls.size());
        calls.push_back(std::move(call));
    }

    // Pattern 3: Markdown code blocks with tool calls
    // Match: ```tool:tool_name\n{...}\n```
    static const std::regex mdPattern(
        "```tool:([^\\n]+)\\n([\\s\\S]*?)\\n```",
        std::regex::ECMAScript
    );

    std::sregex_iterator mdIt(text.begin(), text.end(), mdPattern);
    std::sregex_iterator mdEnd;
    for (; mdIt != mdEnd; ++mdIt) {
        AgenticToolCall call;
        call.name = (*mdIt)[1].str();
        call.arguments = (*mdIt)[2].str();
        call.id = "call_" + std::to_string(calls.size());
        calls.push_back(std::move(call));
    }

    return calls;
}

// =============================================================================
// STATS
// =============================================================================

AgenticStreamerBridge::Stats AgenticStreamerBridge::stats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void AgenticStreamerBridge::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Stats{};
}

// =============================================================================
// INTERNAL PROCESSING
// =============================================================================

void AgenticStreamerBridge::processToken_(const Token& tok) {
    // Accumulate text for parsing
    std::string text;
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        pendingBuffer_ += tok.text;
        text = pendingBuffer_;
    }

    // Detect tool calls when we see sentence boundaries or code blocks
    if (tok.flags & (TokenFlags::EndOfSentence | TokenFlags::EndOfCodeBlock)) {
        detectToolCalls_(text);
        detectReasoning_(text);

        // Keep last N characters for context
        std::lock_guard<std::mutex> lock(bufferMutex_);
        if (pendingBuffer_.size() > 4096) {
            pendingBuffer_ = pendingBuffer_.substr(pendingBuffer_.size() - 2048);
        }
    }
}

void AgenticStreamerBridge::detectToolCalls_(const std::string& text) {
    auto calls = parseToolCalls(text);
    if (calls.empty()) return;

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.toolCallsDetected += calls.size();
    }

    for (auto& call : calls) {
        // Notify callback
        if (toolCallback_) {
            toolCallback_(call);
        }

        // Auto-execute if executor is available
        if (toolExecutor_) {
            executeToolCall(call, [this](const AgenticToolResult& result) {
                std::lock_guard<std::mutex> lock(statsMutex_);
                if (result.success) {
                    stats_.toolCallsExecuted++;
                } else {
                    stats_.toolCallsFailed++;
                }
            });
        }
    }
}

void AgenticStreamerBridge::detectReasoning_(const std::string& text) {
    // Detect reasoning steps in text
    // Pattern: "Thought: ... Action: ... Observation: ..."
    static const std::regex reasoningPattern(
        "Thought:\\s*([\\s\\S]+?)\\s*Action:\\s*([\\s\\S]+?)\\s*Observation:\\s*([\\s\\S]+?)(?:\\n|$)",
        std::regex::ECMAScript
    );

    std::sregex_iterator it(text.begin(), text.end(), reasoningPattern);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        AgenticReasoningStep step;
        step.thought = (*it)[1].str();
        step.action = (*it)[2].str();
        step.observation = (*it)[3].str();
        step.confidence = 0.85f; // Default confidence

        if (reasoningCallback_) {
            reasoningCallback_(step);
        }

        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.reasoningSteps++;
    }
}

AgenticToolResult AgenticStreamerBridge::executeToolInternal_(const AgenticToolCall& call) {
    AgenticToolResult result;
    result.callId = call.id;

    if (!toolExecutor_) {
        result.success = false;
        result.error = "No tool executor configured";
        return result;
    }

    // Build execution request
    Agentic::ExecutionRequest request;
    request.tool_name = call.name;

    // Parse arguments as space-separated or JSON
    if (!call.arguments.empty() && call.arguments[0] == '{') {
        // JSON arguments - try to parse
        try {
            auto json = nlohmann::json::parse(call.arguments);
            for (auto& [key, value] : json.items()) {
                request.args.push_back(key + "=" + value.dump());
            }
        } catch (...) {
            // Fallback: treat as raw argument
            request.args.push_back(call.arguments);
        }
    } else {
        // Space-separated arguments
        std::istringstream iss(call.arguments);
        std::string arg;
        while (iss >> arg) {
            request.args.push_back(arg);
        }
    }

    // Execute
    auto start = std::chrono::steady_clock::now();
    auto execResult = toolExecutor_->execute(request);
    auto end = std::chrono::steady_clock::now();

    result.durationMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
    );
    result.success = execResult.success;
    result.output = execResult.stdout_text;
    result.error = execResult.stderr_text;
    if (!execResult.success && result.error.empty()) {
        result.error = "Tool execution failed with exit code " + std::to_string(execResult.exit_code);
    }

    // Update average latency
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        if (stats_.toolCallsExecuted + stats_.toolCallsFailed > 0) {
            double total = stats_.averageToolLatencyMs * (stats_.toolCallsExecuted + stats_.toolCallsFailed);
            total += result.durationMs;
            stats_.averageToolLatencyMs = total / (stats_.toolCallsExecuted + stats_.toolCallsFailed + 1);
        } else {
            stats_.averageToolLatencyMs = result.durationMs;
        }
    }

    return result;
}

} // namespace Maximus
