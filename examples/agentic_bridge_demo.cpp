#include "agentic_streamer_bridge.h"
#include "maximus_streamer.h"
#include <iostream>
#include <iomanip>

using namespace Maximus;

// =============================================================================
// DEMO: Agentic Streaming Bridge
// =============================================================================

void demo_simple_agentic_stream() {
    std::cout << "=== Demo: Simple Agentic Stream ===\n\n";

    auto streamer = std::shared_ptr<MaximusStreamer>(makeStreamer());
    auto bridge = makeAgenticBridge(streamer);

    // Set up tool callback
    bridge->setToolCallback([](const AgenticToolCall& call) {
        std::cout << "[TOOL CALL] " << call.name << "(" << call.arguments << ")\n";
    });

    // Set up result callback
    bridge->setResultCallback([](const AgenticToolResult& result) {
        if (result.success) {
            std::cout << "[TOOL RESULT] Success: " << result.output << "\n";
        } else {
            std::cout << "[TOOL ERROR] " << result.error << "\n";
        }
    });

    // Stream agent output
    bridge->streamAgentOutput(
        "Analyze the codebase and suggest improvements",
        [](const std::string& text) {
            std::cout << text;
        },
        []() {
            std::cout << "\n\n[STREAM COMPLETE]\n";
        }
    );

    // Print stats
    auto stats = bridge->stats();
    std::cout << "\nStats:\n"
              << "  Tokens streamed: " << stats.tokensStreamed << "\n"
              << "  Tool calls detected: " << stats.toolCallsDetected << "\n"
              << "  Tool calls executed: " << stats.toolCallsExecuted << "\n"
              << "  Tool calls failed: " << stats.toolCallsFailed << "\n"
              << "  Reasoning steps: " << stats.reasoningSteps << "\n"
              << "  Avg tool latency: " << std::fixed << std::setprecision(2)
              << stats.averageToolLatencyMs << "ms\n";
}

void demo_reasoning_detection() {
    std::cout << "\n=== Demo: Reasoning Detection ===\n\n";

    auto streamer = std::shared_ptr<MaximusStreamer>(makeStreamer());
    auto bridge = makeAgenticBridge(streamer);

    bridge->setReasoningCallback([](const AgenticReasoningStep& step) {
        std::cout << "[REASONING]\n"
                  << "  Thought: " << step.thought << "\n"
                  << "  Action: " << step.action << "\n"
                  << "  Observation: " << step.observation << "\n"
                  << "  Confidence: " << (step.confidence * 100) << "%\n\n";
    });

    bridge->streamAgentOutput(
        "Debug this error: segmentation fault at line 42",
        [](const std::string& text) {
            std::cout << text;
        }
    );
}

void demo_tool_parsing() {
    std::cout << "\n=== Demo: Tool Call Parsing ===\n\n";

    auto streamer = std::shared_ptr<MaximusStreamer>(makeStreamer());
    auto bridge = makeAgenticBridge(streamer);

    // Test JSON tool call parsing
    std::string jsonToolCall = R"({
        "name": "git_status",
        "arguments": {"path": ".", "short": true}
    })";

    auto calls = bridge->parseToolCalls(jsonToolCall);
    std::cout << "Parsed " << calls.size() << " tool call(s):\n";
    for (const auto& call : calls) {
        std::cout << "  - " << call.name << "(" << call.arguments << ")\n";
    }

    // Test XML tool call parsing
    std::string xmlToolCall = R"(<tool name="cmake">
        <arg>--build</arg>
        <arg>.</arg>
        <arg>--config</arg>
        <arg>Release</arg>
    </tool>)";

    calls = bridge->parseToolCalls(xmlToolCall);
    std::cout << "\nParsed " << calls.size() << " XML tool call(s):\n";
    for (const auto& call : calls) {
        std::cout << "  - " << call.name << "(" << call.arguments << ")\n";
    }

    // Test Markdown tool call parsing
    std::string mdToolCall = "```tool:clang-format\n-i --style=LLVM src/main.cpp\n```";

    calls = bridge->parseToolCalls(mdToolCall);
    std::cout << "\nParsed " << calls.size() << " Markdown tool call(s):\n";
    for (const auto& call : calls) {
        std::cout << "  - " << call.name << "(" << call.arguments << ")\n";
    }
}

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << "=== RawrXD Agentic Streamer Bridge Demo ===\n\n";

    demo_simple_agentic_stream();
    demo_reasoning_detection();
    demo_tool_parsing();

    std::cout << "\n=== All demos complete ===\n";
    return 0;
}
